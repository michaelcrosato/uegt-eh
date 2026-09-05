using UnrealBuildTool;
using System.Collections.Generic;

public class AfterlightTarget : TargetRules
{
    public AfterlightTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Afterlight");
    }
}
