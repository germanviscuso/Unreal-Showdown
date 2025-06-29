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
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"

DEFINE_LOG_CATEGORY_STATIC(LogShowdownEditor, Log, All);
#define LOCTEXT_NAMESPACE "FShowdownEditorModule"

// Add this helper function to your FShowdownEditorModule class in the .h or .cpp file
// It returns the path to the new, masked image, or an empty string on failure.
FString CreateMaskedImage(const FString& OriginalImagePath)
{
    // --- 1. Load the original screenshot file into a byte array ---
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *OriginalImagePath))
    {
        UE_LOG(LogShowdownEditor, Error, TEXT("Failed to load original image file: %s"), *OriginalImagePath);
        return FString();
    }

    // --- 2. Use the Image Wrapper to decode the PNG into raw color data ---
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

    if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        TArray64<uint8> RawColorDataBytes;
        // Use ERGBFormat::BGRA because that's what Unreal's FColor expects.
        if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawColorDataBytes))
        {
            const int32 Width = ImageWrapper->GetWidth();
            const int32 Height = ImageWrapper->GetHeight();

            // Add this after successfully getting RawColorDataBytes
            FColor* RawColorData = reinterpret_cast<FColor*>(RawColorDataBytes.GetData());

            // --- 3. This is the core logic: Create a transparent area (a mask) ---
            // For this example, we'll erase a 256x256 square in the center of the image.
            const int32 MaskSize = 1024;
            const int32 StartX = (Width / 2) - (MaskSize / 2);
            const int32 StartY = (Height / 2) - (MaskSize / 2);

            // ...then use RawColorData in your masking loop:
            for (int32 y = StartY; y < StartY + MaskSize; ++y)
            {
                for (int32 x = StartX; x < StartX + MaskSize; ++x)
                {
                    // Check bounds just in case
                    if (x >= 0 && x < Width && y >= 0 && y < Height)
                    {
                        // Set the Alpha channel to 0 (fully transparent)
                        RawColorData[y * Width + x].A = 0;
                    }
                }
            }

            // --- 4. Compress the modified raw data back into a PNG byte array ---
            TArray<uint8> NewFileData;
            ImageWrapper->SetRaw(RawColorData, RawColorDataBytes.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8);
            NewFileData = ImageWrapper->GetCompressed(100);

            // --- 5. Save the new masked image to a file ---
            FString Directory = FPaths::ProjectSavedDir() + TEXT("Screenshots/");
            FString NewFilename = FString::Printf(TEXT("MaskedCapture_%s.png"), *FDateTime::Now().ToString());
            FString NewFilePath = FPaths::ConvertRelativePathToFull(Directory + NewFilename);

            if (FFileHelper::SaveArrayToFile(NewFileData, *NewFilePath))
            {
                UE_LOG(LogShowdownEditor, Log, TEXT("Successfully created masked image at: %s"), *NewFilePath);
                return NewFilePath; // Return the path to the NEW image
            }
            else
            {
                UE_LOG(LogShowdownEditor, Error, TEXT("Failed to save masked image file."));
            }
        }
    }

    return FString(); // Return empty on failure
}

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

    // This takes the screenshot and saves it. It is an asynchronous operation.
    FScreenshotRequest::RequestScreenshot(FilePath, false, false);

    UE_LOG(LogShowdownEditor, Log, TEXT("Scene capture requested. Will be saved to: %s. Processing and sending to OpenAI in 2 seconds..."), *FilePath);

    FTimerHandle TimerHandle;
    FTimerDelegate TimerDelegate;

    // We use a lambda to capture the original file path
    TimerDelegate.BindLambda([this, FilePath]()
        {
            // First, create the masked version of the screenshot
            // The FPaths::FileExists is important because the screenshot might not be saved yet.
            if (FPaths::FileExists(FilePath))
            {
                const FString MaskedImagePath = CreateMaskedImage(FilePath);

                // Only proceed if the masked image was created successfully
                if (!MaskedImagePath.IsEmpty())
                {
                    // Now, send the *masked* image to OpenAI
                    SendImageToOpenAI(MaskedImagePath);
                }
                else
                {
                    UE_LOG(LogShowdownEditor, Error, TEXT("Could not create a masked image from the screenshot. Aborting OpenAI request."));
                }
            }
            else
            {
                UE_LOG(LogShowdownEditor, Error, TEXT("Screenshot file was not found after 2 seconds. Aborting OpenAI request."));
            }
        });

    // We wait 2 seconds to give the engine time to write the screenshot file to disk.
    // You might need to make this longer for very high-resolution screenshots.
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
        FString Filename = FString::Printf(TEXT("SceneEdit_%s.png"), *FDateTime::Now().ToString());
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

    // We now use FOpenAIImageEdit and set the prompt.
    FOpenAIImageEdit ImageEditRequest;
    ImageEditRequest.Image.Add(ImagePath); // The path to the image file
    ImageEditRequest.Prompt = TEXT("A moody, post-apocalyptic version of this scene, with rubble and overgrown vines. Buildings must have vines. Sky should be cloudy and stormy"); // prompt
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