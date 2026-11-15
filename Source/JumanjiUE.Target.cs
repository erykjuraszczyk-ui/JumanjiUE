using UnrealBuildTool;

public class JumanjiUETarget : TargetRules
{
    public JumanjiUETarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("JumanjiUE");
    }
}
