#pragma once

namespace ui
{
    class NoteSelectorEditor : public juce::Component, public juce::DeletedAtShutdown
    {
    public:
        static NoteSelectorEditor& Instance()
        {
            KickLooknFeel::getLnf();
            static auto* noteSel = new NoteSelectorEditor;
            return *noteSel;
        }

        NoteSelectorEditor();
        ~NoteSelectorEditor() = default;

        void         show( const juce::String& Title, std::function<void( int )> CallbackOnExit );
        int          getNoteNumber() { return noteValue; }
        juce::String getNoteName() { return noteName; }
        void         setNote( int Note )
        {
            octaves[mt::ScaleAndNotes::getOctave( Note )].triggerClick();
            rootNotes[mt::ScaleAndNotes::getRootNameIndex( mt::ScaleAndNotes::getNoteName( Note ) )].triggerClick();
        }

    private:
        void resized() override;
        void paint( juce::Graphics& ) override;
        void refresh();

    private:
        std::array<KickToggleButton, 11> octaves;
        std::array<KickToggleButton, 12> rootNotes;
        juce::String                     noteName{ "" };
        KickSlider                       slider{ "", juce::Slider::LinearBar, juce::Slider::NoTextBox };
        KickTextEditor                   textEditor{ "" };
        KickImageButton                  inc{ "+", "numPad_P", "", "numPad_P_Active" };
        KickImageButton                  dec{ "-", "numPad_M", "", "numPad_M_Active" };
        KickToggleButton                 sharp{ "SHARP", "SHARP", "SHARP" };
        KickImageButton                  apply{ "Apply", "Virtual_Apply" };
        KickImageButton                  cancel{ "Cancel", "Virtual_Cancel" };
        KickToggleButton                 flat{ "FLAT", "FLAT", "FLAT" };
        KickToggleButton                 defaultOctave2{ "-2", "-2", "-2" };
        KickToggleButton                 defaultOctave1{ "-1", "-1", "-1" };
        KickLabel                        startAt{ "START AT" }, style{ "STYLE" }, octave{ "OCTAVE" }, note{ "NOTE" };
        std::vector<juce::String>        rootNoteFlat{ mt::Note::flatNames };
        std::vector<juce::String>        rootNoteSharp{ mt::Note::sharpNames };
        juce::Rectangle<int>             textBounds1{ 20, 162, 780, 1 }, textBounds2{ 20, 292, 780, 0 };
        int                              octaveIndex{ 0 }, rootNoteIndex{ 0 }, noteValue{ 0 };
        int                              tempOct{ 0 }, tempRoot{ 0 };
    };

    class KickNoteSelector : public KickLabel
    {
    public:
        std::function<void( int )> onNoteChanged;

        KickNoteSelector( const juce::String Name );
        ~KickNoteSelector() = default;

        void setNote( int Index )
        {
            noteValue = juce::jlimit( 0, (int)mt::ScaleAndNotes::midiNoteNames.size() - 1, Index );
            setText( mt::ScaleAndNotes::getNoteName( noteValue ), juce::dontSendNotification );
        }
        int          getNoteNumber() { return noteValue; }
        juce::String getNoteName() { return getText(); }

    private:
        void mouseDoubleClick( const juce::MouseEvent& ) override;

    private:
        int  noteValue{ 0 };
        bool isEditing{ false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickNoteSelector )
    };
}
