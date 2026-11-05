"""Resolved package module paths for standalone native image test hosts."""
import os
import subprocess
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT


def image_module_args():
    paths = {
        name: subprocess.check_output(
            [ZIRAN, "pkg", "path", name], cwd=ROOT, text=True,
        ).strip()
        for name in ("avif", "webp", "image_codec")
    }
    # The same explicit avif selection CMake's kryon_image_package honours.
    paths["avif"] = os.environ.get("KRYON_AVIF_PACKAGE_DIR") or paths["avif"]
    roots = [
        f"avif={paths['avif']}/src", f"{paths['avif']}/src",
        f"{paths['avif']}/src/bridge", f"webp={paths['webp']}/src",
        f"image_codec={paths['image_codec']}/src",
        f"std={ZIRAN_ROOT}/std", str(ZIRAN_ROOT / "std"),
    ]
    return [value for root in roots for value in ("--module-path", root)]
