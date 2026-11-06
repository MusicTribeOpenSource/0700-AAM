
juce_add_modules(
    dywapitchtrack
    fpm
    magic_enum
    melatonin_parameters
    moodycamel
    signal
    waveform
)

if(USE_VALUE_TREE_DEBUGGER)
juce_add_modules(
    juce-toys/jcf_debug
    juce-toys/jcf_advanced_leak_detector
    juce-toys/multithreading
)
endif()

# juce_serialport was removed from the public release (no license). Use it if
# someone restores it here, otherwise fall back to the emulator's clean-room
# POSIX implementation.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/juce_serialport)
    juce_add_module(juce_serialport)
elseif(KICK_EMU_DIR)
    juce_add_module(${KICK_EMU_DIR}/modules/juce_serialport)
else()
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/../../../emu/modules/juce_serialport)
endif()
