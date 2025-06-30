#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShowdownEditorBlueprintLibrary.generated.h"

UCLASS()
class UShowdownEditorBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Finds the running ShowdownEditor module and tells it to process the cached screenshot with the given prompt.
     * This is the node you will call from your BP_EditorUI widget.
    */
    UFUNCTION(BlueprintCallable, Category = "Showdown Editor Tools")
    static void TriggerSceneEditWithPrompt(const FString& Prompt);
    /**
    * Loads a PNG or JPG from a full file path and creates a transient UTexture2D object from it.
    * @param FilePath The full path to the image file on disk.
    * @return A new UTexture2D object, or nullptr if the load failed.
   */
    UFUNCTION(BlueprintCallable, Category = "Showdown Editor Tools")
    static UTexture2D* LoadTextureFromFile(const FString& FilePath);
};