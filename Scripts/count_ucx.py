"""Vega: count simple-collision convex pieces per Forge part/tube asset vs IMPORT_README (2026-10-30 08:15)."""
import unreal
reg = unreal.AssetRegistryHelpers.get_asset_registry()
def exp(n):
    if n.startswith("SM_BoardingTube_"):
        if n in ("SM_BoardingTube_BP2", "SM_BoardingTube_BP3"): return 17
        return 1 if "_Door_" in n else 5
    return 1
tot = bad = 0
for path in ("/Game/Ships/Corvette4C/Parts", "/Game/Ships/FighterC/Parts", "/Game/Ships/Corvette4C_BoardingTubes", "/Game/Ships/Corvette4C_BoardingTubes/Parts"):
    for ad in sorted(reg.get_assets_by_path(path, recursive=False), key=lambda a: str(a.asset_name)):
        if str(ad.asset_class_path.asset_name) != "StaticMesh": continue
        m = unreal.load_asset(str(ad.package_name)); n = str(ad.asset_name)
        c = len(m.get_editor_property("body_setup").get_editor_property("agg_geom").get_editor_property("convex_elems"))
        e = exp(n); tot += 1; bad += (c != e)
        unreal.log("[VEGA_UCX] %-40s convex %3d expected %3d %s" % (n, c, e, "OK" if c == e else "MISMATCH"))
unreal.log("[VEGA_UCX] %d assets, %d mismatches" % (tot, bad))
