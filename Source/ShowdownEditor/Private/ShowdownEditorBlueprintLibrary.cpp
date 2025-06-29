#include "ShowdownEditorBlueprintLibrary.h"
#include "ShowdownEditor.h"
#include "Modules/ModuleManager.h"

void UShowdownEditorBlueprintLibrary::TriggerSceneEditWithPrompt(const FString& Prompt)
{
    if (Prompt.IsEmpty())
    {
        // Note: I'm using LogTemp here because this file cannot see LogShowdownEditor.
        // To fix this, you would include "ShowdownEditor.h" which has the DECLARE macro.
        UE_LOG(LogTemp, Warning, TEXT("Prompt is empty. Aborting."));
        return;
    }

    FShowdownEditorModule& ShowdownModule = FModuleManager::LoadModuleChecked<FShowdownEditorModule>("ShowdownEditor");
    ShowdownModule.ExecuteCaptureAndEdit(Prompt);
}