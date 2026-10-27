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
        stopThread(-1);
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
            } );
        }
    }

    void LoadProjectAndAnimation::run()
    {
        const auto loaded = mt::getAudioInterface().loadProject(proj);
        // Publish completion after all message-thread persistence barriers.
        // Quit can then join this worker without blocking a barrier it needs.
        juce::MessageManager::callAsync([this, loaded] {
            if (!loaded) stopAnimation();
            ui::RecentProject::isProjectLoading = false;
        });
    }
}
