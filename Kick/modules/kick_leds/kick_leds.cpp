
#include "kick_leds.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "contracts/dcl/dcl_signals.h"

namespace kick_ui
{
    namespace
    {
        // Only ids below BtnLedSequence::SIZE have a button LED on the panel. The enum also carries
        // EN_COLOR_EDIT (54), which is a screen id with no LED, and EN_INVALID_ID (-1).
        bool hasButtonLed( contracts::DeviceControlContracts::BtnLedSequence id )
        {
            return id >= 0 && id < contracts::DeviceControlContracts::BtnLedSequence::SIZE;
        }
    } // namespace

    KickLEDs::KickLEDs()
    {
        ae.addListener( this );

        // Connect signals to handle front panel activities...
        mt::signals::bankChanged.connect( [&]( int Index, juce::String Bank ) {
            if( invalidScreens.find( currentScreen ) == invalidScreens.end() )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( Index ) );
                contracts::DclSignals::refreshPadLeds();
            }
        } );

        contracts::DclSignals::refreshPadLeds.connect( [&]() {
            auto pProg = ae.getSelectedProgramData();
            if( pProg )
            {
                if( pProg->name.get().isNotEmpty() )
                    changePadColours( pProg->name.get(), pProg->type.get() );
            }
        } );

        mt::signals::padColourChanged.connect( [&]( int PadNo, int Color ) {
            if( 0 > PadNo )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                             setAllPadDataColourInfoMessage( contracts::DeviceControlContracts::PadButtonState::StateOn, Color ) );
            }

            else
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                             setPadDataMessage( PadNo, contracts::DeviceControlContracts::StateOn, Color ) );
            }
        } );

        contracts::DclSignals::onMainModeTabSelected.connect( [&]( int TabIndex ) {
            switch( TabIndex )
            {
                case 0: onSequenceSelectionChange( ae.getSelected() ); break;
                case 1: onTrackSelectionChange( ae.getSelected() ); break;
                case 2: onProgramSelectionChange( ae.getSelected() ); break;
                default: break;
            }
        } );

        mt::signals::playStateChanged.connect( [&]( mt::TransportState State ) {
            if( mt::TransportState::Playing == State )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setMediaControlMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_PLAY ) );
            }
            else if( mt::TransportState::Stopped == State )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setMediaControlMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_STOP ) );
            }
            else if( mt::TransportState::Paused == State )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setMediaControlMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_STOP ) );
            }
        } );

        signals::onScreenLoaded.connect( [&]( Screen Scrn ) {
            currentScreen                                        = Scrn;
            contracts::DeviceControlContracts::BtnLedSequence id = contracts::DeviceControlContracts::BtnLedSequence::EN_INVALID;
            switch( Scrn )
            {
                DBG( "SIGSCREEN LOADED Connect" );
                case Screen::HomeListView:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_MAINMODE;
                    break;
                }
                case Screen::HomeGridView:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_GRID;
                    break;
                }
                case Screen::MenuFileBrowser:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_BROWSE;
                    break;
                }
                case Screen::HomeXYView:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_FX;
                    break;
                }
                case Screen::HomeSequencerView:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_STEP_SEQ;
                    break;
                }
                case Screen::HomeOptions:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_MENU;
                    break;
                }
                case Screen::MenuPadMixer:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_PAD_MIX;
                    break;
                }
                case Screen::MenuChannelMixer:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_TRACK_MIX;
                    break;
                }
                case Screen::MenuPadMute:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_PAD_MUTE;
                    break;
                }
                case Screen::MenuTrackMute:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_TRACK_MUTE;
                    break;
                }
                case Screen::MenuNextSequence:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_NEXT_SEQ;
                    break;
                }
                case Screen::HomePreferencesView:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_PREF;
                    break;
                }
                case Screen::MenuLooper:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_LOOPER;
                    break;
                }
                case Screen::MenuSampler:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_SAMPLER;
                    break;
                }
                case Screen::MenuSampleEdit:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_SAMPLE_EDIT;
                    break;
                }
                case Screen::MenuProgramEdit:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_PROG_EDIT;
                    break;
                }
                case Screen::MenuMidiControl:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_MIDI_CONTROL;
                    break;
                }
                case Screen::MenuColorEdit:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_COLOR_EDIT;
                    break;
                }
                /*case Screen::MenuKnobEdit: {id = contracts::DeviceControlContracts::BtnLedSequence::EN_LOOPER ; break;}
                case Screen::MenuPadPerform: {id = contracts::DeviceControlContracts::BtnLedSequence::EN_LOOPER ; break;}*/
                default: break;
            }
            if( hasButtonLed( id ) )
            {
                auto msg = contracts::DclContractsHelper::setLEDONMessage( id );
                contracts::DclSignals::sendCommandToDcl( msg );
            }
        } );

        signals::onScreenClosed.connect( [&]( Screen Scrn ) {
            contracts::DeviceControlContracts::BtnLedSequence id = contracts::DeviceControlContracts::BtnLedSequence::EN_INVALID;
            switch( Scrn )
            {
                DBG( "SIGSCREEN LOADED Connect" );
                case Screen::HomeListView:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_MAINMODE;
                    break;
                }
                case Screen::HomeGridView:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_GRID;
                    break;
                }
                case Screen::MenuFileBrowser:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_BROWSE;
                    break;
                }
                case Screen::HomeXYView:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_FX;
                    break;
                }
                case Screen::HomeSequencerView:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_STEP_SEQ;
                    break;
                }
                case Screen::HomeOptions:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_MENU;
                    break;
                }
                case Screen::MenuPadMixer:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_PAD_MIX;
                    break;
                }
                case Screen::MenuChannelMixer:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_TRACK_MIX;
                    break;
                }
                case Screen::MenuPadMute:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_PAD_MUTE;
                    break;
                }
                case Screen::MenuTrackMute:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_TRACK_MUTE;
                    break;
                }
                case Screen::MenuNextSequence:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_NEXT_SEQ;
                    break;
                }
                case Screen::HomePreferencesView:
                {
                    id = id = contracts::DeviceControlContracts::BtnLedSequence::EN_PREF;
                    break;
                }
                case Screen::MenuLooper:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_LOOPER;
                    break;
                }
                case Screen::MenuSampler:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_SAMPLER;
                    break;
                }
                case Screen::MenuSampleEdit:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_SAMPLE_EDIT;
                    break;
                }
                case Screen::MenuProgramEdit:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_PROG_EDIT;
                    break;
                }
                case Screen::MenuMidiControl:
                {
                    id = contracts::DeviceControlContracts::BtnLedSequence::EN_MIDI_CONTROL;
                    break;
                }
                /*case Screen::MenuKnobEdit: {id = contracts::DeviceControlContracts::BtnLedSequence::EN_LOOPER ; break;}
                case Screen::MenuPadPerform: {id = contracts::DeviceControlContracts::BtnLedSequence::EN_LOOPER ; break;}*/
                default: break;
            }
            if( hasButtonLed( id ) )
            {
                auto msg = contracts::DclContractsHelper::setLEDOFFMessage( id );
                contracts::DclSignals::sendCommandToDcl( msg );
            }
        } );
    }

    KickLEDs::~KickLEDs()
    {
        ae.removeListener( this );
    }

    void KickLEDs::onSequenceSelectionChange( const mt::MTSelected& Selected )
    {
        if( currentScreen == Screen::HomeListView )
        {
            contracts::DclSignals::refreshPadLeds();
        }
    }

    void KickLEDs::onTrackSelectionChange( const mt::MTSelected& Selected )
    {
        if( projectLoadCompleted )
        {
            auto Program = ae.getProgramDataForTrack( Selected.sequence, Selected.track );
            if( Program )
                if( Program->name.get().isNotEmpty() )
                    changePadColours( Program->name.get(), Program->type.get() );
        }
    }

    void KickLEDs::onProgramSelectionChange( const mt::MTSelected& Selected )
    {
        if( currentScreen == Screen::HomeListView )
        {
            contracts::DclSignals::refreshPadLeds();
        }
    }

    void KickLEDs::onTrackProgramChanged( int /*Track*/, int Program )
    {
        auto programName = ae.getProgramNameByIndex( Program );
        auto programType = ae.getProgramType( programName );
        changePadColours( programName, programType );
    }

    void KickLEDs::onProgramListChanged()
    {
        if( !mt::getAudioInterface().getNumPrograms() )
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );
        }

        else
        {
            contracts::DclSignals::refreshPadLeds();
        }
    }

    void KickLEDs::onProjectLoaded( juce::File )
    {
        juce::MessageManager::callAsync( [&]() {
            ae.setBank( "A" );
            if( mt::getAudioInterface().getNumSequences() )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setTempoMessage( mt::getAudioInterface().getMasterBPM() ) );
            }

            auto MTIProg = ae.getProgramData( ae.getSelectedProgram() );
            ae.setProgramColourMode( ae.getSelectedProgram(), MTIProg ? MTIProg->colourMode.get() : mt::DIM_VELOCITY );

            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadColorModeMessage( MTIProg ? MTIProg->colourMode.get() : mt::DIM_VELOCITY ) );
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setLEDONMessage( contracts::DeviceControlContracts::EN_FULL_LEVEL ) );

            sendPadSettings();
            projectLoadCompleted = true;
        } );
    }

    void KickLEDs::onProjectClose()
    {
        projectLoadCompleted = false;
    }

    void KickLEDs::sendPadSettings( void )
    {
        std::vector<int> padSettingsData;
        padSettingsData.push_back( 16 );
        padSettingsData.push_back( settings.hardware.threshold().getIntValue() );
        padSettingsData.push_back( settings.hardware.sensitivity().getIntValue() );
        padSettingsData.push_back( settings.hardware.curve().getIntValue() );
        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setPadSettingsMessage( padSettingsData ) );
    }

    void KickLEDs::changePadColours( const juce::String& Program, const mt::MTProgramType Type )
    {
        static uint8_t                  ui8Startup    = 1;
        auto&                           padColourData = ae.getKeyGroupPadColour();
        auto                            progData      = ae.getProgramData( Program );
        auto                            bankOffset    = ae.getBankOffset();
        std::vector<contracts::PadData> padDataList;
        std::vector<mt::MTISample*>     samples;
        ae.getProgramSamples( ae.getSelectedProgram(), samples );
        switch( Type )
        {
            case mt::MTProgramType::Drum:
            case mt::MTProgramType::Audio:
            case mt::MTProgramType::Clip:
                padDataList.clear();
                for( auto colourIndex = bankOffset; colourIndex < bankOffset + NUM_PADS_IN_BANK; colourIndex++ )
                {
                    bool pushedToColourData = false;

                    for( auto& it : samples )
                    {
                        auto index = it->keyNote.get() - bankOffset;
                        if( juce::isPositiveAndBelow( index, 16 ) && it->getSource().length() && colourIndex == it->keyNote.get() )
                        {
                            padDataList.push_back( { colourIndex - bankOffset, contracts::DeviceControlContracts::PadButtonState::StateSampleAssigned, progData ? progData->padColour[colourIndex] : 0 } );
                            pushedToColourData = true;
                            break;
                        }
                    }
                    if( !pushedToColourData )
                        padDataList.push_back( { colourIndex - bankOffset, contracts::DeviceControlContracts::PadButtonState::StateSampleNotAssigned, progData ? progData->padColour[colourIndex] : 0 } );
                }

                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setPadColourAndStateInfoMessage( padDataList ) );
                break;

            case mt::MTProgramType::Keygroup:
            case mt::MTProgramType::Midi:
            case mt::MTProgramType::Plugin:
            case mt::MTProgramType::Cv:
                padDataList.clear();
                for( auto colourIndex = bankOffset; colourIndex < bankOffset + NUM_PADS_IN_BANK; colourIndex++ )
                {
                    if( samples.size() )
                    {
                        padDataList.push_back( { colourIndex, contracts::DeviceControlContracts::PadButtonState::StateSampleAssigned, padColourData.Colours[colourIndex] } );
                    }
                    else
                        padDataList.push_back( { colourIndex, contracts::DeviceControlContracts::PadButtonState::StateSampleNotAssigned, padColourData.Colours[colourIndex] } );
                }
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setPadColourAndStateInfoMessage( padDataList ) );
                break;
        }
    }
}
