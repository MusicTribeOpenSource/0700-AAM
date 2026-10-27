namespace ui
{
    enum KickColorID
    {
        // Default colour
        ID_defaultColour = 0,
        // Title colours...
        ID_titleColour = 100,
        // Text Colours
        ID_textColour = 200,
        // Background Colours
        ID_backgroundColour = 300,
        // Foreground Colours
        ID_foregroundColour = 400,
        // Selection Colours
        ID_selectionColour = 500,
        // Outline Colours
        ID_outlineColour = 600,
        // Highlight Colours
        ID_highlightColour = 700,
        // Waveform Colours
        ID_waveformColour = 800,
        // Meter Colour
        ID_meterColour = 900,
        // Note Colour
        ID_noteColour = 1000,
        // Grid related Colours
        ID_gridColour = 1100,
        // Pad Button LED colours...
        ID_padButton = 1200,
    };

    ///
    /// Static DATA.
    ///
    static juce::ValueTree themeVT;
    static juce::String    themeDir{ "Default" };

    static std::map<KickColorID, juce::uint32>  colorPalette;
    static std::map<juce::String, juce::String> themeImgMap;
    static std::map<int, int>                   fontSizeMap;

    class ThemeLifetime final : public juce::DeletedAtShutdown
    {
        ~ThemeLifetime() override { themeVT = {}; }
    };

    ///
    /// Internal helper API's
    ///
    static void setupPallete()
    {
        if( 0 == colorPalette.size() )
        {
            Theme::loadTheme( "Default", false );
        }
    }

    static juce::Colour getColour( int Id, int DefaultReturnID = ID_defaultColour )
    {
        setupPallete();
        // Locate colour...
        auto it = colorPalette.find( (KickColorID)Id );
        if( it != colorPalette.end() )
            return juce::Colour( it->second );
        // Return default colour id...
        it = colorPalette.find( (KickColorID)DefaultReturnID );
        if( it != colorPalette.end() )
            return juce::Colour( it->second );
        // Worst case...
        return juce::Colours::transparentBlack;
    }

    static int getFontSize( FONT ID, FONT DefaultFont = FONT::M )
    {
        setupPallete();
        // Locate font...
        auto it = fontSizeMap.find( (int)ID );
        if( it != fontSizeMap.end() )
            return it->second;
        // Return default colour id...
        it = fontSizeMap.find( (int)DefaultFont );
        if( it != fontSizeMap.end() )
            return it->second;
        // Worst case...
        return 10;
    }

    static void themeColourParser( const juce::String& Type, KickColorID IDBase )
    {
        auto         vt = themeVT.getChildWithName( Type );
        juce::String id;
        for( auto i = 0; i < 20; ++i )
        {
            auto key          = KickColorID( IDBase + i );
            id                = "_" + juce::String( i );
            auto val          = vt.getProperty( id, 0x00 ).toString();
            auto color        = juce::Colour::fromString( val );
            colorPalette[key] = color.getARGB();
        }
    };

    static void themeImagesParser()
    {
        auto vt = themeVT.getChildWithName( "Images" );
        for( auto it : vt )
        {
            auto key         = it.getProperty( "name" );
            auto value       = it.getProperty( "path" );
            themeImgMap[key] = value;
        }
    }

    static void themeFontParser()
    {
        juce::MemoryBlock fontM;
        auto              vt       = themeVT.getChildWithName( "FONT" );
        juce::String      fontFile = vt.getProperty( "_0" ).toString();
        juce::File        fontTTf  = Theme::getAsset( fontFile );
        if( fontTTf.loadFileAsData( fontM ) )
        {
            auto fontTF = juce::Typeface::createSystemTypefaceFor( fontM.getData(), fontM.getSize() );
            juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypeface( fontTF );
        }

        auto fvt              = themeVT.getChildWithName( "FONT_SIZES" );
        fontSizeMap[FONT::XS] = (int)fvt.getProperty( "XS", 10 );
        fontSizeMap[FONT::S]  = (int)fvt.getProperty( "S", 10 );
        fontSizeMap[FONT::M]  = (int)fvt.getProperty( "M", 10 );
        fontSizeMap[FONT::L]  = (int)fvt.getProperty( "L", 10 );
        fontSizeMap[FONT::XL] = (int)fvt.getProperty( "XL", 10 );
    }

    ///
    /// Theme API's
    ///
    void Theme::loadTheme( const juce::String& ThemeDir, bool TriggerNotification )
    {
        static auto* lifetime = new ThemeLifetime;
        juce::ignoreUnused(lifetime);
        auto dir = getAppDirs( MTAssets ).getChildFile( ThemeDir );
        if( dir.exists() && dir.isDirectory() )
        {
            themeDir = ThemeDir;
            auto n   = dir.getChildFile( "theme.xml" );
            if( n.existsAsFile() )
            {
                auto tvt = juce::ValueTree::fromXml( n.loadFileAsString() );
                if( tvt.isValid() )
                {
                    themeVT = tvt;
                    // Setup colour palette...
                    colorPalette.clear();
                    themeColourParser( "DEFAULT", ID_defaultColour );
                    themeColourParser( "TITLE", ID_titleColour );
                    themeColourParser( "TEXT", ID_textColour );
                    themeColourParser( "BACKGROUND", ID_backgroundColour );
                    themeColourParser( "FOREGROUND", ID_foregroundColour );
                    themeColourParser( "SELECTION", ID_selectionColour );
                    themeColourParser( "OUTLINE", ID_outlineColour );
                    themeColourParser( "HIGHLIGHT", ID_highlightColour );
                    themeColourParser( "PADBUTTON", ID_padButton );
                    themeColourParser( "WAVEFORM", ID_waveformColour );
                    themeColourParser( "METER", ID_meterColour );
                    themeColourParser( "NOTE", ID_noteColour );
                    themeColourParser( "GRID", ID_gridColour );
                }

                // Setup font to use in all screens.
                themeFontParser();

                // Create map of images...
                themeImagesParser();
            }
        }
        // Trigger theme change...
        if( TriggerNotification )
        {
            themeChanged();
        }
    }

    juce::String Theme::getCurrentTheme() noexcept { return themeDir; };

    ///
    /// Asset API's
    ///
    juce::File Theme::getAssetsDirectory() { return getAppDirs( MTAssets ).getChildFile( themeDir + juce::File::getSeparatorString() ); }

    juce::File Theme::getThemeImage( const juce::String& IMGName )
    {
        auto f = getAsset( themeImgMap[IMGName] );
        return f.existsAsFile() ? f : getAsset( IMGName );
    }

    std::unique_ptr<juce::Drawable> Theme::getDrawable( const juce::String& IMGName ) { return getDrawable( getThemeImage( IMGName ) ); }
    std::unique_ptr<juce::Drawable> Theme::getDrawable( juce::File IMGFile ) { return juce::DrawableImage::createFromImageFile( IMGFile ); }

    juce::File  Theme::getAsset( const juce::String& File ) { return Theme::getAssetsDirectory().getChildFile( File ); }
    juce::Image Theme::getImage( const juce::String& Image ) { return Image.isNotEmpty() ? juce::ImageCache::getFromFile( Theme::getThemeImage( Image ) ) : juce::Image{}; }

    ///
    /// Colour API's
    ///
    juce::Colour Theme::padButtonColour( int ID ) { return getColour( KickColorID( ID_padButton + ID ) ); }
    juce::Colour Theme::titleColour( int ID ) { return getColour( KickColorID( ID_titleColour + ID ), KickColorID( ID_titleColour + 1 ) ); }
    juce::Colour Theme::textColour( int ID ) { return getColour( KickColorID( ID_textColour + ID ), KickColorID( ID_textColour + 1 ) ); }
    juce::Colour Theme::backgroundColour( int ID ) { return getColour( KickColorID( ID_backgroundColour + ID ), KickColorID( ID_backgroundColour ) ); }
    juce::Colour Theme::foregroundColour( int ID ) { return getColour( KickColorID( ID_foregroundColour + ID ), KickColorID( ID_foregroundColour + 1 ) ); }
    juce::Colour Theme::selectionColour( int ID ) { return getColour( KickColorID( ID_selectionColour + ID ), KickColorID( ID_selectionColour + 1 ) ); }
    juce::Colour Theme::outlineColour( int ID ) { return getColour( KickColorID( ID_outlineColour + ID ), KickColorID( ID_outlineColour + 1 ) ); }
    juce::Colour Theme::highlightColour( int ID ) { return getColour( KickColorID( ID_highlightColour + ID ), KickColorID( ID_highlightColour + 1 ) ); }
    juce::Colour Theme::waveformColour( int ID ) { return getColour( KickColorID( ID_waveformColour + ID ), KickColorID( ID_waveformColour + 1 ) ); }
    juce::Colour Theme::meterColour( int ID ) { return getColour( KickColorID( ID_meterColour + ID ), KickColorID( ID_meterColour + 1 ) ); }
    juce::Colour Theme::gridColour( int ID ) { return getColour( KickColorID( ID_gridColour + ID ), KickColorID( ID_gridColour + 1 ) ); }
    juce::Colour Theme::noteColour( int velocity )
    {
        if( velocity < 0 )
            return getColour( ID_noteColour );
        else if( velocity < 44 )
        {
            float alpha = (float)velocity / 44.f;
            return getColour( ID_noteColour + 1 ).darker( alpha );
        }
        else if( velocity < 90 )
        {
            float alpha = (float)velocity / 90.f;
            return getColour( ID_noteColour + 2 ).darker( alpha );
        }
        else
        {
            float alpha = (float)velocity / 127.f;
            return getColour( ID_noteColour + 3 ).darker( alpha );
        }
    }

    juce::Colour Theme::multiSelectTabActiveColour() { return getColour( ID_highlightColour + 1 ); }
    juce::Colour Theme::multiSelectTabInActiveColour() { return getColour( ID_highlightColour + 6 ); }

    juce::Colour Theme::XYFXCellColour() { return getColour( ID_gridColour + 1 ); }
    juce::Colour Theme::XYFXCellSelectColour( int alpha ) { return getColour( ID_gridColour + 3 ).withAlpha( ( juce::uint8 )( alpha ) ); }
    juce::Colour Theme::XYFXAxisColour() { return getColour( ID_gridColour + 3 ); }

    juce::Colour Theme::virtualKeyboardTextColour() { return getColour( ID_textColour ); }
    juce::Colour Theme::virtualKeyboardBkColour() { return getColour( ID_textColour + 2 ); }

    int Theme::font( FONT Font ) { return getFontSize( Font ); }
}
