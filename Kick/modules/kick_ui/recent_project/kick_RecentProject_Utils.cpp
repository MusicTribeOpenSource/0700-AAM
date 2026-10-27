#include "kick_ui/recent_project/kick_RecentProject_Utils.h"
namespace ui
{
    /////////////////////////////////////////////////////////////////
    ////////   LoadProjectAndAnimation   ////////////////////////////
    /////////////////////////////////////////////////////////////////

    LoadProjectAndAnimation::LoadProjectAndAnimation()
        : juce::Thread( "LoadProject" )
    {
        ae.addListener( this );
    }

    LoadProjectAndAnimation::~LoadProjectAndAnimation()
    {
        ae.removeListener( this );
    }

    void LoadProjectAndAnimation::startLoading( juce::File project )
    {
        ui::RecentProject::isProjectLoading = true;
        proj                                = project;
        startThread();
        startAnimation();
    }

    void LoadProjectAndAnimation::onProjectLoaded( juce::File file )
    {
        if( isAnimationRunning() )
        {
            juce::Timer::callAfterDelay( 200, [this]() {
                //No need to free the LoadAnimation object, once LoadProjectAndAnimation static object destroyed then it will automatically destroy LoadAnimation object
                stopAnimation();
                kick_ui::signals::sigScreenOpen( kick_ui::Screen::HomeListView );
                ui::RecentProject::isProjectLoading = false;
            } );
        }
    }

    void LoadProjectAndAnimation::run()
    {
        if (!mt::getAudioInterface().loadProject(proj)) {
            juce::MessageManager::callAsync([this] {
                stopAnimation();
                ui::RecentProject::isProjectLoading = false;
            });
        }
    }
}
