
import sys
sys.path.insert(0, r"D:\CST-2024\AMD64\python_cst_libraries")
import cst.interface as ci

for name in ("new_project", "new_mws", "new_ems", "new_fd3d", "new_cs", "new_ps", "new_ds", "new_pcbs", "open_project"):
    f = getattr(ci.DesignEnvironment, name, None)
    print("###", name, "->", repr(getattr(f, "__doc__", None))[:700])

de = ci.DesignEnvironment(mode=ci.DesignEnvironment.StartMode.New)
print("model3d type:", type(de.active_project) if False else "n/a")
try:
    print(de.print_command_line_options())
except Exception as e:
    print("cmdline err", e)
de.close()
