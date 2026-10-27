
#pragma once
#ifndef KICK_SETTIGNS_H_INCLUDE
    #define KICK_SETTIGNS_H_INCLUDE

namespace mt
{
    #define ADDPROJECT      true
    #define DONT_ADDPROJECT false
    const char* const __projectFileSuffix = ".kickPrj";

    enum fileFilter
    {
        OldFile,
        NewFile
    };

    const juce::String __settingsFileName( "preferences" );
    const juce::String __settingsFileSuffix( ".settingsprj" );
    const juce::String __settingsFileVersion( "1.0" );

    /// ///////////////////////////////////
    ///   Browser Favourite.
    /// ///////////////////////////////////
    struct KickBrowserFavourite : public MTIVTBase
    {
        void setup( juce::ValueTree VT, juce::UndoManager* Undo );
        bool isValid();

        // Getters...
        juce::String getPathAssignedToButton( juce::String );
        juce::var    getSelectedLeftRow( juce::String buttonName );
        int          getNumProperties();

        // Setters...
        void setPathToButton( juce::String buttonName, juce::String path );
        void setSelectedLeftRow( juce::String buttonName, int row );
    };

    /// ///////////////////////////////////
    ///    Recent Projects.
    /// ///////////////////////////////////
    struct KickProjectsDefault : public MTIVTBase
    {
        void setup( juce::ValueTree VT, juce::UndoManager* Undo );
        bool isValid();

        Atomic<bool> isAutoLoad;
        Atomic<bool> isAutoSave;
        Atomic<int>  tempo;
        Atomic<int>  bars;
        Atomic<int>  rootNote;
        Atomic<int>  scale;
        Atomic<int>  numSeq;
        Atomic<int>  numTrackPerSeq;
        Atomic<int>  maxRecentProjects;
        Atomic<int>  defProjectState;

        AtomicString onLaunch;
        AtomicString bitDepth;
        AtomicString saveAfter;
        AtomicString numerator;
        AtomicString denominator;

        AtomicString namingStyle;
        AtomicString namePrefix;
        AtomicString currentProject;

        int getNumChildren();

        juce::String getProject( int Index = -1 );
        // Setters...

        void setProject( int Index, const juce::String& Path );
        void addProject( const juce::String& Path );

        void       removeProject( int Index );
        void       removeProject( juce::File& ProjectFile );
        void       renameProject( juce::File& ProjectFile, juce::File& newFile );
        void       createProject( const juce::File& file );
        void       refreshMaxNumOfRecentProj();
        bool       checkAlreadyLoadOrNot( juce::File& ProjectFile );
        juce::File getProjectPath( juce::File& ProjectFile );
        void       rearrangeProject( juce::File& ProjectFile );
    };

    /// ///////////////////////////////////
    ///    Hardware .
    /// ///////////////////////////////////
    struct KickHardware : public MTIVTBase
    {
        void setup( juce::ValueTree VT, juce::UndoManager* Undo );
        bool isValid();

        Atomic<bool> tempoLEDStatus;
        Atomic<int>  velocity;
        Atomic<int>  aftertouch;
        Atomic<int>  brightness;
        AtomicString threshold;
        AtomicString sensitivity;
        AtomicString curve;
        AtomicString numTaps;
        AtomicString timezone;
    };

    /// ///////////////////////////////////
    ///    MIDI Control .
    /// ///////////////////////////////////
    struct KickMidiControl : public MTIVTBase
    {
        void setup( juce::ValueTree VT, juce::UndoManager* Undo );
        bool isValid();

        Atomic<bool> ipTrackState;
        Atomic<bool> opTrackState;
        Atomic<bool> ipControlState;
        Atomic<bool> opControlState;
        AtomicString ipPortName;
        AtomicString opPortName;
    };

    /// ///////////////////////////////////
    ///    Ethernet .
    /// ///////////////////////////////////
    struct KickEthernet : public MTIVTBase
    {
        void setup( juce::ValueTree VT, juce::UndoManager* Undo );
        bool isValid();

        Atomic<bool> enabled;
        AtomicString mode;
        AtomicString ipAddress;
        AtomicString subnetMask;
        AtomicString gateway;
    };

    /// ///////////////////////////////////
    ///    CV .
    /// ///////////////////////////////////
    struct KickCV : public MTIVTBase
    {
        void setup( juce::ValueTree VT, juce::UndoManager* Undo );
        bool isValid();
        // Getters...
        int getNoteStart( int CVPort );
        // Setters...
        void setNoteStart( int CVPort, int Note );
    };

    struct KickCVClib : public MTIVTBase
    {
        void setup( juce::ValueTree VT, juce::UndoManager* Undo );
        bool isValid();
        // Getters...
        int getOffset( int index );
        // Setters...
        void setOffset( int index, int offset );
    };

    /// ///////////////////////////////////
    ///    KickMetronome
    /// ///////////////////////////////////
    struct KickMetronome : public MTIVTBase
    {
        KickMetronome();
        void setup( juce::ValueTree VT, juce::UndoManager* Undo );
        bool isValid();

        Atomic<bool>             enabled;
        Atomic<int>              countIn;
        Atomic<float>            gain;
        Atomic<float>            pan;
        Atomic<MTMetronomeRate>  rate;
        Atomic<MTMetronomeStyle> style;
        Atomic<MTMetronomeMode>  mode;
        AtomicString             source;
    };

    /// ///////////////////////////////////
    ///    KickWifi
    /// ///////////////////////////////////
    struct KickWifi : public MTIVTBase
    {
        void setup( juce::ValueTree VT, juce::UndoManager* Undo );
        bool isValid();

        Atomic<bool> enabled;
        AtomicString ssid;
        AtomicString pwd;
    };

    /// ///////////////////////////////////
    ///    KickPreview
    /// ///////////////////////////////////
    struct KickPreview
    {
        KickPreview();
        void reset(); // Restores default values.

        CachedValue<MidiConstrainer> level;
        CachedValue<bool>            mute;
        CachedValue<bool>            isOn;
        CachedValue<bool>            isAuto;

        // Following parameters should not be written directly. Use them as Read-Only.
        float           gain{ 1 };
        juce::ValueTree state{ "Preview" };
    };

    /// ///////////////////////////////////
    ///    KNOB Control .
    /// ///////////////////////////////////
    struct KickKnobControl
    {
        inline int  getMode() const { return knobMode; };
        inline void setMode( int mode ) { knobMode = mode; };

        int knobMode{ 1 };
    };

    /// ///////////////////////////////////
    ///    KickSettings
    /// ///////////////////////////////////

    class KickSettings : public KickXML, public juce::DeletedAtShutdown
    {
    public:
        KickSettings();
        void makeDefaults( void );

    public:
        KickProjectsDefault  projects;
        KickMidiControl      midiControl;
        KickEthernet         eth;
        KickCV               cv;
        KickHardware         hardware;
        KickBrowserFavourite browser;
        KickCVClib           cvclib;
        KickMetronome        metronome;
        KickWifi             wifi;

        // Runtime settings, not saved in XML
        KickPreview     preview;
        KickKnobControl knobControl;

    private:
        juce::UndoManager undo;
    };

    // Interface class to manage settings...
    KickSettings& getSettings();
}

#endif
