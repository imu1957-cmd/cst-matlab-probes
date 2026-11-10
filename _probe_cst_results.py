
import sys, os
sys.path.insert(0, r"D:\CST-2024\AMD64\python_cst_libraries")
import cst.results as cr
print("results attrs:", [x for x in dir(cr) if not x.startswith("_")])
import inspect
for n in ("ProjectFile","ResultItem","ResultTreeItem"):
    cls = getattr(cr, n, None)
    if cls is None: continue
    print("###", n, [m for m in dir(cls) if not m.startswith("_")])
try:
    prj = cr.ProjectFile(r"D:\dsh-eng-mcp\cst_tmp\probe.cst", allow_interactive=False)
    print("opened project file, type:", prj)
    print("project methods:", [m for m in dir(prj) if not m.startswith("_")])
    r3 = prj.get_3d()
    print("get_3d ->", r3)
    print("tree:", r3.get_result_tree())
    print("trees:", list(r3.get_all_result_trees())[:3])
except Exception as e:
    print("results FAIL", type(e).__name__, str(e)[:400])
