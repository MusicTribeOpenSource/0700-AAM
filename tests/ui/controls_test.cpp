// UI tests for Kick's reusable controls. Components are created, laid out, driven
// with synthesized mouse and knob input, and rendered to off-screen images.

#include <gtest/gtest.h>

#include <kick_ui_controls/kick_ui_controls.h>

#include "ui_test_support.h"

using kicktest::Mouse;

// ----------------------------------------------------------------- buttons

TEST( KickButtons, ClickFiresOnClick )
{
    ui::KickButton b( "go", "Go" );
    b.setSize( 80, 30 );
    int clicks = 0;
    b.onClick = [&] { ++clicks; };

    b.triggerClick();
    b.triggerClick();
    // triggerClick posts a command message, so wait for both to land.
    EXPECT_TRUE( kicktest::pumpUntil( [&] { return clicks == 2; } ) );
    EXPECT_EQ( clicks, 2 );
}

TEST( KickButtons, ToggleButtonKeepsStateBetweenClicks )
{
    ui::KickToggleButton b( "t", "ON", "OFF" );
    b.setSize( 80, 30 );
    b.setClickingTogglesState( true );
    EXPECT_FALSE( b.getToggleState() );

    b.triggerClick();
    EXPECT_TRUE( kicktest::pumpUntil( [&] { return b.getToggleState(); } ) );
    b.triggerClick();
    EXPECT_TRUE( kicktest::pumpUntil( [&] { return !b.getToggleState(); } ) );
}

TEST( KickButtons, ToggleButtonSwapsLabelWithState )
{
    ui::KickToggleButton b( "t", "ON", "OFF" );
    b.setSize( 80, 30 );
    b.setClickingTogglesState( true );
    EXPECT_EQ( b.getButtonText(), "OFF" );
    b.triggerClick();
    EXPECT_TRUE( kicktest::pumpUntil( [&] { return b.getButtonText() == "ON"; } ) );
    b.triggerClick();
    EXPECT_TRUE( kicktest::pumpUntil( [&] { return b.getButtonText() == "OFF"; } ) );
}

TEST( KickButtons, NormalizedValueMapsToToggleState )
{
    ui::KickCheckBoxButton c( "c", "Check" );
    c.setSize( 100, 24 );
    c.setNormalized( 1.0f );
    EXPECT_TRUE( c.getToggleState() );
    EXPECT_FLOAT_EQ( c.getNormalized(), 1.0f );
    c.setNormalized( 0.0f );
    EXPECT_FALSE( c.getToggleState() );
    EXPECT_FLOAT_EQ( c.getNormalized(), 0.0f );
}

TEST( KickButtons, SyntheticMouseDownUpLeavesButtonReleased )
{
    // The click itself comes from the Button base class' mouse handling, which needs a
    // live peer, so this only covers KickButton's own down/up bookkeeping.
    ui::KickButton b( "go", "Go" );
    b.setBounds( 0, 0, 80, 30 );

    Mouse m( b );
    m.down( { 10.f, 10.f } );
    m.up( { 10.f, 10.f } );
    EXPECT_FALSE( b.isDown() );
}

TEST( KickButtons, ButtonRendersItsBackgroundColour )
{
    ui::KickButton b( "go", "" );
    b.setBounds( 0, 0, 60, 40 );
    const auto red = juce::Colour( 0xffcc2222 );
    b.setColours( juce::Colours::white, red, red, 0 );

    auto img = kicktest::snapshot( b );
    ASSERT_EQ( img.getWidth(), 60 );
    ASSERT_EQ( img.getHeight(), 40 );
    EXPECT_GT( kicktest::countPixels( img, red, 24 ), 60 * 40 / 2 ) << "most of the face should be the background colour";
}

// ----------------------------------------------------------------- sliders

TEST( KickSlider, NormalizedValueRoundTrips )
{
    ui::KickSlider s( "vol", juce::Slider::LinearHorizontal );
    s.setBounds( 0, 0, 200, 30 );
    s.setRange( 0.0, 10.0, 0.0 );

    for( double n : { 0.0, 0.25, 0.5, 1.0 } )
    {
        s.setNormalized( n );
        EXPECT_NEAR( s.getNormalized(), n, 1e-9 );
        EXPECT_NEAR( s.getValue(), n * 10.0, 1e-9 );
    }
}

TEST( KickSlider, NormalizedInputIsClamped )
{
    ui::KickSlider s( "vol", juce::Slider::LinearHorizontal );
    s.setRange( 0.0, 1.0, 0.0 );
    s.setNormalized( 5.0 );
    EXPECT_DOUBLE_EQ( s.getValue(), 1.0 );
    s.setNormalized( -5.0 );
    EXPECT_DOUBLE_EQ( s.getValue(), 0.0 );
}

TEST( KickSlider, ValueChangeCallbackReportsNewValue )
{
    ui::KickSlider s( "vol", juce::Slider::LinearHorizontal );
    s.setRange( 0.0, 100.0, 0.0 );
    std::vector<double> seen;
    s.onValueChanged = [&]( double v ) { seen.push_back( v ); };

    s.setValue( 30.0, juce::sendNotificationSync );
    s.setValue( 30.0, juce::sendNotificationSync ); // unchanged, must not re-fire
    s.setValue( 80.0, juce::sendNotificationSync );

    ASSERT_EQ( seen.size(), 2u );
    EXPECT_DOUBLE_EQ( seen[0], 30.0 );
    EXPECT_DOUBLE_EQ( seen[1], 80.0 );
}

TEST( KickSlider, SilentSetDoesNotNotify )
{
    ui::KickSlider s( "vol", juce::Slider::LinearHorizontal );
    int            fired = 0;
    s.onValueChanged     = [&]( double ) { ++fired; };
    s.setNormalized( 0.7, juce::dontSendNotification );
    EXPECT_EQ( fired, 0 );
}

TEST( KickSlider, DefaultRangeIsUnitWithHundredthSteps )
{
    ui::KickSlider s( "x", juce::Slider::LinearHorizontal );
    EXPECT_DOUBLE_EQ( s.getMinimum(), 0.0 );
    EXPECT_DOUBLE_EQ( s.getMaximum(), 1.0 );
    EXPECT_DOUBLE_EQ( s.getInterval(), 0.01 );
}

TEST( KickSlider, PaintedOutputTracksTheValue )
{
    ui::KickSlider s( "vol", juce::Slider::LinearBar );
    s.setBounds( 0, 0, 200, 30 );
    s.setRange( 0.0, 1.0, 0.0 );

    s.setNormalized( 0.1 );
    auto low = kicktest::snapshot( s );
    s.setNormalized( 0.9 );
    auto high = kicktest::snapshot( s );

    int diff = 0;
    for( int y = 0; y < low.getHeight(); ++y )
        for( int x = 0; x < low.getWidth(); ++x )
            if( low.getPixelAt( x, y ) != high.getPixelAt( x, y ) )
                ++diff;
    EXPECT_GT( diff, 200 ) << "moving the thumb from 10% to 90% must repaint a visible region";
}

// ---------------------------------------------------------- toggle options

namespace
{
    struct OptionsListener : ui::KickToggleOptions::Listener
    {
        void toggleOptionsChanged( ui::KickToggleOptions* o ) override { selections.push_back( o->getSelected() ); }
        std::vector<int> selections;
    };
}

TEST( KickToggleOptions, KnobRotationWalksTheOptions )
{
    ui::KickToggleOptions t( "mode", { "A", "B", "C", "D" } );
    t.setBounds( 0, 0, 200, 40 );
    EXPECT_EQ( t.getSelected(), 0 );

    t.knobRotated( 1 );
    EXPECT_EQ( t.getSelected(), 1 );
    t.knobRotated( 2 );
    EXPECT_EQ( t.getSelected(), 3 );
    t.knobRotated( -1 );
    EXPECT_EQ( t.getSelected(), 2 );
}

TEST( KickToggleOptions, SelectionIsClampedAtBothEnds )
{
    ui::KickToggleOptions t( "mode", { "A", "B", "C" } );
    t.knobRotated( 99 );
    EXPECT_EQ( t.getSelected(), 2 );
    t.knobRotated( -99 );
    EXPECT_EQ( t.getSelected(), 0 );
}

TEST( KickToggleOptions, ListenersAndCallbackSeeEachChange )
{
    ui::KickToggleOptions t( "mode", { "A", "B", "C" } );
    OptionsListener       l;
    t.addListener( &l );
    std::vector<juce::String> names;
    t.onSelectionChanged = [&]( int, const juce::String& name ) { names.push_back( name ); };

    t.knobRotated( 1 );
    t.knobRotated( 1 );
    t.knobRotated( 1 ); // already at the end: no change, no notification

    EXPECT_EQ( l.selections, ( std::vector<int>{ 1, 2 } ) );
    ASSERT_EQ( names.size(), 2u );
    EXPECT_EQ( names[0], "B" );
    EXPECT_EQ( names[1], "C" );
    t.removeListener( &l );
}

TEST( KickToggleOptions, ReplacingOptionsKeepsSelectionInRange )
{
    ui::KickToggleOptions t( "mode", { "A", "B", "C", "D" } );
    t.knobRotated( 3 );
    t.setOptions( { "X", "Y" } );
    t.setSelected( 5, juce::dontSendNotification );
    EXPECT_EQ( t.getSelected(), 1 );
}

// ------------------------------------------------------------ level meter

namespace
{
    constexpr float kSilence = -100.f;

    void setAll( ui::KickLevelMeter& m, float dB )
    {
        m.setSignalStrength( dB, dB, dB, dB );
    }

    // Count the pixels that differ from the meter's silent appearance.
    int litPixels( ui::KickLevelMeter& m, float dB )
    {
        setAll( m, kSilence );
        auto silent = kicktest::snapshot( m );
        setAll( m, dB );
        return kicktest::differingPixels( kicktest::snapshot( m ), silent );
    }
}

TEST( KickLevelMeter, LouderSignalLightsMorePixels )
{
    ui::KickLevelMeter m( ui::KickLevelMeter::Style::Vertical );
    m.setBounds( 0, 0, 40, 240 );
    m.setRange( -100.0, 0.0 );

    const int quiet = litPixels( m, -60.f );
    const int mid   = litPixels( m, -30.f );
    const int loud  = litPixels( m, -3.f );
    EXPECT_GT( mid, quiet );
    EXPECT_GT( loud, mid );
}

TEST( KickLevelMeter, SilenceMatchesAFreshMeter )
{
    // A meter driven to silence must look like one that never received a signal.
    ui::KickLevelMeter fresh( ui::KickLevelMeter::Style::Vertical );
    ui::KickLevelMeter driven( ui::KickLevelMeter::Style::Vertical );
    for( auto* m : { &fresh, &driven } )
    {
        m->setBounds( 0, 0, 40, 240 );
        m->setRange( -100.0, 0.0 );
    }
    setAll( driven, 0.f );
    setAll( driven, kSilence );

    EXPECT_EQ( kicktest::differingPixels( kicktest::snapshot( driven ), kicktest::snapshot( fresh ) ), 0 );
}

TEST( KickLevelMeter, EveryStyleRespondsToTheVectorForm )
{
    using S = ui::KickLevelMeter::Style;
    for( auto style : { S::Vertical, S::Horizantal, S::VerticalWithoutMarks, S::HorizantalWithMarks, S::VerticalWithMarks } )
    {
        ui::KickLevelMeter m( style );
        m.setBounds( 0, 0, 120, 120 );
        m.setRange( -60.0, 6.0 );
        m.setSignalStrength( std::vector<float>{ -60.f, -60.f } );
        auto silent = kicktest::snapshot( m );
        m.setSignalStrength( std::vector<float>{ 0.f, 0.f } );
        EXPECT_GT( kicktest::differingPixels( kicktest::snapshot( m ), silent ), 0 ) << "style " << static_cast<int>( style );
    }
}

// --------------------------------------------------------------------- theme

TEST( KickTheme, SemanticColoursAreOpaqueAndDistinct )
{
    EXPECT_NE( ui::Theme::backgroundColour(), ui::Theme::foregroundColour() );
    EXPECT_NE( ui::Theme::textColour(), ui::Theme::backgroundColour() );
    for( auto c : { ui::Theme::backgroundColour(), ui::Theme::foregroundColour(), ui::Theme::textColour(), ui::Theme::highlightColour() } )
        EXPECT_GT( c.getAlpha(), 0 );
}

TEST( KickTheme, VelocityColourVariesAcrossTheRange )
{
    EXPECT_NE( ui::Theme::noteColour( 10 ), ui::Theme::noteColour( 120 ) );
}
