// Source/ShowdownEditor/Public/ShowdownEditor.h
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Provider/Types/ImageTypes.h"
#include "Provider/Types/CommonTypes.h"

// Required for FHttpRequestPtr, FHttpResponsePtr
#include "Interfaces/IHttpRequest.h"

class FUICommandList;
class UOpenAIProvider;

class FShowdownEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void OnCaptureScenePressed();

    void SendImageToOpenAI(const FString& ImagePath);
    void OnImageVariationSuccess(const FImageVariationResponse& Response, const FOpenAIResponseMetadata& Meta);
    void OnImageVariationError(const FString& URL, const FString& Content);

    // New function to handle the downloaded image data
    void OnImageDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    TSharedPtr<FUICommandList> PluginCommands;

    UPROPERTY()
    TObjectPtr<UOpenAIProvider> OpenAIProvider;
};