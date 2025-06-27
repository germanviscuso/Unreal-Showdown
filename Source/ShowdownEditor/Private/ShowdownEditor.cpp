#include "ShowdownEditor.h"
#include "ShowdownEditorCommands.h"
#include "LevelEditor.h"
#include "Framework/Commands/UICommandList.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HighResScreenshot.h"
#include "Editor/EditorEngine.h"
#include "Provider/OpenAIProvider.h"
#include "FuncLib/OpenAIFuncLib.h"
#include "Engine/World.h" // Required for GetWorld()
#include "TimerManager.h" // Required for FTimerHandle

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
    LevelEditorModule.GetGlobalLevelEditorActions()->Append(PluginCommands.ToSharedRef());

    TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
    MenuExtender->AddMenuExtension("LevelEditor", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& MenuBuilder)
        {
            MenuBuilder.BeginSection("ShowdownCustomTools", LOCTEXT("CustomTools_Heading", "Custom Tools"));
            MenuBuilder.AddMenuEntry(FShowdownEditorCommands::Get().CaptureScene);
            MenuBuilder.EndSection();
        })
    );
    LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);

    OpenAIProvider = NewObject<UOpenAIProvider>();
    OpenAIProvider->SetLogEnabled(true);
}

void FShowdownEditorModule::ShutdownModule()
{
    FShowdownEditorCommands::Unregister();
}

void FShowdownEditorModule::OnCaptureScenePressed()
{
    FString Directory = FPaths::ProjectSavedDir() + TEXT("Screenshots/");
    FString Filename = FString::Printf(TEXT("SceneCapture_%s.png"), *FDateTime::Now().ToString());
    FString FilePath = FPaths::ConvertRelativePathToFull(Directory + Filename);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*Directory);

    FScreenshotRequest::RequestScreenshot(FilePath, false, false);

    UE_LOG(LogShowdownEditor, Log, TEXT("Scene capture requested. Will be saved to: %s. Sending to OpenAI in 2 seconds..."), *FilePath);

    FTimerHandle TimerHandle;
    FTimerDelegate TimerDelegate;
    TimerDelegate.BindLambda([this, FilePath]()
        {
            SendImageToOpenAI(FilePath);
        });

    if (GEditor->GetEditorWorldContext().World())
    {
        GEditor->GetEditorWorldContext().World()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 2.0f, false);
    }
}

void FShowdownEditorModule::OnImageVariationSuccess(const FImageVariationResponse& Response, const FOpenAIResponseMetadata& Meta)
{
    OpenAIProvider->OnCreateImageVariationCompleted().RemoveAll(this);
    OpenAIProvider->OnRequestError().RemoveAll(this);

    if (Response.Data.Num() > 0)
    {
        const FString URL = Response.Data[0].URL;
        UE_LOG(LogShowdownEditor, Warning, TEXT("OpenAI Image Variation SUCCESS! URL: %s"), *URL);
    }
    else
    {
        UE_LOG(LogShowdownEditor, Error, TEXT("OpenAI Image Variation request succeeded but returned no data."));
    }
}

void FShowdownEditorModule::OnImageVariationError(const FString& URL, const FString& Content)
{
    OpenAIProvider->OnCreateImageVariationCompleted().RemoveAll(this);
    OpenAIProvider->OnRequestError().RemoveAll(this);
    UE_LOG(LogShowdownEditor, Error, TEXT("OpenAI Image Variation FAILED. URL: %s, Error: %s"), *URL, *Content);
}

void FShowdownEditorModule::SendImageToOpenAI(const FString& ImagePath)
{
    if (!FPaths::FileExists(ImagePath))
    {
        UE_LOG(LogShowdownEditor, Error, TEXT("Screenshot file does not exist at path: %s. Cannot send to OpenAI."), *ImagePath);
        return;
    }

    const FString AuthIniPath = FPaths::ProjectDir() + TEXT("OpenAIAuth.ini");
    FOpenAIAuth Auth = UOpenAIFuncLib::LoadAPITokensFromFile(AuthIniPath);

    if (Auth.APIKey.IsEmpty())
    {
        UE_LOG(LogShowdownEditor, Error, TEXT("Failed to load OpenAI API key from OpenAIAuth.ini"));
        return;
    }

    FOpenAIImageVariation ImageVariationRequest;
    ImageVariationRequest.Image = ImagePath;
    ImageVariationRequest.N = 1;
    ImageVariationRequest.Size = UOpenAIFuncLib::OpenAIImageSizeDalle2ToString(EImageSizeDalle2::Size_1024x1024);
    ImageVariationRequest.Response_Format = UOpenAIFuncLib::OpenAIImageFormatToString(EOpenAIImageFormat::URL);

    OpenAIProvider->OnCreateImageVariationCompleted().AddRaw(this, &FShowdownEditorModule::OnImageVariationSuccess);

    OpenAIProvider->OnRequestError().AddRaw(this, &FShowdownEditorModule::OnImageVariationError);

    UE_LOG(LogShowdownEditor, Log, TEXT("Sending image variation request to OpenAI for file: %s"), *ImagePath);
    OpenAIProvider->CreateImageVariation(ImageVariationRequest, Auth);
}

IMPLEMENT_MODULE(FShowdownEditorModule, ShowdownEditor);
#undef LOCTEXT_NAMESPACE