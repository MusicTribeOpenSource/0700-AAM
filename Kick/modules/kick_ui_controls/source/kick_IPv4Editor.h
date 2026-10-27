#pragma once
namespace ui
{
    class IPv4Editor : public juce::Component, public juce::DeletedAtShutdown
    {
    public:
        IPv4Editor();
        ~IPv4Editor() = default;

        static IPv4Editor& Instance()
        {
            KickLooknFeel::getLnf();
            static auto* iPv4 = new IPv4Editor;
            return *iPv4;
        }

        juce::String       getIPAddressAsString() { return ipAddress; }
        void               setIPAddress( std::array<int, 4> );
        std::array<int, 4> getIPAddress() { return editorsValue; }

        void show( juce::String, std::function<void( int )> CallbackOnExit );

    private:
        void addInput( int Number );
        void resized() override;
        void refreshDisplay();

    private:
        std::array<ui::KickTextEditor, 4> editors;
        std::array<int, 4>                intermidentValues;
        std::array<int, 4>                editorsValue;
        std::array<ui::KickLabel, 3>      editorSep;
        int                               selectedEditor{ 0 };

        ui::KickImageButton num1{ "1", "numPad_1", "numPad_1", "numPad_1_Active" };
        ui::KickImageButton num2{ "2", "numPad_2", "numPad_2", "numPad_2_Active" };
        ui::KickImageButton num3{ "3", "numPad_3", "numPad_3", "numPad_3_Active" };
        ui::KickImageButton num4{ "4", "numPad_4", "numPad_4", "numPad_4_Active" };
        ui::KickImageButton num5{ "5", "numPad_5", "numPad_5", "numPad_5_Active" };
        ui::KickImageButton num6{ "6", "numPad_6", "numPad_6", "numPad_6_Active" };
        ui::KickImageButton num7{ "7", "numPad_7", "numPad_7", "numPad_7_Active" };
        ui::KickImageButton num8{ "8", "numPad_8", "numPad_8", "numPad_8_Active" };
        ui::KickImageButton num9{ "9", "numPad_9", "numPad_9", "numPad_9_Active" };
        ui::KickImageButton num0{ "0", "numPad_0", "numPad_0", "numPad_0_Active" };
        ui::KickImageButton clr{ "Clear", "numPad_C", "numPad_C", "numPad_C_Active" };
        ui::KickImageButton apply{ "Apply", "numPad_Apply" };
        ui::KickImageButton cancel{ "Cancel", "numPad_Clear" };

        juce::String ipAddress{ "0.0.0.0" };

        std::chrono::time_point<std::chrono::steady_clock> lastInputTime;
        std::chrono::milliseconds                          timeoutDuration{ 1000 }; // 2 seconds

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( IPv4Editor )
    };

    class KickIPv4 : public KickLabel
    {
    public:
        std::function<void( std::array<int, 4>, juce::String )> onIPv4Changed;
        KickIPv4( const juce::String&, const juce::String& );
        ~KickIPv4() = default;

        juce::String       getIPAddressAsString() { return getText(); }
        void               setIPAddress( int ( &arr )[4] );
        std::array<int, 4> getIPAddress() { return editorValues; }

    private:
        std::array<int, 4> editorValues;
        bool               isEditing{ false };

    private:
        void mouseDoubleClick( const juce::MouseEvent& ) override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickIPv4 )
    };
}
