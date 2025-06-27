#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FShowdownEditorCommands : public TCommands<FShowdownEditorCommands>
{
public:
    FShowdownEditorCommands()
        : TCommands<FShowdownEditorCommands>(
            TEXT("ShowdownEditor"), // Context name
            NSLOCTEXT("Contexts", "ShowdownEditor", "Showdown Editor"), // Display name
            NAME_None,
            FAppStyle::GetAppStyleSetName()
        )
    {
    }

    // TCommands<> interface
    virtual void RegisterCommands() override;

public:
    TSharedPtr<FUICommandInfo> CaptureScene;
};