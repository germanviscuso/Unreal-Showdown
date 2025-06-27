// Source/ShowdownEditor/Private/ShowdownEditor.cpp

#include "ShowdownEditor.h" // PCH must be first
#include "ShowdownEditorCommands.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "Framework/Commands/UICommandList.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HighResScreenshot.h"
#include "Editor/EditorEngine.h"
#include "LevelEditorViewport.h"

DEFINE_LOG_CATEGORY_STATIC(LogShowdownEditor, Log, All);

void FShowdownEditorModule::StartupModule()
{
    UE_LOG(LogShowdownEditor, Log, TEXT("ShowdownEditorModule starting up."));

    FShowdownEditorCommands::Register();
    PluginCommands = MakeShareable(new FUICommandList);

    PluginCommands->MapAction(
        FShowdownEditorCommands::Get().CaptureScene,
        FExecuteAction::CreateRaw(this, &FShowdownEditorModule::OnCaptureScenePressed),
        FCanExecuteAction());

    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

    // FIX #1: This registers our command list globally, making the shortcut work.
    LevelEditorModule.GetGlobalLevelEditorActions()->Append(PluginCommands.ToSharedRef());

    // FIX #2: This adds our tool to a new section within the main "Tools" menu.
    TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
    MenuExtender->AddMenuExtension(
        "LevelEditor", // The name of the menu to extend
        EExtensionHook::After,
        PluginCommands,
        FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& MenuBuilder)
            {
                MenuBuilder.BeginSection("ShowdownEditorTools", TAttribute<FText>(FText::FromString("Custom Tools")));
                MenuBuilder.AddMenuEntry(FShowdownEditorCommands::Get().CaptureScene);
                MenuBuilder.EndSection();
            })
    );
    LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

void FShowdownEditorModule::ShutdownModule()
{
    FShowdownEditorCommands::Unregister();
}

void FShowdownEditorModule::OnCaptureScenePressed()
{
    // FIX #3: This section correctly forces Game View for the screenshot.
    FViewport* ActiveViewport = GEditor->GetActiveViewport();
    FLevelEditorViewportClient* ViewportClient = static_cast<FLevelEditorViewportClient*>(ActiveViewport->GetClient());

    if (!ViewportClient)
    {
        UE_LOG(LogShowdownEditor, Warning, TEXT("Could not find a valid Level Editor Viewport Client."));
        return;
    }

    // Store the current view state so we can restore it later
    const bool bWasInGameView = ViewportClient->IsInGameView();

    // Force the viewport into Game View to hide gizmos
    ViewportClient->SetGameView(true);

    // Set the file path
    FString Directory = FPaths::ProjectSavedDir() + TEXT("Screenshots/");
    FString Filename = FString::Printf(TEXT("SceneCapture_%s.png"), *FDateTime::Now().ToString());
    FString FilePath = FPaths::ConvertRelativePathToFull(Directory + Filename);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);

    // Request the screenshot
    FScreenshotRequest::RequestScreenshot(FilePath, false, false);

    // Restore the original view state
    ViewportClient->SetGameView(bWasInGameView);

    UE_LOG(LogShowdownEditor, Log, TEXT("Scene capture requested. Saved to: %s"), *FilePath);
}

IMPLEMENT_MODULE(FShowdownEditorModule, ShowdownEditor)