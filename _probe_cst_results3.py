
import sys
sys.path.insert(0, r"D:\CST-2024\AMD64\python_cst_libraries")
import cst.results as cr
for n in ("get_tree_items","get_result_item","get_run_ids"):
    f = getattr(cr.ResultModule, n, None)
    print("###", n, "->", repr(getattr(f,"__doc__",None))[:400])
for n in ("get_data","get_ydata","get_xdata","title","xlabel"):
    f = getattr(cr.ResultItem, n, None)
    print("### ResultItem.%s ->" % n, repr(getattr(f,"__doc__",None))[:300])
