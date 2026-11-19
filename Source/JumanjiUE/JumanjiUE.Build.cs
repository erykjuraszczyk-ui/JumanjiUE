using UnrealBuildTool;

public class JumanjiUE : ModuleRules
{
    public JumanjiUE(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreOnline",
                "CoreUObject",
                "Engine",
                "InputCore",
                "EnhancedInput",
                "GameplayAbilities",
                "GameplayTags",
                "GameplayTasks",
                "UMG",
                "AIModule",
                "NavigationSystem",
                "Niagara",
                "HTTP",
                "Json",
                "JsonUtilities"
            }
        );
    }
}
