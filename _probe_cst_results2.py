
import sys
sys.path.insert(0, r"D:\CST-2024\AMD64\python_cst_libraries")
import cst.results as cr
prj = cr.ProjectFile(r"D:\dsh-eng-mcp\cst_tmp\probe.cst", allow_interactive=False)
r3 = prj.get_3d()
print("ResultModule:", [m for m in dir(r3) if not m.startswith("_")])
try:
    trees = list(r3.get_all_result_trees())
    print("trees:", trees)
except Exception as e:
    print("trees err", type(e).__name__, str(e)[:200])
try:
    print("tree dict:", r3.get_result_tree())
except Exception as e:
    print("tree err", type(e).__name__, str(e)[:150])
