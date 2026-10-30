# Corvette 4C gray-box lighting + hangar launch sound list (UE 5.8.3)

| File | What |
|---|---|
| `C4C_GrayboxLights.json` / `.csv` | 653 lights: every room, corridor, stair landing and airlock from `levels/interior_data.json`, emergency twins (49) and beacons (40) on the `levels/interior_sockets.csv` sockets, plus the hangar (12 high-bays, 8 battle reds, 4 work floods aimed at the craft spots, 6 door-edge reds, 3 beacons, 3 emergency twins). Per light: fixture/mesh, group, DC section, room, Blender m + UE cm (Y flipped), yaw/pitch, candela, attenuation, Kelvin or colour, IES, rect/cone size, `shadow_baseline`, `shadow_high`, `max_draw_distance_cm`, `ref_socket` (hangar). |
| `C4C_LightStatePresets.json` | Group multipliers and extras for **Normal, RedAlert, Damaged, Breach, PowerLoss, ZeroG** (+ `HangarDoorCycle` layer) and the two render paths. |
| `spawn_graybox_lights_ue.py` | Editor Utility (Python). Spawns the lights with tags (`GB4C`, group, section, room, fixture, ref socket), IES from `/Game/Ship/Lighting/IES`, `PATH = 'baseline' | 'high'`, previews a `STATE`, checks the hangar sockets on the imported `SM_Corvette4C_Body` (from `art/export_UE/Corvette4C/`), and can attach everything to the ship actor. |
| `budget_check.json` | Baseline shadow budget check (pass: max 12 shadow-casting lights in view, worst space C5-SPF). |
| `renders/` | Blender 5.2.2 Cycles previews, 960×540: hangar and the D1 keel corridor (C1-SPA) in Normal, RedAlert and PowerLoss (2.4 MB total). |
| `C4C_SoundList.csv` / `.md` | 31 cues: interior ambience, gravity engine on/off, door horn and open/close, door field, clamps, fighter and dropship spool-up / idle / RCS / gear / launch, mapped to the door and launch notify files. |
| `tools/` | `build_graybox_lights.py` (rebuilds the JSON/CSV/presets/budget), `render_previews_blender.py`, `build_sound_list.py`. |

## Render paths
- **Baseline (default, mid-range GPU, no ray tracing):** MegaLights off (`r.MegaLights.Allowed 0`), no HWRT. Only `shadow_baseline` lights cast shadows: 157 of 653 (main fixtures only; every 4th corridor panel, max 3 per room, 6 high-bays + 2 floods in the hangar), each with a 25 m shadow draw distance. Battle reds, emergency lights and beacons never cast in the baseline. Lumen software RT (High) or Lumen Lite (Medium). Flicker from the flicker subsystem, not light functions.
- **High (optional):** MegaLights + hardware RT; `shadow_high` lights cast (main, task, emergency); Light Function Atlas on.
- Budget method (`budget_check.json`): own space + open-connected corridors in full + at most one caster per door-connected room. Re-check in the editor with `stat lights` / `r.Shadow.Virtual` visualisation once the level is assembled.

## Style
Matte practical fixtures only (Kit housings `SM_Fixture_*`, `M_FixtureLens` driven by CPD `Intensity01`); no emissive strips. Colour temperature: 4000K corridors/general, 4500K engineering/reactor/plant spaces, 3000K quarters/mess/medbay, 3200K console tasks, 5000K hangar high-bays, 2700K emergency, #C4161C battle red, #FF8A00 beacons (red #D01A10 during the hangar door cycle).

## Inputs
`levels/interior_sockets.csv`, `levels/interior_data.json`, the 4C model v6.3 and its UE export (`art/export_UE/Corvette4C/SM_Corvette4C_Body.fbx`, 59 sockets: `Hangar`, `Hangar_Spot_Fighter`, `Hangar_Spot_Dropship`, `Mount_HangarDoor_L/R`, `HangarLaunch_Threshold`), `art/models/Kit` fixture housings, `audio_vfx/IES/`.

## Open
- Launch notifies: confirmed, read from the two `*_HangarLaunch_01_notifies.json` files.
- The renders are an approximation (candela → watts, gray matte blockout, all shadows on). The UE baseline is the reference.
- Rebuild: `python3 tools/build_graybox_lights.py`, then `blender -b --factory-startup -P tools/render_previews_blender.py -- <hangar|corridor> <Normal|RedAlert|PowerLoss>`, then `python3 tools/build_sound_list.py`.

## v1.8 (realism pass + UE script fixes)
- `C4C_GrayboxLights.json/.csv`: 664 lights with physical intensities. Each light has `lux_floor_direct`; the targets are in spec §11.1.
- `C4C_MaterialRealism.json`: albedo, roughness and metallic ranges, plus grime and edge-wear layers.
- `C4C_Baseline_DeviceProfile.ini`: the baseline non-RT profile (`r.MegaLights.Allowed=0`) and the High profile.
- `C4C_GravityAudioMap.json`: maps `OnGravityStateChanged` events and notifies to the gravity cues.
- `ue/ue_build_sound_cues.py`: builds the cues through `initial_sound_waves` and verifies them (AssetRegistry dependencies, duration, size); exits 1 on failure.
- `ue/ue_add_lights_and_sounds.py`: unattended runner for IES, cues, baseline lights and save.
  `UnrealEditor-Cmd.exe <proj> -run=pythonscript -script="<path>/ue/ue_add_lights_and_sounds.py" -unattended`
  Environment variables: `GB4C_ON_EXISTING=overwrite|skip`, `GB4C_AUDIO_SRC`, `GB4C_IES_SRC`, `GB4C_MAP`.
- `spawn_graybox_lights_ue.py`: no socket reads, `log()` defined, `main(..., on_existing='overwrite'|'skip')`; the baseline applies its cvars.

## v1.8.1 (fixes after playtest_03)
- `ue/cpp/SliceAudioLibrary.h/.cpp`: drop-in replacement for `Source/LunarAbyss/SliceAudioLibrary.*`. It adds `BuildMultiWaveCue`, `GetCueWavePaths` and `GetCueDuration`, so rebuild the editor target before running the cue script.
- `ue/ue_build_sound_cues.py`: rebuilds each cue in place, or creates it with `create_asset` and a factory, with no dialogs. Multi-wave cues use a Random node with every wave wired. The size check is gone.
- MegaLights cvar: `r.MegaLights.Allowed=0` (device profile) plus `r.MegaLights.EnableForProject=0` (`DefaultEngine.ini`).

## v1.8.2
- `ue/cpp/SliceAudioLibrary.cpp`: Random children are added with `InsertChildNode` and removed with `RemoveChildNode`, matching Vega's fix. Setting `ChildNodes` directly tripped the `SoundCueGraph.cpp:61` pin assertion.
- `ue/ue_build_sound_cues.py`: imports every wave first, saves them (`save_loaded_assets`), rescans and reloads them, then builds the cues. Each cue is saved and rescanned before its check, so a fresh project passes in one run.

## v1.8.3
- `ue/ue_build_sound_cues.py`: the .uasset size test is back, alongside the wave-link, dependency and duration checks. A cue fails if its file is under **5000 B**.
  - Why 5000 B: empty cues saved at about 4 KB. In the playtest_03 report the smallest wired cues were 5394–5436 B (single-wave cues, e.g. `SND_4C_GRV_001` = 5426 B), and 5000 B falls between the two.
  - The old 6 KB limit flagged good cues. Override with `GB4C_MIN_CUE_BYTES`.
- The report is written to `code/playtest_03/gb4c_audio_report.txt`, with a copy in `Saved/`.
  - `code/playtest_03/` is looked for in the project folder and the folder above it, then `../space_rpg/`; if none exists, it is created under the project.
  - Override with `GB4C_REPORT_DIR`.

## v1.8.4
- `ue/ue_build_sound_cues.py`: the .uasset size test now only logs a warning and doesn't fail the run.
  - Reason: in Vega's clean v1.8.3 run (`code/playtest_03/qa155_0858/`), the 29 single-wave cues were wired correctly but saved at 4078–4133 B, the same size as empty cues. The 3 multi-wave cues were the only ones that passed.
- Pass or fail is decided by the wave-link check alone:
  - every wave must be reachable through `GetCueWavePaths`;
  - every wave must be a hard AssetRegistry dependency of the cue;
  - every wave must have a duration > 0.
- An empty graph fails with `no wave paths: graph is empty`. A missing C++ helper also fails, because the check can't run without it.
