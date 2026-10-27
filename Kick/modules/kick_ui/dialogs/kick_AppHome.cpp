/*
 * Copyright (c) 2021 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <future>

#include "kick_AppHome.h"
#include "../modules/kick_ui_settings/source/kick_ui_settings_cv.h"
#include "program_edit_ui/program_edit_ui.h"
#include "contracts/dcl/dcl_signals.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "contracts/utils/keyboard/keyboard_detector.h"

namespace kick_ui
{
    sigslot::signal<Screen> signals::sigScreenClose = {};
    sigslot::signal<Screen> signals::sigScreenOpen  = {};
    sigslot::signal<Screen> signals::onScreenLoaded = {};
    sigslot::signal<Screen> signals::onScreenClosed = {};

    sigslot::signal<Screen, void*> signals::onScreenData = {};

    class TempScreen
        : public IScreen
        , public juce::Component
    {
    public:
        TempScreen()
            : IScreen( Screen::Temp )
        {
            setLookAndFeel( ui::KickLooknFeel::getLnf() );

            header.showSettingsButton( true );
            addAndMakeVisible( header );

            for( auto row = 7; row >= 0; --row )
            {
                auto pB     = &pads[row];
                auto pBText = "Pad-" + juce::String( row + 1 );
                pB->setToggleButtonText( pBText, pBText );
                pB->setValueFontHeight( 24.f );
                addAndMakeVisible( pB );
                for( auto col = 0; col < 16; ++col )
                {
                    auto sB = &steps[row][col];
                    sB->setValueBackgroundColour( ui::Theme::backgroundColour( 3 ).withAlpha( 0.65f ) );
                    addAndMakeVisible( sB );
                }
            }

            padTexts[0] = "DANCE KICK";
            padTexts[1] = "TECHNO SNARE";
            padTexts[2] = "TECHNO HI HAT";
            padTexts[3] = "DANCE CLAP";
            padTexts[4] = "METAL SNARE";
            padTexts[5] = "DRUM 0";
            padTexts[6] = "DRUM BASE";
            padTexts[7] = "LOW HAT";
        }

        void handleCommand( CPktMgr::HWMsg_t& ) override {}

    private:
        KickModeHeader                       header{ "Step Sequencer", "Icon_listEdit" };
        std::array<juce::Rectangle<int>, 8>  stepBounds;
        std::array<juce::String, 8>          padTexts;
        std::array<ui::KickToggleButton, 8>  pads;
        std::array<ui::KickToggleButton, 16> steps[8];

        void paint( juce::Graphics& g ) override
        {
            auto       tColour = juce::Colours::grey.darker();
            auto       tJust   = juce::Justification::centredLeft;
            auto       tHeight = 28.f;
            juce::Font tFont;

            tFont.setSizeAndStyle( tHeight, juce::Font::italic | juce::Font::bold, 1.f, 0.4f );

            g.setColour( tColour );
            g.setFont( tFont );
            for( auto row = 7; row >= 0; --row )
            {
                g.drawFittedText( padTexts[row].toUpperCase(), stepBounds[row], tJust, 1 );
            }
        }

        void resized() override
        {
            auto b = getLocalBounds();
            header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
            b = b.reduced( 7, 10 );
            //
            auto height = b.getHeight() / 8;

            auto padWidth  = 104;
            auto padBounds = b.removeFromLeft( padWidth );

            auto stepWidth = b.getWidth() / 16;

            for( auto row = 7; row >= 0; --row )
            {
                auto pB         = padBounds.removeFromTop( height ).reduced( 0, 5 );
                auto sB         = b.removeFromTop( height ).reduced( 0, 5 );
                stepBounds[row] = sB.reduced( 25, 0 );

                pads[row].setBounds( pB.reduced( 3, 0 ) );
                for( auto col = 0; col < 16; ++col )
                {
                    steps[row][col].setBounds( sB.removeFromLeft( stepWidth ).reduced( 3, 0 ) );
                }
            }
        }

        void visibilityChanged() override {}
    };

    KickHome::KickHome()
        : statusBar_( &AppStatusbar::Instance() )
    {
        mt::getAudioInterface().addListener(this);
        //mt::FocusDebugger::enableFocusDebugger( true );

        // Setup default look and feel.
        setLookAndFeel( ui::KickLooknFeel::getLnf() );

        // Connect sceeen signals.
        // Open screen as passed in Argument.
        signals::sigScreenOpen.connect( [&]( Screen Scrn ) {
            if( !( ( currentScreen == Screen::HomeOptions ) || ( currentScreen == Screen::HomeListView ) || ( currentScreen == Screen::Invalid ) ) )
            {
                stack.push( currentScreen );
            }

            juce::MessageManager::callAsync( [Scrn, this]() {
                load( Scrn );
            } );
        } );

        // Close existing screen and open screen as passed in argument.
        signals::sigScreenClose.connect( [&]( Screen NewScrn ) {
            Screen loadScreen = NewScrn;
            if( !stack.empty() )
            {
                if( NewScrn == Screen::Invalid )
                    loadScreen = stack.top();

                stack.pop();
            }

            juce::MessageManager::callAsync( [loadScreen, this]() {
                load( loadScreen );
            } );
        } );

        // Connect handler to listen to input commands.

        contracts::DclSignals::hardwareInputTriggered.connect( [&]( CPktMgr::HWMsg_t& f_HwMsg ) {
            this->handleCommand( f_HwMsg );
        } );

        // Function to add screens.
        auto screenAdder = [&]( Screen Type, std::unique_ptr<IScreen> Screen ) {
            if( auto p = dynamic_cast<juce::Component*>( Screen.get() ) )
            {
                p->setVisible( false );
                addChildComponent( p );
                screens_[Type] = std::move( Screen );
            }
        };

        // Global components
        ctrlBar_ = std::make_unique<AppTopbar>();
        mt::utils::addChildComponents( this, ctrlBar_.get(), statusBar_ );
        // First launch screen.
        screenAdder( Screen::RecentProject, std::make_unique<ui::RecentProject>() );

        // Start listining to inputs now...
        setWantsKeyboardFocus( true );
        addKeyListener( &contracts::getKeypressDetector() );
        // LED control
        ledControl_ = std::make_unique<KickLEDs>();

        // Setup size and other application options.
        setInterceptsMouseClicks( false, true );
        setBufferedToImage( true );
        setPaintingIsUnclipped( true );
        setSize( ui::appWidth, ui::appHeight );

        // Load default startup screen.
        //signals::sigScreenOpen( Screen::Temp );
        signals::sigScreenOpen( Screen::RecentProject );

        // Now, Lazily create and load rest of the screens
        mt::threadPool.addJob( [&, screenAdder]() {
            juce::MessageManager::callAsync( [&, screenAdder]() {
                // Home Screen
                screenAdder( Screen::HomeListView, std::make_unique<AppMainmode>() );
                screenAdder( Screen::HomeGridView, std::make_unique<AppGrid>() );
                screenAdder( Screen::HomeSequencerView, std::make_unique<StepSequencer>() );
                screenAdder( Screen::HomeXYView, std::make_unique<XyFxView>() );
                screenAdder( Screen::HomePreferencesView, std::make_unique<Settings>() );
                screenAdder( Screen::HomeOptions, std::make_unique<AppMenuOptions>() );
                screenAdder( Screen::MenuSampleEdit, std::make_unique<SampleEditor>() );
                screenAdder( Screen::MenuProgramEdit, std::make_unique<ProgramEdit>() );
                screenAdder( Screen::MenuSampler, std::make_unique<Sampler>() );
                screenAdder( Screen::MenuLooper, std::make_unique<Looper>() );
                screenAdder( Screen::MenuPadMute, std::make_unique<KickPadMute>() );
                screenAdder( Screen::MenuTrackMute, std::make_unique<KickTrackMute>() );
                screenAdder( Screen::MenuPadMixer, std::make_unique<PadMixer>() );
                screenAdder( Screen::MenuChannelMixer, std::make_unique<ChannelMixer>() );
                screenAdder( Screen::MenuKnobEdit, std::make_unique<KnobEdit>() );
                screenAdder( Screen::MenuListEdit, std::make_unique<ListEditMode>() );
                screenAdder( Screen::MenuPadPerform, std::make_unique<PadPerform>() );
                screenAdder( Screen::MenuMidiControl, std::make_unique<MidiControl>() );
                screenAdder( Screen::MenuFileBrowser, std::make_unique<ui::FileBrowser>() );
                screenAdder( Screen::MenuNextSequence, std::make_unique<KickNextSequenceMode>() );
                screenAdder( Screen::MenuSong, std::make_unique<KickSongMode>() );
                screenAdder( Screen::MenuColorEdit, std::make_unique<KickPadColorMode>() );
                screenAdder( Screen::MenuAssignSample, std::make_unique<AppAssignSample>() );
                screenAdder( Screen::SWUpdateView, std::make_unique<SWUpdateView>() );
                screenAdder( Screen::SWUpdateWindow, std::make_unique<SWUpdateScreen>() );
                screenAdder( Screen::ProcessEdit, std::make_unique<SampleProcess>() );
                screenAdder( Screen::CVClib, std::make_unique<CVClib>() );
                screenAdder( Screen::Inserts, std::make_unique<Inserts>() );
                //screenAdder( Screen::Temp, std::make_unique<TempScreen>() );
            } );
        } );
    }

    void KickHome::onProjectSaveFailed(const juce::String& reason)
    {
        if (statusBar_) statusBar_->setBoxInfo("UNSAVED: " + reason);
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Project not saved", reason + "\nYour changes remain open. The previous saved file has been preserved.");
    }

    void KickHome::onProjectDirtyChanged(bool dirty)
    {
        if (statusBar_ && mt::getAudioInterface().getProjectSaveError().isEmpty())
            statusBar_->setBoxInfo(dirty ? "Unsaved project changes" : "Project saved");
    }

    KickHome::~KickHome()
    {
        mt::getAudioInterface().removeListener(this);
        removeKeyListener( &contracts::getKeypressDetector() );
        contracts::DclSignals::hardwareInputTriggered.disconnect_all();
        ctrlBar_.reset();
    }

    IScreen* KickHome::getScreen( Screen screen )
    {
        return screens_[screen].get();
    }

    void KickHome::load( Screen screen )
    {
        ui::clearFocusComponent();

        // Emit signal...
        signals::onScreenClosed( currentScreen );

        // Bounds check...
        currentScreen = ( screens_.end() != screens_.find( screen ) ) ? screen : Screen::HomeListView;
        // Load current screen...
        triggerAsyncUpdate();
    }

    void KickHome::setupLoadScreenVisibility()
    {
        if( auto p = screens_[currentScreen].get() )
        {
            // Turn off all screens.
            for( const auto& it : screens_ )
            {
                if( auto comp = dynamic_cast<juce::Component*>( it.second.get() ) )
                    if( comp->isVisible() )
                        comp->setVisible( false );
            }
            // Set control and status bar visibility.
            ctrlBar_->setVisible( p->requiresControlBar() );
            statusBar_->setVisible( p->requiresStatusBar() );
            // Set loading screen visibility.
            if( auto pScr = dynamic_cast<juce::Component*>( p ) )
            {
                // Update size, if not corrrect.
                auto scrBounds = getScreenBounds( p );
                if( pScr->getLocalBounds() != scrBounds )
                    pScr->setBounds( scrBounds );

                pScr->setVisible( true );
            }

            // Emit signal...
            signals::onScreenLoaded( currentScreen );
        }
    }

    void KickHome::handleAsyncUpdate()
    {
        setupLoadScreenVisibility();
    }

    juce::Rectangle<int> KickHome::getScreenBounds( IScreen* pScreen )
    {
        auto bounds = getLocalBounds();
        if( pScreen )
        {
            if( pScreen->requiresControlBar() )
                bounds.removeFromTop( ui::layout::header.getHeight() );

            if( pScreen->requiresStatusBar() )
                bounds.removeFromBottom( ui::layout::statusBar.getHeight() );
        }
        return bounds;
    }

    void KickHome::resized()
    {
        auto bounds = getLocalBounds();
        {
            auto b = bounds;
            ctrlBar_->setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
            statusBar_->setBounds( b.removeFromBottom( ui::layout::statusBar.getHeight() ) );
        }
    }

    void KickHome::AssignSampleStartUp()
    {
        dynamic_cast<AppAssignSample*>( screens_[Screen::MenuAssignSample].get() )->startup();
    }

    void KickHome::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        auto&        ae        = mt::getAudioInterface();
        auto         prjLoaded = ae.isProjectLoaded();
        juce::String log;
        log += "Data[0] From DCL:" + juce::String( f_HwMsg.data[0] ) + " cmd From DCL : " + juce::String( f_HwMsg.pktCmd ) + +" type From DCL : " + juce::String( f_HwMsg.pktType );
        juce::Logger::writeToLog( log );

        if( contracts::DclContractsHelper::isSystemMessage( f_HwMsg ) )
        {
            if( contracts::DclContractsHelper::isSoftwareVersionMessage( f_HwMsg ) )
            {
                mt::HwPref::getHwPref().setSurfaceVersion( juce::String( f_HwMsg.data[0] ) + "." + juce::String( f_HwMsg.data[1] ) );
                return;
            }
            if( contracts::DclContractsHelper::isSystemCommInit( f_HwMsg ) )
            {
                CPktMgr::HWMsg_t sendMsg;

                sendMsg.pktType = CPktMgr::EN_PKT_TYPE_SYSTEM;
                sendMsg.pktCmd  = contracts::DeviceControlContracts::SysCmd::EN_SYS_COMM_INIT;
                sendMsg.pktDir  = CPktMgr::EN_PKT_REQUEST;

                sendMsg.data.push_back( (int)contracts::DeviceControlContracts::SysCmd::EN_SYS_COMM_INIT );
                contracts::DclSignals::createAndSendCommandToDcl( sendMsg );
                juce::Logger::writeToLog( "Send Comm Init" );
                return;
            }
            if( contracts::DclContractsHelper::isSystemCommLive( f_HwMsg ) )
            {
                CPktMgr::HWMsg_t sendMsg;

                sendMsg.pktType = CPktMgr::EN_PKT_TYPE_SYSTEM;
                sendMsg.pktCmd  = contracts::DeviceControlContracts::SysCmd::EN_SYS_CHECK_COMM_STATUS;
                sendMsg.pktDir  = CPktMgr::EN_PKT_REQUEST;

                sendMsg.data.push_back( 2 );
                contracts::DclSignals::createAndSendCommandToDcl( sendMsg );
                juce::Logger::writeToLog( "Send Comm Live" );
                return;
            }
        }
        else if( prjLoaded && !ui::RecentProject::isProjectLoading )
        {
            log += " In Recent Prj";
            juce::Logger::writeToLog( log );
            if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
            {
                log += " In is button";
                juce::Logger::writeToLog( log );
                auto buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
                if( getCurrentScreen() != Screen::MenuMidiControl )
                {
                    log += " In screen " + juce::String( buttonCode );
                    juce::Logger::writeToLog( log );
                    switch( buttonCode )
                    {
                        case contracts::DeviceControlContracts::EN_BROWSE: signals::sigScreenClose( Screen::MenuFileBrowser ); break;
                        case contracts::DeviceControlContracts::EN_PAD_MIX: signals::sigScreenClose( Screen::MenuPadMixer ); break;
                        case contracts::DeviceControlContracts::EN_TRACK_MIX: signals::sigScreenClose( Screen::MenuChannelMixer ); break;
                        case contracts::DeviceControlContracts::EN_PAD_MUTE: signals::sigScreenClose( Screen::MenuPadMute ); break;
                        case contracts::DeviceControlContracts::EN_TRACK_MUTE: signals::sigScreenClose( Screen::MenuTrackMute ); break;
                        case contracts::DeviceControlContracts::EN_NEXT_SEQ: signals::sigScreenClose( Screen::MenuNextSequence ); break;
                        case contracts::DeviceControlContracts::EN_FX: signals::sigScreenClose( Screen::HomeXYView ); break;
                        case contracts::DeviceControlContracts::EN_STEP_SEQ: signals::sigScreenClose( Screen::HomeSequencerView ); break;

                        case contracts::DeviceControlContracts::EN_LOOPER: signals::sigScreenClose( Screen::MenuLooper ); break;
                        case contracts::DeviceControlContracts::EN_SAMPLER: signals::sigScreenClose( Screen::MenuSampler ); break;
                        case contracts::DeviceControlContracts::EN_SAMPLE_EDIT: signals::sigScreenClose( Screen::MenuSampleEdit ); break;
                        case contracts::DeviceControlContracts::EN_LINK_EDIT: signals::sigScreenClose( Screen::MenuKnobEdit ); break;
                        case contracts::DeviceControlContracts::EN_PROG_EDIT: signals::sigScreenClose( Screen::MenuProgramEdit ); break;
                        case contracts::DeviceControlContracts::EN_MIDI_CONTROL: signals::sigScreenClose( Screen::MenuMidiControl ); break;
                        case contracts::DeviceControlContracts::EN_NOTES: signals::sigScreenClose( Screen::MenuPadPerform ); break;

                        // Bank buttons...
                        case contracts::DeviceControlContracts::EN_BANK_A: ae.setBank( "A" ); break;
                        case contracts::DeviceControlContracts::EN_BANK_B: ae.setBank( "B" ); break;
                        case contracts::DeviceControlContracts::EN_BANK_C: ae.setBank( "C" ); break;
                        case contracts::DeviceControlContracts::EN_BANK_D: ae.setBank( "D" ); break;
                        case contracts::DeviceControlContracts::EN_BANK_E: ae.setBank( "E" ); break;
                        case contracts::DeviceControlContracts::EN_BANK_F: ae.setBank( "F" ); break;
                        case contracts::DeviceControlContracts::EN_BANK_G: ae.setBank( "G" ); break;
                        case contracts::DeviceControlContracts::EN_BANK_H: ae.setBank( "H" ); break;
                        case contracts::DeviceControlContracts::EN_HALF_LEVEL:

                            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setLEDONMessage( contracts::DeviceControlContracts::EN_HALF_LEVEL ) );
                            contracts::DclContractsHelper::setHalfLevel( true );
                            break;
                        case contracts::DeviceControlContracts::EN_FULL_LEVEL:
                            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setLEDONMessage( contracts::DeviceControlContracts::EN_FULL_LEVEL ) );
                            contracts::DclContractsHelper::setHalfLevel( false );
                            break;

                        case contracts::DeviceControlContracts::EN_LINK:
                            dynamic_cast<KnobEdit*>( screens_[Screen::MenuKnobEdit].get() )->knobLongPress();
                            break;
                        case contracts::DeviceControlContracts::EN_SAVE:
                            if (!ae.saveProject())
                                statusBar_->setBoxInfo("UNSAVED: " + ae.getProjectSaveError());
                            break;

                        default: break;
                    }
                }

                switch( buttonCode )
                {
                    case contracts::DeviceControlContracts::EN_MENU: signals::sigScreenClose( Screen::HomeOptions ); break;
                    case contracts::DeviceControlContracts::EN_MAINMODE: signals::sigScreenClose( Screen::HomeListView ); break;
                    case contracts::DeviceControlContracts::EN_GRID: signals::sigScreenClose( Screen::HomeGridView ); break;
                    case contracts::DeviceControlContracts::EN_PREF: signals::sigScreenClose( Screen::HomePreferencesView ); break;
                }
            }
            auto mode = mt::getSettings().knobControl.getMode();

            if( !mode && contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) && contracts::DclContractsHelper::getKnobCode( f_HwMsg ) != contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER )
            {
                log += " knob message ";
                juce::Logger::writeToLog( log );
                auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
                auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
                getScreen( Screen::MenuKnobEdit )->knobCommand( knob, value );
                return;
            }

            if( contracts::DclContractsHelper::isNavigationMessage( f_HwMsg ) && ui::getComponentInFocus() && !contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
            {
                log += " is Navigation message";
                juce::Logger::writeToLog( log );
                ui::getComponentInFocus()->handleCommand( f_HwMsg );
            }

            else if( getScreen( getCurrentScreen() )->requiresHandleCommand() )
            {
                log += " register handle command";
                juce::Logger::writeToLog( log );
                getScreen( getCurrentScreen() )->handleCommand( f_HwMsg );
            }
        }
        else if( !ui::RecentProject::isProjectLoading && contracts::DclContractsHelper::isCenterKnobMessage( f_HwMsg ) )
        {
            log += " project is loading and center knob message";
            juce::Logger::writeToLog( log );
            getScreen( getCurrentScreen() )->handleCommand( f_HwMsg );
        }
    }
}
