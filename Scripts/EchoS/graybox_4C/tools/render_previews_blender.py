"""Blender 5.2.2 light previews of the 4C gray-box lights (Cycles, 960x540).
blender -b --factory-startup -P render_previews_blender.py -- <hangar|corridor> <Normal|RedAlert|PowerLoss>
Approximation only: candela -> Blender watts (point/spot W = cd*4*pi/683, area W = cd*pi/683 * 2), matte gray blockout
v1.8 weathered materials (painted steel albedo ~0.09 linear, roughness 0.55-0.92 with AO grime, ~5% convex edge wear), lenses emit only in proportion to their light. Fixture housings are small dark boxes."""
import bpy, sys, json, math, os
from mathutils import Vector, Euler
argv = sys.argv[sys.argv.index('--') + 1:]; SCENE, STATE = argv[0], argv[1]
HERE = os.path.dirname(os.path.abspath(__file__)); GB = os.path.dirname(HERE); R = '/workspace/space_rpg'
L = json.load(open(f'{GB}/C4C_GrayboxLights.json'))['lights']; P = json.load(open(f'{GB}/C4C_LightStatePresets.json'))['states'][STATE]
bpy.ops.wm.read_factory_settings(use_empty=True); sc = bpy.context.scene

def append_all(path, skip=('UCX_', 'SOCKET_', 'Shield_Plane')):
    with bpy.data.libraries.load(path) as (src, dst): dst.objects = [n for n in src.objects if not n.startswith(skip)]
    objs = [o for o in dst.objects if o is not None]
    for o in objs: sc.collection.objects.link(o)
    return objs
def weathered(name, base, rough=(0.55, 0.8), metal=0.0):
    """v1.8 matte weathered steel: dark albedo, AO-driven grime (+rough, -albedo), convex edge wear to bare metal."""
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; n = nt.nodes; k = nt.links
    b = n['Principled BSDF']; b.inputs['Metallic'].default_value = metal
    tc = n.new('ShaderNodeTexCoord')
    nz = n.new('ShaderNodeTexNoise'); nz.inputs['Scale'].default_value = 1.6; nz.inputs['Detail'].default_value = 8; k.new(tc.outputs['Object'], nz.inputs['Vector'])
    ao = n.new('ShaderNodeAmbientOcclusion'); ao.inputs['Distance'].default_value = 0.35; ao.samples = 8
    cv = n.new('ShaderNodeAmbientOcclusion'); cv.inside = True; cv.inputs['Distance'].default_value = 0.04; cv.samples = 8
    gm = n.new('ShaderNodeMath'); gm.operation = 'MULTIPLY'; k.new(ao.outputs['AO'], gm.inputs[0]); k.new(nz.outputs['Fac'], gm.inputs[1])
    gr = n.new('ShaderNodeMapRange'); gr.inputs['From Min'].default_value = 0.15; gr.inputs['From Max'].default_value = 0.55; k.new(gm.outputs[0], gr.inputs['Value'])  # 1=clean 0=grimy
    col = n.new('ShaderNodeMix'); col.data_type = 'RGBA'; col.inputs['A'].default_value = tuple(c * 0.75 for c in base) + (1,); col.inputs['B'].default_value = base + (1,); k.new(gr.outputs[0], col.inputs['Factor'])
    ro = n.new('ShaderNodeMapRange'); ro.inputs['To Min'].default_value = rough[1] + 0.12; ro.inputs['To Max'].default_value = rough[0]; k.new(gr.outputs[0], ro.inputs['Value'])
    ew = n.new('ShaderNodeMapRange'); ew.inputs['From Min'].default_value = 0.93; ew.inputs['From Max'].default_value = 0.99; k.new(cv.outputs['AO'], ew.inputs['Value'])  # convex -> 0
    ewi = n.new('ShaderNodeMath'); ewi.operation = 'SUBTRACT'; ewi.inputs[0].default_value = 1; k.new(ew.outputs[0], ewi.inputs[1])
    ewn = n.new('ShaderNodeMath'); ewn.operation = 'MULTIPLY'; k.new(ewi.outputs[0], ewn.inputs[0]); k.new(nz.outputs['Fac'], ewn.inputs[1])
    ewc = n.new('ShaderNodeMath'); ewc.operation = 'GREATER_THAN'; ewc.inputs[1].default_value = 0.42; k.new(ewn.outputs[0], ewc.inputs[0])
    c2 = n.new('ShaderNodeMix'); c2.data_type = 'RGBA'; c2.inputs['B'].default_value = (0.5, 0.5, 0.52, 1); k.new(col.outputs['Result'], c2.inputs['A']); k.new(ewc.outputs[0], c2.inputs['Factor'])
    r2 = n.new('ShaderNodeMix'); r2.inputs['B'].default_value = 0.38; k.new(ro.outputs[0], r2.inputs['A']); k.new(ewc.outputs[0], r2.inputs['Factor'])
    m2 = n.new('ShaderNodeMath'); m2.operation = 'MAXIMUM'; m2.inputs[1].default_value = metal; k.new(ewc.outputs[0], m2.inputs[0])
    k.new(c2.outputs['Result'], b.inputs['Base Color']); k.new(r2.outputs['Result'], b.inputs['Roughness']); k.new(m2.outputs[0], b.inputs['Metallic'])
    return m
mat = weathered('M_C4C_PaintedSteel', (0.075, 0.085, 0.09))
deck = weathered('M_C4C_DeckTread', (0.06, 0.06, 0.06), (0.6, 0.75), 0.4)
mark = bpy.data.materials.new('Marking'); mark.use_nodes = True
mb = mark.node_tree.nodes['Principled BSDF']; mb.inputs['Base Color'].default_value = (0.16, 0.11, 0.015, 1); mb.inputs['Roughness'].default_value = 0.8
dark = bpy.data.materials.new('Housing'); dark.use_nodes = True; dark.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.05, 0.05, 0.05, 1)
def matte(objs):
    for o in objs:
        if o.type == 'MESH':
            o.data.materials.clear(); nm = o.name.lower(); o.data.materials.append(mark if 'marking' in nm else deck if any(t in nm for t in ('floor', 'deck', 'ramp')) else mat)

if SCENE == 'hangar':
    root = bpy.data.objects.new('HangarRoot', None); sc.collection.objects.link(root)
    root.location = (6.675, 1.0, 9.667); root.rotation_euler = (0, 0, math.radians(90))
    objs = append_all(f'{R}/blockouts/SM_Hangar_Blockout.blend')
    for o in objs:
        if o.parent is None: o.parent = root
    matte(objs)
    for f, (x, y) in (('SM_Fighter_Blockout', (-3.825, 3.5)), ('SM_DS12_Blockout', (16.175, 2.25))):
        cr = bpy.data.objects.new(f + '_root', None); sc.collection.objects.link(cr); cr.location = (x, y, 9.667); cr.rotation_euler = (0, 0, math.radians(90))
        co = append_all(f'{R}/blockouts/{f}.blend')
        for o in co:
            if o.parent is None: o.parent = cr
        matte(co)
    sel = [l for l in L if l['room'] == 'HANGAR']
    cam_loc, cam_tgt, lens = (6.675, -11.2, 12.2), (6.675, 14.0, 13.0), 14
else:
    objs = append_all(f'{R}/levels/SM_Corvette4C_Interior_Blockout.blend')
    keep = ('D1_',)
    for o in list(objs):
        if not o.name.startswith(('D1_', 'Door_')): bpy.data.objects.remove(o)
    matte([o for o in sc.objects])
    sel = [l for l in L if 4.0 < l['z_m'] < 9.0 and -24 < l['x_m'] < 8 and abs(l['y_m']) < 2.2]
    cam_loc, cam_tgt, lens = (5.0, 0.0, 6.55), (-20.0, 0.0, 6.3), 16

G = P.get('groups', {})
for l in sel:
    g = G.get(l['group'], {}); mult = g.get('mult', 1.0)
    if g.get('only_tag') or g.get('at'): mult = 0.0   # tagged subsets (battery consoles, sealed doors, muster points) are not in this preview
    if l['group'] == 'LG_Beacon' and STATE == 'PowerLoss': mult = 0.0
    if STATE == 'RedAlert' and l['group'] == 'LG_Beacon' and 'HANGAR' not in l['room']: mult = 0.0
    cd = l['intensity_cd'] * mult
    loc = Vector((l['x_m'], l['y_m'], l['z_m']))
    yaw = math.radians(-l['ue_yaw']); pitch = math.radians(l['ue_pitch'])
    d = Vector((math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), math.sin(pitch)))
    # housing
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc - d * 0.09)   # housing sits behind the lens; light is 3 cm in front
    h = bpy.context.object; h.scale = (0.7, 0.4, 0.1) if l['fixture'] == 'CagedPanel' else (0.2, 0.2, 0.1)
    h.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler(); h.data.materials.append(dark)
    if cd <= 0: continue
    kind = {'PointLight': 'POINT', 'SpotLight': 'SPOT', 'RectLight': 'AREA'}[l['ue_class']]
    ld = bpy.data.lights.new(l['id'], kind)
    if kind == 'AREA':
        ld.shape = 'RECTANGLE'; ld.size = l['rect_w_cm'] / 100; ld.size_y = l['rect_h_cm'] / 100; ld.energy = cd * math.pi / 683 * 2
        if l['fixture'] == 'WorkFlood': ld.spread = math.radians(70)
    else:
        ld.energy = cd * 4 * math.pi / 683; ld.shadow_soft_size = 0.05
        if kind == 'SPOT': ld.spot_size = math.radians(l['cone_outer'] * 2); ld.spot_blend = 0.4
    k = g.get('kelvin_override') or l['kelvin']
    if g.get('color_override'): c = g['color_override']
    elif l['color']: c = l['color']
    else: c = None
    if c: ld.color = tuple(int(c.lstrip('#')[i:i + 2], 16) / 255 for i in (0, 2, 4))
    else:
        ld.use_temperature = True; ld.temperature = k if k else 4000
    ob = bpy.data.objects.new(l['id'], ld); sc.collection.objects.link(ob); ob.location = loc + d * 0.03
    ob.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    ob.visible_shadow = True; ld.use_shadow = True   # preview shows the High path (all shadows)

cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.location = cam_loc; cam.data.lens = lens; cam.data.clip_start = 0.05
cam.rotation_euler = (Vector(cam_tgt) - Vector(cam_loc)).to_track_quat('-Z', 'Y').to_euler()
if STATE == 'PowerLoss':   # spec: helmet lamps (5600K) carry the scene in Power Loss; preview adds one at the camera
    hl = bpy.data.lights.new('HelmetLamp', 'SPOT'); hl.energy = 900 * 4 * math.pi / 683; hl.spot_size = math.radians(40); hl.spot_blend = 0.5
    hl.use_temperature = True; hl.temperature = 5600
    ho = bpy.data.objects.new('HelmetLamp', hl); sc.collection.objects.link(ho); ho.location = Vector(cam_loc) + Vector((0, 0.25, 0.1)); ho.rotation_euler = cam.rotation_euler
w = bpy.data.worlds.new('W'); w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.0; sc.world = w
sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 48; sc.cycles.use_denoising = True; sc.cycles.max_bounces = 4
sc.render.resolution_x, sc.render.resolution_y = 960, 540
sc.view_settings.view_transform = 'AgX'; sc.view_settings.look = 'None'
sc.view_settings.exposure = 1.6 + {'Normal': 0.0, 'RedAlert': 1.0, 'PowerLoss': 4.5}[STATE] + (0.5 if SCENE == 'hangar' else 0.0)  # preview EV offset (candela->W approximation)
sc.render.image_settings.file_format = 'PNG'
os.makedirs(f'{GB}/renders', exist_ok=True)
sc.render.filepath = f'{GB}/renders/C4C_Graybox_{SCENE.capitalize()}_{STATE}.png'
bpy.ops.render.render(write_still=True)
print('WROTE', sc.render.filepath, len(sel), 'lights')
