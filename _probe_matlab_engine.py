
import sys, time
sys.path.insert(0, r"D:\MATLAB\extern\engines\python\dist")
t0=time.time()
try:
    import matlab.engine
    print("import OK", round(time.time()-t0,1))
except Exception as e:
    print("import FAIL", type(e).__name__, e); sys.exit(2)
t1=time.time()
try:
    eng = matlab.engine.start_matlab()
    print("start OK", round(time.time()-t1,1))
except Exception as e:
    print("start FAIL", type(e).__name__, str(e)[:400]); sys.exit(3)
try:
    r = eng.eval("1+1", nargout=1)
    print("eval 1+1 =", r)
    v = eng.version()
    print("matlab version:", v)
finally:
    eng.quit()
