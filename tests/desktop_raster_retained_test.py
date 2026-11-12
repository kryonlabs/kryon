#!/usr/bin/env python3
"""Source/saved native retained pixels and independent active scheduling."""
import os
import shlex
import subprocess
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT

OUT = ROOT / "build/scratch/desktop-raster-retained"
ENV = dict(os.environ)
for name in ("DISPLAY","WAYLAND_DISPLAY","XAUTHORITY","DBUS_SESSION_BUS_ADDRESS",
             "SESSION_MANAGER","DESKTOP_SESSION","XDG_SESSION_ID","XDG_SESSION_TYPE",
             "XDG_SESSION_DESKTOP","XDG_CURRENT_DESKTOP","XDG_RUNTIME_DIR"):
    ENV.pop(name,None)
ENV.update(HARMONY_DESKTOP_RECOVERY="0",YUE_DESKTOP_RECOVERY="0")
ENTRY = "desktop_raster_retained_test:main"

def run(args):
    subprocess.run(list(map(str,args)),cwd=ROOT,env=ENV,check=True)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    initial=OUT/"ir"
    for directory in (initial,OUT/"saved-ir"):
        for old in directory.glob("*.zir"):
            old.unlink()
    run([ZIRAN,"ir","--project","--root",ROOT/"tests","--entry",ENTRY,
         "-o",initial,ROOT/"tests/desktop_raster_retained_test.zi"])
    compiler=shlex.split(os.environ.get("KRYON_TEST_CC",os.environ.get("CC","cc")))
    libs=shlex.split(subprocess.check_output(
        ["pkg-config","--libs","sdl2","cairo","freetype2","pangocairo","fontconfig","glib-2.0","x11"],text=True))
    for saved in (False,True):
        ir=initial
        if saved:
            ir=OUT/"saved-ir"
            run([ZIRAN,"ir","--root",initial,"--module-path",initial,"--entry",ENTRY,
                 "-o",ir,initial/"desktop_raster_retained_test.zir"])
        generated=OUT/"c"
        generated.mkdir(exist_ok=True)
        for suffix in ("*.c","*.h"):
            for old in generated.glob(suffix):
                old.unlink()
        run([ZIRAN.with_name("zi2c"),"--root",ir,"--entry",ENTRY,"-o",generated,*sorted(ir.glob("*.zir"))])
        executable=OUT/"desktop_raster_retained_test"
        run([*compiler,"-std=c11","-O2","-ffunction-sections","-fdata-sections",
             "-I"+str(ZIRAN_ROOT/"include"),"-iquote",generated,*sorted(generated.glob("*.c")),
             "-Wl,--gc-sections",*libs,"-lm","-lpthread","-o",executable])
        run(["xvfb-run","-a","-s","-screen 0 640x480x24 -nolisten tcp -extension MIT-SHM",executable])
    print("Retained source/saved pixels, aggregated requests, 60Hz pending and 15Hz idle passed")

if __name__=="__main__":
    main()
