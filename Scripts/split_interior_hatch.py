import bpy,sys
src=sys.argv[-1]
bpy.ops.wm.read_homefile(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
h=bpy.data.objects['SM_Hatch_DCD77']; u=bpy.data.objects['UCX_SM_Hatch_DCD77_00']
import mathutils
bb=[h.matrix_world@mathutils.Vector(c) for c in h.bound_box]
print("HB", [round(min(v[i] for v in bb),3) for i in range(3)], [round(max(v[i] for v in bb),3) for i in range(3)], tuple(round(x,3) for x in h.rotation_euler), h.parent)
print("CUBE", [ (o.name,o.type,len(o.data.vertices) if o.type=='MESH' else 0) for o in bpy.data.objects if o.name.startswith('Cube')])
# hatch-only FBX, pivot at its origin (hinge), exported in object local space
for o in bpy.data.objects: o.select_set(False)
hw=h.matrix_world.copy()
for o in (h,u): o.select_set(True)
bpy.context.view_layer.objects.active=h
for o in (h,u): o.matrix_world = hw.inverted() @ o.matrix_world
bpy.ops.export_scene.fbx(filepath='/workspace/s8/SM_Hatch_DCD77.fbx', use_selection=True, object_types={'MESH'}, apply_unit_scale=True, bake_space_transform=False, mesh_smooth_type='FACE')
for o in (h,u): o.matrix_world = hw @ o.matrix_world
# interior without the hatch (and its UCX)
bpy.data.objects.remove(h); bpy.data.objects.remove(u); [bpy.data.objects.remove(o) for o in list(bpy.data.objects) if o.name.startswith('TRIG_') or o.name.startswith('UCX_TRIG')]
for o in bpy.data.objects: o.select_set(True)
bpy.ops.export_scene.fbx(filepath='/workspace/s8/SM_Corvette4C_Interior_Blockout.fbx', use_selection=False, object_types={'MESH','EMPTY'}, apply_unit_scale=True, mesh_smooth_type='FACE')
print("DONE")
