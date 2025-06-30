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
#include "Engine/World.h"
#include "TimerManager.h"
#include "HttpModule.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include <ShowdownWidgetBase.h>

// The DEFINE macro goes here and ONLY here.
//DEFINE_LOG_CATEGORY_STATIC(LogShowdownEditor, Log, All);

#define LOCTEXT_NAMESPACE "FShowdownEditorModule"

// NOTE: I am moving your CreateMaskedImage function into this file, as it doesn't belong to a class.
// If it is a member function of FShowdownEditorModule, its definition should start with FShowdownEditorModule::
FString CreateMaskedImage(const FString& OriginalImagePath)
{
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *OriginalImagePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load original image file: %s"), *OriginalImagePath);
        return FString();
    }

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

    if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        TArray64<uint8> RawColorDataBytes;
        if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawColorDataBytes))
        {
            const int32 Width = ImageWrapper->GetWidth();
            const int32 Height = ImageWrapper->GetHeight();
            FColor* RawColorData = reinterpret_cast<FColor*>(RawColorDataBytes.GetData());

            const int32 MaskSize = 1024;
            const int32 StartX = (Width / 2) - (MaskSize / 2);
            const int32 StartY = (Height / 2) - (MaskSize / 2);

            for (int32 y = StartY; y < StartY + MaskSize; ++y)
            {
                for (int32 x = StartX; x < StartX + MaskSize; ++x)
                {
                    if (x >= 0 && x < Width && y >= 0 && y < Height)
                    {
                        RawColorData[y * Width + x].A = 0;
                    }
                }
            }

            TArray<uint8> NewFileData;
            // The first parameter for SetRaw should be a pointer to the data buffer
            ImageWrapper->SetRaw(RawColorDataBytes.GetData(), RawColorDataBytes.Num(), Width, Height, ERGBFormat::BGRA, 8);
            NewFileData = ImageWrapper->GetCompressed(100);

            FString Directory = FPaths::ProjectSavedDir() + TEXT("Screenshots/");
            FString NewFilename = FString::Printf(TEXT("MaskedCapture_%s.png"), *FDateTime::Now().ToString());
            FString NewFilePath = FPaths::ConvertRelativePathToFull(Directory + NewFilename);

            if (FFileHelper::SaveArrayToFile(NewFileData, *NewFilePath))
            {
                UE_LOG(LogTemp, Log, TEXT("Successfully created masked image at: %s"), *NewFilePath);
                return NewFilePath;
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to save masked image file."));
            }
        }
    }
    return FString();
}

// This helper function finds our active widget tab and casts it to our C++ base class
// Replace your existing GetActiveShowdownWidget function with this one.

UShowdownWidgetBase* GetActiveShowdownWidget()
{
    UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
    if (!EditorUtilitySubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("GetActiveShowdownWidget: Could not get the Editor Utility Subsystem."));
        return nullptr;
    }

    const FString WidgetBlueprintPath = TEXT("/Game/DreamLite/BP_EditorUI.BP_EditorUI");
    UEditorUtilityWidgetBlueprint* EditorWidgetBlueprint = LoadObject<UEditorUtilityWidgetBlueprint>(nullptr, *WidgetBlueprintPath);

    if (EditorWidgetBlueprint)
    {
        UEditorUtilityWidget* FoundWidget = EditorUtilitySubsystem->FindUtilityWidgetFromBlueprint(EditorWidgetBlueprint);

        if (FoundWidget)
        {
            return Cast<UShowdownWidgetBase>(FoundWidget);
        }
    }

    return nullptr;
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

//void FShowdownEditorModule::OnCaptureScenePressed()
//{
//    FString Directory = FPaths::ProjectSavedDir() + TEXT("Screenshots/");
//    FString Filename = FString::Printf(TEXT("SceneCapture_%s.png"), *FDateTime::Now().ToString());
//    CachedScreenshotPath = FPaths::ConvertRelativePathToFull(Directory + Filename);
//
//    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
//    PlatformFile.CreateDirectoryTree(*Directory);
//
//    FScreenshotRequest::RequestScreenshot(CachedScreenshotPath, false, false);
//    UE_LOG(LogTemp, Log, TEXT("Screenshot requested, path cached: %s"), *CachedScreenshotPath);
//
//    const FString WidgetBlueprintPath = TEXT("/Game/DreamLite/BP_EditorUI.BP_EditorUI");
//    UObject* WidgetBlueprintObject = LoadObject<UObject>(nullptr, *WidgetBlueprintPath);
//    UEditorUtilityWidgetBlueprint* EditorWidgetBlueprint = Cast<UEditorUtilityWidgetBlueprint>(WidgetBlueprintObject);
//
//    if (EditorWidgetBlueprint)
//    {
//        UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
//        if (EditorUtilitySubsystem)
//        {
//            EditorUtilitySubsystem->SpawnAndRegisterTab(EditorWidgetBlueprint);
//        }
//    }
//    else
//    {
//        UE_LOG(LogTemp, Error, TEXT("Failed to load Editor Utility Widget at path: %s"), *WidgetBlueprintPath);
//    }
//}

void FShowdownEditorModule::OnCaptureScenePressed()
{
    // --- Immediately start the screenshot process ---
    FString Directory = FPaths::ProjectSavedDir() + TEXT("Screenshots/");
    FString Filename = FString::Printf(TEXT("SceneCapture_%s.png"), *FDateTime::Now().ToString());
    CachedScreenshotPath = FPaths::ConvertRelativePathToFull(Directory + Filename);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*Directory);

    FScreenshotRequest::RequestScreenshot(CachedScreenshotPath, false, false);
    UE_LOG(LogTemp, Log, TEXT("Screenshot requested, path cached: %s"), *CachedScreenshotPath);

    // --- NEW DEBUGGING LOGIC ---
    UE_LOG(LogTemp, Log, TEXT("--- Starting Widget Load Debug ---"));

    const FString WidgetBlueprintPath = TEXT("/Game/DreamLite/BP_EditorUI.BP_EditorUI");
    UE_LOG(LogTemp, Log, TEXT("1. Attempting to load UObject at path: %s"), *WidgetBlueprintPath);

    // STEP 1: Try to load the object from disk.
    UObject* WidgetBlueprintObject = LoadObject<UObject>(nullptr, *WidgetBlueprintPath);

    if (!WidgetBlueprintObject)
    {
        UE_LOG(LogTemp, Error, TEXT("2. DEBUG FAILED: LoadObject returned NULL. The engine cannot find the asset at this path. This could be a file corruption or a very deep engine bug."));
        UE_LOG(LogTemp, Log, TEXT("--- Widget Load Debug Finished ---"));
        return; // Stop here
    }

    UE_LOG(LogTemp, Log, TEXT("2. DEBUG SUCCESS: LoadObject found an asset named '%s'."), *WidgetBlueprintObject->GetName());
    UE_LOG(LogTemp, Log, TEXT("3. The found asset's C++ class is: '%s'"), *WidgetBlueprintObject->GetClass()->GetName());

    // STEP 2: Try to cast the found object to the type we need.
    UEditorUtilityWidgetBlueprint* EditorWidgetBlueprint = Cast<UEditorUtilityWidgetBlueprint>(WidgetBlueprintObject);

    if (!EditorWidgetBlueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("4. DEBUG FAILED: Cast to UEditorUtilityWidgetBlueprint failed. The asset you created is not the correct type, even if its parent is set correctly in the editor."));
        UE_LOG(LogTemp, Log, TEXT("--- Widget Load Debug Finished ---"));
        return; // Stop here
    }

    UE_LOG(LogTemp, Log, TEXT("4. DEBUG SUCCESS: Cast to UEditorUtilityWidgetBlueprint was successful."));

    // STEP 3: Try to get the subsystem and spawn the tab.
    UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
    if (EditorUtilitySubsystem)
    {
        UE_LOG(LogTemp, Log, TEXT("5. Spawning tab now..."));
        EditorUtilitySubsystem->SpawnAndRegisterTab(EditorWidgetBlueprint);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("5. DEBUG FAILED: Could not get EditorUtilitySubsystem."));
    }

    UE_LOG(LogTemp, Log, TEXT("--- Widget Load Debug Finished ---"));
}

void FShowdownEditorModule::ExecuteCaptureAndEdit(const FString& Prompt)
{
    if (CachedScreenshotPath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Execute called but there is no valid cached screenshot path."));
        return;
    }

    FString ScreenshotToProcess = CachedScreenshotPath;
    CachedScreenshotPath.Empty();

    // --- START NEW DEBUG CODE ---
    if (UShowdownWidgetBase* ActiveWidget = GetActiveShowdownWidget())
    {
        UE_LOG(LogTemp, Log, TEXT("C++ DEBUG: Found the active widget! Calling OnSetOriginalImage..."));
        ActiveWidget->OnSetOriginalImage(ScreenshotToProcess);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("C++ DEBUG: FAILED to find the active widget when trying to set original image."));
    }
    // --- END NEW DEBUG CODE ---

    FTimerHandle TimerHandle;
    FTimerDelegate TimerDelegate;

    TimerDelegate.BindLambda([this, ScreenshotToProcess, Prompt]()
        {
            if (FPaths::FileExists(ScreenshotToProcess))
            {
                const FString MaskedImagePath = CreateMaskedImage(ScreenshotToProcess);
                if (!MaskedImagePath.IsEmpty())
                {
                    SendImageToOpenAI(MaskedImagePath, Prompt);
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Screenshot file was not found after waiting."));
            }
        });

    if (GEditor->GetEditorWorldContext().World())
    {
        GEditor->GetEditorWorldContext().World()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 1.0f, false);
    }
}

void FShowdownEditorModule::OnImageDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response.IsValid())
    {
        TArray<uint8> ImageData = Response->GetContent();
        FString Directory = FPaths::ProjectSavedDir() + TEXT("Screenshots/");
        FString Filename = FString::Printf(TEXT("SceneEdit_%s.png"), *FDateTime::Now().ToString());
        FString FilePath = FPaths::ConvertRelativePathToFull(Directory + Filename);

        if (FFileHelper::SaveArrayToFile(ImageData, *FilePath))
        {
            UE_LOG(LogTemp, Warning, TEXT("New image edit successfully downloaded and saved to: %s"), *FilePath);

            // --- START NEW DEBUG CODE ---
            if (UShowdownWidgetBase* ActiveWidget = GetActiveShowdownWidget())
            {
                UE_LOG(LogTemp, Log, TEXT("C++ DEBUG: Found the active widget! Calling OnSetGeneratedImage..."));
                ActiveWidget->OnSetGeneratedImage(FilePath);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("C++ DEBUG: FAILED to find the active widget when trying to set generated image."));
            }
            // --- END NEW DEBUG CODE ---
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to save downloaded image."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to download image from OpenAI URL."));
    }
}

void FShowdownEditorModule::SendImageToOpenAI(const FString& ImagePath, const FString& Prompt)
{
    if (!FPaths::FileExists(ImagePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Screenshot file does not exist at path: %s. Cannot send to OpenAI."), *ImagePath);
        return;
    }

    const FString AuthIniPath = FPaths::ProjectDir() + TEXT("OpenAIAuth.ini");
    FOpenAIAuth Auth = UOpenAIFuncLib::LoadAPITokensFromFile(AuthIniPath);

    if (Auth.APIKey.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load OpenAI API key from OpenAIAuth.ini"));
        return;
    }

    FOpenAIImageEdit ImageEditRequest;
    ImageEditRequest.Image.Add(ImagePath);
    ImageEditRequest.Prompt = Prompt;
    ImageEditRequest.N = 1;
    ImageEditRequest.Size = UOpenAIFuncLib::OpenAIImageSizeDalle2ToString(EImageSizeDalle2::Size_1024x1024);
    ImageEditRequest.Response_Format = UOpenAIFuncLib::OpenAIImageFormatToString(EOpenAIImageFormat::URL);

    OpenAIProvider->OnCreateImageEditCompleted().AddRaw(this, &FShowdownEditorModule::OnImageEditSuccess);
    OpenAIProvider->OnRequestError().AddRaw(this, &FShowdownEditorModule::OnImageEditError);

    UE_LOG(LogTemp, Log, TEXT("Sending image EDIT request to OpenAI for file: %s"), *ImagePath);
    OpenAIProvider->CreateImageEdit(ImageEditRequest, Auth);
}

void FShowdownEditorModule::OnImageEditSuccess(const FImageEditResponse& Response, const FOpenAIResponseMetadata& Meta)
{
    OpenAIProvider->OnCreateImageEditCompleted().RemoveAll(this);
    OpenAIProvider->OnRequestError().RemoveAll(this);

    if (Response.Data.Num() > 0)
    {
        const FString URL = Response.Data[0].URL;
        UE_LOG(LogTemp, Warning, TEXT("OpenAI Image Edit SUCCESS! URL: %s"), *URL);

        UE_LOG(LogTemp, Log, TEXT("Downloading new image from URL..."));
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
        HttpRequest->OnProcessRequestComplete().BindRaw(this, &FShowdownEditorModule::OnImageDownloaded);
        HttpRequest->SetURL(URL);
        HttpRequest->SetVerb(TEXT("GET"));
        HttpRequest->ProcessRequest();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("OpenAI Image Edit request succeeded but returned no data."));
    }
}

void FShowdownEditorModule::OnImageEditError(const FString& URL, const FString& Content)
{
    OpenAIProvider->OnCreateImageEditCompleted().RemoveAll(this);
    OpenAIProvider->OnRequestError().RemoveAll(this);
    UE_LOG(LogTemp, Error, TEXT("OpenAI Image Edit FAILED. URL: %s, Error: %s"), *URL, *Content);
}

IMPLEMENT_MODULE(FShowdownEditorModule, ShowdownEditor);

#undef LOCTEXT_NAMESPACE