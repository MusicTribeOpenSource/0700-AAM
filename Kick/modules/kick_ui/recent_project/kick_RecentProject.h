#pragma once

namespace ui
{
    class LongPressComp : public juce::Component
    {
    public:
        std::function<void( int )> onLoadTriggered;
        std::function<void( int )> onRemoveTriggered;
        LongPressComp();
        ~LongPressComp() {}

        void showComp( int );
        void setCompBounds( juce::Rectangle<int> Bounds );

    private:
        int                  width        = 200;
        int                  height       = 164;
        int                  currentIndex = -1;
        juce::Rectangle<int> bounds;
        ui::KickButton       loadBtn{ "LoadButton", "Load" };
        ui::KickButton       removeBtn{ "RemoveButton", "Remove" };

        void paint( juce::Graphics& g ) override;
        void resized() override;
        void mouseDown( const juce::MouseEvent& e ) override;
    };

    class NewProjectBtn : public juce::Component
    {
    public:
        std::function<void()> onClick = [&]() {};

        NewProjectBtn();
        ~NewProjectBtn() = default;

    private:
        ui::KickImageButton icon{ "", "Icon_NewRecent" };

        void resized() override;
        void paint( juce::Graphics& g ) override;
        void mouseDown( const juce::MouseEvent& e ) override { onClick(); };
    };

    class RecentItems : public juce::Component
        , public juce::Timer
    {
    public:
        std::function<void( int, juce::File )> onMouseDown;
        std::function<void( int )>             onLongPressed;
        std::function<void()>                  clearAllShowBorderCallback;
        std::function<void( int )>             updateSelectedIndex;

        RecentItems();
        ~RecentItems() = default;

        void setProperties( int Index, const juce ::String& Path );
        void showBorder( bool );

        inline bool isShowBorder() const noexcept { return bShowBorder; }
        inline int  getID() const noexcept { return index; }

        void mouseDown( const juce::MouseEvent& e ) override;
        void mouseUp( const juce::MouseEvent& e ) override;
        void mouseDrag( const juce::MouseEvent& e ) override;
        void mouseDoubleClick( const juce::MouseEvent& e ) override;
        void timerCallback() override;

    private:
        bool bShowBorder{ false };
        bool isLongPressTriggered{ false };
        bool isDragging{ false };
        bool isDoubleClicked{ false };
        int  longPressThreshold = 500; // Set the long press threshold (in milliseconds)
        int  dragThreshold      = 5;
        int  index{ -1 };

        ui::KickLabel label;
        ui::KickLabel date;
        ui::KickLabel timeStamp;
        ui::KickImage image;
        ui::KickImage icon{ "", "Icon_DefaultFile1" };
        juce::File    path;

        void resized() override;
        void paint( juce::Graphics& g ) override;
    };

    class RecentList
        : public KickViewport
    {
    public:
        explicit RecentList();
        ~RecentList() = default;

        std::vector<RecentItems*> listOptions;
        int                       selectedIndex{ -1 };
        juce::Component           listComp;

        void clear() noexcept;
        void addComp( RecentItems* ) noexcept;

        inline int getSize() const noexcept { return listComp.getNumChildComponents(); };

    private:
        int numOptions{ 0 };
        int reversedRow{ 10 };

        juce::File getRecentProject( int Row );
        void       resized() override;
        void       visibilityChanged() override;
    };

    class RecentProject : public kick_ui::IScreen
        , public KickControlBase<juce::Component>
        , private mt::MTAudioInterfaceListener
    {
    public:
        std::function<void( juce::String, juce::File )> onKeyBoardEnd;
        std::function<void( void )>                     onAutoLoadClick;

        RecentProject();
        ~RecentProject() override { ae.removeListener(this); }
        bool       canClose() override { return true; }
        bool       requiresControlBar() override { return false; }
        bool       requiresStatusBar() override { return false; }
        void       handleCommand( CPktMgr::HWMsg_t& ) override;
        void       knobClicked() override;
        void       knobRotated( double ) override;
        void       sliderMoved( float ) override;
        void       resized();
        void       removeFileFromVTree( int );
        void       createProject( juce::String value );
        int        getReversedRow( int );
        int        getNumOfChild();
        juce::File getRecentProject( int );
        void       setNumOptions( int NumOptions );

        static bool enableRecentProjVTreeEntry;
        static bool isProjectLoading;

    private:
        bool bFirstLaunch{ true };
        int  reversedRow, childNum, Rows = 10;
        int  recentProjectChildNum;
        int  maxNumOfRecentProject = 5;
        int  selectedIndex{ -1 };
        int  child;

        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        mt::KickSettings&     settings{ mt::getSettings() };

        kick_ui::KickModeHeader header{ "Projects", "RecentProject" };
        ui::KickNameLabel       projectName{ "", "Project Name" };
        ui::KickImageButton     fileBrowser{ "File Browser", "Browser_Recent" };

        juce::String            seprator = juce::File::getSeparatorString();
        juce::Component         listComp;
        juce::Array<juce::File> demoDirList;
        juce::Array<juce::File> templateDirList;
        juce::File              projectLoadFile;

        RecentList                     demoList;
        RecentList                     templateList;
        RecentList                     projectList;
        NewProjectBtn                  newProj;
        std::unique_ptr<LongPressComp> longPressComp;

        std::vector<std::unique_ptr<RecentItems>> recentItems;
        void                                      openVirtualKeyBoard( juce::File );
        void                                      copyProjIntoProjectDir( juce::String, juce::File );
        void                                      addDemos();
        void                                      addTemplates();
        void                                      addProjectList( int index );
        void                                      clearAllShowBorder();
        void                                      loadProject( juce::File& Project );

        void visibilityChanged() override;
        void onProjectLoaded( juce::File ) override;
    };
}
