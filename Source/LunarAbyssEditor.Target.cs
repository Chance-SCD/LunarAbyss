using UnrealBuildTool;

public class LunarAbyssEditorTarget : TargetRules
{
	public LunarAbyssEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LunarAbyss");
	}
}
