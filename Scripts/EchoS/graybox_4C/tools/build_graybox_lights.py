#!/usr/bin/env python3
"""Builds the Corvette 4C gray-box light placement (JSON + CSV) and state presets.
Inputs : levels/interior_data.json (rooms, bay), levels/interior_sockets.csv (EmLight/Beacon/PA/Spark sockets),
         art/models/Corvette4C (v6.3) socket values below (SOCKET_Hangar, craft spots, door pivots).
Output : ../C4C_GrayboxLights.json, ../C4C_GrayboxLights.csv, ../C4C_LightStatePresets.json, ../budget_check.json
Coordinates: Blender metres (+X fwd, +Y port, +Z up) and UE cm (x100, Y flipped, yaw negated)."""
import json, csv, math, os
HERE = os.path.dirname(os.path.abspath(__file__)); OUT = os.path.dirname(HERE)
ROOT = '/workspace/space_rpg'
D = json.load(open(f'{ROOT}/levels/interior_data.json'))
SOCK = list(csv.DictReader(open(f'{ROOT}/levels/interior_sockets.csv')))
SEC = D['section_fnames']
# v6.3 model values (art/models/Corvette4C/README.md, FINAL_DIMENSIONS.md)
HANGAR = dict(origin=(6.675, 1.0, 9.667), x0=-15.325, x1=28.675, y0=-12.0, y1=14.0, floor=9.667, ceiling=21.667)  # 44 x 26 x 12 m clear
SPOT_FIGHTER = (-3.825, 3.5); SPOT_DROPSHIP = (16.175, 2.25)
APERTURE = dict(x0=-10.325, x1=23.675, y=14.3, z0=9.667, z1=18.667)
PERSONNEL_DOOR = (6.675, -12.0, 9.667)

FIX = {  # fixture type -> UE light settings (baseline). Intensity in candela.
 'CagedPanel':   dict(ue_class='RectLight',  group='LG_Main',      ies='IES_CagedPanel_Wide',   cd=1200, radius_cm=700,  w=60, h=30, mesh='SM_Fixture_CagedPanel'),
 'BulkheadCage': dict(ue_class='PointLight', group='LG_Main',      ies='IES_Bulkhead_Cage',     cd=600, radius_cm=600,  mesh='SM_Fixture_BulkheadCage'),
 'ConsoleTask':  dict(ue_class='SpotLight',  group='LG_Task',      ies='',                      cd=120, radius_cm=300,  cone=(15, 25), kelvin=3200, mesh='SM_Fixture_ConsoleTask'),
 'BattleRed':    dict(ue_class='SpotLight',  group='LG_Battle',    ies='IES_Spot_Beacon_Narrow', cd=300, radius_cm=600, cone=(40, 60), color='#C4161C', mesh='SM_Fixture_BattleRed'),
 'EmergencyTwin':dict(ue_class='SpotLight',  group='LG_Emergency', ies='IES_EmergencyTwinHead', cd=160, radius_cm=900,  cone=(15, 20), kelvin=2700, mesh='SM_Fixture_EmergencyTwin'),
 'Beacon':       dict(ue_class='SpotLight',  group='LG_Beacon',    ies='IES_Spot_Beacon_Narrow', cd=700, radius_cm=900,  cone=(8, 14), color='#FF8A00', mesh='SM_Fixture_BeaconRotating', rotating=True),
 'HighBay':      dict(ue_class='SpotLight',  group='LG_Main',      ies='IES_HighBay_Hangar',    cd=18000, radius_cm=2000, cone=(45, 60), kelvin=5000, mesh='SM_Fixture_HighBay'),
 'WorkFlood':    dict(ue_class='RectLight',  group='LG_Task',      ies='',                      cd=6000, radius_cm=2600, w=42, h=28, kelvin=4000, mesh='SM_Fixture_WorkFlood', barn=(60, 50)),
 'DoorEdgeRed':  dict(ue_class='PointLight', group='LG_Beacon',    ies='',                      cd=40,  radius_cm=400,  color='#D01A10', mesh='SM_Fixture_DoorEdgeRed'),
}
WARM = ('bunk', 'cabin', 'captain', 'officer', 'heads', 'barracks', 'mess', 'wardroom', 'rec room', 'medbay')
ENG = ('reactor', 'engineering', 'pump', 'life support', 'shield generator', 'workshop', 'fuel', 'tanks', 'parts')
CONSOLE_ROOMS = {'D5-01': 6, 'D5-03': 6, 'D5-02': 2, 'D3-05': 4, 'D5-19': 3, 'D5-14': 3, 'D5-15': 3, 'D3-01': 3, 'D4-07': 2, 'D2-03': 2, 'D2-02': 2}

def kelvin_for(r):
    n = r['name'].lower()
    if any(k in n for k in WARM): return 3000
    if any(k in n for k in ENG): return 4500
    return 4000

lights = []
def add(lid, ftype, x, y, z, room, section, yaw=0.0, pitch=-90.0, kelvin=None, shadow_base=False, note=''):
    f = FIX[ftype]
    lights.append(dict(id=lid, fixture=ftype, ue_class=f['ue_class'], mesh=f['mesh'], group=f['group'], room=room, section=section,
        x_m=round(x, 3), y_m=round(y, 3), z_m=round(z, 3), ue_x=round(x * 100, 1), ue_y=round(-y * 100, 1), ue_z=round(z * 100, 1),
        ue_yaw=round(-yaw, 1), ue_pitch=pitch, intensity_cd=f['cd'], attenuation_cm=f['radius_cm'],
        kelvin=kelvin or f.get('kelvin', 0), color=f.get('color', ''), ies=f['ies'],
        rect_w_cm=f.get('w', 0), rect_h_cm=f.get('h', 0), cone_inner=f.get('cone', (0, 0))[0], cone_outer=f.get('cone', (0, 0))[1],
        shadow_baseline=shadow_base, shadow_high=f['group'] in ('LG_Main', 'LG_Task', 'LG_Emergency'), megalights_high=True,
        rotating=f.get('rotating', False), note=note))

rooms = [r for r in D['rooms']]
for r in rooms:
    rid = r['id']; sec = SEC.get(r['dc'], r['dc']); k = kelvin_for(r)
    L, W = r['x1'] - r['x0'], r['y1'] - r['y0']
    if r['kind'] == 'ladder':
        n_ = max(2, int(r['h'] // 3))
        for i in range(n_):
            add(f'L_{rid}_Main_{i+1:02d}', 'BulkheadCage', (r['x0'] + r['x1']) / 2, r['y0'] + 0.1, r['z0'] + 1.5 + i * 3.0, rid, sec, yaw=90, pitch=0, kelvin=4000, shadow_base=(i == 0), note='caged ladder trunk, wall-mounted')
        continue
    if r['kind'] == 'stair':
        for dk in r['decks']:
            zf = D['DZ'][dk]; add(f'L_{rid}_{dk}_Main', 'BulkheadCage', (r['x0'] + r['x1']) / 2, r['y0'] + 0.1, zf + 2.6, rid, sec, yaw=90, pitch=0, kelvin=4000, shadow_base=True, note='landing, wall-mounted')
        continue
    ceil = r['z0'] + r['h'] - 0.16
    if r['kind'] == 'airlock':
        add(f'L_{rid}_Main_01', 'BulkheadCage', r['x0'] + 0.2, (r['y0'] + r['y1']) / 2, r['z0'] + 2.4, rid, sec, pitch=0, kelvin=4000, shadow_base=True)
        add(f'L_{rid}_Main_02', 'BulkheadCage', r['x1'] - 0.2, (r['y0'] + r['y1']) / 2, r['z0'] + 2.4, rid, sec, yaw=180, pitch=0, kelvin=4000)
        add(f'L_{rid}_Red_01', 'BattleRed', (r['x0'] + r['x1']) / 2, (r['y0'] + r['y1']) / 2, ceil, rid, sec)
        continue
    if r['kind'] == 'open_top':  # cockpit: no ceiling; console task lights + one low bulkhead
        add(f'L_{rid}_Main_01', 'BulkheadCage', r['x0'] + 0.3, 0.0, r['z0'] + 2.2, rid, sec, pitch=0, kelvin=3200, shadow_base=True, note='cockpit back wall; instruments are task lights')
    else:
        long_x = L >= W; length, width = (L, W) if long_x else (W, L)
        corr = r['kind'] in ('corr', 'lobby', 'junction')
        pitch_ = 3.0 if corr else 3.4
        n_l = max(1, round(length / pitch_)); n_w = 1 if (corr or width < 4.5) else max(2, round(width / 4.0))
        idx = 0; mains = []
        for i in range(n_l):
            for j in range(n_w):
                a = -length / 2 + (i + 0.5) * length / n_l; b = -width / 2 + (j + 0.5) * width / n_w
                x = (r['x0'] + r['x1']) / 2 + (a if long_x else b); y = (r['y0'] + r['y1']) / 2 + (b if long_x else a)
                idx += 1; mains.append((x, y, i, j))
        # baseline shadow casters: corridors every 3rd panel; rooms <= 4 (spread)
        nm = len(mains)
        if corr: shadow_set = set(range(1, nm, 4)) or {0}
        else:
            step = max(1, math.ceil(nm / min(3, nm))); shadow_set = set(range(0, nm, step))
        for n, (x, y, i, j) in enumerate(mains):
            add(f'L_{rid}_Main_{n+1:02d}', 'CagedPanel', x, y, ceil, rid, sec, yaw=0 if long_x else 90, kelvin=k, shadow_base=n in shadow_set)
            lights[-1]['intensity_cd'] = round(lights[-1]['intensity_cd'] * {4500: 1.6, 3000: 0.6}.get(k, 1.0) * (1.15 if r['h'] >= 3.45 else 1.0))
            if (corr and i % 2 == 0) or (not corr and n % 2 == 0):
                add(f'L_{rid}_Red_{n+1:02d}', 'BattleRed', x + (0.55 if long_x else 0), y + (0 if long_x else 0.55), ceil + 0.05, rid, sec)
    nc = CONSOLE_ROOMS.get(rid, 0)
    for c in range(nc):
        x = r['x0'] + (c + 0.5) * L / nc; y = r['y0'] + 0.6
        add(f'L_{rid}_Task_{c+1:02d}', 'ConsoleTask', x, y, r['z0'] + 1.45, rid, sec)

# sockets: emergency twins + beacons
for s in SOCK:
    if s['Type'] == 'EmLight':
        add('L_' + s['Name'].replace('SOCKET_', ''), 'EmergencyTwin', float(s['X_m']), float(s['Y_m']), float(s['Z_m']), s['Name'], s['Section'], yaw=float(s['Yaw_deg']), pitch=-35)
    elif s['Type'] == 'Beacon':
        add('L_' + s['Name'].replace('SOCKET_', ''), 'Beacon', float(s['X_m']), float(s['Y_m']), float(s['Z_m']), s['Name'], s['Section'], yaw=float(s['Yaw_deg']), pitch=-20)

# hangar (DC_4C_05_Hangar): 4 x 3 high-bay grid, work floods aimed at both craft spots, door-edge reds, door beacons (red), emergency twins
H = HANGAR; hs = 'DC_4C_05_Hangar'; n = 0
xs = [H['x0'] + (i + 0.5) * (H['x1'] - H['x0']) / 4 for i in range(4)]; ys = [H['y0'] + (j + 0.5) * (H['y1'] - H['y0']) / 3 for j in range(3)]
for i, x in enumerate(xs):
    for j, y in enumerate(ys):
        n += 1; add(f'L_HANGAR_HighBay_{n:02d}', 'HighBay', x, y, H['ceiling'] - 1.3, 'HANGAR', hs, shadow_base=(i + j) % 2 == 0)
for i, x in enumerate(xs):   # hangar battle reds beside every high-bay column, back and front rows
    for j, y in ((0, ys[0]), (2, ys[2])):
        add(f'L_HANGAR_Red_{i+1}{j+1}', 'BattleRed', x + 1.0, y, H['ceiling'] - 0.3, 'HANGAR', hs, note='high-ceiling battle red, 12 m throw'); lights[-1]['intensity_cd'] = 6000; lights[-1]['attenuation_cm'] = 1800
for k, (sx, sy) in enumerate((SPOT_FIGHTER, SPOT_DROPSHIP)):
    for side, wy in (('Back', H['y0'] + 0.3), ('Fwd' if k else 'Aft', None)):
        if wy is not None:
            x, y, z = sx, wy, H['floor'] + 7.0; yaw = 90
        else:
            x = H['x1'] - 0.3 if k else H['x0'] + 0.3; y = sy; z = H['floor'] + 7.0; yaw = 180 if k else 0
        dx, dy, dz = sx - x, sy - y, H['floor'] + 1.5 - z
        pitch = round(math.degrees(math.atan2(dz, math.hypot(dx, dy))), 1); yaw = round(math.degrees(math.atan2(dy, dx)), 1)
        add(f'L_HANGAR_Flood_{"Fighter" if k == 0 else "Dropship"}_{side}', 'WorkFlood', x, y, z, 'HANGAR', hs, yaw=yaw, pitch=pitch, shadow_base=(side == 'Back'), note='aimed at craft spot')
for e, z in enumerate((APERTURE['z0'] + 0.5, APERTURE['z0'] + 4.5, APERTURE['z1'] - 0.3)):
    for side, x in (('Aft', APERTURE['x0'] + 0.15), ('Fwd', APERTURE['x1'] - 0.15)):
        add(f'L_HANGAR_DoorEdge_{side}_{e+1}', 'DoorEdgeRed', x, APERTURE['y'] - 0.2, z, 'HANGAR', hs, pitch=0)
for side, x in (('Aft', APERTURE['x0'] + 0.6), ('Fwd', APERTURE['x1'] - 0.6)):
    add(f'L_HANGAR_DoorBeacon_{side}', 'Beacon', x, APERTURE['y'] - 0.4, APERTURE['z1'] + 0.4, 'HANGAR', hs, yaw=-90, pitch=-25, note='hangar door cycle beacon: red in DoorCycle preset')
add('L_HANGAR_PersonnelDoor_Beacon', 'Beacon', PERSONNEL_DOOR[0], PERSONNEL_DOOR[1] + 0.3, PERSONNEL_DOOR[2] + 2.65, 'HANGAR', hs, yaw=90, pitch=-20)
for nm_, (x, y) in (('PersonnelDoor', (PERSONNEL_DOOR[0] + 1.2, PERSONNEL_DOOR[1] + 0.3)), ('Fighter', (SPOT_FIGHTER[0], H['y0'] + 0.3)), ('Dropship', (SPOT_DROPSHIP[0], H['y0'] + 0.3))):
    add(f'L_HANGAR_Emergency_{nm_}', 'EmergencyTwin', x, y, H['floor'] + 2.6, 'HANGAR', hs, yaw=90, pitch=-35)

# v1.8 physically based targets: direct illuminance on the floor under each overhead fixture (E = I / d^2, IES peak, cosine 1)
LUX_TARGET = {'corr': (120, 200), 'room': (150, 300), 'eng': (300, 500), 'warm': (80, 150), 'stair': (75, 150), 'hangar': (250, 400)}
for l in lights:
    if l['ue_pitch'] == -90 and l['group'] == 'LG_Main':
        r = next((x for x in rooms if x['id'] == l['room']), None)
        floor = HANGAR['floor'] if l['room'] == 'HANGAR' else (r['z0'] if r else l['z_m'] - 3.0)
        l['lux_floor_direct'] = round(l['intensity_cd'] / max(0.5, l['z_m'] - floor) ** 2, 1)
    else: l['lux_floor_direct'] = ''
# reference sockets on the 4C body mesh (art/export_UE/Corvette4C, 59 sockets; UE socket name = empty name minus 'SOCKET_')
for l in lights:
    if l['room'] != 'HANGAR': l['ref_socket'] = ''
    elif 'Fighter' in l['id']: l['ref_socket'] = 'Hangar_Spot_Fighter'
    elif 'Dropship' in l['id']: l['ref_socket'] = 'Hangar_Spot_Dropship'
    elif 'DoorEdge_Aft' in l['id'] or 'DoorBeacon_Aft' in l['id']: l['ref_socket'] = 'Mount_HangarDoor_L'
    elif 'DoorEdge_Fwd' in l['id'] or 'DoorBeacon_Fwd' in l['id']: l['ref_socket'] = 'Mount_HangarDoor_R'
    else: l['ref_socket'] = 'Hangar'
# ---- budget check (baseline: <= 12 shadow-casting lights in view) ----
# Proxy for "in view" from anywhere in a space: all casters in the space itself, all casters in open-connected corridors/lobbies
# (corridor-to-corridor openings), and at most 1 caster per room seen through a door.
# Stair trunks count their own landing only. Hangar: enclosed module, own casters + 1 through the personnel door.
# Shadow casters also get max_draw_distance_cm = 2500 in the baseline, so far fixtures down a long corridor drop their shadows.
rb = {r['id']: r for r in rooms}
shadow = [l for l in lights if l['shadow_baseline']]
for l in shadow: l['max_draw_distance_cm'] = 2500
for l in lights: l.setdefault('max_draw_distance_cm', 0)
per_room = {}
for l in shadow: per_room[l['room']] = per_room.get(l['room'], 0) + 1
nbr = {}
for d_ in D['doors']:
    a_, b_ = d_.get('a'), d_.get('b')
    if a_ and b_: nbr.setdefault(a_, set()).add(b_); nbr.setdefault(b_, set()).add(a_)
OPEN = ('corr', 'lobby', 'junction')
worst = []
for rid, r in rb.items():
    if r['kind'] == 'ladder':
        n_ = max(2, int(r['h'] // 3))
        for i in range(n_):
            add(f'L_{rid}_Main_{i+1:02d}', 'BulkheadCage', (r['x0'] + r['x1']) / 2, r['y0'] + 0.1, r['z0'] + 1.5 + i * 3.0, rid, sec, yaw=90, pitch=0, kelvin=4000, shadow_base=(i == 0), note='caged ladder trunk, wall-mounted')
        continue
    if r['kind'] == 'stair': continue
    cnt = per_room.get(rid, 0); parts = {rid: cnt}
    for o in nbr.get(rid, ()):
        if o not in rb: continue
        if rb[o]['kind'] == 'stair': c = 1
        elif r['kind'] in OPEN and rb[o]['kind'] in OPEN: c = per_room.get(o, 0)
        else: c = min(1, per_room.get(o, 0))
        parts[o] = c; cnt += c
    worst.append((cnt, rid, parts))
worst.sort(key=lambda w: -w[0])
hangar_cnt = per_room.get('HANGAR', 0) + 1
mx = max([w[0] for w in worst] + [hangar_cnt])
budget = dict(rule='baseline <= 12 shadow-casting lights in view', method='own space + open-connected corridors in full + max 1 per door-connected room (a 1.2 x 2.4 m door frames about one 3.4 m-pitch fixture); stairs 1; hangar own + 1',
              hangar=hangar_cnt, worst_spaces=[dict(space=w[1], in_view=w[0], breakdown=w[2]) for w in worst[:10]], max_found=mx, passed=mx <= 12)

# ---- state presets ----
G = ['LG_Main', 'LG_Task', 'LG_Battle', 'LG_Emergency', 'LG_Beacon']
def st(main, task, battle, em, beacon, **kw):
    d = {g: v for g, v in zip(G, [main, task, battle, em, beacon])}; d.update(kw); return d
presets = {
 'schema': 'per state: group -> {mult, kelvin_override?, color_override?, flicker, rotate_hz}; extras = exposure/fog/vfx params. mult multiplies each light intensity_cd.',
 'states': {
  'Normal':     dict(groups=st(dict(mult=1.0, flicker='none'), dict(mult=0.8), dict(mult=0.0), dict(mult=0.0), dict(mult=0.0)), ev100=[6, 7], fog_density=0.02, gravity=[0, 0, -980]),
  'RedAlert':   dict(groups=st(dict(mult=0.08), dict(mult=0.4), dict(mult=1.0), dict(mult=0.0), dict(mult=1.0, rotate_hz=0.5, only_tag='muster')), ev100=[2.5, 3.5], fog_density=0.03, gravity=[0, 0, -980]),
  'Damaged':    dict(groups=st(dict(mult=0.45, flicker='health<0.5', dead_fraction=0.15), dict(mult=0.6, flicker='health<0.5'), dict(mult=1.0), dict(mult=1.0, when='main<0.3'), dict(mult=1.0, rotate_hz=1.0)), ev100=[3, 4], fog_density=0.3, smoke=True, gravity=[0, 0, -980]),
  'Breach':     dict(groups=st(dict(mult=0.7), dict(mult=0.6), dict(mult=1.0), dict(mult=1.0), dict(mult=1.0, rotate_hz=2.0, at='sealed_doors')), ev100=[3.5, 4.5], fog_density=0.0, fog_fade_s=4, condensation_flash_s=1.5, gravity=[0, 0, -980]),
  'PowerLoss':  dict(groups=st(dict(mult=0.0, fade_s=0.6, brownout_rebounds=2), dict(mult=0.1, only_tag='battery'), dict(mult=0.0), dict(mult=0.2, kelvin_override=2700, delay_s=0.8), dict(mult=1.0, rotate_hz=1.0, at='sealed_doors', battery=True)), ev100=[1, 2], fog_density=0.03, helmet_lamps=True, gravity=[0, 0, -980]),
  'ZeroG':      dict(groups=st(dict(mult=1.0, hanging_fixture_physics='float'), dict(mult=0.8), dict(mult=0.0), dict(mult=0.0), dict(mult=0.0)), ev100=[6, 7], fog_density=0.02, gravity=[0, 0, 0],
                     vfx=dict(sparks='drift, drag 0.8, life x1.5, no bounce', fire='spherical microgravity flame, spread x0.5, light x0.6', smoke='uniform sphere growth x0.5', props='float 0.05-0.2 m/s'),
                     note='stacks on top of any other state: apply the other state for lights, this one for gravity/VFX'),
  'HangarDoorCycle': dict(scope='HANGAR', groups=dict(LG_Beacon=dict(mult=1.0, color_override='#D01A10', rotate_hz=1.0, door_edge_strobe_hz=1.0), LG_Task=dict(mult=1.0, launch_lane_ramp_s=3.9)), note='layer on Normal/RedAlert during the 7 s horn + door cycle'),
 },
 'fixture_health_flicker': dict(below=0.5, dead_below=0.15),
 'paths': dict(baseline=dict(megalights=False, hw_rt=False, shadow_field='shadow_baseline', shadow_budget_in_view=12, lumen='software or Lite', flicker='subsystem (no light functions)'),
               high=dict(megalights=True, hw_rt=True, shadow_field='shadow_high', light_function_atlas=True)),
}
json.dump(dict(meta=dict(source_sockets='levels/interior_sockets.csv', source_rooms='levels/interior_data.json', model='Corvette4C v6.3', units='Blender m + UE cm (Y flipped)',
                         count=len(lights), shadow_baseline=len(shadow), style='matte practical fixtures, no emissive strips; lenses via CPD Intensity01'),
               fixtures=FIX, lights=lights), open(f'{OUT}/C4C_GrayboxLights.json', 'w'), indent=1)
with open(f'{OUT}/C4C_GrayboxLights.csv', 'w', newline='') as fh:
    w = csv.DictWriter(fh, fieldnames=list(lights[0].keys())); w.writeheader(); w.writerows(lights)
json.dump(presets, open(f'{OUT}/C4C_LightStatePresets.json', 'w'), indent=1)
json.dump(budget, open(f'{OUT}/budget_check.json', 'w'), indent=1)
from collections import Counter
print(len(lights), Counter(l['fixture'] for l in lights), 'shadow', len(shadow)); print(json.dumps(budget)[:900])
