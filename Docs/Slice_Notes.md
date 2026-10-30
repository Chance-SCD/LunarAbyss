# Gray-box slice notes (Vega)

## Controls
| Key | On foot (ACrewCharacter) | Fighter (AFighterPawn) |
|---|---|---|
| WASD / mouse | move / look (jog 3.0 m/s) | throttle, strafe / pitch, yaw |
| Left Alt | toggle walk 1.25 m/s | - |
| C | toggle crouch 1.0 m/s | - |
| Space / L Ctrl | jump; in zero-g push up/down | lift up/down |
| F | near pilot seat (2.5 m): launch fighter + take control; near parked fighter (8 m): take control | back to crew |
| L | launch the parked fighter from the hangar and take control | same |
| G | toggle gravity engine (test) | same |

Console: `SwitchToFighter`, `SwitchToCrew`, `LaunchFighter`, `ToggleGravity`, `DamageGravity 220`, `SetCorvetteSpeed 250`.

## Frame of reference (decision)
The corvette stays at the world origin (`ACorvetteHangar::bMoveInWorld = false`, default). Its speed
(`TargetSpeed`, ramped by `Acceleration` 30 m/s^2 up to `MaxSpeed` 250 m/s) is a ship-frame value, not an
actor translation. Reason: a kinematic base moving at 250 m/s (25,000 cm/s) moves the floor ~4 m per frame
at 60 fps; CharacterMovement based movement does follow it, but it is sensitive to tick order, sweeps against
moving UCX, and coordinates grow to 10^7 cm within minutes (float precision in physics/animation).
Keeping the ship still makes walking, ladders and zero-g rock solid. When a backdrop/universe actor exists,
it should be offset by `-CurrentVelocity` each tick (not built yet). `bMoveInWorld = true` still works for tests.
Fighter launch: speed relative to the ship = `LaunchSpeed` (10 m/s) along the launch socket's +X.

## Art and launch point
Ships are Forge's final handoff (Fighter C v1.3, Corvette 4C v6.3, boarding tubes) via the Body + parts route
(`/Game/Ships/...`). Walkable interior = Atlas-V's gray-box (`/Game/Blockouts/SM_Corvette4C_Interior_Blockout`).
UE strips `SOCKET_` on import; code accepts both forms.
Parked fighter: hull socket `Hangar_Spot_Fighter` (-3.825, 3.5, 9.667) m, nose along the launch lane.
Launch: teleport to `HangarLaunch_Threshold`, pointed at `HangarLaunch_Clear` (out of the port aperture).
Fighter cockpit camera = `Camera_Cockpit` socket; chase arm from `Camera_Chase`.
Pawns ignore the hull's collision (its 140 UCX hulls would enclose the decks); crew collide with the interior only.
Parts (hangar doors, turrets, gear) and tubes are imported but not yet attached/animated.

## Gravity engine (UGravityEngineComponent on the corvette)
HP 220 (Orion, TENTATIVE). Offline at 0 HP or by G; gravity ramps 1 -> 0 over 1.0 s (RampOutTime), then crew inside the interior bounds go MOVE_Flying,
gravity 0, a random drift of ~15 cm/s, push-off accel 0.6 m/s^2, max 1.5 m/s, no braking.
Online again: crew go MOVE_Falling and gravity ramps 0 -> 1 over 1.5 s, then walk on landing.
Fall damage (Ship_Combat_Stats_Spec v0.4.5): from impact speed v, 15 x (v^2 / (2 x 9.81) - 3) HP; only falls that
start during a gravity restore are capped at 40% of MaxHealth (140 -> 56). Jump Z = 3.5 m/s x (g / 9.81)^0.25.
Stub dock point: `DockPortPoint` on hull socket `DockCollar_Port` (yaw -90 in UE). Corvette: Max 250, Boost 320 m/s, accel 30 m/s^2. Fighter: 450/600, 120.

## Import
`Scripts/import_blockouts.py` (headless, see header). Combine Meshes ON (one asset each), sockets from the FBX
plus missing ones from `Scripts/blockout_sockets.json` (extracted from Atlas-V's blend), interior collision
complex-as-simple.

## Jump (Data/DT_Slice_MoveTuning.csv -> /Game/Data/DT_Slice_MoveTuning, row struct FSliceMoveTuningRow)
Base jump height 0.6 m at 1 g (v0 = sqrt(2 x 9.81 x 0.6) = 3.43 m/s), velocity x (g / 9.81)^0.25 with current gravity,
apex capped at 1.4 m while indoors (flag set by the gravity engine when inside the interior bounds; `SetIndoors` for volumes).
Below 0.1 g: `IsMagBootMode()` true (stub, no effect yet).
