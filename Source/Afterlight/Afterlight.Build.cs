using UnrealBuildTool;

public class Afterlight : ModuleRules
{
    public Afterlight(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore", "RHI", "RenderCore", "Slate", "SlateCore", "ApplicationCore",
            "DLSSBlueprint", "StreamlineDLSSGBlueprint", "StreamlineReflexBlueprint",
            "Json", "JsonUtilities"
        });
    }
}
