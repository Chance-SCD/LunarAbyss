using UnrealBuildTool;

public class LunarAbyssTarget : TargetRules
{
	public LunarAbyssTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LunarAbyss");
	}
}
