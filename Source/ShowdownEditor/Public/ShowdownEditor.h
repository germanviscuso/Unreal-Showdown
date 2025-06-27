#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FUICommandList;

class FShowdownEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void OnCaptureScenePressed();
    TSharedPtr<FUICommandList> PluginCommands;
};