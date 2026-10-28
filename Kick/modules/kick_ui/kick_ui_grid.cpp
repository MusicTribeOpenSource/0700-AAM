#ifdef KICK_UI_GRID_H_INCLUDE
    /* When you add this cpp file to your project, you mustn't include it in a file where you've
   already included any other headers - just put it inside a file on its own, possibly with your config
   flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
   header files that the compiler may be using.
*/
    #error "Incorrect use of JUCE cpp file"
#endif

#include "kick_ui_grid.h"

// Grid common
#include "audio_components/gridview/kick_GridData.cpp"

// Grid view
#include "audio_components/gridview/kick_Locate.cpp"
#include "audio_components/gridview/kick_GridEditOptions.cpp"
#include "audio_components/gridview/kick_AppGridHeader.cpp"
#include "audio_components/gridview/kick_ParameterView.cpp"
#include "audio_components/gridview/kick_TimeSigSetting.cpp"
#include "audio_components/gridview/kick_AppGrid.cpp"
#include "audio_components/gridview/kick_GridEraseTool.cpp"
#include "audio_components/gridview/kick_GridSelectTool.cpp"
#include "audio_components/gridview/kick_GridComponent.cpp"
#include "audio_components/gridview/kick_GridPencilTool.cpp"
#include "audio_components/gridview/kick_GridCopyTool.cpp"
#include "audio_components/gridview/kick_GridMagnifyTool.cpp"

// Step sequencer grid
#include "audio_components/step_sequencer/kick_StepPresets.cpp"
#include "audio_components/step_sequencer/kick_DrumGrid.cpp"
#include "audio_components/step_sequencer/kick_stepSequencer.cpp"
#include "audio_components/step_sequencer/kick_StepSequencerControl.cpp"
