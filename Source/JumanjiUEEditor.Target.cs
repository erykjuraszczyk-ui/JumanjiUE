using UnrealBuildTool;

public class JumanjiUEEditorTarget : TargetRules
{
    public JumanjiUEEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("JumanjiUE");
    }
}
