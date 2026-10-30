#!/usr/bin/env python3
"""Builds C4C_SoundList.csv/.md from LICENSES.csv (lengths, licenses) and the door/launch timings."""
import csv, os
AV = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))); os.chdir(AV)
lic = {r['file']: r for r in csv.DictReader(open('LICENSES.csv'))}
NEW = {'Hangar/SFX_Hangar_Clamp_Release_OneShot.wav', 'Hangar/SFX_Hangar_Clamp_Lock_OneShot.wav'} | {f'Craft/SFX_{c}_{n}.wav' for c in ('Fighter', 'Dropship') for n in ('Gear_Retract_OneShot', 'Engine_SpoolUp_OneShot', 'Engine_Idle_Loop', 'RCS_Hover_Loop', 'MainDrive_Launch_OneShot')}
R = []
import glob
def variants(wav):
    # a cue that names variant _01 plays the whole random set (_01.._NN) through one Random node
    base = wav.rsplit('_01.wav', 1)
    if len(base) == 2:
        vs = sorted(os.path.relpath(p, 'Audio') for p in glob.glob(f'Audio/{base[0]}_[0-9][0-9]{base[1]}.wav'))
        return vs or [wav]
    return [wav]
def row(i, area, event, trigger, src, frame, t, wav, attach, cls, notes=''):
    f = 'Audio/' + wav; assert f in lic, f
    for v in variants(wav): assert 'Audio/' + v in lic, v
    R.append(dict(id=i, area=area, event=event, trigger=trigger, trigger_source=src, frame_30fps=frame, time_s=t, wav=f, wavs=';'.join('Audio/' + v for v in variants(wav)), seconds=lic[f]['seconds'], loop=lic[f]['loop'],
                  status='new (graybox pass)' if wav in NEW else 'existing', license=lic[f]['license'], attach=attach, sound_class=cls, notes=notes))
A = '4C interior ambience'
row('SND_4C_AMB_001', A, 'Ship bed, Normal', 'AlertLevel Normal', 'presentation component', '', '', 'Ambience/SFX_Amb_Ship_Normal_Loop.wav', '2D bed per section (Audio Gameplay Volume)', 'SFX_ShipAmbience')
row('SND_4C_AMB_002', A, 'Ship bed, Red Alert / combat', 'AlertLevel RedAlert', 'presentation component', '', '', 'Ambience/SFX_Amb_Ship_Combat_Loop.wav', '2D bed', 'SFX_ShipAmbience', 'crossfade 1.5 s from AMB_001')
row('SND_4C_AMB_003', A, 'Ship bed, Power Loss', 'section PowerLost', 'presentation component', '', '', 'Ambience/SFX_Amb_Ship_PowerLoss_Loop.wav', '2D bed', 'SFX_ShipAmbience', 'fans and hum gone')
row('SND_4C_AMB_004', A, 'Reactor hall room tone', 'listener in DC_4C_08_Reactor', 'Audio Gameplay Volume', '', '', 'Ambience/SFX_Amb_ReactorRoom_Loop.wav', '3D at reactor', 'SFX_ShipAmbience')
row('SND_4C_AMB_005', A, 'Hangar room tone', 'listener in DC_4C_05_Hangar', 'Audio Gameplay Volume', '', '', 'Ambience/SFX_Amb_Hangar_Loop.wav', '2D bed in hangar (RT60 ~3 s)', 'SFX_ShipAmbience')
row('SND_4C_AMB_006', A, 'Fixture buzz', 'fixture Health < 0.5', 'BP_ShipLight', '', '', 'Ambience/SFX_Light_Fixture_Buzz_Loop.wav', '3D at fixture, 4 m', 'SFX_ShipAmbience')
row('SND_4C_AMB_007', A, 'Vacuum (suit interior)', 'listener section Vacuum', 'presentation component', '', '', 'Ambience/SFX_Amb_Vacuum_SuitInterior_Loop.wav', '2D', 'SFX_ShipAmbience', 'Mix_Vacuum')
row('SND_4C_AMB_008', A, 'Relay clacks (light state change)', 'light group state change', 'presentation component', '', '', 'Power/SFX_Relay_Clack_01.wav', '3D at section switchboard', 'SFX_ShipAmbience', 'random 01-03')
row('SND_4C_GRV_001', A, 'Gravity engine off (zero-g)', 'OnGravityStateChanged: New = Off (also Failing -> Off)', 'UShipDamageStateComponent', '', '', 'Power/SFX_GravityEngine_SpinDown_OneShot.wav', '2D -6 dB ship-wide + 3D at gravity engine', 'SFX_ShipDamage')
row('SND_4C_GRV_002', A, 'Gravity engine on (restored)', 'OnGravityStateChanged: New = Restoring', 'UShipDamageStateComponent', '', '', 'Power/SFX_GravityEngine_SpinUp_OneShot.wav', '2D -6 dB + 3D at gravity engine', 'SFX_ShipDamage', 'field engages at 5.0 s; loose props drop')
row('SND_4C_GRV_003', A, 'PA: gravity loss', 'OnGravityStateChanged: New = Off (once per loss)', 'ship PA component', '', '', 'VO/PA/TTS/VO_PA_Generic_PA_Gravity_Loss_TTS.wav', 'PA speakers (SOCKET_PA_*)', 'VO_PA', 'TTS placeholder')
row('SND_4C_GRV_004', A, 'Mag-boot steps in zero-g', 'Footstep_L/R notify while GravityState == Off (OnGravityStateChanged swaps the footstep cue)', 'crew AnimNotify + UShipDamageStateComponent', '', '', 'Crew/SFX_Foley_MagBoot_Clank_01.wav', 'ball_l / ball_r', 'SFX_Foley', 'random 01-04; replaces deck-plate footsteps')
row('SND_4C_GRV_005', A, 'Extinguisher recoil in zero-g', 'SprayStart notify while GravityState == Off', 'crew AnimNotify + UShipDamageStateComponent', '', '', 'Crew/SFX_Foley_ZeroG_Recoil_01.wav', 'spine_03', 'SFX_Foley', 'random 01-02; layered on the spray start')
H = 'Hangar launch sequence'
row('SND_HGR_DOOR_001', H, 'Door cycle horn (lead-in + cycle)', 'door Blueprint: start horn, wait 1.0 s, then play Open/Close', 'door Blueprint', '-30', '-1.00', 'Alarms/SFX_Alarm_Hangar_DoorCycle_Loop.wav', '3D at bay centre + PA speakers', 'SFX_ShipAlarms', '3.0 s loop, stops at clip end (7.0 s total)')
row('SND_HGR_DOOR_002', H, 'Door open (latches, push-out, swing, settle)', 'AN_HangarDoor_Sound f0; sync PushStart f6, SwingStart f44, Settle f162', 'A_Corvette4C_HangarDoor_Open', '0', '0.00', 'Doors/SFX_HangarDoor_Open_OneShot.wav', 'door seam (between Mount_HangarDoor_L/R), 60 m', 'SFX_ShipDamage', 'frame-synced 6.00 s')
row('SND_HGR_DOOR_003', H, 'Door close (swing, pull-in, seal)', 'AN_HangarDoor_Sound f0; sync SwingStart f6, SealStart f132, Sealed f172', 'A_Corvette4C_HangarDoor_Close', '0', '0.00', 'Doors/SFX_HangarDoor_Close_OneShot.wav', 'door seam, 60 m', 'SFX_ShipDamage', 'frame-synced 6.00 s')
row('SND_HGR_FLD_001', H, 'Door field hum at rest', 'field up and doors open', 'presentation component', '', '', 'Shield/SFX_HangarShield_Field_Loop.wav', 'aperture plane (HangarLaunch_Threshold), 6 m', 'SFX_ShipAmbience', '-30 LUFS')
import json
ANIM = '/workspace/space_rpg/art/animation'
for c in ('Fighter', 'Dropship'):
    clip = f'A_{c}_HangarLaunch_01'; nf = f'art/animation/{c}/{clip}_notifies.json'
    N = {n['name']: n['frame'] for n in json.load(open(f'{ANIM}/{c}/{clip}_notifies.json'))[clip]['notifies']}
    assert N['LaunchSpoolUp'] == 0
    lift, gr, burn, cross, clamp = N['LiftOff'], N['GearRetract'], N['BurnStart'], N['HangarFieldCrossed'], N['ClampRelease']
    k = 'FTR' if c == 'Fighter' else 'DRP'; src = nf
    row(f'SND_HGR_{k}_001', H, f'{c} engine spool-up', 'LaunchSpoolUp (confirmed) f0', src, '0', '0.00', f'Craft/SFX_{c}_Engine_SpoolUp_OneShot.wav', 'craft engine sockets', 'SFX_Craft', '4 s, then idle loop')
    row(f'SND_HGR_{k}_002', H, f'{c} engine idle', 'after spool-up until BurnStart', src, '120', '4.00', f'Craft/SFX_{c}_Engine_Idle_Loop.wav', 'craft engine sockets', 'SFX_Craft', 'fade out 1 s at burn')
    row(f'SND_HGR_{k}_003', H, f'{c} clamps release', f'ClampRelease (confirmed) f{clamp}', src, str(clamp), f'{clamp / 30:.2f}', 'Hangar/SFX_Hangar_Clamp_Release_OneShot.wav', 'each of the 3 clamps at the spot, 30 ms apart', 'SFX_Craft')
    row(f'SND_HGR_{k}_004', H, f'{c} RCS lift and creep out', f'LiftOff (confirmed) f{lift} until BurnStart', src, str(lift), f'{lift / 30:.2f}', f'Craft/SFX_{c}_RCS_Hover_Loop.wav', 'craft RCS sockets', 'SFX_Craft')
    row(f'SND_HGR_{k}_005', H, f'{c} gear retract', f'GearRetract (confirmed) f{gr}', src, str(gr), f'{gr / 30:.2f}', f'Craft/SFX_{c}_Gear_Retract_OneShot.wav', 'gear legs', 'SFX_Craft')
    row(f'SND_HGR_{k}_006', H, f'{c} crosses door field', f'HangarFieldCrossed (confirmed) f{cross}', src, str(cross), f'{cross / 30:.2f}', 'Shield/SFX_HangarShield_PassThrough_OneShot.wav', 'aperture plane at the craft', 'SFX_ShipDamage', 'notify-driven (no path check); also fires the 3-4 ripple wake VFX')
    row(f'SND_HGR_{k}_007', H, f'{c} main-drive burn and departure', f'BurnStart (confirmed) f{burn}', src, str(burn), f'{burn / 30:.2f}', f'Craft/SFX_{c}_MainDrive_Launch_OneShot.wav', 'craft engine sockets (moves away)', 'SFX_Craft', 'heard through the open door')
row('SND_HGR_CLP_001', H, 'Clamps lock (craft recovered)', 'ClampLock on landing', 'landing sequence', '', '', 'Hangar/SFX_Hangar_Clamp_Lock_OneShot.wav', 'each clamp', 'SFX_Craft')
with open('graybox_4C/C4C_SoundList.csv', 'w', newline='') as fh:
    w = csv.DictWriter(fh, fieldnames=list(R[0].keys())); w.writeheader(); w.writerows(R)
md = ['# Corvette 4C interior + hangar launch: sound list (gray-box pass)', '',
 'Source of truth: `C4C_SoundList.csv` (same rows). WAVs are in `audio_vfx/Audio/` (48 kHz / 24-bit), licensed per `audio_vfx/LICENSES.csv`. **New in this pass: 12 files** (tie-down clamps, fighter/dropship spool-up, idle, RCS, gear retract, main-drive launch), generated by `tools/gen_audio.py` (`launch_v18`), original work owned by Stephen Cooper.', '',
 'Timing: the launch clips start on the same tick as `A_Corvette4C_HangarDoor_Open` (frame 0); the door Blueprint starts the horn 1.0 s earlier. Door notifies (`PushStart` f6, `SwingStart` f44, `Settle` f162; close `SwingStart` f6, `SealStart` f132, `Sealed` f172) come from `art/animation/Corvette4C/A_Corvette4C_HangarDoor_notifies.json`. Launch notifies are **confirmed** and read from `art/animation/Fighter/A_Fighter_HangarLaunch_01_notifies.json` and `art/animation/Dropship/A_Dropship_HangarLaunch_01_notifies.json` (30 fps): `LaunchSpoolUp`, `ClampRelease`, `LiftOff`, `GearRetract`, `HangarFieldCrossed` (f264 / f311) and `BurnStart`. The door-field pass-through plays on `HangarFieldCrossed`, not on a path check. Add all notifies by hand in UE (FBX does not carry them).', '',
 '| ID | Event | Trigger | Frame | t (s) | WAV | Len (s) | Loop | Status | Attach |', '|---|---|---|---|---|---|---|---|---|---|']
for r in R: md.append(f"| {r['id']} | {r['event']} | {r['trigger']} | {r['frame_30fps']} | {r['time_s']} | `{r['wav'][6:]}` | {r['seconds']} | {r['loop']} | {r['status']} | {r['attach']} |")
md += ['', 'Launch timeline (fighter / dropship, seconds from door-open frame 0): horn −1.0 → door open + spool-up 0.0 → clamps 3.0 / 3.3 → lift-off (RCS) 3.33 / 3.67 → idle loop from 4.0 → gear 4.5 / 5.0 → door settled 5.4 → creep out 6.0 → field crossing 8.8 / 10.37 → burn 12.0 / 14.0.',
 '', 'Gravity: `SND_4C_GRV_*` hook to `OnGravityStateChanged` (`EGravityState` On / Failing / Off / Restoring). In zero-g, footsteps become mag-boot clanks.']
open('graybox_4C/C4C_SoundList.md', 'w').write('\n'.join(md) + '\n')
print(len(R), 'rows')
