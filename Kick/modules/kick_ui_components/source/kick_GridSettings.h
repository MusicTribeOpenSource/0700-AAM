#pragma once
namespace ui
{
    ///////////////////////////////////////////////////////////////////////
    // Grid Settings
    ///////////////////////////////////////////////////////////////////////
    enum AutoScrollMode
    {
        Off,
        Follow,
        Page
    };

    class GridSettings : public juce::Component, public juce::DeletedAtShutdown
    {
    public:
        static GridSettings& Instance()
        {
            KickLooknFeel::getLnf();
            static auto* gridSettings = new GridSettings;
            return *gridSettings;
        }

        enum ScreenType
        {
            GridMidi,
            GridAudio,
            ListEdit
        };

        GridSettings();
        ~GridSettings() = default;

        bool           getHittingPadMode();
        bool           getAutoAdvance();
        int            getSnapMode();
        AutoScrollMode getAutoScrollType();
        void           setScreen( ScreenType Screen );

    private:
        void resized() override;

        ui::KickToggleOptions snapMode{ "SNAP MODE", { "Absolute", "Relative" } };
        ui::KickToggleOptions hittingPadsMode{ "HITTING PAD SELECTS ALL EVENTS", { "Off", "On" } };
        ui::KickToggleOptions autoScroll{ "AUTO SCROLL", { "Off", "Follow", "Page" } };
        ui::KickToggleOptions autoAdvance{ "AUTO ADVANCE ON STEP RECORD", { "Off", "On" } };

        ScreenType screenType{ GridMidi };
    };
}
