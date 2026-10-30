"""UE 5.8.3 Editor Utility (Python) - spawns the Corvette 4C gray-box lights from C4C_GrayboxLights.json.
Run in the editor:  Tools > Execute Python Script...  (or `py "<path>/spawn_graybox_lights_ue.py"` in the Output Log)
Requires: Python Editor Script Plugin enabled. IES assets imported to IES_FOLDER with the same names as in audio_vfx/IES/.
Options (edit below or call main(...) from another script):
  PATH        'baseline' (non-RT: shadows only where shadow_baseline, MegaLights off)  or  'high' (MegaLights + HWRT: shadow_high)
  STATE       preview a state from C4C_LightStatePresets.json: Normal | RedAlert | Damaged | Breach | PowerLoss | ZeroG
  PARENT      optional actor label; spawned lights are attached to it (keep (0,0,0) = ship origin)
Every actor gets tags: GB4C, <group>, <section>, <room>, <fixture>, <ref_socket> (hangar lights: body-mesh socket they were placed against), and the light id as its label. Re-running deletes the old GB4C actors first.
Runtime state switching belongs to the ship presentation component; this script only places lights and previews presets in the editor."""
import json, os, unreal

HERE = os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals() else ''
JSON_PATH = os.path.join(HERE, 'C4C_GrayboxLights.json')
PRESET_PATH = os.path.join(HERE, 'C4C_LightStatePresets.json')
IES_FOLDER = '/Game/Ship/Lighting/IES'
OUTLINER_FOLDER = 'Lighting/Graybox_4C'
PATH = 'baseline'
STATE = 'Normal'
PARENT = ''

# UE handoff (art/export_UE/IMPORT_README.md): import the Body + parts route, not the combined reference FBX.
EXPORT_UE = '/workspace/space_rpg/art/export_UE'
FBX = {
    'body': f'{EXPORT_UE}/Corvette4C/SM_Corvette4C_Body.fbx',             # static body (59 sockets, 140 UCX; not read here)
    'reference': f'{EXPORT_UE}/Corvette4C/SM_Corvette4C.fbx',             # reference only (never Combine Meshes ON)
    'hangar_door_l': f'{EXPORT_UE}/Corvette4C/parts/SM_Corvette4C_HangarDoor_L.fbx',
    'hangar_door_r': f'{EXPORT_UE}/Corvette4C/parts/SM_Corvette4C_HangarDoor_R.fbx',
    'fighter_body': f'{EXPORT_UE}/FighterC/SM_FighterC_Body.fbx',
    'tube_bp2': f'{EXPORT_UE}/Corvette4C_BoardingTubes/SM_BoardingTube_BP2.fbx',
    'tube_bp3': f'{EXPORT_UE}/Corvette4C_BoardingTubes/SM_BoardingTube_BP3.fbx',
}
BODY_ASSET = '/Game/Ship/Corvette4C/SM_Corvette4C_Body'   # informational only (the light file is in ship space; attach to the ship actor)
# Body-mesh sockets the hangar lights were placed against (UE name = empty name minus SOCKET_), ship space in metres.
# Documentation only: this script does NOT read sockets from the mesh (StaticMesh socket access is unreliable headless).
REF_SOCKETS = {
    'Hangar': (6.675, 1.0, 9.667), 'Hangar_Spot_Fighter': (-3.825, 3.5, 9.667), 'Hangar_Spot_Dropship': (16.175, 2.25, 9.667),
    'Mount_HangarDoor_L': (-10.325, 14.6, 9.667), 'Mount_HangarDoor_R': (23.675, 14.6, 9.667), 'HangarLaunch_Threshold': (6.675, 16.5, 14.167),
}
# Interior lights come from levels/interior_sockets.csv (EmLight/Beacon) and the interior blockout rooms; they are not body-mesh sockets.

LOG_PREFIX = '[GB4C] '
REPORT = []
def log(msg, level='info'):
    """Single logging entry point (UE Output Log + in-memory report)."""
    REPORT.append(msg)
    if level == 'warning': unreal.log_warning(LOG_PREFIX + msg)
    elif level == 'error': unreal.log_error(LOG_PREFIX + msg)
    else: unreal.log(LOG_PREFIX + msg)

def is_unattended():
    try: cl = unreal.SystemLibrary.get_command_line().lower()
    except Exception: cl = ''
    return ('-unattended' in cl) or ('-nullrhi' in cl) or os.environ.get('GB4C_UNATTENDED') == '1'

# Baseline (non-RT, mid-range) profile. The same values ship in C4C_Baseline_DeviceProfile.ini.
BASELINE_CVARS = [
    'r.MegaLights.Allowed 0', 'r.MegaLights.EnableForProject 0', 'r.RayTracing.Enable 0', 'r.Lumen.HardwareRayTracing 0',
    'r.DynamicGlobalIlluminationMethod 1', 'r.ReflectionMethod 1', 'r.Shadow.Virtual.Enable 1',
    'r.Shadow.Virtual.MaxPhysicalPages 2048', 'r.LightFunctionAtlas 0',
]
HIGH_CVARS = ['r.MegaLights.Allowed 1', 'r.MegaLights.EnableForProject 1', 'r.RayTracing.Enable 1', 'r.Lumen.HardwareRayTracing 1', 'r.LightFunctionAtlas 1']
def apply_profile(path):
    # UE 5.8: r.MegaLights.Allow does not exist ("deferred - dummy variable"). Allowed = scalability/device-profile gate,
    # EnableForProject = project default (Project Settings > Rendering > Direct Lighting > MegaLights).
    for c in (HIGH_CVARS if path == 'high' else BASELINE_CVARS):
        unreal.SystemLibrary.execute_console_command(None, c)
    log(f'profile {path}: ' + ', '.join(HIGH_CVARS if path == 'high' else BASELINE_CVARS))

CLASSES = {'PointLight': unreal.PointLight, 'SpotLight': unreal.SpotLight, 'RectLight': unreal.RectLight}

def _hex(c):
    c = c.lstrip('#'); return unreal.Color(int(c[0:2], 16), int(c[2:4], 16), int(c[4:6], 16), 255)

def _set(obj, prop, val):
    try:
        obj.set_editor_property(prop, val); return True
    except Exception:
        return False

def clear():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    old = [a for a in sub.get_all_level_actors() if unreal.Name('GB4C') in a.tags]
    for a in old: sub.destroy_actor(a)
    log(f'removed {len(old)} old lights')

def apply_state(actors, state, presets, path):
    st = presets['states'][state]
    for a, L in actors:
        comp = a.get_editor_property('rect_light_component') if isinstance(a, unreal.RectLight) else a.get_editor_property('light_component')
        if st.get('scope') == 'HANGAR' and L['room'] != 'HANGAR': continue
        g = st.get('groups', {}).get(L['group'])
        mult = 1.0 if g is None else g.get('mult', 1.0)
        _set(comp, 'intensity', float(L['intensity_cd']) * mult)
        if g and g.get('kelvin_override'): _set(comp, 'use_temperature', True); _set(comp, 'temperature', float(g['kelvin_override']))
        if g and g.get('color_override'): _set(comp, 'light_color', _hex(g['color_override']))
        a.set_is_temporarily_hidden_in_editor(mult <= 0.0)

ON_EXISTING = 'overwrite'   # 'overwrite' = delete old GB4C lights and respawn; 'skip' = leave an already-lit level untouched

def main(path=PATH, state=STATE, parent=PARENT, on_existing=None):
    on_existing = on_existing or ON_EXISTING
    data = json.load(open(JSON_PATH)); presets = json.load(open(PRESET_PATH))
    sub0 = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = [a for a in sub0.get_all_level_actors() if unreal.Name('GB4C') in a.tags]
    if existing and on_existing == 'skip':
        log(f'{len(existing)} GB4C lights already in the level; on_existing=skip, nothing spawned'); return []
    clear(); apply_profile(path)
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    parent_actor = None
    if parent:
        parent_actor = next((a for a in sub.get_all_level_actors() if a.get_actor_label() == parent), None)
    spawned = []; missing_ies = set()
    with unreal.ScopedSlowTask(len(data['lights']), 'Spawning 4C gray-box lights') as task:
        if not is_unattended(): task.make_dialog(True)
        for L in data['lights']:
            if not is_unattended() and task.should_cancel(): break
            task.enter_progress_frame(1)
            loc = unreal.Vector(L['ue_x'], L['ue_y'], L['ue_z'])
            rot = unreal.Rotator(roll=0.0, pitch=float(L['ue_pitch']), yaw=float(L['ue_yaw']))
            a = sub.spawn_actor_from_class(CLASSES[L['ue_class']], loc, rot)
            a.set_actor_label(L['id']); a.set_folder_path(OUTLINER_FOLDER)
            a.tags = [unreal.Name(t) for t in ('GB4C', L['group'], L['section'], L['room'], L['fixture'], L.get('ref_socket') or 'NoSocket')]
            comp = a.get_editor_property('rect_light_component') if L['ue_class'] == 'RectLight' else a.get_editor_property('light_component')
            _set(comp, 'mobility', unreal.ComponentMobility.MOVABLE)
            _set(comp, 'intensity_units', unreal.LightUnits.CANDELAS)
            _set(comp, 'intensity', float(L['intensity_cd']))
            _set(comp, 'attenuation_radius', float(L['attenuation_cm']))
            if L['kelvin']:
                _set(comp, 'use_temperature', True); _set(comp, 'temperature', float(L['kelvin']))
            if L['color']:
                _set(comp, 'light_color', _hex(L['color']))
            if L['ies']:
                ies = unreal.load_asset(f"{IES_FOLDER}/{L['ies']}")
                if ies: _set(comp, 'ies_texture', ies); _set(comp, 'use_ies_brightness', False)
                else: missing_ies.add(L['ies'])
            if L['ue_class'] == 'SpotLight':
                _set(comp, 'inner_cone_angle', float(L['cone_inner'])); _set(comp, 'outer_cone_angle', float(L['cone_outer']))
            if L['ue_class'] == 'RectLight':
                _set(comp, 'source_width', float(L['rect_w_cm'])); _set(comp, 'source_height', float(L['rect_h_cm']))
            shadows = bool(L['shadow_high'] if path == 'high' else L['shadow_baseline'])
            _set(comp, 'cast_shadows', shadows)
            # MegaLights: per-light opt-in only matters when r.MegaLights is enabled (High setting)
            _set(comp, 'allow_mega_lights', path == 'high')
            if path != 'high' and shadows and L.get('max_draw_distance_cm'):
                _set(comp, 'max_draw_distance', float(L['max_draw_distance_cm'])); _set(comp, 'max_distance_fade_range', 300.0)
            _set(comp, 'volumetric_scattering_intensity', 0.6 if L['group'] in ('LG_Main', 'LG_Emergency') else 0.3)
            if parent_actor: a.attach_to_actor(parent_actor, unreal.Name(''), unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
            spawned.append((a, L))
    apply_state(spawned, state, presets, path)
    n_sh = sum(1 for _, L in spawned if (L['shadow_high'] if path == 'high' else L['shadow_baseline']))
    log(f'spawned {len(spawned)} lights, path={path}, state={state}, shadow casters={n_sh}')
    if missing_ies: log(f'IES not found in {IES_FOLDER}: {sorted(missing_ies)}', 'warning')
    if parent and not parent_actor: log(f'parent actor "{parent}" not found; lights left unattached in ship space', 'warning')
    # check step: count by group, verify shadow budget flag, verify IES assigned
    from collections import Counter
    log('by group: ' + str(dict(Counter(L['group'] for _, L in spawned))))
    if path == 'baseline' and n_sh != sum(1 for L in data['lights'] if L['shadow_baseline']): log('shadow caster count mismatch', 'error')
    return spawned

if __name__ == '__main__':
    main()
