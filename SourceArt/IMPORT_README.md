# UE 5.8.3 import guide: Fighter C (v1.3), Corvette 4C (v6.3), 4C boarding tubes
From Forge, 2026-10-29. Everything here is a copy of the final exports in `art/models/`; the source .blend files and build scripts stay there.
All files are FBX (Blender 5.2.2), in metres at scale 1.0, +X forward, +Z up, origin at ground centre (tubes: origin at their dock socket).
Blender is right-handed and UE is left-handed: the importer flips Y, so rotation signs about Z and X swap in UE.

## Folder layout
- `FighterC/`: `SM_FighterC_Body.fbx` (static body), `SM_FighterC.fbx` (body + all parts + UCX + sockets, for reference), `SK_FighterC.fbx` (skeletal: 17 bones matching Juno's rig, rigid-skinned), `parts/` (16 moving parts).
- `Corvette4C/`: `SM_Corvette4C_Body.fbx`, `SM_Corvette4C.fbx` (reference, everything), `parts/` (22: hangar doors, turrets, gear, gear doors).
- `Corvette4C_BoardingTubes/`: `SM_BoardingTube_BP2.fbx` and `SM_BoardingTube_BP3.fbx` (each a combined static mesh, extended with doors closed), `parts/` (5 moving parts per tube).

## Import settings (Static Mesh)
- Import Uniform Scale **1.0**; Convert Scene ON; Force Front XAxis OFF; Convert Scene Unit ON.
- **Body files and each `parts/*.fbx`: Combine Meshes ON**, Import Mesh LODs ON. LODs arrive from the FBX LodGroup (`_LOD0.._LOD3`; parts carry LOD0-LOD1, tube parts LOD0-LOD3). Suggested screen sizes: 1.0 / 0.5 / 0.25 / 0.1.
- Auto Generate Collision **OFF**: custom collision comes in as `UCX_<MeshObject>_NN` convex hulls, where `<MeshObject>` is the **exact mesh object name inside the FBX** (not the file name): `UCX_SM_Corvette4C_Hull_NN` (140) in both 4C Body and reference files, `UCX_SM_FighterC_Hull_NN` (52) in both Fighter Body and reference files, and `UCX_<part LodGroup name>_NN` for parts and tubes (the LodGroup node is named the same as the file). 1 per part, tubes 17 per tube (14 shell/door + 3 handrail); tube parts 5 per walkable section, 1 per door.
- Sockets: every `SOCKET_*` empty is **parented to its static mesh's LOD0 node** (fixed 2026-10-29 22:55, since UE drops unparented sockets): `SM_Corvette4C_Hull` (59), `SM_FighterC_Hull` (23), `SM_BoardingTube_BPx_LOD0` (5 each + rail grips: 21 on BP2, 27 on BP3), tube parts' `_LOD0` (rail grips only), and each turret or gun part's `_LOD0`. World position and rotation are unchanged (re-import check: ≤0.014 mm, ≤0.00002°).
- **UE socket names have the `SOCKET_` prefix stripped**: `SOCKET_Dock_Port` → `Dock_Port`, `SOCKET_DockCollar_Port` → `DockCollar_Port`, `SOCKET_Muzzle_L` → `Muzzle_L`. Socket tables below list the FBX names; use the stripped name in Blueprints and C++ (`GetSocketLocation("Dock_Port")`). Sockets come in relative to the combined mesh pivot = ship origin, so positions match the tables.
- Normals: Import Normals and Tangents (the FBX has smoothing groups and custom split normals). Generate Lightmap UVs **OFF**: UV1 is a packed, non-overlapping lightmap channel on every static mesh (Lightmap Coordinate Index 1). UV0 is the tiling material channel.
- Do **not** import `SM_FighterC.fbx` / `SM_Corvette4C.fbx` with Combine Meshes ON: that bakes the moving parts into the hull. Use the Body + parts route.

## Skeletal (Fighter C only)
`SK_FighterC.fbx`: Import as Skeletal Mesh. Skeleton = Juno's `SK_Fighter` skeleton (same 17 bone names and positions; part pivots match Juno's to 0.0 mm). Juno's clips in `art/animation/Fighter/` play on it.
- Import Morph Targets OFF; Use T0 As Ref Pose OFF. Create a Physics Asset, then replace the auto capsules with boxes, or use the static UCX body for hit and collision and set the physics asset to query-only.
- Skeletal meshes don't use lightmaps: the SK file's UV1 is a copy of UV0 by design.
- Alternative (recommended, same as the 4C): `SM_FighterC_Body` plus the 16 `parts/` as StaticMeshComponents attached at their pivots, driven by code or Sequencer.

## Material slots (matte, weathered; no emissive anywhere)
- Fighter C: `M_FC_Paint_Olive`, `M_FC_Paint_OliveDark`, `M_FC_Paint_Yellow`, `M_FC_Metal`, `M_FC_Metal_Dark`, `M_FC_Interior`, `M_FC_Hazard`, `M_FC_Glass` (translucent, alpha 0.22), `M_FC_Screen`, `M_FC_TubeDark`, `M_FC_Decal` (masked, the alpha stencil "FTR-C-01", texture `T_FC_Decal_FTRC01.png`).
- Corvette 4C and tubes: `M_C4C_Paint_OffWhite`, `M_C4C_Paint_Gray`, `M_C4C_Paint_DarkGray`, `M_C4C_Paint_Red`, `M_C4C_Paint_Hazard`, `M_C4C_Metal`, `M_C4C_Metal_Dark`, `M_C4C_Interior`, `M_C4C_Glass` (translucent), `M_C4C_Screen` (unlit-looking dark panel, **not** emissive), `M_C4C_TubeDark`.
- Textures (worn paint and metal sets) are in `art/models/Fighter_C/textures/` and `art/models/Corvette4C/textures/`. UV0 world-box mapping is about 512 px/m (Fighter) and 256 px/m (4C hull) at 1k. The boarding tubes are at 512 px/m (open note).
- Mirroring is intentional only on the starboard FTR-C-01 decal quad (UV0 flipped so the text reads correctly).

## Fighter C (FTR-C-01), v1.3
- Size, gear down: **15.09 × 10.40 × 4.464 m**. Canopy open (60°): 5.77 m tall.
- Tris: body LOD0 11,392, LOD1 5,696, LOD2 1,948, LOD3 661; parts 4,290 (total LOD0 15,682); skeletal 15,678.
- Moving parts (pivot = part FBX origin = bone head; ship space):

| Part / bone | Pivot | Tris | Motion |
|---|---|---|---|
| `SM_FighterC_canopy` / bone `canopy` | (1.5, 0, 2.9) | 1122 | opens nose-up about Y, 60° (lifts 0.04 m first) |
| `SM_FighterC_gear_nose_strut` / bone `gear_nose_strut` | (4.53, 0, 1.75) | 216 | retract +90° about Y (folds aft) |
| `SM_FighterC_gear_nose_piston` / bone `gear_nose_piston` | (4.53, 0, 0.92) | 96 | telescopes 0.55 m up the strut |
| `SM_FighterC_gear_nose_pad` / bone `gear_nose_pad` | (4.53, 0, 0.1) | 288 | not keyed (ground align) |
| `SM_FighterC_gear_nose_door_l` / bone `gear_nose_door_l` | (3.15, 0.32, 1.42) | 80 | close −90° about X |
| `SM_FighterC_gear_nose_door_r` / bone `gear_nose_door_r` | (3.15, -0.32, 1.42) | 80 | close +90° about X |
| `SM_FighterC_gear_main_l_strut` / bone `gear_main_l_strut` | (-1.88, 1.3, 1.6) | 216 | retract +90° about Y (folds aft) |
| `SM_FighterC_gear_main_l_piston` / bone `gear_main_l_piston` | (-1.88, 1.388, 0.893) | 124 | telescopes 0.60 m |
| `SM_FighterC_gear_main_l_pad` / bone `gear_main_l_pad` | (-1.88, 1.5, 0.1) | 336 | not keyed |
| `SM_FighterC_gear_main_l_door` / bone `gear_main_l_door` | (-3.05, 1.85, 1.27) | 92 | close −90° about X |
| `SM_FighterC_gear_main_r_strut` / bone `gear_main_r_strut` | (-1.88, -1.3, 1.6) | 216 | retract +90° about Y (folds aft) |
| `SM_FighterC_gear_main_r_piston` / bone `gear_main_r_piston` | (-1.88, -1.388, 0.893) | 124 | telescopes 0.60 m |
| `SM_FighterC_gear_main_r_pad` / bone `gear_main_r_pad` | (-1.88, -1.5, 0.1) | 336 | not keyed |
| `SM_FighterC_gear_main_r_door` / bone `gear_main_r_door` | (-3.05, -1.85, 1.27) | 92 | close +90° about X |
| `SM_FighterC_gun_belly_l` / bone `gun_belly_l` | (5.5, 0.55, 1.5) | 436 | gimbal ±15° yaw (Z) / ±15° pitch (Y) |
| `SM_FighterC_gun_belly_r` / bone `gun_belly_r` | (5.5, -0.55, 1.5) | 436 | gimbal ±15° yaw (Z) / ±15° pitch (Y) |

Fighter sockets (23; ship space, metres):

| Socket | Position |
|---|---|
| `SOCKET_RCS_Nose_L` | (6.2, 1.17, 2.15) |
| `SOCKET_RCS_Nose_R` | (6.2, -1.17, 2.15) |
| `SOCKET_RCS_Nose_Up` | (6.4, 0, 2.55) |
| `SOCKET_RCS_Nose_Down` | (6, 0, 1.57) |
| `SOCKET_RCS_Tail_L` | (-6.3, 2.41, 2.21) |
| `SOCKET_RCS_Tail_R` | (-6.3, -2.41, 2.21) |
| `SOCKET_RCS_Tail_Up_L` | (-6.45, 1.3, 3.41) |
| `SOCKET_RCS_Tail_Up_R` | (-6.45, -1.3, 3.41) |
| `SOCKET_RCS_Tail_Down_L` | (-6, 1.3, 1.3) |
| `SOCKET_RCS_Tail_Down_R` | (-6, -1.3, 1.3) |
| `SOCKET_Gun_L` | (7.32, 0.55, 1.22) |
| `SOCKET_Gun_R` | (7.32, -0.55, 1.22) |
| `SOCKET_Engine_01` | (-7.53, 0.8, 2.4) |
| `SOCKET_Engine_02` | (-7.53, -0.8, 2.4) |
| `SOCKET_Boarding_Port` | (2.6, 3.4, 0) |
| `SOCKET_HangarClamp_Nose` | (4.53, 0, 0) |
| `SOCKET_HangarClamp_MainL` | (-1.88, 1.5, 0) |
| `SOCKET_HangarClamp_MainR` | (-1.88, -1.5, 0) |
| `SOCKET_Camera_Chase` | (-16, 0, 5.2) |
| `SOCKET_Pilot` | (2.18, 0, 2.03) |
| `SOCKET_Camera_Cockpit` | (1.941, -0, 2.891) |
| `SOCKET_Camera_Cockpit_180` | (1.954, -0, 2.846) |
| `SOCKET_Spot_Fighter` | (0, 0, 0) |

## Corvette 4C, v6.3
- Size: **127.35 × 67.0 × 33.067 m**. Tris: body LOD0 78,684, LOD1 39,341, LOD2 18,831, LOD3 6,743; parts 8,680. UCX 140, sockets 59.
- Moving parts (ship-space pivots; attach each part at the body socket of the same name):
  - `HangarDoor_L` (−10.325, 14.6, 9.667) and `_R` (23.675, 14.6, 9.667): push 2.9 m along +Y, then rotate 90° about Z (L +90°, R −90° in Blender).
  - `Turret_01..03_Base`: yaw about Z at (23.675 / 5.675 / −11.325, 0, 29.867). `_Gun` pitches about Y at its base's `SOCKET_Gun`.
  - `Turret_04` (ventral): base (35.675, 0, 3.8), gun (37.25, 0, 2.1).
  - `Gear_FL/FR` (45.675, ±4.5, 8.047): retract +90° about Y. `Gear_RL/RR` (−26.325, ±10.0, 4.527): retract −90° about Y.
  - `GearDoor_<leg>_Out/_In`: hinge along X at `SOCKET_Mount_GearDoor_*`; close 90° about X toward the well. Rest pose = open.
- Turret part FBXs carry their own `SOCKET_Gun` / `SOCKET_Muzzle_L/R`.

4C sockets (ship space):

| Socket | Position |
|---|---|
| `SOCKET_Boarding_BP1` | (-24.69, 0.7, 29.78) |
| `SOCKET_Boarding_BP2` | (39.4, 13.6, 14.87) |
| `SOCKET_Boarding_BP3` | (-25.05, -15.83, 14.87) |
| `SOCKET_Boarding_BP4` | (16.8, 3.6, 2.17) |
| `SOCKET_Camera_Chase` | (-116.325, 0, 48.167) |
| `SOCKET_Camera_Cockpit` | (44.975, 0.95, 25.53) |
| `SOCKET_Dock_Dorsal` | (-24.69, 0.7, 30.18) |
| `SOCKET_Dock_Port` | (39.4, 14, 14.87) |
| `SOCKET_DockCollar_Port` | (39.4, 14.05, 14.87), rot (0, 0, 90°): +X = ship +Y (port, outward). Outer face of the BP-2 airlock collar; mate with station DR-ALAo/ALBo |
| `SOCKET_Dock_Starboard` | (-25.05, -16.23, 14.87) |
| `SOCKET_Dock_Ventral` | (16.8, 3.6, 1.77) |
| `SOCKET_Engine_L_Lower` | (-63.675, 20.5, 20.467) |
| `SOCKET_Engine_L_Upper` | (-63.675, 20.5, 25.867) |
| `SOCKET_Engine_R_Lower` | (-63.675, -20.5, 20.467) |
| `SOCKET_Engine_R_Upper` | (-63.675, -20.5, 25.867) |
| `SOCKET_Hangar` | (6.675, 1, 9.667) |
| `SOCKET_HangarLanding_Approach` | (6.675, 80, 14.167) |
| `SOCKET_HangarLanding_Threshold` | (6.675, 16.5, 14.167) |
| `SOCKET_HangarLaunch_Approach` | (6.675, 80, 14.167) |
| `SOCKET_HangarLaunch_Clear` | (6.675, 30, 14.167) |
| `SOCKET_HangarLaunch_Threshold` | (6.675, 16.5, 14.167) |
| `SOCKET_Hangar_Spot_Dropship` | (16.175, 2.25, 9.667) |
| `SOCKET_Hangar_Spot_Fighter` | (-3.825, 3.5, 9.667) |
| `SOCKET_Mount_GearDoor_FL_In` | (41.725, 3.1, 5.537) |
| `SOCKET_Mount_GearDoor_FL_Out` | (41.725, 5.9, 5.719) |
| `SOCKET_Mount_GearDoor_FR_In` | (41.725, -3.1, 5.537) |
| `SOCKET_Mount_GearDoor_FR_Out` | (41.725, -5.9, 5.719) |
| `SOCKET_Mount_GearDoor_RL_In` | (-24.175, 8.6, 3.274) |
| `SOCKET_Mount_GearDoor_RL_Out` | (-24.175, 11.4, 3.648) |
| `SOCKET_Mount_GearDoor_RR_In` | (-24.175, -8.6, 3.274) |
| `SOCKET_Mount_GearDoor_RR_Out` | (-24.175, -11.4, 3.648) |
| `SOCKET_Mount_Gear_FL` | (45.675, 4.5, 8.047) |
| `SOCKET_Mount_Gear_FR` | (45.675, -4.5, 8.047) |
| `SOCKET_Mount_Gear_RL` | (-26.325, 10, 4.527) |
| `SOCKET_Mount_Gear_RR` | (-26.325, -10, 4.527) |
| `SOCKET_Mount_HangarDoor_L` | (-10.325, 14.6, 9.667) |
| `SOCKET_Mount_HangarDoor_R` | (23.675, 14.6, 9.667) |
| `SOCKET_ShieldEmitter_01` | (32.175, 0, 30.7) |
| `SOCKET_ShieldEmitter_02` | (-30.325, 0, 29.292) |
| `SOCKET_ShieldEmitter_03` | (49.675, 0, 7.507) |
| `SOCKET_ShieldEmitter_04` | (-29.325, 0, 3.454) |
| `SOCKET_ShieldEmitter_05` | (7.675, 29.5, 30.617) |
| `SOCKET_ShieldEmitter_06` | (7.675, -29.5, 30.617) |
| `SOCKET_Turret_01` | (23.675, 0, 29.867) |
| `SOCKET_Turret_01_Gun` | (25.775, 0, 31.867) |
| `SOCKET_Turret_01_Muzzle_L` | (35.795, 1.056, 31.867) |
| `SOCKET_Turret_01_Muzzle_R` | (35.795, -1.056, 31.867) |
| `SOCKET_Turret_02` | (5.675, 0, 29.867) |
| `SOCKET_Turret_02_Gun` | (7.775, 0, 31.867) |
| `SOCKET_Turret_02_Muzzle_L` | (17.795, 1.056, 31.867) |
| `SOCKET_Turret_02_Muzzle_R` | (17.795, -1.056, 31.867) |
| `SOCKET_Turret_03` | (-11.325, 0, 29.867) |
| `SOCKET_Turret_03_Gun` | (-9.225, 0, 31.867) |
| `SOCKET_Turret_03_Muzzle_L` | (0.795, 1.056, 31.867) |
| `SOCKET_Turret_03_Muzzle_R` | (0.795, -1.056, 31.867) |
| `SOCKET_Turret_04` | (35.675, 0, 3.8) |
| `SOCKET_Turret_04_Gun` | (37.25, 0, 2.1) |
| `SOCKET_Turret_04_Muzzle_L` | (45.27, 0.792, 2.1) |
| `SOCKET_Turret_04_Muzzle_R` | (45.27, -0.792, 2.1) |

## Corvette 4C boarding tubes (BP-2 port, BP-3 starboard)
Separate attachments. Place each at the 4C socket listed below **with that socket's rotation**; tube local +X then points outboard. The walkway floor is at local Z −1.2 (= 4C deck D3, Z 13.67). Doors are 1.6 × 2.4 m. Recommended route: build the tube from the 5 parts in `parts/` (Base as root). The combined `SM_BoardingTube_BPx.fbx` is a static fallback in the extended pose with doors closed.
Bellows retract is an X-scale, so its end rings flatten slightly when retracted (cosmetic).

**BP2**: attach at `SOCKET_Dock_Port`. Extended 5.32 m, retracted 1.942 m.

| Part | Pivot (tube local) | Tris LOD0/1/2/3 | UCX | Motion |
|---|---|---|---|---|
| `SM_BoardingTube_BP2_Base` | (0, 0, 0) | 900/450/224/90 | 4 | static root |
| `SM_BoardingTube_BP2_Door_Inner` | (0.47, 0, -1.2) | 92/46/22/10 | 1 | slide -1.65 m along local Y to open |
| `SM_BoardingTube_BP2_Bellows` | (0.65, 0, -1.2) | 3984/1992/996/398 | 4 | retract: scale X to 0.18 about its pivot |
| `SM_BoardingTube_BP2_Head` | (4.77, 0, -1.2) | 960/480/240/96 | 4 | retract: translate −3.378 m along local X |
| `SM_BoardingTube_BP2_Door_Outer` | (5.02, 0, -1.2) | 92/46/22/10 | 1 | rides on the Head; slide +1.65 m along local Y to open |

Sockets: `SOCKET_TubeBase` (0, 0, 0), `SOCKET_TubeEnd` (5.32, 0, 0), `SOCKET_TubeEnd_Retracted` (1.942, 0, 0), `SOCKET_Walkway_Base` (0.45, 0, -1.2), `SOCKET_Walkway_End` (5.02, 0, -1.2)

**BP3**: attach at `SOCKET_Dock_Starboard`. Extended 7.13 m, retracted 2.267 m.

| Part | Pivot (tube local) | Tris LOD0/1/2/3 | UCX | Motion |
|---|---|---|---|---|
| `SM_BoardingTube_BP3_Base` | (0, 0, 0) | 900/450/224/90 | 4 | static root |
| `SM_BoardingTube_BP3_Door_Inner` | (0.47, 0, -1.2) | 92/46/22/10 | 1 | slide +1.65 m along local Y to open |
| `SM_BoardingTube_BP3_Bellows` | (0.65, 0, -1.2) | 5616/2808/1404/560 | 4 | retract: scale X to 0.18 about its pivot |
| `SM_BoardingTube_BP3_Head` | (6.58, 0, -1.2) | 960/480/240/96 | 4 | retract: translate −4.863 m along local X |
| `SM_BoardingTube_BP3_Door_Outer` | (6.83, 0, -1.2) | 92/46/22/10 | 1 | rides on the Head; slide -1.65 m along local Y to open |

Sockets: `SOCKET_TubeBase` (0, 0, 0), `SOCKET_TubeEnd` (7.13, 0, 0), `SOCKET_TubeEnd_Retracted` (2.267, 0, 0), `SOCKET_Walkway_Base` (0.45, 0, -1.2), `SOCKET_Walkway_End` (6.83, 0, -1.2)

Combined-file tris (LOD0/LOD1/LOD2/LOD3): BP2 6,028 / 3,014 / 1,506 / ~600, BP3 7,660 / 3,830 / 1,914 / ~770 (LOD3 = decimate 0.1; exact count shown in UE).

## Checks run on these copies (2026-10-29, Forge)
- Fighter: verify_fbx passes. Scale 1, UV0+UV1 on all meshes, LOD0-3, 52 UCX all convex, all 16 pivots 0.0 mm from Juno's README, no emissive.
- 4C: verify_fbx passes. 140 UCX convex, 59 sockets, LOD0-3, UV0+UV1.
- Tubes: verify_tubes passes. LOD0-3, 17 UCX each, convex, named `UCX_<mesh>_NN`.
- UV1 overlap audit (512² raster) on every static mesh: 0 overlap, except the 4C hull at 0.02 % (raster edge noise). UV0 has no unintended flips.
- Known leftover: one Fighter UCX (`UCX_SM_FighterC_51`, belly mid-section) has a 3 mm near-coplanar sliver under a strict 2 mm test. UE rebuilds hulls from the vertices, so it imports as a valid convex hull.
- Open note: the tubes' UV0 is 2× the 4C hull's texel density (512 vs 256 px/m). It looks fine, but tiling won't line up exactly at the hull seam.

## Collision naming fix (2026-10-30, 00:05 CDT) — SUPERSEDED by the 06:50 correction below; the UCX prefixes in this table are no longer used
UE dropped the 4C Body collision because the hulls were `UCX_SM_Corvette4C_NN` while the asset is `SM_Corvette4C_Body`. Every FBX now uses `UCX_<file name>_NN`. Geometry, transforms, sockets and parenting are unchanged; only the UCX names differ.

| FBX | UCX prefix | UCX | Sockets |
|---|---|---|---|
| Corvette4C/SM_Corvette4C_Body.fbx | `UCX_SM_Corvette4C_Body_` | 140 | 59 |
| Corvette4C/SM_Corvette4C.fbx (reference) | `UCX_SM_Corvette4C_` | 140 | 59 |
| Corvette4C/parts/SM_Corvette4C_<Part>.fbx (22) | `UCX_SM_Corvette4C_<Part>_` | 1 each | turret base 1, gun 2 |
| FighterC/SM_FighterC_Body.fbx | `UCX_SM_FighterC_Body_` | 52 | 23 |
| FighterC/SM_FighterC.fbx (reference) | `UCX_SM_FighterC_` | 52 | 23 |
| FighterC/parts/SM_FighterC_<part>.fbx (16) | `UCX_SM_FighterC_<part>_` | 1 each | belly guns 1 |
| FighterC/SK_FighterC.fbx | none (skeletal) | 0 | 0 |
| Corvette4C_BoardingTubes/SM_BoardingTube_BP2/BP3.fbx | `UCX_SM_BoardingTube_BP2_` / `_BP3_` | 17 each | 5 each |
| Corvette4C_BoardingTubes/parts/*_Base/_Bellows/_Head | `UCX_<file name>_` | 5 each | 0 |
| Corvette4C_BoardingTubes/parts/*_Door_Inner/_Outer | `UCX_<file name>_` | 1 each | 0 |

Stray objects: none in any of the 55 FBX (re-import check, no `Cube`/`Camera`/`Light` or unprefixed objects). The export functions now drop any such object and rename UCX to the file name automatically.
If a `Cube` still shows in UE, delete the old imported assets first; it isn't in the current FBX.

## Collision naming fix, corrected (2026-10-30, 06:50 CDT)
The 00:05 fix was wrong: it named hulls after the FBX file (`UCX_SM_Corvette4C_Body_NN`). UE matches `UCX_` pieces to the **mesh object name inside the FBX** (Vega confirmed). The hulls belong to `SM_Corvette4C_Hull` / `SM_FighterC_Hull`, so they are now `UCX_SM_Corvette4C_Hull_NN` and `UCX_SM_FighterC_Hull_NN` in both the Body and reference files. The 4C and Fighter files were re-exported from the saved .blend (no rebuild); a re-import diff against the previous export shows identical vertices, UVs, transforms, materials, parenting and sockets (max difference 0.0). Only the UCX names changed. Parts and tubes already matched their LodGroup object names and were not renamed. **Delete the old imported 4C/Fighter Body assets before re-importing.**

| FBX | UCX prefix (= object) | UCX | Sockets |
|---|---|---|---|
| Corvette4C/SM_Corvette4C_Body.fbx and SM_Corvette4C.fbx | `UCX_SM_Corvette4C_Hull_` | 140 | 59 |
| Corvette4C/parts/SM_Corvette4C_<Part>.fbx (22) | `UCX_SM_Corvette4C_<Part>_` | 1 each | turret base 1, gun 2 |
| FighterC/SM_FighterC_Body.fbx and SM_FighterC.fbx | `UCX_SM_FighterC_Hull_` | 52 | 23 |
| FighterC/parts/SM_FighterC_<part>.fbx (16) | `UCX_SM_FighterC_<part>_` | 1 each | belly guns 1 |
| Corvette4C_BoardingTubes/SM_BoardingTube_BP2/BP3.fbx | `UCX_SM_BoardingTube_BP2_` / `_BP3_` | 17 each | 21 / 27 |
| Corvette4C_BoardingTubes/parts/*_Base / _Bellows / _Head | `UCX_<file name>_` | 5 each | grips: 1 / 14 (BP2) or 20 (BP3) / 1 |
| Corvette4C_BoardingTubes/parts/*_Door_Inner / _Outer | `UCX_<file name>_` | 1 each | 0 |

### Rail grip sockets (2026-10-30, 06:50 CDT, for Juno's RailPull regrab / Vega's hand IK)
- `SOCKET_RailGrip_##` (UE name `RailGrip_##`), two digits, numbered **from the base outward**: `01` = `_Base` rail piece, then one per `_Bellows` rail piece (BP2 `02`–`15`, BP3 `02`–`21`), last = `_Head` piece (BP2 `16`, BP3 `22`). 16 on BP2, 22 on BP3.
- Each sits at the **centre of its rail piece on the rail axis** (local Y 0.90, Z 0.05 = floor + 1.25 m), rotation identity: socket +X runs along the rail (outward), +Z up.
- Parenting: in `parts/` each grip is a child of the LOD0 mesh of the part that carries that rail piece (`_Base_LOD0`, `_Bellows_LOD0`, `_Head_LOD0`), so bellows grips follow the bellows X-scale and the head grip travels with the head. In the combined `SM_BoardingTube_BPx.fbx` (static, extended) all grips are on `SM_BoardingTube_BPx_LOD0`, next to the 5 existing sockets (now 21 / 27 total).
- **Retracted spacing:** the bellows scale X to 0.18 about the bellows pivot (X 0.65), so bellows grip spacing shrinks from 0.2943 m to 0.0530 m (BP2) and 0.2965 m to 0.0534 m (BP3); the base grip stays put and the head grip moves with the head. Use the nearest grip to the hand in world space at runtime; don't cache positions.

| Tube | Socket | Part (parent in parts/) | Extended X | Retracted X |
|---|---|---|---|---|
| BP2 | `SOCKET_RailGrip_01` | `_Base` | 0.6050 | 0.6050 |
| BP2 | `SOCKET_RailGrip_02` | `_Bellows` | 0.7971 | 0.6765 |
| BP2 | `SOCKET_RailGrip_03` | `_Bellows` | 1.0914 | 0.7295 |
| BP2 | `SOCKET_RailGrip_04` | `_Bellows` | 1.3857 | 0.7824 |
| BP2 | `SOCKET_RailGrip_05` | `_Bellows` | 1.6800 | 0.8354 |
| BP2 | `SOCKET_RailGrip_06` | `_Bellows` | 1.9743 | 0.8884 |
| BP2 | `SOCKET_RailGrip_07` | `_Bellows` | 2.2686 | 0.9413 |
| BP2 | `SOCKET_RailGrip_08` | `_Bellows` | 2.5629 | 0.9943 |
| BP2 | `SOCKET_RailGrip_09` | `_Bellows` | 2.8571 | 1.0473 |
| BP2 | `SOCKET_RailGrip_10` | `_Bellows` | 3.1514 | 1.1003 |
| BP2 | `SOCKET_RailGrip_11` | `_Bellows` | 3.4457 | 1.1532 |
| BP2 | `SOCKET_RailGrip_12` | `_Bellows` | 3.7400 | 1.2062 |
| BP2 | `SOCKET_RailGrip_13` | `_Bellows` | 4.0343 | 1.2592 |
| BP2 | `SOCKET_RailGrip_14` | `_Bellows` | 4.3286 | 1.3121 |
| BP2 | `SOCKET_RailGrip_15` | `_Bellows` | 4.6229 | 1.3651 |
| BP2 | `SOCKET_RailGrip_16` | `_Head` | 4.8500 | 1.4716 |
| BP3 | `SOCKET_RailGrip_01` | `_Base` | 0.6050 | 0.6050 |
| BP3 | `SOCKET_RailGrip_02` | `_Bellows` | 0.7983 | 0.6767 |
| BP3 | `SOCKET_RailGrip_03` | `_Bellows` | 1.0947 | 0.7301 |
| BP3 | `SOCKET_RailGrip_04` | `_Bellows` | 1.3912 | 0.7834 |
| BP3 | `SOCKET_RailGrip_05` | `_Bellows` | 1.6877 | 0.8368 |
| BP3 | `SOCKET_RailGrip_06` | `_Bellows` | 1.9842 | 0.8902 |
| BP3 | `SOCKET_RailGrip_07` | `_Bellows` | 2.2807 | 0.9435 |
| BP3 | `SOCKET_RailGrip_08` | `_Bellows` | 2.5772 | 0.9969 |
| BP3 | `SOCKET_RailGrip_09` | `_Bellows` | 2.8737 | 1.0503 |
| BP3 | `SOCKET_RailGrip_10` | `_Bellows` | 3.1702 | 1.1036 |
| BP3 | `SOCKET_RailGrip_11` | `_Bellows` | 3.4667 | 1.1570 |
| BP3 | `SOCKET_RailGrip_12` | `_Bellows` | 3.7632 | 1.2104 |
| BP3 | `SOCKET_RailGrip_13` | `_Bellows` | 4.0598 | 1.2638 |
| BP3 | `SOCKET_RailGrip_14` | `_Bellows` | 4.3563 | 1.3171 |
| BP3 | `SOCKET_RailGrip_15` | `_Bellows` | 4.6528 | 1.3705 |
| BP3 | `SOCKET_RailGrip_16` | `_Bellows` | 4.9493 | 1.4239 |
| BP3 | `SOCKET_RailGrip_17` | `_Bellows` | 5.2458 | 1.4772 |
| BP3 | `SOCKET_RailGrip_18` | `_Bellows` | 5.5423 | 1.5306 |
| BP3 | `SOCKET_RailGrip_19` | `_Bellows` | 5.8388 | 1.5840 |
| BP3 | `SOCKET_RailGrip_20` | `_Bellows` | 6.1353 | 1.6373 |
| BP3 | `SOCKET_RailGrip_21` | `_Bellows` | 6.4318 | 1.6907 |
| BP3 | `SOCKET_RailGrip_22` | `_Head` | 6.6600 | 1.7974 |

All at local Y 0.90, Z 0.05. Positions come from `build_stats.json` (`rail_grips`).
