juce_add_modules(
    _playback
    audio_interface
    common
    hardware
    input_events
    serial_port
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
)

add_subdirectory(3rdParty)
add_subdirectory(engine)


target_compile_definitions(hardware INTERFACE KICK_ENABLE_HARDWARE=$<BOOL:${KICK_ENABLE_HARDWARE}>)
target_compile_definitions(input_events INTERFACE KICK_ENABLE_HARDWARE=$<BOOL:${KICK_ENABLE_HARDWARE}>)
