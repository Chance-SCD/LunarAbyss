"""Unattended runner (drop-in for la3/Scripts/add_lights_and_sounds.py): IES import -> sound cues + check -> baseline lights -> save.
UnrealEditor-Cmd.exe Proj.uproject -run=pythonscript -script="ue_add_lights_and_sounds.py" -unattended"""
import importlib.util, os, sys, traceback
import unreal
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
IES_SRC = os.environ.get('GB4C_IES_SRC', os.path.join(PROJ, 'SourceArt', 'IES'))
MAP = os.environ.get('GB4C_MAP', '/Game/Maps/L_Slice_Graybox')
os.environ.setdefault('GB4C_UNATTENDED', '1')
def load(name, path):
    s = importlib.util.spec_from_file_location(name, path); m = importlib.util.module_from_spec(s); s.loader.exec_module(m); return m
cues = load('ue_build_sound_cues', os.path.join(HERE, 'ue_build_sound_cues.py'))
log = cues.log
tools = unreal.AssetToolsHelpers.get_asset_tools()
ok = True
for f in sorted(os.listdir(IES_SRC)) if os.path.isdir(IES_SRC) else []:
    n = os.path.splitext(f)[0]; dest = '/Game/Ship/Lighting/IES'
    if unreal.EditorAssetLibrary.does_asset_exist(f'{dest}/{n}') and cues.ON_EXISTING == 'skip': continue
    t = unreal.AssetImportTask(); t.filename, t.destination_path, t.destination_name = os.path.join(IES_SRC, f), dest, n
    t.replace_existing = t.automated = t.save = True; tools.import_asset_tasks([t])


res, fails = cues.main(exit_when_done=False)   # cue failures reported, lights still run
ok &= (not fails) and len(res) == cues.EXPECTED_CUES
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
try:
    lights = load('spawn_graybox_lights_ue', os.path.join(ROOT, 'spawn_graybox_lights_ue.py'))
    spawned = lights.main(path='baseline', state='Normal', parent='Corvette4C', on_existing=cues.ON_EXISTING)
    log(f'lights spawned: {len(spawned)}')
except Exception:
    log('light script error:\n' + traceback.format_exc(), 'error'); ok = False
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
open(os.path.join(unreal.Paths.project_saved_dir(), 'av_report.txt'), 'w').write('\n'.join(cues.REPORT))
rd = cues.report_dir()
open(os.path.join(rd, 'av_report.txt'), 'w').write('\n'.join(cues.REPORT))
cues.copy_ue_log(rd)   # v1.8.5: log copy after the lights too
sys.exit(0 if ok else 1)   # 0 only when all expected cues pass and the lights ran
