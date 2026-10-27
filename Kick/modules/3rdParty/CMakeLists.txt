
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