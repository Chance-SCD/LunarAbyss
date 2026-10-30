"""UE 5.8.3 editor Python: import the gray-box 4C WAVs and build one SoundCue per row of C4C_SoundList.csv,
then CHECK that every cue graph really reaches its SoundWave(s). Replaces the cue half of add_lights_and_sounds.py.

v1.8.3: .uasset size test back (>= 5000 B) alongside the wave-link checks; report -> code/playtest_03/gb4c_audio_report.txt.
v1.8.2: all waves are imported, saved (save_loaded_assets), rescanned and reloaded before any cue is built, and each cue is
  saved and rescanned before its check, so a fresh project passes in one run. Random children are added via InsertChildNode (C++).
v1.8.1 (after playtest_03: 12/32 passed):
  * No CanCreateAsset/dialog path: existing cues are loaded and rebuilt in place; new ones via AssetTools.create_asset
    with SoundCueFactoryNew (fallback: duplicate a built cue). Nothing is deleted.
  * Graph built by LASliceAudioLibrary.build_multi_wave_cue (C++, ue/cpp/): 1 wave -> WavePlayer; N waves -> N WavePlayers
    -> Random (all wired, equal weights). Requires the C++ helper in Source/LunarAbyss.
  * Check: every wave reachable from FirstNode (get_cue_wave_paths) + AssetRegistry hard deps + wave 'duration' property > 0
    (+ cue duration via the helper). Size test restored in v1.8.3 at 5000 B. Exits 1 under -unattended on any failure.
  * log() defined; ON_EXISTING overwrite (rebuild) | skip (keep if it passes the check).
Usage (headless):  UnrealEditor-Cmd.exe Proj.uproject -run=pythonscript -script="ue_build_sound_cues.py" -unattended
   env: GB4C_AUDIO_SRC=<folder holding Audio/...>  GB4C_ON_EXISTING=overwrite|skip
"""
import csv, json, os, sys
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                                   # graybox_4C/
SOUND_LIST = os.path.join(ROOT, 'C4C_SoundList.csv')
GRAVITY_MAP = os.path.join(ROOT, 'C4C_GravityAudioMap.json')
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
AUDIO_SRC = os.environ.get('GB4C_AUDIO_SRC') or next((p for p in (os.path.join(PROJ, 'SourceArt'), os.path.dirname(ROOT))
                                                       if os.path.isdir(os.path.join(p, 'Audio'))), os.path.dirname(ROOT))
WAVE_ROOT, CUE_ROOT = '/Game/Audio', '/Game/Audio/Cues'
ON_EXISTING = os.environ.get('GB4C_ON_EXISTING', 'overwrite')
# v1.8.3 size test (Skye): empty cues saved at ~4 KB; the smallest wired cues in playtest_03 were 5394-5436 B
# (single-wave cues, e.g. SND_4C_GRV_001 = 5426 B). 5000 B sits between the two. Runs alongside the wave-link checks.
MIN_CUE_BYTES = int(os.environ.get('GB4C_MIN_CUE_BYTES', '5000'))
def report_dir():
    """code/playtest_03/ (GB4C_REPORT_DIR overrides). Looks beside and above the project, else creates <Project>/code/playtest_03."""
    env = os.environ.get('GB4C_REPORT_DIR')
    if env: os.makedirs(env, exist_ok=True); return env
    for base in (PROJ, os.path.dirname(PROJ.rstrip('/\\')), os.path.join(os.path.dirname(PROJ.rstrip('/\\')), 'space_rpg')):
        c = os.path.join(base, 'code', 'playtest_03')
        if os.path.isdir(c): return c
    c = os.path.join(PROJ, 'code', 'playtest_03'); os.makedirs(c, exist_ok=True); return c

REPORT = []
def log(msg, level='info'):
    REPORT.append(('' if level == 'info' else level.upper() + ': ') + msg)
    (unreal.log_error if level == 'error' else unreal.log_warning if level == 'warning' else unreal.log)('[GB4C_AUDIO] ' + msg)

def is_unattended():
    try: cl = unreal.SystemLibrary.get_command_line().lower()
    except Exception: cl = ''
    return '-unattended' in cl or os.environ.get('GB4C_UNATTENDED') == '1'

EAL = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()

def wave_asset_path(rel):                                      # 'Audio/Crew/X.wav' -> '/Game/Audio/Crew/X'
    rel = rel[6:] if rel.startswith('Audio/') else rel
    return f"{WAVE_ROOT}/{os.path.dirname(rel)}".rstrip('/') + '/' + os.path.splitext(os.path.basename(rel))[0], rel

def import_wave(rel, loop):
    path, r = wave_asset_path(rel)
    if EAL.does_asset_exist(path) and ON_EXISTING == 'skip':
        w = unreal.load_asset(path)
    else:
        src = os.path.join(AUDIO_SRC, 'Audio', r)
        if not os.path.isfile(src): log(f'missing source WAV {src}', 'error'); return None
        t = unreal.AssetImportTask()
        t.filename, t.destination_path, t.destination_name = src, os.path.dirname(path), os.path.basename(path)
        t.replace_existing, t.automated, t.save = True, True, True
        tools.import_asset_tasks([t]); w = unreal.load_asset(path)
    if not w: log(f'WAV import failed {rel}', 'error'); return None
    if bool(w.get_editor_property('looping')) != loop:
        w.set_editor_property('looping', loop); EAL.save_loaded_asset(w, only_if_is_dirty=False)
    return w

LIB = getattr(unreal, 'LASliceAudioLibrary', None)   # LunarAbyss C++ helper (ue/cpp/SliceAudioLibrary.*, v1.8.1)

def wave_duration(w):
    for prop in ('duration', 'Duration'):
        try: return float(w.get_editor_property(prop))
        except Exception: pass
    return float(getattr(w, 'duration', 0.0) or 0.0)

def cue_duration(cue):
    if LIB and hasattr(LIB, 'get_cue_duration'): return float(LIB.get_cue_duration(cue))
    try: return float(cue.get_editor_property('duration'))
    except Exception: return -2.0                                   # unknown; wave durations are checked instead

def check_cue(cue_path, waves, loop):
    """Return list of problems (empty = cue graph reaches every wave and the waves have length). No size check."""
    probs = []
    cue = unreal.load_asset(cue_path)
    if not cue: return ['cue not loadable']
    want = {w.get_path_name().split('.')[0] for w in waves}
    if LIB and hasattr(LIB, 'get_cue_wave_paths'):
        got = {str(x) for x in LIB.get_cue_wave_paths(cue)}
        for m in sorted(want - got): probs.append(f'graph does not reach {m}')
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False, include_hard_package_references=True,
                                                 include_searchable_names=False, include_soft_management_references=False,
                                                 include_hard_management_references=False)
    deps = {str(d) for d in (registry.get_dependencies(unreal.Name(cue_path), opts) or [])}
    for m in sorted(want - deps): probs.append(f'no package dependency on {m}')
    for w in waves:
        if not wave_duration(w) > 0: probs.append(f'wave {w.get_name()} has no duration')
    f = unreal.Paths.convert_relative_path_to_full(os.path.join(unreal.Paths.project_content_dir(), cue_path[len('/Game/'):] + '.uasset'))
    if not os.path.isfile(f): probs.append('uasset not on disk (cue not saved)')
    elif os.path.getsize(f) < MIN_CUE_BYTES: probs.append(f'uasset only {os.path.getsize(f)} B < {MIN_CUE_BYTES} B (empty graph)')
    d = cue_duration(cue)
    if d != -2.0 and not loop and not d > 0: probs.append(f'cue duration {d}')
    return probs

def get_or_create_cue(cue_id):
    """No dialogs, no CanCreateAsset prompt: load and rebuild an existing cue in place, else create it with a factory."""
    path = f'{CUE_ROOT}/{cue_id}'
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path), 'rebuilt-in-place'
    cue = tools.create_asset(cue_id, CUE_ROOT, unreal.SoundCue, unreal.SoundCueFactoryNew())
    if cue: return cue, 'created'
    tmpl = TEMPLATE_CUE[0]                                          # fallback: duplicate a cue that already exists
    if tmpl and EAL.duplicate_asset(tmpl, path): return unreal.load_asset(path), 'created-from-template'
    return None, 'failed'

TEMPLATE_CUE = [None]
def build_cue(cue_id, waves, loop, sound_class=None):
    path = f'{CUE_ROOT}/{cue_id}'
    if EAL.does_asset_exist(path) and ON_EXISTING == 'skip' and not check_cue(path, waves, loop): return path, 'kept'
    cue, how = get_or_create_cue(cue_id)
    if not cue: log(f'{cue_id}: could not create or load the cue', 'error'); return path, 'failed'
    if not LIB or not hasattr(LIB, 'build_multi_wave_cue'):
        log('LASliceAudioLibrary.build_multi_wave_cue missing: add ue/cpp/SliceAudioLibrary.* to Source/LunarAbyss and rebuild', 'error')
        return path, 'failed'
    if not LIB.build_multi_wave_cue(cue, waves, loop): log(f'{cue_id}: graph build reported missing waves', 'error')
    if sound_class:
        sc = unreal.load_asset(f'/Game/Audio/Classes/{sound_class}') if EAL.does_asset_exist(f'/Game/Audio/Classes/{sound_class}') else None
        if sc: cue.set_editor_property('sound_class_object', sc)
    EAL.save_loaded_asset(cue, only_if_is_dirty=False)
    if not TEMPLATE_CUE[0]: TEMPLATE_CUE[0] = path
    return path, how

def main():
    rows = list(csv.DictReader(open(SOUND_LIST, newline='')))
    log(f'{len(rows)} cues from {SOUND_LIST}; audio source {AUDIO_SRC}; on_existing={ON_EXISTING}')
    results, fails = {}, []
    # v1.8.2 pass 1: import every wave, save them all, then reload, so the AssetRegistry knows the new
    # packages before any cue references them (fresh project: first-run 'no package dependency' false failure).
    imported = {}
    for r in rows:
        loop = r['loop'].strip().lower() in ('yes', 'true', '1')
        for x in (r.get('wavs') or r['wav']).split(';'):
            if x not in imported: imported[x] = import_wave(x, loop)
    live = [w for w in imported.values() if w]
    EAL.save_loaded_assets(live, only_if_is_dirty=False)
    for x, w in list(imported.items()):
        if w:
            pkg = w.get_path_name().split('.')[0]
            registry.scan_files_synchronous([unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir() + pkg[len('/Game/'):] + '.uasset')], True)
            imported[x] = unreal.load_asset(pkg)
    log(f'imported + saved + reloaded {len(live)} waves')
    for r in rows:
        loop = r['loop'].strip().lower() in ('yes', 'true', '1')
        waves = [imported[x] for x in (r.get('wavs') or r['wav']).split(';') if imported.get(x)]
        if not waves: fails.append((r['id'], ['no wave'])); continue
        path, how = build_cue(r['id'], waves, loop, r.get('sound_class') or None)
        if EAL.does_asset_exist(path):
            EAL.save_loaded_assets([unreal.load_asset(path)], only_if_is_dirty=False)
            registry.scan_files_synchronous([unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir() + path[len('/Game/'):] + '.uasset')], True)
        probs = check_cue(path, waves, loop)
        if LIB and hasattr(LIB, 'describe_cue') and EAL.does_asset_exist(path): log(f"  {r['id']} graph: {LIB.describe_cue(unreal.load_asset(path))}")
        results[r['id']] = path
        if probs: fails.append((r['id'], probs))
        log(f"{r['id']}: {how}, {len(waves)} wave(s), {'OK' if not probs else 'FAIL ' + '; '.join(probs)}")
    # gravity hook check: every cue the GravityState map names must exist and pass
    gm = json.load(open(GRAVITY_MAP))
    for ev in gm['events']:
        for c in ev['cues']:
            if c not in results: fails.append((c, [f"named by GravityState {ev['state']} but not in sound list"]))
    log(f'CHECK: {len(rows) - len(fails)}/{len(rows)} cues wired; failures: {fails if fails else "none"}', 'error' if fails else 'info')
    out = os.path.join(report_dir(), 'gb4c_audio_report.txt')
    log('report -> ' + out)
    open(out, 'w').write('\n'.join(REPORT))
    open(os.path.join(unreal.Paths.project_saved_dir(), 'gb4c_audio_report.txt'), 'w').write('\n'.join(REPORT))
    if fails and is_unattended(): sys.exit(1)
    return results, fails

if __name__ == '__main__':
    main()
