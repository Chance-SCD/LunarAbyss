"""Headless gray-box import + slice map build. Run with:
UnrealEditor-Cmd.exe LunarAbyss.uproject -run=pythonscript -script="<abs path>/Scripts/import_blockouts.py" -unattended -nop4 -nosplash -NullRHI
Source FBX: SourceArt/ (Forge export_UE handoff + Atlas-V interior blockout).
"""
import json, os
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJ, "SourceArt")
DEST = "/Game/Blockouts"
MAP = "/Game/Maps/L_Slice_Graybox"
SOCKETS = json.load(open(os.path.join(PROJ, "Scripts", "blockout_sockets.json")))
# Forge handoff (art/export_UE, IMPORT_README.md): Body + parts route, Combine ON per file, LODs ON,
# normals+tangents from FBX, no auto collision (UCX), no lightmap UV generation (UV1 = lightmap, index 1).
# Not imported: SM_FighterC.fbx / SM_Corvette4C.fbx (reference only), SK_FighterC.fbx (skeletal; SM route used).
JOBS = []  # (fbx relative to SourceArt, destination folder)
for sub, dest in (("FighterC", "/Game/Ships/FighterC"), ("Corvette4C", "/Game/Ships/Corvette4C"),
                  ("Corvette4C_BoardingTubes", "/Game/Ships/Corvette4C_BoardingTubes")):
    base = os.path.join(SRC, sub)
    for f in sorted(os.listdir(base)):
        if f.endswith(".fbx") and f not in ("SM_FighterC.fbx", "SM_Corvette4C.fbx", "SK_FighterC.fbx"):
            JOBS.append((os.path.join(sub, f), dest))
    pdir = os.path.join(base, "parts")
    if os.path.isdir(pdir):
        for f in sorted(os.listdir(pdir)):
            if f.endswith(".fbx"):
                JOBS.append((os.path.join(sub, "parts", f), dest + "/Parts"))
JOBS.append(("SM_Corvette4C_Interior_Blockout.fbx", DEST))  # Atlas-V gray-box interior (handoff has none)
# Expected X length (cm) to verify metres -> cm.
EXPECT_X = {"SM_FighterC_Body": 1509.0, "SM_Corvette4C_Body": 12735.0, "SM_Corvette4C_Interior_Blockout": 7000.0}
REPORT = []
def log(m): unreal.log("[LA_IMPORT] " + m)
# Use the legacy FBX pipeline so FbxImportUI options (combine, LODs, UCX) apply.
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX False")
KEY_SOCKETS = {
    "SM_Corvette4C_Body": ["Hangar", "Hangar_Spot_Fighter", "Hangar_Spot_Dropship", "HangarLaunch_Threshold", "HangarLaunch_Clear",
                           "Camera_Cockpit", "DockCollar_Port", "Dock_Port", "Mount_HangarDoor_L", "Mount_HangarDoor_R"],
    "SM_FighterC_Body": ["Camera_Cockpit", "Camera_Chase", "Pilot", "Spot_Fighter", "Engine_01"],
    "SM_Corvette4C_Interior_Blockout": ["Seat_Pilot", "PlayerSpawn_Bridge", "HangarPersonnelDoor"],
}

def strip(n): return n[7:] if n.startswith("SOCKET_") else n

def import_one(rel, dest, scale):
    name = os.path.splitext(os.path.basename(rel))[0]
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = True
    ui.import_textures = False
    ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    sm = ui.static_mesh_import_data
    sm.combine_meshes = True
    sm.auto_generate_collision = False
    sm.one_convex_hull_per_ucx = True
    sm.generate_lightmap_u_vs = False
    sm.import_mesh_lo_ds = True
    sm.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    sm.convert_scene = True
    sm.convert_scene_unit = True
    sm.force_front_x_axis = False
    sm.import_uniform_scale = scale
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, rel)
    t.destination_path = dest
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = False
    t.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    return name, unreal.load_asset(dest + "/" + name)

def x_len(mesh):
    b = mesh.get_bounding_box()
    return b.max.x - b.min.x

for rel, dest in JOBS:
    name, mesh = import_one(rel, dest, 1.0)
    if not mesh:
        log("FAILED import " + rel); REPORT.append("FAILED " + rel); continue
    L = x_len(mesh)
    if name in EXPECT_X and L < EXPECT_X[name] * 0.2:
        log("%s X length %.1f cm looks unscaled; reimporting x100" % (name, L))
        name, mesh = import_one(rel, dest, 100.0); L = x_len(mesh)
    def has(n): return bool(mesh.find_socket(strip(n)) or mesh.find_socket("SOCKET_" + strip(n)))
    added = 0
    for s in SOCKETS.get(name, []):
        if has(s["name"]): continue
        sock = unreal.StaticMeshSocket(mesh)
        sock.set_editor_property("socket_name", strip(s["name"]))
        sock.set_editor_property("relative_location", unreal.Vector(*s["loc"]))
        sock.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, s["yaw"]))
        mesh.add_socket(sock); added += 1
    try:
        mesh.set_editor_property("light_map_coordinate_index", 1)
    except Exception as e:
        log("lightmap index: %s" % e)
    nlod = unreal.EditorStaticMeshLibrary.get_lod_count(mesh) if hasattr(unreal, "EditorStaticMeshLibrary") else -1
    nmat = len(mesh.get_editor_property("static_materials"))
    if name == "SM_Corvette4C_Interior_Blockout":
        bs = mesh.get_editor_property("body_setup")
        if bs:
            bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    missing = [k for k in KEY_SOCKETS.get(name, []) if not has(k)]
    line = "%s: X %.1f cm, LODs %d, mats %d, +%d json sockets, key sockets missing: %s" % (name, L, nlod, nmat, added, missing or "none")
    log(line); REPORT.append(line)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
# ---- Map ----
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    unreal.EditorAssetLibrary.delete_asset(MAP)
les.new_level(MAP)

def spawn(cls, loc, rot=unreal.Rotator(0, 0, 0)):
    return eas.spawn_actor_from_class(cls, unreal.Vector(*loc), rot)

sun = spawn(unreal.DirectionalLight, (0, 0, 5000), unreal.Rotator(0, -40, -30))
sun.light_component.set_editor_property("atmosphere_sun_light", True)
spawn(unreal.SkyAtmosphere, (0, 0, 0))
sky = spawn(unreal.SkyLight, (0, 0, 6000))
sky.light_component.set_editor_property("real_time_capture", True)
sky.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
spawn(unreal.ExponentialHeightFog, (0, 0, -20000))

corv_cls = unreal.load_class(None, "/Script/LunarAbyss.CorvetteHangar")
corv = spawn(corv_cls, (0, 0, 0))
corv.set_actor_label("Corvette4C")
corv.get_editor_property("hull").set_static_mesh(unreal.load_asset("/Game/Ships/Corvette4C/SM_Corvette4C_Body"))
corv.get_editor_property("interior").set_static_mesh(unreal.load_asset(DEST + "/SM_Corvette4C_Interior_Blockout"))

# Player start at SOCKET_PlayerSpawn_Bridge (35.7, 0, 22.97) m + half capsule.
ps = spawn(unreal.PlayerStart, (3570.0, 0.0, 2297.0 + 100.0))
les.save_current_level()
log("Map saved: " + MAP)
open(os.path.join(PROJ, "Saved", "import_report.txt"), "w").write("\n".join(REPORT))

# ---- Move tuning data table ----
try:
    t = unreal.AssetImportTask()
    t.filename = os.path.join(PROJ, "Data", "DT_Slice_MoveTuning.csv")
    t.destination_path = "/Game/Data"; t.destination_name = "DT_Slice_MoveTuning"
    t.replace_existing = True; t.automated = True; t.save = True
    f = unreal.CSVImportFactory()
    f.automated_import_settings.import_row_struct = unreal.load_object(None, "/Script/LunarAbyss.SliceMoveTuningRow")
    t.factory = f
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    log("DT_Slice_MoveTuning: %s" % bool(unreal.load_asset("/Game/Data/DT_Slice_MoveTuning")))
except Exception as e:
    log("DT_Slice_MoveTuning import FAILED: %s" % e)
open(os.path.join(PROJ, "Saved", "import_report.txt"), "w").write("\n".join(REPORT))
