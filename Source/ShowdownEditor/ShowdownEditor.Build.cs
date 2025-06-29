// ShowdownEditor.Build.cs

using UnrealBuildTool;

public class ShowdownEditor : ModuleRules
{
    public ShowdownEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "InputCore",
                "UnrealEd", // Important for editor access
                "LevelEditor", // Important for menu extensions
                "ShowdownQuest",
                "OpenAI",
                "HTTP",
                "ImageWrapper",
                "RenderCore",
                "UMG",
                "UMGEditor",
                "Blutility",
                "EditorSubsystem"
            }
        );
    }
}