
import sys, os, time
sys.path.insert(0, r"D:\CST-2024\AMD64\python_cst_libraries")
import cst.interface as ci

t0 = time.time()
try:
    de = ci.DesignEnvironment(mode=ci.DesignEnvironment.StartMode.New)
    print("ctor OK in %.1fs version=%s pid=%s" % (time.time()-t0, de.version(), de.pid()))
except Exception as e:
    print("ctor FAIL", type(e).__name__, str(e)[:600]); raise SystemExit(2)

tmp = r"D:\dsh-eng-mcp\cst_tmp"; os.makedirs(tmp, exist_ok=True)
prj = None
for attempt in (("probe", tmp), (tmp, "probe")):
    try:
        prj = de.new_mws_project(*attempt)
        print("new_mws_project OK", attempt, "->", prj.filename()); break
    except Exception as e:
        print("new_mws_project FAIL", attempt, type(e).__name__, str(e)[:250])
if prj is None:
    try: de.close()
    except Exception: pass
    raise SystemExit(3)

try:
    vba = 'With Units\n .Geometry "mm"\n .Frequency "GHz"\n .Time "ns"\nEnd With'
    print("add_to_history ->", prj.model3d.add_to_history("ProbeUnits", vba))
    prj.save()
    print("saved:", prj.filename())
    print("solver:", prj.model3d.get_solver_type() if hasattr(prj.model3d, "get_solver_type") else "n/a")
finally:
    try: prj.close()
    except Exception as e: print("close prj err", e)
    try: de.close()
    except Exception as e: print("close de err", e)
print("DONE")
