#include "ShowdownEditorCommands.h"

#define LOCTEXT_NAMESPACE "FShowdownEditorModule"

void FShowdownEditorCommands::RegisterCommands()
{
    // Define the command details and assign the shortcut Ctrl+Alt+C
    UI_COMMAND(CaptureScene, "Capture Scene View", "Captures the active viewport to a file.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Alt, EKeys::C));
}

#undef LOCTEXT_NAMESPACE