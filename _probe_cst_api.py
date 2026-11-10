
import sys
print("py", sys.version.split()[0])
sys.path.insert(0, r"D:\CST-2024\AMD64\python_cst_libraries")
import cst
print("cst dir:", [x for x in dir(cst) if not x.startswith("_")])
import cst.interface as ci
print("interface:", [x for x in dir(ci) if not x.startswith("_")])
for nm in ("Project", "Model3D"):
    cls = getattr(ci, nm, None)
    if cls is None:
        print(nm, "MISSING"); continue
    print(nm, ":", [m for m in dir(cls) if not m.startswith("_")])
print("DE:", [m for m in dir(ci.DesignEnvironment) if not m.startswith("_")])
