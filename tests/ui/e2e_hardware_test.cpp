// End-to-end: front-panel bytes on the wire -> PktMgr framing -> HWMsg -> IPC JSON
// -> a real Kick control reacting, and the control's reply travelling back out to
// the panel as bytes. No serial port or device involved: the "hardware" is a PktMgr
// encoder, which is exactly what the MCU firmware speaks.

#include <gtest/gtest.h>

#include <kick_ui_controls/kick_ui_controls.h>

#include "support/pktmgr_harness.h"
#include "ui_test_support.h"

using contracts::DclContractsHelper;
using contracts::DclSignals;
using contracts::DeviceControlContracts;

namespace
{
    // Wire bytes -> decoded HWMsg, through the same CPktMgr the product uses.
    CPktMgr::HWMsg_t receive( kicktest::PktHarness& rx, const std::vector<char>& wire )
    {
        rx.mgr.ClearReceiver();
        rx.feed( wire );
        EXPECT_TRUE( rx.mgr.IsFrameRcvComplete() ) << "frame did not survive the PktMgr decoder";
        CPktMgr::HWMsg_t m;
        m.pktType = rx.mgr.GetRxPktType();
        m.pktDir  = rx.mgr.GetPktDir();
        m.pktCmd  = rx.mgr.GetRxCommand();
        for( uint8_t b : rx.drainPayload() )
            m.data.push_back( b );
        rx.mgr.ClearReceiver();
        return m;
    }

    std::vector<char> panelButton( kicktest::PktHarness& mcu, int code, int state = 1 )
    {
        return mcu.encode( CPktMgr::EN_PKT_TYPE_BUTTON, CPktMgr::EN_PKT_REQUEST, DeviceControlContracts::EN_BTN_STATUS, { code, state } );
    }

    // Collects everything the UI asks the DCL (panel) layer to send.
    struct DclTap
    {
        DclTap()
        {
            conn = DclSignals::sendCommandToDcl.connect( [this]( std::shared_ptr<ipc::MTIPCJson> m ) { sent.push_back( m ); } );
        }
        ~DclTap() { conn.disconnect(); }

        std::vector<std::shared_ptr<ipc::MTIPCJson>> sent;
        sigslot::connection                           conn;
    };
}

class HardwareToUi : public ::testing::Test
{
protected:
    kicktest::PktHarness mcu; // the panel's firmware
    kicktest::PktHarness host; // the Linux host's decoder
};

TEST_F( HardwareToUi, KnobClockwiseSteppsAToggleSelector )
{
    ui::KickToggleOptions sel( "mode", { "Pads", "Keys", "Seq" } );
    sel.setBounds( 0, 0, 200, 40 );

    sel.handleCommand( *std::make_unique<CPktMgr::HWMsg_t>( receive( host, panelButton( mcu, DeviceControlContracts::EN_KNOB_CW ) ) ) );
    EXPECT_EQ( sel.getSelected(), 1 );
}

TEST_F( HardwareToUi, KnobTurnsWalkForwardAndBack )
{
    ui::KickToggleOptions sel( "mode", { "A", "B", "C", "D" } );
    auto                  press = [&]( int code ) {
        auto m = receive( host, panelButton( mcu, code ) );
        sel.handleCommand( m );
    };
    press( DeviceControlContracts::EN_KNOB_CW );
    press( DeviceControlContracts::EN_KNOB_CW );
    press( DeviceControlContracts::EN_KNOB_CW );
    EXPECT_EQ( sel.getSelected(), 3 );
    press( DeviceControlContracts::EN_KNOB_CCW );
    EXPECT_EQ( sel.getSelected(), 2 );
    press( DeviceControlContracts::EN_MINUS );
    EXPECT_EQ( sel.getSelected(), 1 );
    press( DeviceControlContracts::EN_PLUS );
    EXPECT_EQ( sel.getSelected(), 2 );
}

TEST_F( HardwareToUi, CenterKnobRotationUsesDirectionAndAmount )
{
    ui::KickToggleOptions sel( "mode", { "A", "B", "C", "D", "E", "F" } );
    auto                  turn = [&]( int dir, int amount ) {
        auto wire = mcu.encode( CPktMgr::EN_PKT_TYPE_KNOBS, CPktMgr::EN_PKT_REQUEST, 0, { DeviceControlContracts::EN_KNOB_CENTER, dir, amount } );
        auto m    = receive( host, wire );
        sel.handleCommand( m );
    };
    turn( DeviceControlContracts::EN_KNOB_CLOCKWISE, 3 );
    EXPECT_EQ( sel.getSelected(), 3 );
    turn( DeviceControlContracts::EN_KNOB_CLOCKWISE + 1, 2 ); // anti-clockwise
    EXPECT_EQ( sel.getSelected(), 1 );
}

TEST_F( HardwareToUi, TouchStripDrivesSliderToThatPercentage )
{
    ui::KickSlider s( "vol", juce::Slider::LinearHorizontal );
    s.setBounds( 0, 0, 200, 30 );
    s.setRange( 0.0, 1.0, 0.0 );

    auto wire = mcu.encode( CPktMgr::EN_PKT_TYPE_SLIDER, CPktMgr::EN_PKT_REQUEST, DeviceControlContracts::EN_SLIDER_TOUCH_INPUT, { 75 } );
    auto m    = receive( host, wire );
    s.handleCommand( m );

    EXPECT_NEAR( s.getNormalized(), 0.75, 1e-6 );
}

TEST_F( HardwareToUi, TouchStripValueAbove100IsClamped )
{
    ui::KickSlider s( "vol", juce::Slider::LinearHorizontal );
    s.setRange( 0.0, 1.0, 0.0 );
    auto m = receive( host, mcu.encode( CPktMgr::EN_PKT_TYPE_SLIDER, CPktMgr::EN_PKT_REQUEST, DeviceControlContracts::EN_SLIDER_TOUCH_INPUT, { 200 } ) );
    s.handleCommand( m );
    EXPECT_NEAR( s.getNormalized(), 1.0, 1e-6 );
}

TEST_F( HardwareToUi, SliderMovementIsEchoedBackToThePanelAsAnLedBar )
{
    DclTap       tap;
    ui::KickSlider s( "vol", juce::Slider::LinearHorizontal );
    s.setRange( 0.0, 1.0, 0.0 );

    auto m = receive( host, mcu.encode( CPktMgr::EN_PKT_TYPE_SLIDER, CPktMgr::EN_PKT_REQUEST, DeviceControlContracts::EN_SLIDER_TOUCH_INPUT, { 40 } ) );
    s.handleCommand( m );

    ASSERT_FALSE( tap.sent.empty() ) << "the control must tell the panel what it now shows";

    // Reverse the trip: IPC JSON -> HWMsg -> bytes -> what the MCU would parse.
    CPktMgr::HWMsg_t out;
    DclContractsHelper::ipcMessageToHWMessage( tap.sent.back(), out );
    EXPECT_EQ( out.pktType, CPktMgr::EN_PKT_TYPE_SLIDER );

    auto wire = host.encode( out.pktType, out.pktDir, out.pktCmd, out.data );
    auto back = receive( mcu, wire );
    EXPECT_EQ( back.pktType, CPktMgr::EN_PKT_TYPE_SLIDER );
    EXPECT_EQ( back.pktCmd, DeviceControlContracts::EN_SLIDER_BAR_LED );
    ASSERT_FALSE( back.data.empty() );
    EXPECT_EQ( back.data[0], 40 );
}

TEST_F( HardwareToUi, UnrelatedButtonDoesNotMoveControls )
{
    ui::KickToggleOptions sel( "mode", { "A", "B", "C" } );
    ui::KickSlider        s( "vol", juce::Slider::LinearHorizontal );
    s.setRange( 0.0, 1.0, 0.0 );
    s.setNormalized( 0.5 );

    auto m = receive( host, panelButton( mcu, DeviceControlContracts::EN_SHIFT ) );
    sel.handleCommand( m );
    s.handleCommand( m );

    EXPECT_EQ( sel.getSelected(), 0 );
    EXPECT_NEAR( s.getNormalized(), 0.5, 1e-9 );
}

TEST_F( HardwareToUi, HwMessageSurvivesTheIpcJsonRoundTrip )
{
    auto wire = mcu.encode( CPktMgr::EN_PKT_TYPE_PAD, CPktMgr::EN_PKT_REQUEST, DeviceControlContracts::EN_PAD_STATE, { 7, 1, 100, 0 } );
    auto hw   = receive( host, wire );

    auto json = DclContractsHelper::prepareHWMessage( hw.pktType, hw.pktDir, hw.pktCmd, hw.data );
    CPktMgr::HWMsg_t again;
    DclContractsHelper::ipcMessageToHWMessage( json, again );

    EXPECT_EQ( again.pktType, hw.pktType );
    EXPECT_EQ( again.pktDir, hw.pktDir );
    EXPECT_EQ( again.pktCmd, hw.pktCmd );
    EXPECT_EQ( again.data, hw.data );
    EXPECT_TRUE( DclContractsHelper::isPadButtonMessage( again ) );
    EXPECT_EQ( DclContractsHelper::getPadNumber( again ), 7 );
    EXPECT_EQ( DclContractsHelper::getPadVelocity( again ), 100 );
}

// ----------------------------------------------------- a whole mini screen

namespace
{
    // A throwaway screen wired the way Kick screens are: controls feed a model,
    // and hardware events reach whichever control has focus.
    struct MiniScreen : juce::Component
    {
        MiniScreen()
            : mode( "mode", { "Pads", "Keys", "Seq" } )
            , volume( "vol", juce::Slider::LinearBar )
        {
            addAndMakeVisible( mode );
            addAndMakeVisible( volume );
            volume.setRange( 0.0, 1.0, 0.0 );
            mode.onSelectionChanged = [this]( int i, const juce::String& n ) { model.modeIndex = i; model.modeName = n; };
            setSize( 240, 90 );
        }
        void resized() override
        {
            mode.setBounds( 0, 0, getWidth(), 40 );
            volume.setBounds( 0, 50, getWidth(), 30 );
        }

        struct { int modeIndex = 0; juce::String modeName; } model;
        ui::KickToggleOptions mode;
        ui::KickSlider        volume;
    };
}

TEST_F( HardwareToUi, PanelSessionUpdatesModelAndPixels )
{
    MiniScreen screen;
    auto       before = kicktest::snapshot( screen );

    auto send = [&]( ui::KickControlsProcessor& target, const std::vector<char>& wire ) {
        auto m = receive( host, wire );
        target.handleCommand( m );
    };

    send( screen.mode, panelButton( mcu, DeviceControlContracts::EN_KNOB_CW ) );
    send( screen.mode, panelButton( mcu, DeviceControlContracts::EN_KNOB_CW ) );
    send( screen.volume, mcu.encode( CPktMgr::EN_PKT_TYPE_SLIDER, CPktMgr::EN_PKT_REQUEST, DeviceControlContracts::EN_SLIDER_TOUCH_INPUT, { 65 } ) );
    kicktest::pumpUntil( [&] { return screen.model.modeIndex == 2; } );

    EXPECT_EQ( screen.model.modeIndex, 2 );
    EXPECT_EQ( screen.model.modeName, "Seq" );
    // Hardware-driven slider moves set the value silently, so read the control, not the callback.
    EXPECT_NEAR( screen.volume.getNormalized(), 0.65, 1e-6 );

    const int changed = kicktest::differingPixels( kicktest::snapshot( screen ), before );
    EXPECT_GT( changed, 100 ) << "the screen should look different after the session";
}
