#pragma once

namespace ui
{
    FileBrowser::FileBrowser()
        : kick_ui::IScreen( kick_ui::Screen::MenuFileBrowser ), ae( mt::getAudioInterface() ), fileList( ae.getFiles() ), dirList( ae.getDirectories() ), sampleAnimation( std::make_unique<LoadSamplesAndAnimation>() )
    {
        startTimer( 1000 ); //to monitor the external device

        bottom.volumeBtn.setToggleState( false, juce::dontSendNotification );
        addMouseListener( this, true );
        bottom.volumeBtn.removeMouseListener( this );
        mt::utils::addChildComponents( this, left, right, top, bottom );

        pwd      = ae.getInternalDir().getFullPathName();
        root     = ae.getInternalDir().getParentDirectory().getFullPathName();
        tempList = dirList;
        bottom.allButton.setToggleState( true, juce::dontSendNotification );
        row.push_back( ae.getInternalDir().getFullPathName() );
        row.push_back( ae.getSoundsDir().getFullPathName() );
        row.push_back( ae.getDemosDir().getFullPathName() );

        addData( "Icon_Internal", "Icon_InternalActive", 16.0f, 16.0f, "Internal", false );
        addData( "Icon_Library", "Icon_LibraryActive", 18.0f, 16.0f, "Factory Sample Library", false );
        addData( "Icon_Demo", "Icon_DemoActive", 16.0f, 16.0f, "Demos", false );
        mt::utils::addAndMakeVisible( this, header, left, right, bottom, pathLabel, top, auditionComp );
        mt::utils::makeHidden( auditionComp );

        auditionComp.onGainChanged = [&]( double Value ) {
            settings.preview.mute = auditionComp.isMuted();
            ae.setPreviewGain( Value );
        };

        auditionComp.onMuteChanged = [&]( bool State ) {
            settings.preview.mute = State;

            if( State )
            {
                mt::utils::makeHidden( bottom.volumeBtn );
                mt::utils::makeVisible( bottom.volumeMuteBtn );
                ae.setPreviewMute( State );
            }
            else
            {
                mt::utils::makeHidden( bottom.volumeMuteBtn );
                mt::utils::makeVisible( bottom.volumeBtn );
                ae.setPreviewMute( State );
            }
            /// Set preview Muted...
        };

        pathLabel.setText( "Folder Path/Folder", juce::dontSendNotification );
        header.showNewProjectButton( true );
        header.showOptions( false, false );
        header.onBackButtonPressed = [this]( bool ) {
            closeScreen();
        };

        //Adding favourite Image Buttons
        for( int i = 5; i > 0; --i )
        {
            std::stringstream buttonName;
            buttonName << "button" << i;

            std::stringstream imagePath;
            imagePath << "Header/buttons/" << i << ".svg";

            std::stringstream activeImagePath;
            activeImagePath << "Header/buttons/ActiveButtons/"
                            << "Button - Memory " << i + 5 << ".svg";

            grpNum.push_back( std::make_unique<ui::KickImageButton>( buttonName.str(), imagePath.str(), activeImagePath.str(), activeImagePath.str() ) );
        }
        favouriteButtonImplementation();

        //TOP BUTTONS
        juce::String originalFullPath = fileUtils::getFilePath( ae.getCurrentDir().getFullPathName(), replacementPath );
        top.searchLabel.setText( originalFullPath, juce::dontSendNotification );

        top.searchLabel.onKeyBoardEnd = [&]() {
            searchImplementation();
            right.backButton.setEnabled( true );
            searchMode = true;
        };
        top.deleteBtn.onClick = [&]() {
            deleteSystem();
        };

        bottom.sampleButton.onClick = [&]() {
            if( bottom.sampleButton.getToggleState() )
            {
                filterNum = Samples;
                updateTemp( filterNum );
                right.table->refresh();
                if( tempList.size() ) //safety check
                {
                    selectedFile.clear();
                    selectedFile.add( juce::File( tempList[0] ) );
                    right.table->selectRow( 0 );
                }
            }
        };

        bottom.projectButton.onClick = [&]() {
            if( bottom.projectButton.getToggleState() )
            {
                filterNum = Projects;
                updateTemp( filterNum );
                right.table->refresh();
                if( tempList.size() ) //safety check
                {
                    selectedFile.clear();
                    selectedFile.add( juce::File( tempList[0] ) );
                    right.table->selectRow( 0 );
                }
            }
        };

        bottom.allButton.onClick = [&]() {
            if( bottom.allButton.getToggleState() )
            {
                bottom.allButton.getToggleState() ? filterNum = All : filterNum = Null;
                updateTemp( filterNum );
                right.table->refresh();
                if( tempList.size() ) //safety check
                {
                    selectedFile.clear();
                    selectedFile.add( juce::File( tempList[0] ) );
                    right.table->selectRow( 0 );
                }
            }
        };

        right.backButton.onClick = [&]() {
            if( searchMode )
            {
                enterDirectory( -1 );
                searchMode = false;
            }
            else
            {
                juce::String pwdParentPath = ae.getCurrentDir().getFullPathName();
                exitDirectory();
                if( tempList.size() ) //safety check
                {
                    selectedFile.clear();
                    selectedFile.add( juce::File( tempList[0] ) );
                    for( auto row = 0; row < tempList.size(); row++ )
                    {
                        if( tempList[row] == pwdParentPath )
                        {
                            right.table->selectRow( row );
                            break;
                        }
                    }
                }
            }
            right.backButton.setWantsKeyboardFocus( false );
        };

        header.onNewProjectClick = [&]() {
            juce::String str        = settings.projects.namePrefix();
            auto&        keyboard   = ui::KickVirtualKeyBoard::getWithText( str );
            keyboard.onEnterPressed = [&keyboard, this]( int R ) {
                mt::utils::exitDialog( keyboard.getCurrentlyFocusedComponent(), UI::APPLY );
            };
            keyboard.show(
                "CREATE NEW PROJECT", ui::KickVirtualKeyBoard::Type::KEYS_LOWERCASE_ALPHABETIC,
                [&keyboard, this]( int R ) {
                    if( R == UI::APPLY )
                    {
                        auto       separator = juce::File::getSeparatorString();
                        juce::File file      = getAppDirs( MTProjects ).getFullPathName() + separator + keyboard.getInputText();
                        settings.projects.createProject( file );
                        ui::LoadProjectAndAnimation::getInstance().startLoading( file );
                        juce::File curDir = juce::File( pwd );
                        ae.setCurrentDir( curDir );
                        enterDirectory( -1 );
                    }
                },
                ui::layout::keyBoardPopup );
            header.setNewProjectToggleState( false );
        };

        //BOTTOM BUTTONS
        settings.preview.isOn ? bottom.playIconButton.setEnabled( true ) : bottom.playIconButton.setEnabled( false );

        bottom.playIconButton.onClick = [&]() {
            if( ( bottom.playIconButton.getToggleState() ) && ( selectedFile.getLast().hasFileExtension( sampleFileList ) ) )
                audition( juce::File( selectedFile.getLast() ) );
            if( settings.preview.isAuto )
            {
                if( bottom.playIconButton.getToggleState() == false )
                    ae.stopPreview();
            }
            else if( !bottom.playIconButton.getToggleState() )
                ae.stopPreview();
        };

        bottom.loadButton.onClick = [&]() {
            loadSample( selectedFile, false );
        };

        bottom.loadAllButton.onClick = [&]() {
            loadSample( dirList, true );
        };
        bottom.auditionButton.onClick = [&]() {
            bottom.auditionButton.incrementSelection();

            int sel = bottom.auditionButton.getCurrentSelection();
            if( sel == 0 )
            {
                settings.preview.isOn = false;
                ae.stopPreview();
                bottom.auditionButton.setButtonText( "OFF" );
                bottom.playIconButton.setToggleState( false, juce::dontSendNotification );
            }
            else if( sel == 1 )
            {
                settings.preview.isOn   = true;
                settings.preview.isAuto = true;
                bottom.auditionButton.setButtonText( "AUTO" );
                bottom.playIconButton.setToggleState( true, juce::dontSendNotification );
            }
            else if( sel == 2 )
            {
                settings.preview.isOn   = true;
                settings.preview.isAuto = false;
                bottom.auditionButton.setButtonText( "MAN" );
                bottom.playIconButton.setToggleState( false, juce::dontSendNotification );
            }
            settings.preview.isOn ? bottom.playIconButton.setEnabled( true ) : bottom.playIconButton.setEnabled( false );
        };
        //Edit Filter
        //mt::utils::makeVisible( top.renameBtn );
        mt::utils::makeVisible( top.deleteBtn );
        mt::utils::makeHidden( top.cutBtn, top.copyBtn, top.pasteBtn, top.renameBtn );

        top.selectBtn.onClick = [&]() {
            selectBtnOnClick();
        };
        top.renameBtn.onClick = [&]() {
            if( top.renameBtn.getToggleState() )
                top.selectBtn.setEnabled( false );
            else
                top.selectBtn.setEnabled( true );
        };

        top.copyBtn.onClick = [&]() {
            if( !selectedFile.size() )
            {
                top.copyBtn.setToggleState( false, juce::dontSendNotification );

                auto p = new FilePopup{ "Please Select the File or Folder to copy", false, false };
                mt::utils::showDialog(
                    "WARNING!", p, [this, p]( int R ) { delete p; }, ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
            }
            else
            {
                top.cutBtn.setToggleState( false, juce::dontSendNotification );
                source = selectedFile;
            }
        };
        top.cutBtn.onClick = [&]() {
            if( !selectedFile.size() )
            {
                top.cutBtn.setToggleState( false, juce::dontSendNotification );

                auto p = new FilePopup{ "Please Select the File or Folder to cut", false, false };
                mt::utils::showDialog(
                    "WARNING!", p, [this, p]( int R ) { delete p; }, ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
            }
            else
            {
                if( !checkIsCurrProj() )
                {
                    top.copyBtn.setToggleState( false, juce::dontSendNotification );
                    source = selectedFile;
                }
            }
        };
        top.pasteBtn.onClick = [&]() {
            top.pasteBtn.setToggleState( false, juce::dontSendNotification );
            if( !source.size() )
            {
                auto p = new FilePopup{ "Please Select the File or Folder to paste", false, false };
                mt::utils::showDialog(
                    "WARNING!", p, [this, p]( int R ) { delete p; }, ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
            }
            else
            {
                if( top.cutBtn.getToggleState() )
                {
                    for( auto sourceFile : source )
                    {
                        destination = juce::File( pwd ).getChildFile( sourceFile.getFileName() );
                        sourceFile.moveFileTo( destination );
                    }
                }
                else if( top.copyBtn.getToggleState() )
                {
                    for( auto sourceFile : source )
                    {
                        destination = juce::File( pwd ).getChildFile( sourceFile.getFileName() );
                        if( sourceFile.isDirectory() )
                            sourceFile.copyDirectoryTo( destination );
                        else
                            sourceFile.copyFileTo( destination );
                    }
                }

                top.cutBtn.setToggleState( false, juce::dontSendNotification );
                top.copyBtn.setToggleState( false, juce::dontSendNotification );

                selectedFile.clear();
                right.table->clearSelection();
                enterDirectory( -1 );
            }
        };
        top.renameBtn.onClick = [&]() {
            renamePopup();
        };

        auto handleVolumeOnClick = [&]() {
            bool isVolumeEnabled            = bottom.volumeBtn.getToggleState();
            bool isAuditionComponentVisible = auditionComp.isVisible();
            if( isAuditionComponentVisible )
                hideAuditionComponent();
            else
                showAuditionComponent();
        };
        bottom.volumeBtn.onClick     = handleVolumeOnClick;
        bottom.volumeMuteBtn.onClick = handleVolumeOnClick;
        //TABLE
        right.table->onTableKnobClicked = [&]( int row ) {
            if( !ui::RecentProject::isProjectLoading )
                right.table->onRowDoubleClicked( row );
        };
        left.table->enableSetFocus( false );
        setupProvider();

        themeChanged.connect( [&]() {
            auto i = 5;
            for( auto& it : grpNum )
            {
                auto path1 = "Header/buttons/" + juce::String( i ) + ".svg";
                auto path2 = "Header/buttons/ActiveButtons/Button - Memory " + juce::String( i + 5 ) + ".svg";
                it->setImages( path1, path2, path2 );
                --i;
            }
            header.setIcon( "Icon_Browser" );
        } );
    }

    void FileBrowser::selectBtnOnClick()
    {
        if( top.selectBtn.getToggleState() )
        {
            right.table->enableSelectMode( true );

            mt::utils::makeVisible( top.cutBtn, top.copyBtn, top.pasteBtn, top.renameBtn );
            // mt::utils::makeHidden( top.renameBtn );
            mt::utils::makeHidden( top.deleteBtn );
            top.onElementChangedCallback();
        }
        else
        {
            right.table->enableSelectMode( false );
            // mt::utils::makeVisible( top.renameBtn );
            mt::utils::makeVisible( top.deleteBtn );
            mt::utils::makeHidden( top.cutBtn, top.copyBtn, top.pasteBtn, top.renameBtn );
            top.onElementChangedCallback();
        }
    }
    int  FileBrowser::numRows = 1;
    void FileBrowser::setupProvider()
    {
        left.provider.numRows = [&]() -> int {
            return static_cast<int>( deviceData.size() );
        };

        left.provider.cellComponent = [&]( int Row, Component* Existing ) -> juce::Component* {
            numRows      = 1;
            static int c = 1;
            if( juce::isPositiveAndBelow( Row, 5 ) )
            {
                auto comp = static_cast<FileLeftEntry*>( Existing );
                if( comp == nullptr )
                {
                    comp = new FileLeftEntry( Row );
                    comp->setInterceptsMouseClicks( false, true );
                }
                if( row[Row] == fileUtils::getSdCardMountedPath() )
                    sdCardRow = Row;
                else if( row[Row] == fileUtils::getUsbMountedPath() )
                    USBRow = Row;

                comp->ejectCallback = [&]( int rowId ) {
                    int row1 = rowId;

                    if( rowId == sdCardRow )
                    {
                        fileUtils::setSdCardEjectStatus( true );
                        row1 = sdCardRow;
                    }
                    else if( rowId == USBRow )
                    {
                        fileUtils::setUsbEjectStatus( true );
                        row1 = USBRow;
                    }

                    deviceData.erase( deviceData.begin() + row1 );
                    row.erase( row.begin() + row1 );

                    if( rowId == sdCardRow )
                    {
                        sdCardRow = -1;
                        if( deviceData.size() == 4 )
                            USBRow = 3;
                        sdCardFirstTime = 1;
                    }

                    else if( rowId == USBRow )
                    {
                        USBRow = -1;
                        if( deviceData.size() == 4 )
                            sdCardRow = 3;
                        usbFirstTime = 1;
                    }

                    left.table->refresh();
                    left.table->selectRow( 0 );
                };

                comp->setProperties( deviceData[Row] );

                return comp;
            }
            return nullptr;
        };

        left.provider.onRowSelected = [&]( int Row ) {
            if( Row == 1 || Row == 2 )
            {
                top.pasteBtn.setEnabled( false );
                top.renameBtn.setEnabled( false );
                top.cutBtn.setEnabled( false );
                top.deleteBtn.setEnabled( false );
            }
            else
            {
                top.pasteBtn.setEnabled( true );
                top.renameBtn.setEnabled( true );
                top.cutBtn.setEnabled( true );
                top.deleteBtn.setEnabled( true );
            }

            if( Row == 0 )
                header.setEnableNewProject( true );
            else
                header.setEnableNewProject( false );
            if( !boolLoad ) //when user clicks the favourite button,file has loaded in onSecondClick callback so no need to load again
            {
                mt::utils::LoadAnimation& loadAnimation = mt::utils::LoadAnimation::getInstance();

                sizeCalculationEnd = [&]() {
                    if( loadAnimation.isAnimationRunning() )
                    {
                        loadAnimation.stopAnimation();
                    }
                };
                if( tempList.size() )
                    loadAnimation.startAnimation();

                if( !onFirstLaunch )
                    load( row[Row] );
                else
                    onFirstLaunch = false;
            }
            else
                boolLoad = false;
            right.table->clearSelection();

            if( tempList.size() && !top.renameBtn.getToggleState() )
            {
                right.table->selectRow( 0 );
                selectedFile = juce::File( tempList[right.table->getSelectedRow()] );
            }
            else
            {
                selectedFile.clear();
            }

            auto& comps = left.table->getRowComponents();
            for( auto& it : comps )
            {
                auto* comp = static_cast<FileLeftEntry*>( it );
                if( comp->getRowID() == Row )
                    comp->setHighlight( true );
                else
                    comp->setHighlight( false );
            }
            right.table->setFocus( true );
        };

        right.provider.numRows = [&]() -> int {
            if( filterNum != Null )
                return static_cast<int>( tempList.size() );
            else return 0;
        };

        right.provider.cellComponent = [&]( int Row, Component* Existing ) -> juce::Component* {
            auto t1 = juce::Time::currentTimeMillis();

            auto comp = static_cast<FileRightEntry*>( Existing );
            if( comp == nullptr )
            {
                comp = new FileRightEntry();
                comp->setInterceptsMouseClicks( false, true );
            }
            auto t3 = juce::Time::currentTimeMillis();
            if( tempList[Row].exists() )
            {
                auto t5 = juce::Time::currentTimeMillis();

                std::vector<std::pair<juce::File, int>> fileWithNumbers;

                for( const auto& file : tempList )
                {
                    int num = file.getFileName().retainCharacters( "0123456789" ).getIntValue();
                    fileWithNumbers.emplace_back( file, num );
                }

                // Sort based on the precomputed numeric values
                std::sort( fileWithNumbers.begin(), fileWithNumbers.end(),
                           []( const auto& a, const auto& b ) {
                               return a.second < b.second;
                           } );

                auto t6       = juce::Time::currentTimeMillis();
                auto callback = [this, comp, Row]( juce::int64 totalSize ) {
                    auto         t1         = juce::Time::currentTimeMillis();
                    juce::String sizeString = juce::File::descriptionOfSizeInBytes( totalSize );
                    if( comp )
                    {
                        if( bottom.allButton.getToggleState() )
                        {
                            if( tempList[Row].isDirectory() )
                                comp->setProperties( "Vector", true, 22.0f, 16.0f, tempList[Row].getFileName(), tempList[Row].getLastModificationTime().toString( true, false, false, false ), tempList[Row].getLastModificationTime().toString( false, true, false, true ), sizeString );
                            else if( tempList[Row].hasFileExtension( projectFileList ) )
                                comp->setProperties( "Icon_ProjectFile", false, 14.0f, 16.0f, tempList[Row].getFileName(), tempList[Row].getLastModificationTime().toString( true, false, false, false ), tempList[Row].getLastModificationTime().toString( false, true, false, true ), sizeString );
                            else if( tempList[Row].hasFileExtension( sampleFileList ) )
                                comp->setProperties( "Icon_SampleFile", false, 14.0f, 16.0f, tempList[Row].getFileName(), tempList[Row].getLastModificationTime().toString( true, false, false, false ), tempList[Row].getLastModificationTime().toString( false, true, false, true ), sizeString );
                            else
                                comp->setProperties( "Icon_DefaultFile", true, 22.0f, 16.0f, tempList[Row].getFileName(), tempList[Row].getLastModificationTime().toString( true, false, false, false ), tempList[Row].getLastModificationTime().toString( false, true, false, true ), sizeString );
                        }
                        else if( bottom.sampleButton.getToggleState() )
                            comp->setProperties( "Icon_SampleFile", false, 14.0f, 16.0f, tempList[Row].getFileName(), tempList[Row].getLastModificationTime().toString( true, false, false, false ), tempList[Row].getLastModificationTime().toString( false, true, false, true ), sizeString );

                        else if( bottom.projectButton.getToggleState() )
                            comp->setProperties( "Icon_ProjectFile", false, 14.0f, 16.0f, tempList[Row].getFileName(), tempList[Row].getLastModificationTime().toString( true, false, false, false ), tempList[Row].getLastModificationTime().toString( false, true, false, true ), sizeString );

                        DBG( "Num Rows " << numRows << " temp size  " << tempList.size() );
                        if( numRows++ == tempList.size() )
                        {
                            DBG( "sizeCalculationEnd" );
                            numRows = 1;
                            if( sizeCalculationEnd )
                                sizeCalculationEnd();
                        }
                    }
                };
                startFolderSizeCalculation( tempList[Row], callback );
            }

            return comp;
        };

        //Double Click

        right.table->onRowDoubleClicked = [&]( int Row ) {
            if( !top.selectBtn.getToggleState() )
            {
                if( tempList[Row].isDirectory() )
                {
                    int row;
                    for( row = 0; row < dirList.size(); row++ )
                        if( dirList[row] == tempList[Row] )
                            break;
                    enterDirectory( row );
                }
                else if( tempList[Row].getFileExtension() == mt::__projectFileSuffix )
                {
                    juce::File file = tempList[Row].getParentDirectory();
                    if( left.table->getSelectedRow() == DEMOSROW )
                        RecentProject::enableRecentProjVTreeEntry = false;
                    ui::LoadProjectAndAnimation::getInstance().startLoading( file );
                }
                else
                    loadSample( tempList[Row], false );
            }
        };

        //Single Click

        right.provider.onRowSelected = [&]( int Row ) {
            if( !ui::RecentProject::isProjectLoading )
            {
                if( top.selectBtn.getToggleState() && tempList.size() )
                {
                    if( selectedFile.contains( juce::File( tempList[Row] ) ) )
                        selectedFile.removeFirstMatchingValue( juce::File( tempList[Row] ) );
                    else
                        selectedFile.add( juce::File( tempList[Row] ) );
                }
                else if( tempList.size() )
                {
                    selectedFile.clear();
                    selectedFile.add( juce::File( tempList[Row] ) );
                }
                else //Safety handle
                    selectedFile.clear();
                if( settings.preview.isAuto )
                    audition( juce::File( selectedFile.getLast() ) );
                else if( ae.isPreviewRunning() )
                {
                    bottom.playIconButton.setToggleState( false, juce::dontSendNotification );
                    ae.stopPreview();
                }
            }
        };
    }

    void FileBrowser::startFolderSizeCalculation( const juce::File& folder, std::function<void( juce::int64 )> callback )
    {
        auto totalSize = fileUtils::calculateFolderSize( folder );

        juce::MessageManager::callAsync( [=]() {
            callback( totalSize );
        } );
    }

    void FolderSizeCalculator::calculateFolderSize()
    {
        totalSize = fileUtils::calculateFolderSize( folderToCalculate );

        juce::MessageManager::callAsync( [=]() {
            resultCallback( totalSize );
        } );
    }

    void FileBrowser::addMultipleAssets( juce::Array<juce::File> assetFiles )
    {
        int                       filesNum = 0;
        std::vector<juce::String> assets   = ae.getAssets( mt::AssetTypes::Samples );

        for( auto assetFile : assetFiles )
        {
            for( auto asset : assets )
            {
                if( asset == assetFile.getFullPathName() )
                    filesNum++;
                else
                    ae.addAsset( assetFile.getFullPathName(), mt::AssetTypes::Samples );
            }
        }
        if( !assets.size() )
        {
            for( auto dir : assetFiles )
                if( dir.hasFileExtension( sampleFileList ) )
                    ae.addAsset( dir.getFullPathName(), mt::AssetTypes::Samples );
        }
        if( filesNum )
        {
            auto p = new FilePopup{ juce::String( filesNum ) + " Samples has already Loaded", false, false };
            mt::utils::showDialog(
                "WARNING!", p, [this, p]( int R ) { delete p; }, ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        }
    }

    void FileBrowser::favouriteButtonImplementation()
    {
        for( auto& button : grpNum )
        {
            header.addButton( button.get(), numButtonWidth );
            juce::String buttonString = settings.browser.getPathAssignedToButton( button->getName() );
            if( buttonString.isEmpty() )
                button->setToggleState( false, juce::dontSendNotification );
            else
                button->setToggleState( true, juce::dontSendNotification );

            button->onClick = [&]() {
                if( settings.browser.isValid() )
                {
                    juce::String buttonString = settings.browser.getPathAssignedToButton( button->getName() );
                    if( button->getToggleState() && buttonString.isEmpty() )
                        onFirstClick( button.get() );
                    else
                        onSecondClick( button.get() );
                }
            };
            button->onDoubleClick = [&]() {
                button->setToggleState( false, juce::dontSendNotification );
                onDoubleClick( button.get() );
            };
        }

        onFirstClick = [this]( ui::KickImageButton* button ) {
            for( auto i = 0; i < settings.browser.getNumProperties(); i++ )
            {
                for( auto j = 0; j < grpNum.size(); j++ )
                {
                    juce::String propertyName = "button" + juce::String( j + 1 );
                    juce::String path         = settings.browser.getPathAssignedToButton( propertyName );

                    if( path == ae.getCurrentDir().getFullPathName() )
                    {
                        button->setToggleState( false, juce::dontSendNotification );
                        buttonId = grpNum.size() - j - 1;
                        timerInt = 1;
                        stopTimer();
                        startTimer( 100 ); //to blink the button
                        return;
                    }
                }
            }
            settings.browser.setPathToButton( button->getName(), ae.getCurrentDir().getFullPathName() );
            settings.browser.setSelectedLeftRow( button->getName() + "selectedLeftRow", left.table->getSelectedRow() );
            pwd = ae.getCurrentDir().getFullPathName();
            button->setToggleState( true, juce::dontSendNotification );
        };

        onSecondClick = [&]( ui::KickImageButton* button ) {
            juce::String button1Var      = settings.browser.getPathAssignedToButton( button->getName() );
            juce::var    leftSelectedRow = settings.browser.getSelectedLeftRow( button->getName() + "selectedLeftRow" );
            load( button1Var );
            boolLoad = true;
            left.table->selectRow( leftSelectedRow );
            button->setToggleState( true, juce::dontSendNotification );
        };

        onDoubleClick = [&]( ui::KickImageButton* button ) {
            settings.browser.setPathToButton( button->getName(), "" );
            button->setToggleState( false, juce::dontSendNotification );
        };
    }

    void FileBrowser::renameImplementation( juce::String newFileName )
    {
        auto& prjSettings = settings.projects;
        for( auto i = 0; i < prjSettings.getNumChildren(); i++ )
        {
            juce::File childFile = juce::File( prjSettings.getProject( i ) );
            if( newFileName == childFile.getFileNameWithoutExtension() )
            {
                auto p = new FilePopup( "A project with the same name already exists ", false, false );
                mt::utils::showDialog(
                    "WARNING!", p, [&, p]( int R ) {
                        delete p;
                    },
                    ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );

                return;
            }
        }

        auto p = selectedFile.getLast().getFileExtension();
        newFileName += p;
        juce::File newFile = selectedFile.getLast().getParentDirectory().getChildFile( newFileName );
        juce::File sel     = selectedFile.getLast();

        if( selectedFile.getLast().exists() && !newFile.exists() )
            selectedFile.getLast().moveFileTo( newFile );

        //If User Renaming the Project Directory , update the project file also
        juce::Array<juce::File> arr = newFile.findChildFiles( 2, false, "*.kickPrj" );
        if( newFile.isDirectory() && arr.getFirst().getFileExtension() == mt::__projectFileSuffix )
        {
            juce::File childFile = arr.getFirst().getParentDirectory().getChildFile( newFileName + mt::__projectFileSuffix );
            if( arr.getFirst().exists() && !childFile.exists() )
                arr.getFirst().moveFileTo( childFile );
        }

        //If User Renaming the Project File,update the project directory also
        if( newFile.getFileExtension() == mt::__projectFileSuffix )
        {
            juce::String fileName     = newFile.getFileNameWithoutExtension();
            juce::File   originalFile = newFile.getParentDirectory();
            juce::File   parentFile   = originalFile.getParentDirectory().getChildFile( fileName );
            if( originalFile.exists() && !parentFile.exists() )
                originalFile.moveFileTo( parentFile );
            newFile = parentFile;
            exitDirectory();
        }

        juce::File projectFile;
        ( sel.getFileExtension() == mt::__projectFileSuffix ) ? projectFile = sel.getParentDirectory() : projectFile = sel;
        settings.projects.renameProject( projectFile, newFile );
        enterDirectory( -1 );
        selectedFile.clear();
        right.table->clearSelection();
    }

    void FileBrowser::renamePopup()
    {
        if( !selectedFile.size() )
        {
            auto p = new FilePopup{ "Please Select the File or Folder to Rename", false, false };
            mt::utils::showDialog(
                "WARNING!", p, [this, p]( int R ) { delete p; }, ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        }
        else
        {
            if( !checkIsCurrProj() )
            {
                auto p = new NewProjectPopupData( "Please Enter the name of the Project", "New File Name" );
                mt::utils::showOptions(
                    "RENAME POPUP", p, [this, p]( int R ) {
                        if( R == UI::APPLY )
                        {
                            p->close();
                            renameImplementation( p->getInputText() );
                            delete p;
                        }
                    },
                    ui::layout::warningPopup );
            }
        }
    }

    void FileBrowser::updateTemp( int num )
    {
        switch( num )
        {
            default:
            case All:
                tempList = dirList;
                break;
            case Projects:
                fileExtension( projectFileList );
                break;
            case Samples:
                fileExtension( sampleFileList );
                break;
            case Null:
                break;
        }
    }

    void FileBrowser::searchImplementation()
    {
        juce::File              currentDirectory = juce::File( pwd );
        juce::Array<juce::File> foundFiles;
        currentDirectory.findChildFiles( foundFiles, juce::File::findFilesAndDirectories, true, "*" );
        auto searchText = top.searchLabel.getText().toUpperCase().toStdString();
        tempList.clear();

        for( auto dir : foundFiles )
            if( dir.getFileNameWithoutExtension().toUpperCase().toStdString().find( searchText ) != std::string::npos )
                tempList.add( dir );

        foundFiles.clear();
        right.table->refresh();
    }

    void FileBrowser::fileExtension( juce::String FileList )
    {
        tempList.clear();
        juce::File              currentDirectory = ae.getCurrentDir();
        juce::Array<juce::File> foundFiles;
        currentDirectory.findChildFiles( foundFiles, juce::File::findFiles, true, "*" );
        for( auto dir : foundFiles )
        {
            if( dir.hasFileExtension( FileList ) )
                tempList.add( dir );
        }
    }

    void FileBrowser::load( juce::String dir )
    {
        juce::File buttonFile = juce::File( dir );
        ae.setCurrentDir( buttonFile );
        enterDirectory( -1 );
    }

    FileBrowser::~FileBrowser()
    {}
    void FileBrowser::enterDirectory( int index )
    {
        ae.dirEnter( index );
        juce::File pth = ae.getCurrentDir().getFullPathName();
        if( pth.existsAsFile() )
            ae.dirGoUp();
        pwd                   = ae.getCurrentDir().getFullPathName();
        juce::String filePath = fileUtils::getFilePath( pwd, replacementPath );
        top.searchLabel.setText( filePath, juce::dontSendNotification );
        tempList.clear();
        updateTemp( filterNum );
        juce::String pwdParentPath = ae.getCurrentDir().getParentDirectory().getFullPathName();
        pwdParentPath == root ? right.backButton.setEnabled( false ) : right.backButton.setEnabled( true );
        right.table->refresh();
        if( !tempList.size() && sizeCalculationEnd )
            sizeCalculationEnd();
        if( tempList.size() ) //safety check
        {
            selectedFile.clear();
            selectedFile.add( juce::File( tempList[0] ) );
            right.table->selectRow( 0 );
        }
    }

    void FileBrowser::exitDirectory()
    {
        ae.dirGoUp();
        pwd                   = ae.getCurrentDir().getFullPathName();
        juce::String filePath = fileUtils::getFilePath( pwd, replacementPath );
        top.searchLabel.setText( filePath, juce::dontSendNotification );
        updateTemp( filterNum );
        juce::String pwdParentPath = ae.getCurrentDir().getParentDirectory().getFullPathName();
        pwdParentPath == root ? right.backButton.setEnabled( false ) : right.backButton.setEnabled( true );
        right.table->refresh();
        right.table->clearSelection();
        selectedFile.clear();
    }

    void FileBrowser::audition( juce::File f )
    {
        ae.stopPreview();
        if( f.existsAsFile() )
        {
            ae.createPreview( f, settings.preview.gain );
            if( !bottom.playIconButton.getToggleState() )
                bottom.playIconButton.setToggleState( true, juce::dontSendNotification );
        }
    }
    void FileBrowser::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) && contracts::DclContractsHelper::getKnobCode( f_HwMsg ) == contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER )
            knobRotated( contracts::DclContractsHelper::getKnobValue( f_HwMsg ) );

        if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && contracts::DclContractsHelper::getButtonCode( f_HwMsg ) == contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_ENTER && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
            knobClicked();

        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) && contracts::DclContractsHelper::getPadState( f_HwMsg ) )
        {
            ui::fileUtils::setHighlightLED( contracts::DclContractsHelper::getPadNumber( f_HwMsg ) );
            int              bankOffset = ae.getBankOffset();
            auto             padNo      = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            auto             padState   = contracts::DclContractsHelper::getPadState( f_HwMsg );
            auto             velocity   = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );
            std::vector<int> pads;

            padNo += bankOffset;
            pads.push_back( padNo );

            for( auto& it : pads )
            {
                juce::MidiMessage Msg;

                if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
                    Msg = juce::MidiMessage::noteOn( 1, it, (juce::uint8)velocity );
                else if( padState == contracts::DeviceControlContracts::BTN_RELEASED )
                    Msg = juce::MidiMessage::noteOff( 1, it );

                ae.addLiveMidiMsg( ae.getSelected(), Msg );
            }
            pads.clear();
        }
        else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
            }
        }
    }

    void FileBrowser::visibilityChanged()
    {
        numRows = 1;

        if( isShowing() || isVisible() )
        {
            onFirstLaunch = true;
            right.table->setAutoScrollToSelection( false );
            left.table->refresh();
            auto rightSelectedRow = right.table->getSelectedRow();
            load( pwd );
            left.table->selectRow( left.table->getSelectedRow() );
            right.table->selectRow( rightSelectedRow );

            right.table->setAutoScrollToSelection( true );

            if( !ae.isProjectLoaded() )
            {
                bottom.loadButton.setEnabled( false );
                bottom.loadAllButton.setEnabled( false );
                bottom.sampleAssignButton.setEnabled( false );
            }
            else
            {
                bottom.loadButton.setEnabled( true );
                bottom.loadAllButton.setEnabled( true );
                hideAuditionComponent();
                if( !ae.getNumPrograms() )
                    bottom.sampleAssignButton.setEnabled( false );

                else
                    bottom.sampleAssignButton.setEnabled( true );
            }
        }
        else
        {
            ae.stopPreview();
            if( ae.isPreviewRunning() )
            {
                bottom.playIconButton.setToggleState( false, juce::dontSendNotification );
            }
            bottom.auditionButton.setButtonText( "OFF" );
            bottom.playIconButton.setEnabled( false );
            settings.preview.isAuto = false;
            settings.preview.isOn   = false;
            top.selectBtn.setEnabled( true );
            top.selectBtn.setToggleState( false, juce::dontSendNotification );
            top.renameBtn.setToggleState( false, juce::dontSendNotification );
            selectBtnOnClick();
            bottom.allButton.setToggleState( true, juce::dontSendNotification );
            statusBar.setBoxInfo( "Box Info" );
        }
    }

    void FileBrowser::knobRotated( double value )
    {
        right.table->knobRotated( value );
    }

    void FileBrowser::knobClicked()
    {
        right.table->onRowDoubleClicked( right.table->getSelectedRow() );
    }

    void FileBrowser::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 5 ) );
    }

    void FileBrowser::resized()
    {
        auto b = getLocalBounds();

        auto headerBounds = b.removeFromTop( ui::layout::header.getHeight() );
        header.setBounds( headerBounds );
        top.setBoundsRelative( 0.0f, 0.1558f, 1.0f, 0.083f );
        right.setBoundsRelative( 0.354f, 0.254f, 0.6379f, 0.6429f );
        left.setBoundsRelative( 0.0097f, 0.254f, 0.3346f, 0.6429f );
        bottom.setBoundsRelative( 0.0f, 0.915f, 1.0f, 0.0668f );
        auditionComp.setBoundsRelative( .8894f, .43f, .1027f, .4582f );
    }

    void FileBrowser::hideAuditionComponent()
    {
        bottom.volumeBtn.setToggleState( false, juce::dontSendNotification );
        mt::utils::makeHidden( auditionComp );
    }

    void FileBrowser::showAuditionComponent()
    {
        auditionComp.setVolume( settings.preview.gain );
        mt::utils::makeVisible( auditionComp );
    }

    void FileBrowser::mouseDown( const juce::MouseEvent& e )
    {
        juce::Point<int>     clickPosition         = e.getMouseDownScreenPosition();
        juce::Rectangle<int> parentBounds          = bottom.getBounds();
        juce::Point<int>     bottomClickPosition   = clickPosition - parentBounds.getPosition();
        juce::Rectangle<int> volumeBtnBounds       = bottom.volumeBtn.getBounds();
        juce::Rectangle<int> volumeComponentBounds = auditionComp.getBounds();

        if( !volumeComponentBounds.contains( clickPosition ) && ( !volumeBtnBounds.contains( bottomClickPosition ) ) )
            hideAuditionComponent();
    }

    bool FileBrowser::checkIsCurrProj()
    {
        for( auto sel : selectedFile )
        {
            juce::String currProj = juce::File( mt::getSettings().projects.currentProject() ).getFileNameWithoutExtension();
            if( sel.getFileNameWithoutExtension() == currProj )
            {
                auto p = new FilePopup{ "Current Project is not allowed to Cut/Delete/Rename", false, false };
                mt::utils::showDialog(
                    "WARNING!", p, [&, p]( int R ) { top.deleteBtn.setToggleState(false,juce::dontSendNotification);delete p; }, ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );

                return true;
            }
        }
        return false;
    }

    void FileBrowser::addData( const juce::String& buttonText, const juce::String& buttonTextActive, float width, float height, const juce::String& itemName, bool isEject )
    {
        DeviceData newData;
        newData.imgBtn       = buttonText;
        newData.imgBtnActive = buttonTextActive;
        newData.width        = width;
        newData.height       = height;
        newData.fileName     = itemName;
        newData.isEjectBtn   = isEject;
        deviceData.push_back( newData );
    }

    void FileBrowser::deleteSystem()
    {
        if( !checkIsCurrProj() )
        {
            top.deleteBtn.setToggleState( false, juce::dontSendNotification );
            if( selectedFile.size() )
            {
                juce::String data, imageIcon, fileName, timeStamp;
                int          width, height;

                if( selectedFile.getLast().isDirectory() )
                {
                    imageIcon = "Vector-1";
                    width     = directoryWidth;
                    height    = directoryHeight;
                }
                else
                {
                    imageIcon = "Icon_DefaultFile1";
                    width     = fileWidth;
                    height    = fileHeight;
                }
                if( selectedFile.size() == 1 )
                {
                    data      = "Are you sure you want to permanently delete this file?";
                    fileName  = selectedFile.getLast().getFileNameWithoutExtension();
                    timeStamp = selectedFile.getLast().getLastModificationTime().toString( true, false, false, false );
                }
                else
                {
                    data                  = "Are you sure you want to permanently delete these " + juce::String( selectedFile.size() ) + " files ? ";
                    fileName              = "";
                    juce::int64 totalSize = 0;

                    for( auto sel : selectedFile )
                        totalSize += sel.getSize();
                    timeStamp = "Total Size: " + juce::String( totalSize ) + " bytes";
                }
                auto popup = new FileLeftEntryPopup( data, imageIcon, width, height, fileName, timeStamp );
                mt::utils::showDialog(
                    "DELETE CONFIRMATION", popup, [popup]( int R ) { delete popup; }, ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );

                popup->button1.onClick = [&]() {
                    popup->close();
                };
                popup->button2.onClick = [&]() {
                    popup->close();
                    for( auto sel : selectedFile )
                    {
                        settings.projects.removeProject( sel );
                        sel.deleteRecursively();
                    }
                    enterDirectory( -1 );
                    selectedFile = juce::File( tempList[right.table->getSelectedRow()] );
                    right.table->clearSelection();
                    selectedFile.clear();
                    right.table->selectRow( 0 );
                };
            }

            else
            {
                auto p = new FilePopup{ "Please Select the File or Folder to delete", false, false };
                mt::utils::showDialog(
                    "WARNING!", p, [&, p]( int R ) { delete p; }, ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
            }
        }
    }

    void FileBrowser::createProject( juce::String value )
    {
        auto nChild = settings.projects.getNumChildren();
        for( auto i = 0; i < nChild; i++ )
        {
            juce::String childProperty = settings.projects.getProject( i );
            juce::File   childFile     = juce::File( childProperty );
            if( value == childFile.getFileName() )
            {
                auto p = new FilePopup( "A project with the same name already exists ", false, false );
                mt::utils::showDialog(
                    "WARNING!", p, [&, p]( int R ) {
                        delete p;
                    },
                    ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
                return;
            }
        }
        auto       seprator = juce::File::getSeparatorString();
        juce::File file     = getAppDirs( MTProjects ).getFullPathName() + seprator + value;
        settings.projects.createProject( file );
        ui::LoadProjectAndAnimation::getInstance().startLoading( file );
    }

    void FileBrowser::loadSample( const juce::Array<juce::File>& assetFiles, bool IsMultiple )
    {
        sampleAnimation->startLoading( assetFiles, IsMultiple, [&, assetFiles]() {
            sampleAnimation->stopAnimation();
            if( sampleAnimation->isSampleLoaded() )
            {
                for( auto asset : assetFiles )
                {
                    if( asset.hasFileExtension( sampleFileList ) )
                    {
                        statusBar.setBoxInfo( asset.getFileNameWithoutExtension() + " has Loaded" );
                        break;
                    }
                }
            }
        } );
    }

    void FileBrowser::timerCallback()
    {
        if( getTimerInterval() == 100 )
        {
            if( timerInt & 1 )
                grpNum[buttonId]->setToggleState( false, juce::dontSendNotification );
            else
                grpNum[buttonId]->setToggleState( true, juce::dontSendNotification );
            if( timerInt++ == 6 )
            {
                stopTimer();
                startTimer( 1000 );
            }
        }

        if( getTimerInterval() == 1000 )
        {
#if defined( LINUX ) || defined( __linux__ )

            // Checking SD card
            if( fileUtils::getSdCardStatus() )
            {
                sdCardOn = true;

                if( !fileUtils::getSdCardEjectStatus() )
                {
                    if( sdCardFirstTime ) //add it only on 1st time
                    {
                        sdCardFirstTime = false;
                        row.push_back( fileUtils::getSdCardMountedPath() );
                        addData( "SD", "SDactive", 12.0f, 16.0f, "External (SD Card)", true );
                        left.table->refresh();
                    }
                }
            }
            //else part will execute when there is no SD card
            else
            {
                if( sdCardOn ) //if we remove the sd card
                {
                    if( !fileUtils::getSdCardEjectStatus() )
                    {
                        deviceData.erase( deviceData.begin() + sdCardRow );
                        row.erase( row.begin() + sdCardRow );

                        sdCardRow = -1;
                        if( deviceData.size() == 4 )
                            USBRow = 3;
                        sdCardFirstTime = true;
                        left.table->refresh();
                        left.table->selectRow( 0 );
                    }
                }

                sdCardOn = false;
                fileUtils::setSdCardEjectStatus( false );
            }

            //checking USB
            if( fileUtils::getUsbStatus() )
            {
                usbOn = true;

                if( !fileUtils::getUsbEjectStatus() )
                {
                    if( usbFirstTime )
                    {
                        usbFirstTime = false;
                        row.push_back( fileUtils::getUsbMountedPath() );
                        addData( "USB", "UDBactive", 12.0f, 16.0f, "External (USB)", true );
                        left.table->refresh();
                    }
                }
            }
            //else part will execute when there is no USB
            else
            {
                if( usbOn )
                {
                    if( !fileUtils::getUsbEjectStatus() )
                    {
                        deviceData.erase( deviceData.begin() + USBRow );
                        row.erase( row.begin() + USBRow );
                        USBRow = -1;
                        if( deviceData.size() == 4 )
                            sdCardRow = 3;
                        usbFirstTime = true;
                        left.table->refresh();
                        left.table->selectRow( 0 );
                    }
                }

                usbOn = false;
                fileUtils::setUsbEjectStatus( false );
            }

#endif
        }
    }

    ///////////////////////////////////////
    // Class FileBrowserLeftPanel
    ///////////////////////////////////////
    FileBrowserLeftPanel::FileBrowserLeftPanel()
    {
        setLookAndFeel( KickLooknFeel::getLnf() );
        mt::utils::addAndMakeVisible( this, places, content, expansion, location );

        location.setPadding( labelLeftMargin );
        location.setFontHeight( labelFontHeight );
        location.setValueJustification( juce::Justification::centredLeft );
        location.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        table.reset( new KickTableView( "List", provider ) );
        addAndMakeVisible( *table );
        table->setRowHeight( leftTableRowHeight );
        table->setRowBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        table->setSelectedRowBackgroundColour( ui::Theme::backgroundColour( 0 ) );
        table->setSelectedRowOutlineColour( ui::Theme::outlineColour( 0 ) );

        table->setRowSpacing( 0 );
        table->enableScrollBar( false );

        themeChanged.connect( [&]() {
            places.setImages( "Places" );
            content.setImages( "Content" );
            expansion.setImages( "Expansion" );
        } );
    }

    void FileBrowserLeftPanel::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds();
        //Top Rectangle
        mt::utils::fillRoundedRect( g, leftTopBounds, ui::Theme::backgroundColour( 0 ), 6.0f, true, true, false, false );

        //Centre Rectangle
        mt::utils::fillRoundedRect( g, leftCentreBounds, ui::Theme::backgroundColour( 1 ), 6.0f, false, false, true, true );

        //Draw Border
        g.setColour( ui::Theme::outlineColour( 3 ) );
        g.drawRoundedRectangle( b.getX(), b.getY(), b.getWidth(), b.getHeight(), cornerSizeX, lineThickness );

        //Draw Centre Line
        g.setColour( ui::Theme::outlineColour( 3 ) );
        g.drawRect( { lineX, lineY, lineWidth, lineHeight }, lineThickness );
    }

    void FileBrowserLeftPanel::resized()
    {
        auto b = getLocalBounds();
        table->setBoundsRelative( 0.0f, 0.16f, 1.0f, 0.8421f );
        location.setBoundsRelative( 0.0292f, 0.012f, 0.35f, 0.1294f );
    }

    //////////////////////////////////
    // Class FileBrowserRightPanel
    //////////////////////////////////
    FileBrowserRightPanel::FileBrowserRightPanel()
    {
        table.reset( new KickTableView( "List", provider ) );
        mt::utils::addAndMakeVisible( this, *table, name, date, timeStamp, size, backButton );

        table->setRowHeight( rightTableRowHeight );
        table->setRowSpacing( 0 );
        table->setRowBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        table->setSelectedRowBackgroundColour( ui::Theme::backgroundColour( 0 ) );
        table->setSelectedRowOutlineColour( ui::Theme::outlineColour( 0 ) );
        table->setAutoScrollToSelection( true );

        name.setPadding( labelLeftMargin );
        name.setFontHeight( labelFontHeight );
        date.setFontHeight( labelFontHeight );
        timeStamp.setFontHeight( labelFontHeight );
        size.setFontHeight( labelFontHeight );

        name.setValueJustification( juce::Justification::centredLeft );
        date.setValueJustification( juce::Justification::centred );
        timeStamp.setValueJustification( juce::Justification::centred );
        size.setValueJustification( juce::Justification::centred );

        name.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        date.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        timeStamp.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        size.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        themeChanged.connect( [&]() {
            backButton.setImages( "Back_Browser" );
        } );
    }

    void FileBrowserRightPanel::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds();
        //Top Rectangle
        mt::utils::fillRoundedRect( g, rightTopBounds, ui::Theme::backgroundColour( 1 ), 6.0f, true, true, false, false );

        //Centre Rectangle
        mt::utils::fillRoundedRect( g, rightCentreBounds, ui::Theme::backgroundColour( 3 ), 6.0f, false, false, true, true );

        //Draw Border
        g.setColour( ui::Theme::outlineColour( 3 ) );
        g.drawRoundedRectangle( b.getX(), b.getY(), b.getWidth(), b.getHeight(), cornerSizeX, lineThickness );

        //Draw Centre Line
        g.setColour( ui::Theme::outlineColour( 3 ) );
        g.drawRect( { lineX, lineY, 652, lineHeight }, lineThickness );
    }

    void FileBrowserRightPanel::resized()
    {
        table->setBoundsRelative( 0.012f, 0.16f, 0.987f, 0.842f );
        backButton.setBoundsRelative( 0.0153f, 0.0292f, 0.0521f, 0.0994f );
        name.setBoundsRelative( 0.09227f, 0.0146f, 0.2622f, 0.1286f );
        date.setBoundsRelative( 0.3857f, 0.0146f, 0.1840f, 0.1286f );
        timeStamp.setBoundsRelative( 0.5991f, 0.0146f, 0.1840f, 0.1286f );
        size.setBoundsRelative( 0.81780f, 0.0146f, 0.1840f, 0.1286f );
    }

    //////////////////////////////////
    // Class FileBrowserTopControls
    ///////////////////////////////////

    FileBrowserTopControls::FileBrowserTopControls()
    {
        mt::utils::addAndMakeVisible( this, searchLabel, searchButton, deleteBtn, selectBtn, renameBtn, cutBtn, copyBtn, pasteBtn );
        searchLabel.setTextColour( ui::Theme::textColour( 1 ) );
        searchLabel.setEditable( false, true );
        searchLabel.enableKeyboardEmptyText( true );
        searchLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );
        searchLabel.setPadding( 10 );
        searchLabel.setFontHeight( 18.0f );

        onElementChangedCallback = [&]() {
            repaint();
            resized();
        };

        themeChanged.connect( [&]() {
            searchButton.setImages( "Search_Browser", "Search_BrowserActive" );
            backButton.setImages( "Back_Browser", "Back_Browser", "Back_BrowserActive", "Back_Browser_Disabled" );
            projectButton.setImages( "Project_Browser", "Project_BrowserActive" );
            sampleButton.setImages( "Sample_Browser", "Sample_BrowserActive" );
            allButton.setImages( "All_Browser", "All_BrowserActive" );
            cutBtn.setImages( "Cut Button", "Cut Button_Active", "Cut Button", "Cut Disabled" );
            copyBtn.setImages( "Copy Button", "Copy Button", "Copy Button_Active" );
            pasteBtn.setImages( "Paste Button", "Paste Button", "Paste Button", "Paste Disabled" );
            deleteBtn.setImages( "Trash", "Trash_Active", "Trash", "Trash Disabled" );
            renameBtn.setImages( "Rename", "Rename_Active", "Rename", "Rename Disabled" );
            selectBtn.setImages( "Select_Normal", "Select_Active" );
        } );
    }

    void FileBrowserTopControls::paint( juce::Graphics& g )
    {
        juce::Rectangle<int> bounds;
        selectBtn.getToggleState() ? ( bounds = { 10, 0, 750, 43 } ) : ( bounds = { 10, 0, 904, 43 } );

        mt::utils::renderRoundedRect( g, bounds, ui::Theme::outlineColour( 3 ), 1, 6.0f );
        mt::utils::fillRoundedRect( g, bounds, ui::Theme::backgroundColour( 3 ), 6.0f );
    }

    void FileBrowserTopControls::resized()
    {
        if( !selectBtn.getToggleState() )
        {
            searchLabel.setBoundsRelative( .0097f, .0f, .8828f, .9772f );
            searchButton.setBoundsRelative( .8515f, .0f, .0429f, 1.0f );
            selectBtn.setBoundsRelative( .8984f, .0f, .0429f, 1.0f );
            deleteBtn.setBoundsRelative( .9453f, .0f, .0429f, 1.0f );
        }
        else
        {
            searchLabel.setBoundsRelative( .0097f, .0f, .7324f, .9772f );
            searchButton.setBoundsRelative( .7099f, .0f, .0429f, 1.0f );
            selectBtn.setBoundsRelative( .7578f, .0f, .0429f, 1.0f );
            renameBtn.setBoundsRelative( .9453f, .0f, .0429f, 1.0f );
            pasteBtn.setBoundsRelative( .8984f, .0f, .0429f, 1.0f );
        }

        cutBtn.setBoundsRelative( .8046f, .0f, .0429f, 1.0f );
        copyBtn.setBoundsRelative( .8515f, .0f, .0429f, 1.0f );
        repaint();
    }

    ///////////////////////////////////
    // Class FileBrowserBottomControls
    ///////////////////////////////////
    FileBrowserBottomControls::FileBrowserBottomControls()
        : TabbedComponent( juce::TabbedButtonBar::Orientation::TabsAtBottom )
    {
        mt::utils::setRadioGroupID( (int)kick_ui::RadioGroupId::FileBrowserFilter, allButton, projectButton, sampleButton );

        auditionButton.incrementSelection();

        sampleAssignButton.setValueColour( ui::Theme::textColour( 1 ) );
        sampleAssignButton.setValueFontHeight( 16 );

        loadButton.setValueColour( ui::Theme::textColour( 1 ) );
        loadButton.setValueFontHeight( 16 );

        loadAllButton.setValueColour( ui::Theme::textColour( 1 ) );
        loadAllButton.setValueFontHeight( 16 );

        mt::utils::addAndMakeVisible( this, sampleAssignButton, playIconButton, auditionButton, loadButton, loadAllButton, projectButton, sampleButton, allButton, volumeBtn, volumeMuteBtn );
        volumeMuteBtn.setVisible( false );
        sampleAssignButton.onClick = [&]() {
            kick_ui::signals::sigScreenOpen( kick_ui::Screen::MenuAssignSample );
        };

        themeChanged.connect( [&]() {
            projectButton.setImages( "Project_Browser", "Project_BrowserActive" );
            sampleButton.setImages( "Sample_Browser", "Sample_BrowserActive" );
            allButton.setImages( "All_Browser", "All_BrowserActive" );
            playIconButton.setImages( "Play_browser", "Play_browser", "Play_browser_Active" );
            volumeBtn.setImages( "Volume_Browser", "Volume_BrowserActive" );
            volumeMuteBtn.setImages( "Button_VolumeMute", "Button_VolumeMute" );
        } );
    }

    void FileBrowserBottomControls::paint( juce::Graphics& g )
    {}

    void FileBrowserBottomControls::resized()
    {
        auto b = getLocalBounds();

        projectButton.setBounds( 10, 0, 100, 34 );
        sampleButton.setBounds( 114, 0, 100, 34 );
        allButton.setBounds( 218, 0, 100, 34 );
        sampleAssignButton.setBounds( 394, 0, 100, 34 );
        loadButton.setBounds( 498, 0, 100, 34 );
        loadAllButton.setBounds( 602, 0, 100, 34 );
        playIconButton.setBounds( 706, 0, 100, 34 );
        auditionButton.setBounds( 810, 0, 100, 34 );
        volumeBtn.setBounds( 914, 0, 100, 34 );
        volumeMuteBtn.setBounds( 914, 0, 100, 34 );
    }

    ///////////////////////////////////////
    // Class FileLeftEntry
    ///////////////////////////////////////
    FileLeftEntry::FileLeftEntry( int rowId )
    {
        row = rowId;
        file.setTextColour( ui::Theme::textColour( 1 ) );
        file.setFontHeight( 14.0f );
        file.setPadding( 10 );
        file.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        imageButton.setInterceptsMouseClicks( false, false );
        file.setInterceptsMouseClicks( false, false );
        ejectButton.setInterceptsMouseClicks( true, false );
        ejectButton.onClick = [&]() {
            auto p = new FilePopup{ "Do you want to Eject this device?" };
            //need to implement the functionality
            mt::utils::showDialog(
                "EJECT", p, [p, this]( int R ) {
                    if(R==UI::APPLY)
                    {
                        if( ejectCallback )
                            ejectCallback( row );
                    }
                    delete p; },
                ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        };

        formatButton.onClick = [&]() {
            auto p1 = new FilePopup{ "Do you want to Format this device?", true, true, 182, 129 };
            //need to implement the functoonality
            mt::utils::showDialog(
                "FORMAT DRIVE", p1, [p1, this]( int R ) { delete p1; },
                ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        };

        themeChanged.connect( [&]() {
            ejectButton.setImages( "Eject", "Eject", "Eject_Active" );
            formatButton.setImages( "Format", "Format", "Format_Active" );
        } );
    }

    void FileLeftEntry::setHighlight( const bool state )
    {
        imageButton.setToggleState( state, juce::dontSendNotification );
        state ? file.setTextColour( ui::Theme::textColour( 2 ) ) : file.setTextColour( ui::Theme::textColour( 1 ) );
        repaint();
    }

    void FileLeftEntry::resized()
    {
        auto b = getLocalBounds();
        imageButton.setBoundsRelative( 0.06f, 0.3396f, width / b.getWidth(), height / b.getHeight() );
        file.setBoundsRelative( 0.126f, 0.188f, 0.671f, 0.622f );
        ejectButton.setBoundsRelative( 0.867f, 0.188f, 0.102f, 0.641f );
        formatButton.setBoundsRelative( 0.73f, 0.188f, 0.102f, 0.641f );
    }

    void FileLeftEntry::paint( juce::Graphics& g )
    {}
    void FileLeftEntry::setProperties( const DeviceData& fileData )
    {
        height = fileData.height;
        width  = fileData.width;
        file.setText( fileData.fileName, juce::dontSendNotification );
        imageButton.setImages( fileData.imgBtn, fileData.imgBtnActive );
        mt::utils::addAndMakeVisible( this, imageButton, file );
        if( fileData.isEjectBtn )
            mt::utils::addAndMakeVisible( this, ejectButton /*,formatButton*/ ); //Format Button  needs to be implemented
        resized();
    }
    ///////////////////////////////////////
    // Class FileLeftEntryPopup
    ///////////////////////////////////////
    FileLeftEntryPopup::FileLeftEntryPopup( juce::String Info, juce::String image, int folderW, int folderH, juce::String FileName, juce::String Date )
        : info( Info ), width( folderW ), height( folderH )
    {
        mt::utils::addAndMakeVisible( this, button1, button2, info, folder, fileName, date );
        folder.setImages( image );
        fileName.setText( FileName, juce::dontSendNotification );
        date.setText( Date, juce::dontSendNotification );

        info.setFontHeight( 18.0f );
        info.setTextColour( ui::Theme::textColour( 1 ) );
        info.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        info.setValueJustification( juce::Justification::centred );

        fileName.setTextColour( ui::Theme::titleColour( 3 ) );
        fileName.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        fileName.setFontHeight( 18.0f );

        date.setTextColour( ui::Theme::titleColour( 3 ) );
        date.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        date.setFontHeight( 18.0f );
        image == "" ? y = 123 : y = 48;
        themeChanged.connect( [&]() {
            button1.setImages( "No" );
            button2.setImages( "Yes" );
            folder.setImages( "Vector-1" );
        } );
    }

    void FileLeftEntryPopup::close()
    {
        if( auto dw = getCurrentlyModalComponent() )
            dw->exitModalState( 0 );
    }

    void FileLeftEntryPopup::resized()
    {
        auto b = getLocalBounds();

        info.setBounds( x, y, 403, 22 );
        folder.setBounds( 172, 125, width, height );
        fileName.setBounds( 239, 123, 180, 22 );
        date.setBounds( 239, 145, 180, 22 );
        button1.setBoundsRelative( 0.05f, 0.86f, 0.438f, 0.106f );
        button2.setBoundsRelative( 0.51f, 0.86f, 0.438f, 0.106f );
    }

    void FileLeftEntryPopup::paint( juce::Graphics& g )
    {}

    FileRightEntry::FileRightEntry()
    {
        file.setTextColour( ui::Theme::textColour( 1 ) );
        file.setFontHeight( 14.0f );
        file.setPadding( 10 );
        file.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        date.setValueJustification( juce::Justification::centred );
        date.setTextColour( ui::Theme::textColour( 1 ) );
        date.setFontHeight( 14.0f );
        date.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        time.setValueJustification( juce::Justification::centred );
        time.setTextColour( ui::Theme::textColour( 1 ) );
        time.setFontHeight( 14.0f );
        time.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        size.setValueJustification( juce::Justification::centred );
        size.setTextColour( ui::Theme::textColour( 1 ) );
        size.setFontHeight( 14.0f );
        size.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        mt::utils::addAndMakeVisible( this, file, date, time, size );
    }

    void FileRightEntry::resized()
    {
        auto  b = getLocalBounds();
        float x;
        isFolder ? x = 0.028f : x = 0.034f;
        imageBtn.setBoundsRelative( x, 0.33f, width / b.getWidth(), height / b.getHeight() );
        file.setBoundsRelative( 0.08f, 0.185f, 0.33f, 0.611f );
        date.setBoundsRelative( 0.383f, 0.203f, 0.19f, 0.574f );
        time.setBoundsRelative( 0.6f, 0.203f, 0.19f, 0.574f );
        size.setBoundsRelative( 0.826f, 0.203f, 0.19f, 0.574f );
    }
    void FileRightEntry::paint( juce::Graphics& g )
    {}

    void FileRightEntry::setProperties( const juce::String& imageButtonFile, bool bFolder, const float Width, const float Height, const juce::String& File, const juce::String& Date, const juce::String& Time, const juce::String& Size )
    {
        isFolder = bFolder;
        imageBtn.setImages( imageButtonFile );
        width  = Width;
        height = Height;
        file.setText( File, juce::dontSendNotification );
        date.setText( Date, juce::dontSendNotification );
        time.setText( Time, juce::dontSendNotification );
        size.setText( Size, juce::dontSendNotification );
        mt::utils::addAndMakeVisible( this, imageBtn, file, date, time, size );
        resized();
    }

    ///////////////////////////////////////
    // Class NewProjectPopupData
    ///////////////////////////////////////
    NewProjectPopupData::NewProjectPopupData( juce::String Info, juce::String TextEditor )
        : info( Info ), text( TextEditor )
    {
        info.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        text.setTextColour( ui::Theme::textColour( 1 ) );
        text.setValueJustification( juce::Justification::centred );
        info.setValueJustification( juce::Justification::centred );
        mt::utils::addAndMakeVisible( this, info, text );
        text.setEditable( true );
        text.setFontHeight( 20 );
        info.setFontHeight( 16 );
    }

    void NewProjectPopupData::paint( juce::Graphics& g )
    {}
    void NewProjectPopupData::resized()
    {
        info.setBoundsRelative( .3f, .45f, .4166f, .1388f );
        text.setBoundsRelative( .3383f, .5888f, .3333f, .1388f );
    }
    void NewProjectPopupData::close()
    {
        if( auto dw = getCurrentlyModalComponent() )
            dw->exitModalState( 0 );
    }
    juce::String NewProjectPopupData::getInputText()
    {
        return text.getText();
    }

    AuditionComponent::AuditionComponent()
    {
        mt::utils::addAndMakeVisible( this, slider, volume );

        slider.setRange( MIDI::MIN, MIDI::MAX, MIDI::STEP );
        slider.setValue( settings.preview.level.get() );
        slider.setValueProportion( .10 );

        slider.onValueChanged = [&]( double value ) {
            settings.preview.level = value;
            if( !value )
                volume.setToggleState( true, juce::sendNotification );
            else
                volume.setToggleState( false, juce::sendNotification );

            onGainChanged( getVolume() );
        };

        volume.onClick = [&]() {
            onMuteChanged( volume.getToggleState() );
        };
    }

    void AuditionComponent::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, getLocalBounds(), ui::Theme::backgroundColour( 1 ), 6.0f );
    }

    void AuditionComponent::resized()
    {
        slider.setBoundsRelative( .3523f, .0667f, .2857f, .7428f );
        volume.setBoundsRelative( .019f, .8164f, .9523f, .1914f );
    }

}
