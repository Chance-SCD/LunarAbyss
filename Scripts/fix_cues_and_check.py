"""Headless: (1) verify/rebuild the 31 gray-box sound cues (WavePlayer + looping), (2) refresh the 4C actor
(hangar door meshes) in L_Slice_Graybox, (3) trace the fighter launch lane against the hull for clearance."""
import csv, os
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
REPORT = []
def log(m): unreal.log("[LA_FIX] " + m); REPORT.append(m)
Lib = unreal.LASliceAudioLibrary

# ---- 1. Cues ----
rows = list(csv.DictReader(open(os.path.join(PROJ, "Scripts", "echo_s", "C4C_SoundList.csv"))))
ok_count = 0
for r in rows:
    rel = r["wav"][6:] if r["wav"].startswith("Audio/") else r["wav"]
    wav_path = "/Game/Audio/" + os.path.dirname(rel) + "/" + os.path.splitext(os.path.basename(rel))[0]
    loop = r["loop"].strip().lower() in ("yes", "true", "1")
    cue = unreal.load_asset("/Game/Audio/Cues/" + r["id"])
    wave = unreal.load_asset(wav_path)
    if not cue or not wave:
        log("%s: MISSING cue=%s wave=%s" % (r["id"], bool(cue), bool(wave))); continue
    before = Lib.describe_cue(cue)
    want = "SoundNodeWavePlayer(wave=%s, loop=%d)" % (wave.get_path_name(), 1 if loop else 0)
    if want not in before:
        built = Lib.build_wave_cue(cue, wave, loop)
        unreal.EditorAssetLibrary.save_loaded_asset(cue, only_if_is_dirty=False)
        after = Lib.describe_cue(cue)
        log("%s: REBUILT (%s) before: %s | after: %s" % (r["id"], built, before, after))
    else:
        after = before
    good = want in after and "Duration=0.000" not in after
    ok_count += good
    log("%s: %s %s" % (r["id"], "OK" if good else "BAD", after))
log("Cues verified OK: %d / %d" % (ok_count, len(rows)))

# ---- 2. Map refresh ----
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level("/Game/Maps/L_Slice_Graybox")
cls = unreal.load_class(None, "/Script/LunarAbyss.CorvetteHangar")
corv = next((a for a in eas.get_all_level_actors() if a.get_class() == cls), None)
if not corv:
    log("Corvette actor not found in map")
else:
    corv.rerun_construction_scripts() if hasattr(corv, "rerun_construction_scripts") else None
    hull = corv.get_editor_property("hull")
    # Relink after a clean reimport (QA-139): hull, interior, and any fighter mesh component left empty.
    hull.set_static_mesh(unreal.load_asset("/Game/Ships/Corvette4C/SM_Corvette4C_Body"))
    corv.get_editor_property("interior").set_static_mesh(unreal.load_asset("/Game/Blockouts/SM_Corvette4C_Interior_Blockout"))
    fbody = unreal.load_asset("/Game/Ships/FighterC/SM_FighterC_Body")
    for a in eas.get_all_level_actors():
        if a.get_class().get_name() == "FighterPawn":
            for c in a.get_components_by_class(unreal.StaticMeshComponent):
                if c.static_mesh is None or c.static_mesh.get_name() == "SM_FighterC_Body": c.set_static_mesh(fbody)
    for p in ("/Game/Ships/Corvette4C/SM_Corvette4C_Body", "/Game/Ships/FighterC/SM_FighterC_Body", "/Game/Blockouts/SM_Hatch_DCD77"):
        m = unreal.load_asset(p)
        log("collision %s: %d convex" % (p, len(m.get_editor_property("body_setup").get_editor_property("agg_geom").get_editor_property("convex_elems")) if m else -1))
    for n in ("hangar_door_l", "hangar_door_r"):
        d = corv.get_editor_property(n)
        log("%s mesh=%s attach_socket=%s world=%s" % (n, d.static_mesh.get_name() if d.static_mesh else None,
            d.get_attach_socket_name(), d.get_world_location()))
    # LT-H caged slide ladder: shaft X 27.0..28.4, Y -13.95..-13.05 (Blender m), C2-SP floor 9.667 -> top 24.17, D5 floor 22.97.
    lcls = unreal.load_class(None, "/Script/LunarAbyss.LadderVolume")
    for a in [a for a in eas.get_all_level_actors() if a.get_class() == lcls]: eas.destroy_actor(a)
    lad = eas.spawn_actor_from_class(lcls, unreal.Vector(2770.0, 1350.0, (966.7 + 2417.0) / 2), unreal.Rotator(0, 0, 0))
    lad.set_actor_label("Ladder_LTH")
    lad.get_editor_property("shaft").set_box_extent(unreal.Vector(70.0, 45.0, (2417.0 - 966.7) / 2), True)
    lad.set_editor_property("bottom_floor_z", 966.7)
    lad.set_editor_property("top_floor_z", 2297.0)
    lad.set_editor_property("top_exit_local", unreal.Vector(0.0, -130.0, 0.0))  # through DR-76 into vestibule C5-HL
    log("Ladder_LTH placed at %s extent %s" % (lad.get_actor_location(), lad.get_editor_property("shaft").get_unscaled_box_extent()))
    les.save_current_level()

    # ---- 3. Launch lane clearance ----
    world = unreal.EditorLevelLibrary.get_editor_world()
    def sock(n): return hull.get_socket_location(n)
    spot, thr, clr = sock("Hangar_Spot_Fighter"), sock("HangarLaunch_Threshold"), sock("HangarLaunch_Clear")
    log("Lane: Spot_Fighter %s -> Threshold %s -> Clear %s" % (spot, thr, clr))
    fm = unreal.load_asset("/Game/Ships/FighterC/SM_FighterC_Body")
    b = fm.get_bounding_box()
    half = unreal.Vector((b.max.x - b.min.x) / 2, (b.max.y - b.min.y) / 2, (b.max.z - b.min.z) / 2)
    yaw = (clr - thr).rotation().yaw if hasattr(clr - thr, "rotation") else -90.0
    rot = unreal.Rotator(0.0, 0.0, yaw)
    types = [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1, unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2]  # WorldStatic, WorldDynamic
    def trace(a, z, label, h):
        hits = unreal.SystemLibrary.box_trace_multi_for_objects(world, a, z, h, rot, types, False, [], unreal.DrawDebugTrace.NONE, True) or []
        names = []
        for hr in hits:
            t = hr.to_tuple()
            comp = t[10]
            names.append("%s@%.0fcm%s" % (comp.get_name() if comp else "?", t[3], "(start-overlap)" if t[1] else ""))
        log("%s (half %s): %d hits %s" % (label, h, len(hits), names))
    trace(thr, clr, "Box sweep Threshold->Clear, fighter size", half)
    trace(spot + unreal.Vector(0, 0, half.z + 20), thr, "Box sweep Spot->Threshold (raised), fighter size", half)
    trace(thr, clr, "Box sweep Threshold->Clear, 50% size", half * 0.5)
open(os.path.join(PROJ, "Saved", "fix_report.txt"), "w").write("\n".join(REPORT))

# ---- 4. Reimport DT_Slice_MoveTuning (sprint + speeds added) ----
try:
    t = unreal.AssetImportTask()
    t.filename = os.path.join(PROJ, "Data", "DT_Slice_MoveTuning.csv")
    t.destination_path = "/Game/Data"; t.destination_name = "DT_Slice_MoveTuning"
    t.replace_existing = True; t.automated = True; t.save = True
    f = unreal.CSVImportFactory()
    f.automated_import_settings.import_row_struct = unreal.load_object(None, "/Script/LunarAbyss.SliceMoveTuningRow")
    t.factory = f
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    dt = unreal.load_asset("/Game/Data/DT_Slice_MoveTuning")
    log("DT_Slice_MoveTuning SprintSpeed column: %s" % unreal.DataTableFunctionLibrary.get_data_table_column_as_string(dt, "SprintSpeed"))
except Exception as e:
    log("DT reimport FAILED: %s" % e)
open(os.path.join(PROJ, "Saved", "fix_report.txt"), "w").write("\n".join(REPORT))
