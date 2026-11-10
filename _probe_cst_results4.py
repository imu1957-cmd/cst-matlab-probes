
import sys
sys.path.insert(0, r"D:\CST-2024\AMD64\python_cst_libraries")
import cst.interface as ci
SM = ci.DesignEnvironment.StartMode
print("StartMode members:", [m for m in dir(SM) if not m.startswith("_")])
import cst.results as cr
for p in (r"D:\cst_work\study_wg.cst", r"D:\cst_work\trace2_one.cst"):
    try:
        prj = cr.ProjectFile(p, allow_interactive=False)
        mod = prj.get_3d()
        items = mod.get_tree_items("0D/1D")
        print(p, "-> items:", items[:15], "count", len(items))
    except Exception as e:
        print(p, "ERR", type(e).__name__, str(e)[:200])
