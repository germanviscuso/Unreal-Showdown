// Source/ShowdownEditor/Public/ShowdownEditor.h

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Provider/Types/ImageTypes.h"
#include "Provider/Types/CommonTypes.h"
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

    // Changed to handle the correct response type
    void OnImageEditSuccess(const FImageEditResponse& Response, const FOpenAIResponseMetadata& Meta);
    void OnImageEditError(const FString& URL, const FString& Content);

    void OnImageDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    TSharedPtr<FUICommandList> PluginCommands;

    UPROPERTY()
    TObjectPtr<UOpenAIProvider> OpenAIProvider;
};