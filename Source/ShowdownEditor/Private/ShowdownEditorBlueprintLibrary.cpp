#include "ShowdownEditorBlueprintLibrary.h"
#include "ShowdownEditor.h"
#include "Modules/ModuleManager.h"
// Add these to the top of ShowdownEditorBlueprintLibrary.cpp
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"

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

// Add this entire function to the bottom of ShowdownEditorBlueprintLibrary.cpp

UTexture2D* UShowdownEditorBlueprintLibrary::LoadTextureFromFile(const FString& FilePath)
{
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("LoadTextureFromFile: File not found at path: %s"), *FilePath);
        return nullptr;
    }

    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("LoadTextureFromFile: Failed to load file data from: %s"), *FilePath);
        return nullptr;
    }

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
    if (ImageFormat == EImageFormat::Invalid)
    {
        UE_LOG(LogTemp, Error, TEXT("LoadTextureFromFile: Could not determine image format for file: %s"), *FilePath);
        return nullptr;
    }

    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
    if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        UE_LOG(LogTemp, Error, TEXT("LoadTextureFromFile: Failed to create or set image wrapper for file: %s"), *FilePath);
        return nullptr;
    }

    TArray64<uint8> UncompressedBGRA;
    if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA))
    {
        UE_LOG(LogTemp, Error, TEXT("LoadTextureFromFile: Failed to decompress image file: %s"), *FilePath);
        return nullptr;
    }

    FString TextureName = FPaths::GetBaseFilename(FilePath);
    UTexture2D* NewTexture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8, *TextureName);

    if (!NewTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("LoadTextureFromFile: Failed to create transient texture for file: %s"), *FilePath);
        return nullptr;
    }

    void* TextureData = NewTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(TextureData, UncompressedBGRA.GetData(), UncompressedBGRA.Num());
    NewTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
    NewTexture->UpdateResource();

    return NewTexture;
}