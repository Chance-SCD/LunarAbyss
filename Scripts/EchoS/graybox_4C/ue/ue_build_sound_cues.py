"""UE 5.8.3 editor Python: import the gray-box 4C WAVs and build one SoundCue per row of C4C_SoundList.csv,
then CHECK that every cue graph really reaches its SoundWave(s). Replaces the cue half of add_lights_and_sounds.py.

Fixes over the LunarAbyss test (commit 3701aa9, 31 cues at ~4 KB = empty graphs):
  * log() is defined here (no dependency on a caller's log).
  * SoundCueFactoryNew.initial_sound_waves gets the full variant list (one wave -> WavePlayer -> Output;
    several -> WavePlayers -> Random -> Output). The list is read back from the factory before create_asset.
  * initial_sound_wave (singular) is NOT used: it is not the 5.x property and silently leaves the graph empty.
  * Unattended-safe: ON_EXISTING = 'overwrite' (delete + recreate) or 'skip' (keep cue if it passes the check). No dialogs.
  * Check step: AssetRegistry hard dependencies of each cue must contain every wave package, get_duration() > 0
    (or -1/INDEFINITELY for loops), and the saved .uasset must be > 6 KB. Failures are listed and the script exits non-zero
    under -unattended.
  * Gravity cues are verified against C4C_GravityAudioMap.json (GravityState event -> cue).
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
MIN_CUE_BYTES = 6 * 1024                                       # an empty SoundCue saves at ~4 KB

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

def check_cue(cue_path, waves, loop):
    """Return list of problems (empty = cue really plays its wave(s))."""
    probs = []
    cue = unreal.load_asset(cue_path)
    if not cue: return ['cue not loadable']
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False, include_hard_package_references=True,
                                                 include_searchable_names=False, include_soft_management_references=False,
                                                 include_hard_management_references=False)
    deps = {str(d) for d in (registry.get_dependencies(unreal.Name(cue_path), opts) or [])}
    for w in waves:
        pkg = w.get_path_name().split('.')[0]
        if pkg not in deps: probs.append(f'graph does not reference {pkg}')
    # Vega local patch (UE 5.8 Python has no SoundCue.get_duration): read Duration via LunarAbyss's LASliceAudioLibrary.
    try: d = cue.get_duration()
    except AttributeError:
        desc = unreal.LASliceAudioLibrary.describe_cue(cue); d = float(desc.rsplit('Duration=', 1)[-1]) if 'Duration=' in desc else 0.0
    if not loop and not d > 0: probs.append(f'duration {d}')
    if loop and not (d > 0 or d < 0): probs.append(f'loop duration {d}')
    f = unreal.Paths.convert_relative_path_to_full(os.path.join(unreal.Paths.project_content_dir(), cue_path[len('/Game/'):] + '.uasset'))
    if os.path.isfile(f) and os.path.getsize(f) < MIN_CUE_BYTES: probs.append(f'uasset only {os.path.getsize(f)} B (empty graph)')
    return probs

def build_cue(cue_id, waves, sound_class=None):
    path = f'{CUE_ROOT}/{cue_id}'
    if EAL.does_asset_exist(path):
        if ON_EXISTING == 'skip': return path, 'kept'
        if not EAL.delete_asset(path) or EAL.does_asset_exist(path):
            # Vega local patch: referenced cues (C++/map) cannot be deleted unattended -> rebuild the graph in place.
            cue = unreal.load_asset(path)
            ok = unreal.LASliceAudioLibrary.build_wave_cue(cue, waves[0], bool(waves[0].get_editor_property('looping'))) if cue and waves else False
            EAL.save_loaded_asset(cue, only_if_is_dirty=False) if cue else None
            return path, 'rebuilt-in-place' if ok else 'failed'
    fac = unreal.SoundCueFactoryNew()
    fac.set_editor_property('initial_sound_waves', waves)
    got = list(fac.get_editor_property('initial_sound_waves') or [])
    if len(got) != len(waves): log(f'{cue_id}: factory took {len(got)}/{len(waves)} waves', 'error')
    cue = tools.create_asset(cue_id, CUE_ROOT, unreal.SoundCue, fac)
    if not cue: log(f'{cue_id}: create_asset failed', 'error'); return path, 'failed'
    if sound_class:
        sc = unreal.load_asset(f'/Game/Audio/Classes/{sound_class}')
        if sc: cue.set_editor_property('sound_class_object', sc)
    EAL.save_loaded_asset(cue, only_if_is_dirty=False)
    return path, 'built'

def main():
    rows = list(csv.DictReader(open(SOUND_LIST, newline='')))
    log(f'{len(rows)} cues from {SOUND_LIST}; audio source {AUDIO_SRC}; on_existing={ON_EXISTING}')
    results, fails = {}, []
    for r in rows:
        loop = r['loop'].strip().lower() in ('yes', 'true', '1')
        waves = [w for w in (import_wave(x, loop) for x in (r.get('wavs') or r['wav']).split(';')) if w]
        if not waves: fails.append((r['id'], ['no wave'])); continue
        path, how = build_cue(r['id'], waves, r.get('sound_class') or None)
        probs = check_cue(path, waves, loop)
        if probs and how == 'kept':                            # stale empty cue from an earlier run: rebuild once
            EAL.delete_asset(path); path, how = build_cue(r['id'], waves, r.get('sound_class')); probs = check_cue(path, waves, loop)
        results[r['id']] = path
        if probs: fails.append((r['id'], probs))
        log(f"{r['id']}: {how}, {len(waves)} wave(s), {'OK' if not probs else 'FAIL ' + '; '.join(probs)}")
    # gravity hook check: every cue the GravityState map names must exist and pass
    gm = json.load(open(GRAVITY_MAP))
    for ev in gm['events']:
        for c in ev['cues']:
            if c not in results: fails.append((c, [f"named by GravityState {ev['state']} but not in sound list"]))
    log(f'CHECK: {len(rows) - len(fails)}/{len(rows)} cues wired; failures: {fails if fails else "none"}', 'error' if fails else 'info')
    out = os.path.join(unreal.Paths.project_saved_dir(), 'gb4c_audio_report.txt')
    open(out, 'w').write('\n'.join(REPORT)); log('report -> ' + out)
    if fails and is_unattended(): sys.exit(1)
    return results, fails

if __name__ == '__main__':
    main()
