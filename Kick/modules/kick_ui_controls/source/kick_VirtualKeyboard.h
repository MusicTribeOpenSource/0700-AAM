#pragma once
namespace ui
{

    struct IKeyBoardBase : public juce::Component
    {
        std::function<void( const juce::String& Key )> onKeyPressed_;
        std::function<void( int )>                     onEnterPressed;

        std::vector<std::unique_ptr<juce::Button>> row[4];

        KickImageButton applyButton{ "APPLY", "Virtual_Apply" };
        KickImageButton cancelButton{ "CANCEL", "Virtual_Cancel" };

        IKeyBoardBase();
        virtual ~IKeyBoardBase();

        void addAllMakeVisibleAndAddListener();
        void resized() override;
    };

    class KickVirtualKeyBoard : public juce::Component, public juce::DeletedAtShutdown
    {
    public:
        std::function<void( int )> onEnterPressed = []( int ) {};

        enum class Type
        {
            KEYS_NUMERIC,
            KEYS_LOWERCASE_ALPHABETIC,
            KEYS_SPECIAL,
            KEYS_UPPERCASE_ALPHABETIC
        };

        explicit KickVirtualKeyBoard( const juce::String& initialKeyvalue );

        inline void         setInputText( const juce::String& Text ) { inputTextLabel.setText( Text ); }
        inline juce::String getInputText() { return inputTextLabel.getText(); }

        void showKeyboard( Type type );

        void show( const juce::String& Title, Type KeyboardType, std::function<void( int )> Callback, juce::Rectangle<int> Bounds );
        void close();

        // GET Static instance of this class...
        static KickVirtualKeyBoard& getWithText( const juce::String& InitalValue );

    private:
        struct SpecialSymbols : public IKeyBoardBase
        {
            SpecialSymbols();
        };

        struct LowerCaseLetterComponent : public IKeyBoardBase
        {
            LowerCaseLetterComponent();
        };

        struct NumberComponent : public IKeyBoardBase
        {
            NumberComponent();
        };

        struct UpperCaseLetterComponent : public IKeyBoardBase
        {
            UpperCaseLetterComponent();
        };

        Type                     currentType{ Type::KEYS_LOWERCASE_ALPHABETIC };
        LowerCaseLetterComponent letterComp;
        NumberComponent          numComp;
        SpecialSymbols           speSym;
        UpperCaseLetterComponent upperCaseComp;
        KickTextEditor           inputTextLabel;

        void resized() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickVirtualKeyBoard )
    };
}
