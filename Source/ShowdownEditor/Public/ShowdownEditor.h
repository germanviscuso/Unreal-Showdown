#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Provider/Types/ImageTypes.h"
#include "Provider/Types/CommonTypes.h"

class FUICommandList;
class UOpenAIProvider;

class FShowdownEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void OnCaptureScenePressed();

    // Corrected function signature
    void OnImageVariationSuccess(const FImageVariationResponse& Response, const FOpenAIResponseMetadata& Meta);
    void OnImageVariationError(const FString& URL, const FString& Content);

    void SendImageToOpenAI(const FString& ImagePath);

    TSharedPtr<FUICommandList> PluginCommands;

    UPROPERTY()
    TObjectPtr<UOpenAIProvider> OpenAIProvider;
};