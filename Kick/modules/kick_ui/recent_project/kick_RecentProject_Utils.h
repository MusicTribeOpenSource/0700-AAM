#pragma once
namespace ui
{
    class LoadProjectAndAnimation
        : private mt::MTAudioInterfaceListener
        , private juce::Thread
        , private mt::utils::LoadAnimation
    {
    public:
        ~LoadProjectAndAnimation();

        static LoadProjectAndAnimation& getInstance()
        {
            static auto* instance = new LoadProjectAndAnimation;
            return *instance;
        }

        void startLoading( juce::File project );

    private:
        LoadProjectAndAnimation();

        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        juce::File            proj;
        void                  run() override;
        void                  onProjectLoaded( juce::File file ) override;
    };
}
