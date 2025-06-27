#include "ShowdownEditor.h"
#include "ShowdownEditorCommands.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "Framework/Commands/UICommandList.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HighResScreenshot.h"
#include "Editor/EditorEngine.h" // Required for GEditor

// Define a log category for our module
DEFINE_LOG_CATEGORY_STATIC(LogShowdownEditor, Log, All);

void FShowdownEditorModule::StartupModule()
{
    UE_LOG(LogShowdownEditor, Warning, TEXT("--- ShowdownEditorModule is starting up! ---"));

    // Register the command set
    FShowdownEditorCommands::Register();
    PluginCommands = MakeShareable(new FUICommandList);

    // Map the "CaptureScene" command to our OnCaptureScenePressed function
    PluginCommands->MapAction(
        FShowdownEditorCommands::Get().CaptureScene,
        FExecuteAction::CreateRaw(this, &FShowdownEditorModule::OnCaptureScenePressed),
        FCanExecuteAction());

    // Add a menu item to the "Tools" menu
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    {
        TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
        MenuExtender->AddMenuExtension("LevelEditor", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& Builder)
        {
            Builder.AddMenuEntry(FShowdownEditorCommands::Get().CaptureScene);
        }));
        LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
    }
}

void FShowdownEditorModule::ShutdownModule()
{
    FShowdownEditorCommands::Unregister();
}

void FShowdownEditorModule::OnCaptureScenePressed()
{
    // Get the active level viewport
    FViewport* ActiveViewport = GEditor->GetActiveViewport();
    if (!ActiveViewport)
    {
        UE_LOG(LogShowdownEditor, Warning, TEXT("Could not find an active viewport to capture."));
        return;
    }

    // Set the file path in the project's Saved/Screenshots folder
    FString Directory = FPaths::ProjectSavedDir() + TEXT("Screenshots/");
    FString Filename = FString::Printf(TEXT("SceneCapture_%s.png"), *FDateTime::Now().ToString());
    FString FilePath = FPaths::ConvertRelativePathToFull(Directory + Filename);

    // Ensure the directory exists
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);

    // Request the screenshot. The 'true' argument ensures we capture in game view (no gizmos).
    FScreenshotRequest::RequestScreenshot(FilePath, true, false);

    UE_LOG(LogShowdownEditor, Log, TEXT("Scene capture requested and will be saved to: %s"), *FilePath);
}


IMPLEMENT_MODULE(FShowdownEditorModule, ShowdownEditor)