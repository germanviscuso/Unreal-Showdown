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
#include "HttpModule.h"

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

// Add this new function to your ShowdownEditor.cpp file
void FShowdownEditorModule::OnImageDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response.IsValid())
    {
        // Get the raw image data from the response
        TArray<uint8> ImageData = Response->GetContent();

        // Create a new file path for the variation
        FString Directory = FPaths::ProjectSavedDir() + TEXT("Screenshots/");
        FString Filename = FString::Printf(TEXT("SceneVariation_%s.png"), *FDateTime::Now().ToString());
        FString FilePath = FPaths::ConvertRelativePathToFull(Directory + Filename);

        // Save the downloaded data to the new file
        if (FFileHelper::SaveArrayToFile(ImageData, *FilePath))
        {
            UE_LOG(LogShowdownEditor, Warning, TEXT("New image variation successfully downloaded and saved to: %s"), *FilePath);
        }
        else
        {
            UE_LOG(LogShowdownEditor, Error, TEXT("Failed to save downloaded image."));
        }
    }
    else
    {
        UE_LOG(LogShowdownEditor, Error, TEXT("Failed to download image from OpenAI URL."));
    }
}

// Replace the SendImageToOpenAI function in ShowdownEditor.cpp

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

    // --- THIS IS THE MAIN CHANGE ---
    // We now use FOpenAIImageEdit and set the prompt.
    FOpenAIImageEdit ImageEditRequest;
    ImageEditRequest.Image.Add(ImagePath); // The path to the image file
    ImageEditRequest.Prompt = TEXT("A moody, post-apocalyptic version of this scene, with rubble and overgrown vines."); // Our new prompt!
    ImageEditRequest.N = 1;
    ImageEditRequest.Size = UOpenAIFuncLib::OpenAIImageSizeDalle2ToString(EImageSizeDalle2::Size_1024x1024);
    ImageEditRequest.Response_Format = UOpenAIFuncLib::OpenAIImageFormatToString(EOpenAIImageFormat::URL);

    // Bind the new success and error handlers
    OpenAIProvider->OnCreateImageEditCompleted().AddRaw(this, &FShowdownEditorModule::OnImageEditSuccess);
    OpenAIProvider->OnRequestError().AddRaw(this, &FShowdownEditorModule::OnImageEditError);

    UE_LOG(LogShowdownEditor, Log, TEXT("Sending image EDIT request to OpenAI for file: %s"), *ImagePath);
    // Call the correct API function
    OpenAIProvider->CreateImageEdit(ImageEditRequest, Auth);
}

// Replace the OnImageVariationSuccess function with this new version
void FShowdownEditorModule::OnImageEditSuccess(const FImageEditResponse& Response, const FOpenAIResponseMetadata& Meta)
{
    OpenAIProvider->OnCreateImageEditCompleted().RemoveAll(this);
    OpenAIProvider->OnRequestError().RemoveAll(this);

    if (Response.Data.Num() > 0)
    {
        const FString URL = Response.Data[0].URL;
        UE_LOG(LogShowdownEditor, Warning, TEXT("OpenAI Image Edit SUCCESS! URL: %s"), *URL);

        UE_LOG(LogShowdownEditor, Log, TEXT("Downloading new image from URL..."));
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
        HttpRequest->OnProcessRequestComplete().BindRaw(this, &FShowdownEditorModule::OnImageDownloaded);
        HttpRequest->SetURL(URL);
        HttpRequest->SetVerb(TEXT("GET"));
        HttpRequest->ProcessRequest();
    }
    else
    {
        UE_LOG(LogShowdownEditor, Error, TEXT("OpenAI Image Edit request succeeded but returned no data."));
    }
}

// Replace the OnImageVariationError function with this new version
void FShowdownEditorModule::OnImageEditError(const FString& URL, const FString& Content)
{
    OpenAIProvider->OnCreateImageEditCompleted().RemoveAll(this);
    OpenAIProvider->OnRequestError().RemoveAll(this);
    UE_LOG(LogShowdownEditor, Error, TEXT("OpenAI Image Edit FAILED. URL: %s, Error: %s"), *URL, *Content);
}

IMPLEMENT_MODULE(FShowdownEditorModule, ShowdownEditor);
#undef LOCTEXT_NAMESPACE