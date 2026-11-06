#pragma once
#include <kick_ui/modes/filebrowser/kick_fileUtils.h>
#if KICK_EMU
    #include "kick_emu_hal.h"
#endif
namespace ui
{
    namespace fileUtils
    {
        int lastPressedPad = -1;

        juce::String getSdCardMountedPath()
        {
#if KICK_EMU
            return kick_emu::devicePath( "/mnt/sdcard" );
#endif
            return "/mnt//sdcard";
        }
        juce::String getUsbMountedPath()
        {
#if KICK_EMU
            return kick_emu::devicePath( "/mnt/flash" );
#endif
            return "/mnt//flash";
        }

        bool getSdCardStatus()
        {
#if KICK_EMU
            return kick_emu::deviceExists( "/dev/mmcblk1" );
#endif
#if defined( LINUX ) || defined( __linux__ )
            const char* sdCardDevicePath = "/dev/mmcblk1";
            if( access( sdCardDevicePath, F_OK ) != -1 )
                return true;
            else
                return false;
#endif
            return false;
        }

        bool getUsbStatus()
        {
#if KICK_EMU
            return kick_emu::deviceExists( "/dev/sda" );
#endif
#if defined( LINUX ) || defined( __linux__ )
            const char* usbDevicePath = "/dev/sda";
            if( access( usbDevicePath, F_OK ) != -1 )
                return true;
            else
                return false;
#endif
            return false;
        }

        juce::int32 calculateFolderSize( juce::File filePath )
        {
            juce::int64 totalSize = 0;

            if( filePath.isDirectory() )
            {
                juce::Array<juce::File> files;
                filePath.findChildFiles( files, juce::File::findFilesAndDirectories, true );

                for( const auto& file : files )
                {
                    if( file.isDirectory() )
                        calculateFolderSize( file );
                    else
                        totalSize += file.getSize();
                }
            }
            else
                totalSize = filePath.getSize();
            return totalSize;
        }
        void setSdCardEjectStatus( bool status )
        {
            isSdCardEject = status;
        }
        void setUsbEjectStatus( bool status )
        {
            isUSBEject = status;
        }
        bool getSdCardEjectStatus()
        {
            return isSdCardEject;
        }
        bool getUsbEjectStatus()
        {
            return isUSBEject;
        }

        juce::String getFilePath( juce::String originalFullPath, juce::String replacementPath )
        {
            juce::String searchLabel = getDefaultPath().getFullPathName();
            int          pos         = originalFullPath.indexOf( searchLabel );
            if( pos != -1 )
                originalFullPath = originalFullPath.replaceSection( pos, searchLabel.length(), replacementPath );
            return originalFullPath;
        }

        bool hasSampleAssignedToPad( int LastPressedPad )
        {
            auto&                       ae = mt::getAudioInterface();
            std::vector<mt::MTISample*> samples;

            ae.getProgramSamples( ae.getSelectedProgram(), samples );
            for( auto& sample : samples )
            {
                if( ae.getSelectedProgramData()->type == mt::MTProgramType::Keygroup )
                    return kick_ui::Pad_Assigned;
                auto index = (int)sample->keyNote.get() - ae.getBankOffset();
                if( juce::isPositiveAndBelow( index, 16 ) && sample->getSource( 0 ).length() )
                    if( index == LastPressedPad )
                        return kick_ui::Pad_Assigned;
            }
            return kick_ui::Pad_NotAssigned;
        }

        void setHighlightLED( int hwPadIndex )
        {
            if( juce::isPositiveAndBelow( hwPadIndex, _NUM_PADS_IN_COMPONENT ) )
            {
                if( ui::fileUtils::lastPressedPad < 0 )
                {
                    ui::fileUtils::lastPressedPad = hwPadIndex;
                    contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                 setPadDataMessage( lastPressedPad,
                                                                                    contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                    contracts::DeviceControlContracts::padColourlightGreen1 ) );
                }
                if( ui::fileUtils::lastPressedPad != hwPadIndex )
                {
                    if( hasSampleAssignedToPad( ui::fileUtils::lastPressedPad ) == kick_ui::Pad_Assigned )
                        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                     setPadDataMessage( lastPressedPad,
                                                                                        contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                        contracts::DeviceControlContracts::padColourDarkYellow ) );
                    else
                        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                     setPadDataMessage( lastPressedPad,
                                                                                        contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                        contracts::DeviceControlContracts::PadColourTrans ) );
                    contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                 setPadDataMessage( hwPadIndex,
                                                                                    contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                    contracts::DeviceControlContracts::padColourlightGreen1 ) );
                    ui::fileUtils::lastPressedPad = hwPadIndex;
                }
            }
        }
    }

    /////////////////////////////////////////////////////////////////
    ////////   LoadSamplesAndAnimation   ////////////////////////////
    /////////////////////////////////////////////////////////////////

    LoadSamplesAndAnimation::LoadSamplesAndAnimation()
        : juce::Thread( "LoadSample" )
    {
    }

    LoadSamplesAndAnimation::~LoadSamplesAndAnimation()
    {
    }

    void LoadSamplesAndAnimation::startLoading( const juce::Array<juce::File>& assetsFiles, bool IsMultiple, std::function<void()> onCompleteCallback )
    {
        sampleLoaded       = false;
        completionCallback = std::move( onCompleteCallback );
        assetsFile         = assetsFiles;
        isMultiple         = IsMultiple;
        startThread();
        startAnimation();
    }

    void LoadSamplesAndAnimation::addMultipleAssets()
    {
        auto&                     ae        = mt::getAudioInterface();
        std::vector<juce::String> assets    = ae.getAssets( mt::AssetTypes::Samples );
        bool                      firstTime = true;
        for( auto assetFile : assetsFile ) //Whatever we want to load
        {
            for( auto asset : assets ) //Already Loaded Samples
            {
                if( ( asset != assetFile.getFullPathName() ) )
                    if( assetFile.hasFileExtension( sampleFileList ) )
                    {
                        if( ae.addAsset( assetFile.getFullPathName(), mt::AssetTypes::Samples ) == mt::AssetAdded )
                        {
                            if( firstTime ) //assigning one sample to one pad for the first time
                            {
                                if( ui::fileUtils::lastPressedPad < 0 )
                                    ui::fileUtils::lastPressedPad = 0;
                                ae.addSampleToProgram( ae.getSelectedProgram(), ui::fileUtils::lastPressedPad + mt::getAudioInterface().getBankOffset(), assetFile.getFullPathName() );
                                juce::Logger::outputDebugString( assetFile.getFullPathName() + " Sample assigned to Pad: " + juce::String( ui::fileUtils::lastPressedPad + mt::getAudioInterface().getBankOffset() ) );
                                firstTime = false;
                            }
                            sampleLoaded = true;
                        }
                    }
            }
        }
        if( !assets.size() ) //if nothing is loaded
        {
            for( auto dir : assetsFile )
                if( dir.hasFileExtension( sampleFileList ) )
                {
                    if( ae.addAsset( dir.getFullPathName(), mt::AssetTypes::Samples ) == mt::AssetAdded )
                    {
                        if( firstTime )
                        {
                            if( ui::fileUtils::lastPressedPad < 0 )
                                ui::fileUtils::lastPressedPad = 0;
                            ae.addSampleToProgram( ae.getSelectedProgram(), ui::fileUtils::lastPressedPad + mt::getAudioInterface().getBankOffset(), dir.getFullPathName() );
                            juce::Logger::outputDebugString( dir.getFullPathName() + " Sample assigned to Pad: " + juce::String( ui::fileUtils::lastPressedPad + mt::getAudioInterface().getBankOffset() ) );
                            firstTime = false;
                        }
                        sampleLoaded = true;
                    }
                }
        }
    }

    void LoadSamplesAndAnimation::addAsset()
    {
        juce::MessageManager::callAsync( [&]() {
            if( !assetsFile.size() )
            {
                auto p = new FilePopup( "Please Select the sample to load", false, false );
                mt::utils::showDialog(
                    "WARNING!", p, [p, this]( int R ) { delete p; }, { 0, 0, 600, 360 }, Theme::backgroundColour( 1 ), Theme::titleColour( 1 ), Theme::backgroundColour( 2 ) );
            }
            else
            {
                if( assetsFile.getLast().hasFileExtension( sampleFileList ) )
                {
                    auto& ae = mt::getAudioInterface();
                    if( !ae.addAsset( assetsFile.getLast().getFullPathName(), mt::AssetTypes::Samples ) )
                    {
                        auto p = new FilePopup( "This Sample has already Loaded", false, false );
                        mt::utils::showDialog(
                            "WARNING!", p, [p, this]( int R ) { delete p; }, { 0, 0, 600, 360 }, Theme::backgroundColour( 1 ), Theme::titleColour( 1 ), Theme::backgroundColour( 2 ) );
                    }
                    else
                    {
                        if( ui::fileUtils::lastPressedPad < 0 )
                            ui::fileUtils::lastPressedPad = 0;
                        ae.addSampleToProgram( ae.getSelectedProgram(), ui::fileUtils::lastPressedPad + mt::getAudioInterface().getBankOffset(), assetsFile.getLast().getFullPathName() );
                        juce::Logger::outputDebugString( "Sample assigned to Pad: " + juce::String( ui::fileUtils::lastPressedPad + mt::getAudioInterface().getBankOffset() ) );
                        sampleLoaded = true;
                        return;
                    }
                }
                else
                {
                    auto p = new FilePopup( "Selected file cannot be loaded", false, false );
                    mt::utils::showDialog(
                        "WARNING!", p, [p, this]( int R ) { delete p; }, { 0, 0, 600, 360 }, Theme::backgroundColour( 1 ), Theme::titleColour( 1 ), Theme::backgroundColour( 2 ) );
                }
            }
        } );
        sampleLoaded = false;
    }

    void LoadSamplesAndAnimation::run()
    {
        if( isMultiple )
            addMultipleAssets();
        else
            addAsset();
        if( completionCallback ) // Call the completion callback on the main thread
        {
            juce::MessageManager::callAsync( completionCallback );
        }
    }

}
