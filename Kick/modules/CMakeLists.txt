juce_add_modules(
    _playback
    audio_interface
    common
    hardware
    input_events
    kick_ui
    kick_ui_channel_mixer
    kick_ui_common
    kick_ui_components
    kick_ui_controls
    kick_ui_header
    kick_ui_selectors
    kick_ui_systemResources
    program_edit_ui
    kick_ui_settings
    kick_leds
	ipc
	contracts
	gesture
)

add_subdirectory(3rdParty)
add_subdirectory(engine)

