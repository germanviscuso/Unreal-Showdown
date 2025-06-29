#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Provider/Types/ImageTypes.h"
#include "Provider/Types/CommonTypes.h"
#include "Interfaces/IHttpRequest.h"

class FUICommandList;
class UOpenAIProvider;

//DECLARE_LOG_CATEGORY_EXTERN(LogShowdownEditor, Log, All);

class FShowdownEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** This is called by our Blueprint Library to kick off the main process */
    void ExecuteCaptureAndEdit(const FString& Prompt);

private:
    void OnCaptureScenePressed();
    void SendImageToOpenAI(const FString& ImagePath, const FString& Prompt);

    /** Holds the path to the screenshot while the UI is open. */
    FString CachedScreenshotPath;

    void OnImageEditSuccess(const FImageEditResponse& Response, const FOpenAIResponseMetadata& Meta);
    void OnImageEditError(const FString& URL, const FString& Content);
    void OnImageDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    TSharedPtr<FUICommandList> PluginCommands;

    UPROPERTY()
    TObjectPtr<UOpenAIProvider> OpenAIProvider;
};