#pragma once

#include "CoreMinimal.h"
#include "Editor/Blutility/Classes/EditorUtilityWidget.h"
#include "ShowdownWidgetBase.generated.h"

UCLASS()
class UShowdownWidgetBase : public UEditorUtilityWidget
{
    GENERATED_BODY()

public:
    // This is a C++ function that we can implement visually in our Blueprint's Event Graph.
    // C++ will call this function when the screenshot is taken.
    UFUNCTION(BlueprintImplementableEvent, Category = "Showdown Widget Events")
    void OnSetOriginalImage(const FString& FilePath);

    // C++ will call this function when the AI image has been downloaded.
    UFUNCTION(BlueprintImplementableEvent, Category = "Showdown Widget Events")
    void OnSetGeneratedImage(const FString& FilePath);
};