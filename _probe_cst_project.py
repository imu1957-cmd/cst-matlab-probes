
import sys, os, time
sys.path.insert(0, r"D:\CST-2024\AMD64\python_cst_libraries")
import cst.interface as ci

for cls, names in ((ci.Project, ("save","close","model3d","filename","get_messages")),):
    for n in names:
        f = getattr(cls, n, None)
        print("### Project.%s -> %s" % (n, repr(getattr(f, "__doc__", None))[:400]))

de = ci.DesignEnvironment(mode=ci.DesignEnvironment.StartMode.New)
prj = de.new_mws()
print("prj type:", type(prj), "file:", prj.filename())
m3 = prj.model3d
print("model3d attrs:", [x for x in dir(m3) if not x.startswith("_")])
for n in ("add_to_history","run_solver","start_solver","is_solver_running","get_solver_type","reset_history"):
    f = getattr(type(m3), n, None)
    print("### model3d.%s -> %s" % (n, repr(getattr(f, "__doc__", None))[:300]))

vba = 'With Units\n .Geometry "mm"\n .Frequency "GHz"\n .Time "ns"\nEnd With'
print("add_to_history:", m3.add_to_history("ProbeUnits", vba))
tmp = r"D:\dsh-eng-mcp\cst_tmp"
os.makedirs(tmp, exist_ok=True)
target = os.path.join(tmp, "probe.cst")
try:
    prj.save(target)
    print("save(path) OK ->", os.path.exists(target), target)
except Exception as e:
    print("save(path) FAIL", type(e).__name__, str(e)[:300])
    try:
        prj.save()
        print("save() OK, file:", prj.filename())
    except Exception as e2:
        print("save() FAIL", e2)
prj.close(); de.close()
print("DONE")
