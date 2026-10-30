"""Vega: run Echo-S's baseline light spawn alone (the v1.8 runner exits on the cue check before the lights)."""
import importlib.util, os, unreal
ROOT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Scripts', 'EchoS', 'graybox_4C')
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Maps/L_Slice_Graybox')
s = importlib.util.spec_from_file_location('spawn_graybox_lights_ue', os.path.join(ROOT, 'spawn_graybox_lights_ue.py')); m = importlib.util.module_from_spec(s); s.loader.exec_module(m)
spawned = m.main(path='baseline', state='Normal', parent='Corvette4C', on_existing='overwrite')
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
n = sum(1 for a in eas.get_all_level_actors() if a.actor_has_tag('GB4C'))
unreal.log('[VEGA_LIGHTS] spawned %d, GB4C-tagged actors in level %d, shadow casters %d' % (len(spawned), n,
    sum(1 for a in eas.get_all_level_actors() if a.actor_has_tag('GB4C') and a.get_component_by_class(unreal.LightComponent) and a.get_component_by_class(unreal.LightComponent).get_editor_property('cast_shadows'))))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
