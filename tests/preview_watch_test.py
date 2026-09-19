#!/usr/bin/env python3
"""Run a generated app through live build failure and recovery on an X display."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(sys.argv[1]).resolve()
SOURCE = '''#import "kryon.h"
#import "probe.h"
#style <material> as material
app "Preview Fixture" {
    size 480 320
}
state {
    counter: int = 0
}
Preview :: (viewport: Rectangle) #ui {
    counter += 1
    ProbeFrame(GENERATION, counter)
    Screen root: {
        bounds = viewport
        Text((TextProps){.bounds={24,24,400,40},.text="Preview generation GENERATION",.font=24})
        Button((ButtonProps){.bounds={24,84,160,40},.label="Working preview",.id=101})
    }
}
'''
PROBE = r'''
#include <stdio.h>
#include <stdlib.h>
void ProbeFrame(int generation, int counter) {
    FILE *file = fopen(getenv("PREVIEW_FRAME_PROBE"), "w");
    if(file != NULL) {
        fprintf(file, "%d %d\n", generation, counter);
        fclose(file);
    }
}
'''


def wait_for(predicate, process, log, timeout=20):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = predicate()
        if result:
            return result
        if process.poll() is not None:
            raise AssertionError(log.read_text())
        time.sleep(0.05)
    raise AssertionError("preview timed out\n" + log.read_text())


with tempfile.TemporaryDirectory(prefix="kryon-watch.") as temporary:
    project = Path(temporary)
    source = project / "main.kry"
    source.write_text(SOURCE.replace("GENERATION", "1"))
    (project / "probe.h").write_text("void ProbeFrame(int generation, int counter);\n")
    (project / "probe.c").write_text(PROBE)
    (project / "Makefile").write_text(
        ".PHONY: kryon-host\nkryon-host:\n"
        "\tmkdir -p build/kryon/generated\n"
        f'\t"{BINARY.parent / "k2c"}" --no-main --root . -o build/kryon/generated main.kry\n'
        f'\tcc -shared -fPIC -I. -I"{ROOT / "include"}" '
        f'-I"{ROOT / "build/linux-x86_64/generated/include"}" '
        f'-I"{ROOT / "build/linux-x86_64/generated/src"}" '
        '-Ibuild/kryon/generated build/kryon/generated/*.c probe.c -o build/kryon/app_host.so\n'
    )
    probe = project / "build/frames.txt"
    log = project / "watch.log"
    image = project / "build/final.png"
    environment = dict(os.environ, PREVIEW_FRAME_PROBE=str(probe), TMPDIR=str(project))
    other = None
    with log.open("w") as output:
        process = subprocess.Popen([str(BINARY), "watch", "--project", str(project),
                                    "--source", "main.kry", "--width", "480", "--height", "320",
                                    "--frames", "720", "--output", str(image)],
                                   stdout=output, stderr=output, env=environment)
    try:
        def frame():
            try:
                values = probe.read_text().split()
                return tuple(map(int, values)) if len(values) == 2 else None
            except (FileNotFoundError, ValueError):
                return None

        initial = wait_for(lambda: frame(), process, log)
        assert initial[0] == 1, initial
        source.write_text("#style missing\n")
        wait_for(lambda: "build failed; keeping" in log.read_text(), process, log)
        retained = wait_for(lambda: (current if (current := frame()) and current[1] > initial[1] + 10 else None),
                            process, log)
        assert retained[0] == 1, retained
        source.write_text(SOURCE.replace("GENERATION", "2"))
        recovered = wait_for(lambda: (current if (current := frame()) and current[0] == 2 else None),
                             process, log)
        assert process.wait(timeout=25) == 0, log.read_text()
        assert image.is_file() and image.stat().st_size > 1000, log.read_text()
        destination = Path(os.environ.get("PREVIEW_TEST_CAPTURE", "/tmp/kryon-preview-watch.png"))
        destination.write_bytes(image.read_bytes())
        assert not list(project.glob("kryon-preview.*")), list(project.iterdir())
        print(f"live preview: generated host kept drawing through syntax failure and recovered; capture {destination}")

        source.write_text("#style missing\n")
        result = subprocess.run(
            [str(BINARY), "watch", "--project", str(project), "--width", "480",
             "--height", "320", "--frames", "120", "--output", str(image)],
            capture_output=True, text=True, env=environment, timeout=20)
        assert result.returncode == 1, result.stderr
        assert '"code":"parse.syntax"' in result.stderr, result.stderr
        assert image.is_file() and image.stat().st_size > 1000
        destination.with_name(destination.stem + "-error.png").write_bytes(image.read_bytes())
        assert not list(project.glob("kryon-preview.*")), list(project.iterdir())

        (project / "Makefile").write_text(".PHONY: kryon-host\nkryon-host:\n\tsleep 30\n")
        with log.open("w") as output:
            process = subprocess.Popen([str(BINARY), "watch", "--project", str(project)],
                                       stdout=output, stderr=output, env=environment)
        wait_for(lambda: "sleep 30" in log.read_text(), process, log)
        other_log = project / "other.log"
        with other_log.open("w") as output:
            other = subprocess.Popen([str(BINARY), "watch", "--project", str(project)],
                                     stdout=output, stderr=output, env=environment)
        wait_for(lambda: "sleep 30" in other_log.read_text(), other, other_log)
        assert process.poll() is None, "opening a second preview terminated the first"
        assert len(list(project.glob("kryon-preview.*"))) == 2
        other.terminate()
        assert other.wait(timeout=5) == 1, other_log.read_text()
        assert process.poll() is None, "closing one preview terminated the other"
        process.terminate()
        result = process.wait(timeout=5)
        assert result == 1, (result, log.read_text())
        assert not list(project.glob("kryon-preview.*")), list(project.iterdir())
        print("live preview: initial failure reporting, parallel sessions, and interrupted-build cleanup passed")
    finally:
        if other is not None and other.poll() is None:
            other.kill()
            other.wait()
        if process.poll() is None:
            process.kill()
            process.wait()
