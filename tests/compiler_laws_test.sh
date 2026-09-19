#!/bin/sh
set -eu

root=$(cd "${1:-.}" && pwd)
build=${2:-$root/build/linux-x86_64}

python3 - "$root" "$build" <<'PY'
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(sys.argv[1])
build = Path(sys.argv[2]).resolve()
compilers = ["k2c", "k2cpp", "k2go", "k2kir", "k2b"]
if os.environ.get("KRYON_INCLUDE_PAUSED_JS") == "1":
    compilers.append("k2js")
blocked = ["Texture", "DrawTexture", "DrawTexturePro", "DrawTextureRec",
           "TextInputControl", "UITextLegacy", "UIRenderLegacy"]
law = "image.surface.no_low_level_calls"
checks = 0

with tempfile.TemporaryDirectory(prefix="kryon-compiler-laws-") as temporary:
    work = Path(temporary)
    good = work / "allowed.kry"
    good.write_text('''#import "kryon.h"
Allowed :: () -> int #export {
    # Texture(0) and DrawTexture(0) in comments are not calls.
    return 0
}
''')
    for compiler in compilers:
        modes = [[], ["--strict"], ["--no-strict"]]
        if compiler == "k2kir":
            modes = [[]]
        elif compiler == "k2b":
            modes = [[], ["--allow-unsupported"]]
        for mode_index, mode in enumerate(modes):
            output = work / f"{compiler}-{mode_index}-allowed"
            command = [str(build / "bin" / compiler), *mode, "--root", str(work), "-o", str(output)]
            result = subprocess.run([*command, str(good)], text=True, capture_output=True)
            if result.returncode != 0:
                raise SystemExit(f"{compiler} {mode} rejected valid source:\n{result.stderr}")
            for name in blocked:
                source = work / "blocked.kry"
                source.write_text(f'''#import "kryon.h"
Blocked :: () -> int #export {{
    {name}(0)
    return 0
}}
''')
                output = work / f"{compiler}-{mode_index}-{name}"
                command = [str(build / "bin" / compiler), *mode, "--root", str(work), "-o", str(output)]
                result = subprocess.run([*command, str(source)], text=True, capture_output=True)
                if result.returncode == 0 or law not in result.stderr:
                    raise SystemExit(f"{compiler} {mode} did not enforce {law} for {name}:\n{result.stderr}")
                if output.exists() and any(path.is_file() for path in output.rglob("*")):
                    raise SystemExit(f"{compiler} emitted output despite a law violation")
                checks += 1
print(f"compiler laws: {checks} forbidden calls rejected across {len(compilers)} frontends")
PY
