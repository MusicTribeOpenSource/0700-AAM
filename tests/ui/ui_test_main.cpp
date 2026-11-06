// gtest entry point that boots JUCE's GUI subsystem (MessageManager, fonts, Desktop)
// once for the whole process, so components can be created and painted off-screen.

#include <gtest/gtest.h>
#include <juce_gui_basics/juce_gui_basics.h>

int main( int argc, char** argv )
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ::testing::InitGoogleTest( &argc, argv );
    return RUN_ALL_TESTS();
}
