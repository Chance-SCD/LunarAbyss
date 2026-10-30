import bpy,sys
bpy.ops.wm.read_homefile(use_empty=True)
bpy.ops.import_scene.fbx(filepath=sys.argv[-2]); bpy.context.view_layer.update()
gone=[o.name for o in list(bpy.data.objects) if o.name.startswith('TRIG_') or o.name.startswith('UCX_TRIG')]
for n in gone: bpy.data.objects.remove(bpy.data.objects[n])
for o in bpy.data.objects: o.select_set(True)
bpy.ops.export_scene.fbx(filepath=sys.argv[-1], object_types={'MESH','EMPTY'}, apply_unit_scale=True, mesh_smooth_type='FACE')
print("STRIPPED",gone)
