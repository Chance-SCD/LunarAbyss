"""Headless: import IES + the 31 gray-box 4C sound cues, then run Echo-S's spawn_graybox_lights_ue.py
against L_Slice_Graybox (parented to the Corvette4C actor) and save. Run after import_blockouts.py."""
import csv, os, sys, importlib.util, traceback
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SA = os.path.join(PROJ, "SourceArt")
ECHO = os.path.join(PROJ, "Scripts", "echo_s")
MAP = "/Game/Maps/L_Slice_Graybox"
REPORT = []
def log(m): unreal.log("[LA_AV] " + m); REPORT.append(m)
tools = unreal.AssetToolsHelpers.get_asset_tools()

def imp(filename, dest, name):
    t = unreal.AssetImportTask()
    t.filename = filename; t.destination_path = dest; t.destination_name = name
    t.replace_existing = True; t.automated = True; t.save = True
    tools.import_asset_tasks([t])
    return unreal.load_asset(dest + "/" + name)

# IES -> /Game/Ship/Lighting/IES (folder the Echo-S script expects)
for f in sorted(os.listdir(os.path.join(SA, "IES"))):
    n = os.path.splitext(f)[0]
    if not imp(os.path.join(SA, "IES", f), "/Game/Ship/Lighting/IES", n): log("IES import FAILED " + f)

# Sounds: one SoundWave per WAV, one SoundCue per cue ID
rows = list(csv.DictReader(open(os.path.join(ECHO, "C4C_SoundList.csv"))))
made = 0
for r in rows:
    wav_rel = r["wav"][6:] if r["wav"].startswith("Audio/") else r["wav"]; wav_name = os.path.splitext(os.path.basename(wav_rel))[0]
    wdest = "/Game/Audio/" + os.path.dirname(wav_rel).replace("\\", "/")
    wave = unreal.load_asset(wdest + "/" + wav_name) if unreal.EditorAssetLibrary.does_asset_exist(wdest + "/" + wav_name) else None
    if not wave:
        wave = imp(os.path.join(SA, "Audio", wav_rel), wdest, wav_name)
    if not wave: log("WAV import FAILED " + wav_rel); continue
    if r["loop"].strip().lower() in ("yes", "true", "1"):
        wave.set_editor_property("looping", True); unreal.EditorAssetLibrary.save_loaded_asset(wave)
    f = unreal.SoundCueFactoryNew()
    ok = False
    for prop, val in (("initial_sound_waves", [wave]), ("initial_sound_wave", wave)):
        try: f.set_editor_property(prop, val); ok = True; break
        except Exception as e: err = "%s: %s" % (prop, e)
    if not ok: log("SoundCueFactoryNew could not take the wave (" + err + "); cue " + r["id"] + " created EMPTY")
    cue_path = "/Game/Audio/Cues/" + r["id"]
    if unreal.EditorAssetLibrary.does_asset_exist(cue_path): unreal.EditorAssetLibrary.delete_asset(cue_path)
    cue = tools.create_asset(r["id"], "/Game/Audio/Cues", unreal.SoundCue, f)
    if cue:
        unreal.EditorAssetLibrary.save_loaded_asset(cue); made += 1
        try:
            if ok and cue.get_duration() <= 0.0: log("cue %s has no duration (wave not wired?)" % r["id"])
        except Exception as e: log("duration check: %s" % e)
    else: log("Cue create FAILED " + r["id"])
log("Sound cues created: %d / %d" % (made, len(rows)))

# Echo-S lights
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level(MAP)
try:
    spec = importlib.util.spec_from_file_location("spawn_graybox_lights_ue", os.path.join(ECHO, "spawn_graybox_lights_ue.py"))
    mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    mod.BODY_ASSET = "/Game/Ships/Corvette4C/SM_Corvette4C_Body"  # our import path (script default is /Game/Ship/...)
    spawned = mod.main(path="baseline", state="Normal", parent="Corvette4C")
    log("Echo-S lights spawned: %d" % len(spawned))
except Exception:
    log("Echo-S script ERROR:\n" + traceback.format_exc())
les.save_current_level()
open(os.path.join(PROJ, "Saved", "av_report.txt"), "w").write("\n".join(REPORT))
