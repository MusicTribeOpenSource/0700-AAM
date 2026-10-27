
juce_add_modules(
    audio_engine
    audio_file_manager
    audio_grouping
    audio_midi
    audio_program
    interface
    project
)

target_link_libraries(audio_program INTERFACE kick_clap_juce)

