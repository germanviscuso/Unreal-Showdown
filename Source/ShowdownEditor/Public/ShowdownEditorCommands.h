#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FShowdownEditorCommands : public TCommands<FShowdownEditorCommands>
{
public:
    FShowdownEditorCommands()
        : TCommands<FShowdownEditorCommands>(
            TEXT("ShowdownEditor"),
            NSLOCTEXT("Contexts", "ShowdownEditor", "Showdown Editor"),
            NAME_None,
            FAppStyle::GetAppStyleSetName()
        )
    {
    }

    virtual void RegisterCommands() override;

public:
    TSharedPtr<FUICommandInfo> CaptureScene;
};