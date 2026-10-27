#ifdef KICK_UI_H_INCLUDE
    /* When you add this cpp file to your project, you mustn't include it in a file where you've
   already included any other headers - just put it inside a file on its own, possibly with your config
   flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
   header files that the compiler may be using.
*/
    #error "Incorrect use of JUCE cpp file"
#endif

#include "kick_ui.h"
#include "kick_ui_grid.h"

//dialogs
#include "dialogs/kick_AppMainMode.cpp"
#include "dialogs/kick_AppHome.cpp"
#include "dialogs/kick_AppMenuOptions.cpp"
#include "dialogs/kick_AppAssignSample.cpp"
#include "dialogs/kick_AppTopbar.cpp"
#include "dialogs/kick_AppStatusbar.cpp"
#include "dialogs/kick_Inserts.cpp"

// Abstract Interfaces
#include "row_view/kick_IRowView.cpp"

// Program_view
#include "program_view/source/kick_ClapPanel.h"
#include "program_view/source/kick_AppProgram.cpp"
#include "program_view/source/kick_AppProgramComponent.cpp"
#include "program_view/source/kick_AppProgramOptions.cpp"

// Sequence_view
#include "sequence_view/source/kick_AppSequence.cpp"
#include "sequence_view/source/kick_AppSequenceComponent.cpp"
#include "sequence_view/source/kick_AppSequenceOptions.cpp"

// Track view
#include "track_view/source/kick_AppTrack.cpp"
#include "track_view/source/kick_AppTrackComponent.cpp"
#include "track_view/source/kick_AppTrackOptions.cpp"

//sample editor
#include "audio_components/sample_editor/kick_SampleEditorSettings.cpp"
#include "audio_components/sample_editor/kick_SEScaleComponent.cpp"
#include "audio_components/sample_editor/kick_SampleEditorCue.cpp"
#include "audio_components/sample_editor/kick_SEViewComponent.cpp"
#include "audio_components/sample_editor/kick_SampleEditor.cpp"
#include "audio_components/sample_editor/kick_SampleEditorLayer.cpp"
#include "audio_components/sample_editor/kick_SampleEditorOptions.cpp"
#include "audio_components/sample_editor/kick_SampleEditorData.cpp"
#include "audio_components/sample_editor/kick_SampleEditorProcess.cpp"
#include "audio_components/sample_editor/kick_SampleEditorAssign.cpp"
#include "audio_components/sample_editor/kick_SampleLayerSettings.cpp"
#include "audio_components/xyfx/kick_XyFxView.cpp"

// preferences
#include "swupdate/kick_SWUpdateView.cpp"
#include "swupdate/kick_SWUpdateScreen.cpp"

// erase
#include "modes/kick_Erase.cpp"
#include "modes/kick_Copy.cpp"
#include "modes/kick_Delete.cpp"

// Main Menu Modes...
#include "modes/kick_ListEdit.cpp"
#include "modes/kick_PadColor.cpp"
#include "modes/kick_trackMute.cpp"
#include "modes/kick_padMute.cpp"
#include "modes/kick_nextSequence.cpp"
#include "modes/kick_song.cpp"
#include "modes/mixers/kick_MixerTabView.cpp"
#include "modes/mixers/kick_PadMixer.cpp"
#include "modes/knobedit/kick_KnobTabs.cpp"
#include "modes/knobedit/kick_KnobEdit.cpp"
#include "modes/knobedit/kick_KnobGrid.cpp"
#include "modes/midicontrol/kick_MidiControlTabs.cpp"
#include "modes/midicontrol/kick_MidiControl.cpp"
#include "modes/padperform/kick_PadPerform.cpp"
#include "modes/filebrowser/kick_fileUtils.cpp"
#include "modes/filebrowser/kick_fileBrowser.cpp"

//recentproject
#include "recent_project/kick_RecentProject.cpp"
#include "recent_project/kick_RecentProject_Utils.cpp"

//smapler
#include "audio_components/sampler/kick_SaveSampleDialog.cpp"
#include "audio_components/sampler/kick_Sampler.cpp"
#include "audio_components/sampler/kick_SamplerRecorder.cpp"
#include "audio_components/sampler/kick_KeepDiscardSampleComponent.cpp"

//looper
#include "audio_components/looper/kick_Looper.cpp"
#include "audio_components/looper/kick_LoopRecorder.cpp"
