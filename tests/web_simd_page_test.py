#!/usr/bin/env python3
"""A web profile's page carries a SIMD128 build and a baseline build.

Builds a Canvas project with `kryon build --profile web` and loads its page
in private headless Firefox and Chromium. Where the browser validates
WebAssembly SIMD128 the page must start the SIMD128 build, whose
std/resample Lanczos kernels are SIMD128; with WebAssembly.validate
rejecting SIMD, as a browser without it does, the same page must start the
baseline build. Every Lanczos resize the app makes must give the same bytes
in both builds, and the same bytes as the SIMD128 build's own scalar
kernels. No Node, owner browser or display.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from toolchain import ZIRAN, ZIRAN_ROOT
import avif_firefox_test as harness
from cursor_browser import Browser

ROOT = Path(__file__).resolve().parents[1]
# Chromium's socket sits under the profile, which must stay short.
WORK = ROOT / 'build/scratch/ws'
ENV = dict(os.environ)
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
    ENV.pop(name, None)
ENV['YUE_DESKTOP_RECOVERY'] = '0'
ENV.setdefault('EM_CACHE', str(ROOT / 'build/emscripten-cache'))

# Lanczos resizes up, down, along one axis and along both, of a fixed noise
# picture; the app publishes an FNV-1a hash of each result, first with the
# best kernels and then with the scalar ones.
APP = '''using UI :: #import "kryon/Widgets";
#import "std/resample"
#import "std/web"
#import "std/vec"
#import "std/format"
host_api :: #system_library "host_api";
RequestWindowClose :: () #foreign host_api;

noise: [240000]u8;
resized: [163840]u8;
reported: bool;

Resized :: (width: s32, height: s32, out_width: s32, out_height: s32) -> u64 {
    state: u64 = 2463534242
    for at: cast(s64)0..cast(s64)width * height * 4 - 1 {
        state = (state ^ (state << 13)) & 4294967295
        state = state ^ (state >> 17)
        state = (state ^ (state << 5)) & 4294967295
        noise[at] = cast(u8)(state & 255)
    }
    plan: ResamplePlan
    if !ResamplePlanInto(*plan, width, height, 0.0, 0.0, cast(float64)width, cast(float64)height,
        out_width, out_height, RESAMPLE_LANCZOS) { return 0 }
    ResamplePlanRun(*plan, *noise[0], cast(s64)width * 4, *resized[0], cast(s64)out_width * 4)
    hash: u64 = 2166136261
    for at: cast(s64)0..cast(s64)out_width * out_height * 4 - 1 {
        hash = ((hash ^ cast(u64)resized[at]) * 16777619) & 4294967295
    }
    return hash
}

Hashes :: (text: *Vec(u8)) {
    BuilderPrint(text, " % % % % %", Resized(97, 61, 256, 160), Resized(300, 200, 41, 27),
        Resized(128, 128, 128, 77), Resized(200, 50, 61, 50), Resized(33, 300, 200, 31))
}

#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    if !reported {
        reported = true
        text: Vec(u8)
        BuilderPrint(*text, "kernels %", ResampleKernels())
        Hashes(*text)
        unused ResampleUseKernels(RESAMPLE_SCALAR)
        BuilderPrint(*text, " scalar")
        Hashes(*text)
        scope := WebScope()
        WebSet(WebGlobal, WebName("kryonResample"), WebText(BuilderFinish(text)))
        WebEnd(scope)
    }
    RequestWindowClose()
    return 0
}
'''

# A browser without SIMD128: WebAssembly.validate rejects the SIMD probe.
NO_SIMD = ('<script>(() => { const validate = WebAssembly.validate;'
           ' WebAssembly.validate = function (bytes, ...rest) {'
           ' const view = new Uint8Array(ArrayBuffer.isView(bytes) ? bytes.buffer : bytes);'
           ' if (view.length < 64 && view.includes(253)) return false;'
           ' return validate.call(this, bytes, ...rest); }; })()</script>')

STATE = ('JSON.stringify({result: document.getElementById("result")?.textContent,'
         ' build: globalThis.Module?.kryonBuild, resample: globalThis.kryonResample,'
         ' left: document.querySelectorAll("script.kryon-file, script[id^=\'kryon-module-\']").length})')


def build():
    project = WORK / 'project'
    shutil.rmtree(project, ignore_errors=True)
    (project / 'src').mkdir(parents=True)
    (project / 'ziran.toml').write_text(
        '[package]\nname = "simd_probe"\nentry = "src/app.zi"\n'
        'module_roots = ["src"]\nbridge_modules = ["app"]\n\n'
        '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\nref = "master"\n\n'
        '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\nref = "master"\n\n'
        '[tool.kryon]\ndefault_profile = "web"\n\n'
        '[tool.kryon.profiles.web]\nbackend = "canvas"\n')
    (project / 'ziran.local.toml').write_text(
        f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n')
    (project / 'src/app.zi').write_text(APP)
    for command in ([ZIRAN, 'lock'], [ZIRAN, 'tool', 'kryon', 'build', '--profile', 'web']):
        subprocess.run(list(map(str, command)), cwd=project, env=ENV, check=True, timeout=1200)
    return project / 'build/simd_probe-web.html'


def check_page(page):
    text = page.read_text(errors='replace')
    for marker in ('id="kryon-module-simd128"', 'id="kryon-module-baseline"', 'data-mount="/kryon-font.ttf"'):
        assert text.count(marker) == 1, f'page has {text.count(marker)} {marker}'
    assert '<title>simd_probe</title>' in text, 'page is not named after the program'
    variants = {'simd128.html': text, 'baseline.html': text.replace('<head>', '<head>' + NO_SIMD, 1)}
    assert variants['baseline.html'] != text, 'page has no <head> for the shim'
    serve = WORK / 'serve'
    serve.mkdir(parents=True, exist_ok=True)
    for name, content in variants.items():
        (serve / name).write_text(content)
    return serve


def parse(state, browser, page):
    """(build, best kernels, best hashes, scalar hashes) of one page load."""
    value = json.loads(state)
    assert value['result'] == 'PASS', f'{browser} {page}: {value}'
    assert value['left'] == 0, f'{browser} {page}: {value["left"]} unused build or file elements stay'
    words = (value['resample'] or '').split()
    assert len(words) == 13 and words[0] == 'kernels' and words[7] == 'scalar', value['resample']
    return value['build'], int(words[1]), words[2:7], words[8:13]


def firefox(serve):
    states = {}
    harness.WORK = WORK
    with harness.firefox(serve) as (command, base):
        for name in ('simd128.html', 'baseline.html'):
            command('WebDriver:Navigate', {'url': f'{base}/{name}'})
            deadline = time.monotonic() + 120
            while True:
                state = command('WebDriver:ExecuteScript', {'script': 'return ' + STATE, 'args': []})['value']
                if '"result":"pending"' not in state or time.monotonic() > deadline:
                    break
                time.sleep(0.1)
            states[name] = state
    return states


def chromium(serve):
    states = {}
    for name in ('simd128.html', 'baseline.html'):
        browser = Browser(WORK / 'c')
        try:
            browser.open(serve / name)
            browser.until('(text => !!text && text !== "pending")(document.getElementById("result")?.textContent)',
                          f'Chromium {name}', 120)
            states[name] = browser.evaluate(STATE)
        finally:
            browser.close()
    return states


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    page = build()
    serve = check_page(page)
    expected = None
    for browser, states in (('Firefox', firefox(serve)), ('Chromium', chromium(serve))):
        simd = parse(states['simd128.html'], browser, 'simd128.html')
        baseline = parse(states['baseline.html'], browser, 'baseline.html')
        assert simd[:2] == ('simd128', 4), f'{browser} did not run the SIMD128 build: {simd[:2]}'
        assert baseline[:2] == ('baseline', 0), f'{browser} did not fall back to the baseline build: {baseline[:2]}'
        hashes = simd[2]
        assert simd[3] == hashes, f'{browser}: SIMD128 kernels {hashes} differ from scalar {simd[3]}'
        assert baseline[2] == hashes and baseline[3] == hashes, f'{browser}: baseline {baseline[2]} differs from {hashes}'
        expected = expected or hashes
        assert hashes == expected, f'{browser} resized differently: {hashes} {expected}'
        print(f'{browser}: SIMD128 build selected, kernels simd128; fallback baseline, kernels scalar; '
              f'resizes {" ".join(hashes)} in both')
    print(f'Web page with SIMD128 and baseline builds: {page.stat().st_size} bytes')


if __name__ == '__main__':
    main()
