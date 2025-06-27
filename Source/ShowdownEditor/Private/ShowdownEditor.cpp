// Source/ShowdownEditor/Private/ShowdownEditor.cpp

#include "ShowdownEditor.h"
#include "ShowdownEditorCommands.h"
#include "LevelEditor.h"
#include "Framework/Commands/UICommandList.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Editor/EditorEngine.h"
#include "LevelEditorViewport.h"
#include "Logging/LogMacros.h"
#include "RenderingThread.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"


DEFINE_LOG_CATEGORY_STATIC(LogShowdownEditor, Log, All);
#define LOCTEXT_NAMESPACE "FShowdownEditorModule"

void FShowdownEditorModule::StartupModule()
{
    FShowdownEditorCommands::Register();
    PluginCommands = MakeShareable(new FUICommandList);

    PluginCommands->MapAction(
        FShowdownEditorCommands::Get().CaptureScene,
        FExecuteAction::CreateRaw(this, &FShowdownEditorModule::OnCaptureScenePressed),
        FCanExecuteAction());

    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

    // THE SHORTCUT FIX: This correctly registers the command list so the shortcut works globally.
    LevelEditorModule.GetGlobalLevelEditorActions()->Append(PluginCommands.ToSharedRef());

    // THE MENU FIX: This adds a new section to the main "Tools" menu in a reliable way.
    TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
    MenuExtender->AddMenuExtension(
        "LevelEditor", // Hooking into a standard Level Editor extension point
        EExtensionHook::After,
        PluginCommands,
        FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& MenuBuilder)
            {
                MenuBuilder.BeginSection("ShowdownCustomTools", LOCTEXT("CustomTools_Heading", "Custom Tools"));
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

// In ShowdownEditor.cpp

void FShowdownEditorModule::OnCaptureScenePressed()
{
    // This simple method requests a screenshot and lets the engine handle it.
    // It will capture gizmos, but it is reliable.

    FString Directory = FPaths::ProjectSavedDir() + TEXT("Screenshots/");
    FString Filename = FString::Printf(TEXT("SceneCapture_%s.png"), *FDateTime::Now().ToString());
    FString FilePath = FPaths::ConvertRelativePathToFull(Directory + Filename);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*Directory);

    // Use the simple, asynchronous screenshot request.
    FScreenshotRequest::RequestScreenshot(FilePath, false, false);

    UE_LOG(LogShowdownEditor, Log, TEXT("Scene capture requested. It will be saved to: %s"), *FilePath);
}

IMPLEMENT_MODULE(FShowdownEditorModule, ShowdownEditor);
#undef LOCTEXT_NAMESPACE