using UnrealBuildTool;

public class Afterlight : ModuleRules
{
    public Afterlight(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore", "RHI", "RenderCore",
            "DLSSBlueprint", "StreamlineDLSSGBlueprint", "StreamlineReflexBlueprint",
            "Json", "JsonUtilities"
        });
    }
}
