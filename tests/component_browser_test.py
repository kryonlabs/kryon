#!/usr/bin/env python3
"""Run production independent ZX UI components in Chromium with WebGL disabled."""
from __future__ import annotations
import functools
import http.server
import json
import os
from pathlib import Path
import shutil
import subprocess
import threading
from toolchain import UI_MODULE_ARGS, ROOT, ZIRAN, ZIRAN_ROOT
import canvas_no_webgl_test as canvas_browser

# Stable disposable output; a short private Chromium profile path.
WORK = ROOT / 'build/cmpw'
OUTPUT = WORK / 'component.html'
ENV = dict(canvas_browser.ENV)
ENV.update(TMPDIR=str(WORK / 'tmp'), EMCC_CORES='1',
           EM_CACHE=str(ROOT / 'build/emscripten-cache/zx-canvas'))
EMCC = Path(os.environ.get('EMCC', Path.home() / 'emsdk/upstream/emscripten/emcc'))


def run(command, directory=ROOT):
    subprocess.run(list(map(str, command)), cwd=directory, env=ENV,
                   check=True, timeout=900)


def prepare():
    # Only disposable output owned by this test is recreated.
    shutil.rmtree(WORK, ignore_errors=True)
    (WORK / 'tmp').mkdir(parents=True)
    guests = ['counter_v1', 'counter_v2', 'status', 'bad_clipboard',
              'bad_abi', 'bad_schema', 'bad_resource', 'bad_init',
              'inputs_v1', 'inputs_v2', 'inputs_bad']
    for name in guests:
        base = 'bad_migrated' if name == 'bad_clipboard' else name
        source = ((ROOT / 'tests/component_inputs_guest.zi').read_text() if name.startswith('inputs_')
                  else (ROOT / f'tests/fixtures/component_{base}/component.zi').read_text())
        guest = WORK / 'guests' / name
        guest.mkdir(parents=True)
        if name in ('counter_v1', 'counter_v2', 'bad_clipboard'):
            source = '#import "component_browser_clipboard"\n' + source
            needle = ('    return ifx component_count' if name == 'bad_clipboard'
                      else '    return 0\n}')
            condition = '>= 0.0' if name == 'bad_clipboard' else '< 0.0'
            assert source.count(needle) == 1, name
            source = source.replace(needle,
                f'    BrowserClipboardProbe(session, ComponentSeconds() {condition})\n' + needle)
        if name == 'status':
            source = source.replace('component_schema_version: s64 = 1;',
                                    'component_schema_version: s64 = 1;\ncomponent_text_at: s64;')
            source = source.replace('    if ComponentText() == "advance" { component_count += 1 }',
                '''    expected := "advance"
    for byte: ComponentText() {
        if byte == expected[component_text_at] { component_text_at += 1 }
        else { component_text_at = ifx byte == expected[0] then 1 else 0 }
        if component_text_at == expected.count { component_count += 1; component_text_at = 0 }
    }''')
        (guest / 'component.zi').write_text(source)
        local_protocol = ROOT / f'tests/fixtures/component_{base}/component_protocol.zi'
        if local_protocol.is_file():
            shutil.copyfile(local_protocol, guest / 'component_protocol.zi')
        modules = ['--module-path', ROOT / 'tests', '--module-path', ROOT / 'src/backend',
                   *UI_MODULE_ARGS, '--module-path', ZIRAN_ROOT / 'std']
        if name == 'inputs_v2': modules += ['--define', 'COMPONENT_INPUTS_V2']
        if name == 'inputs_bad': modules += ['--define', 'COMPONENT_INPUTS_BAD']
        ir = guest / 'ir'
        run([ZIRAN, 'ir', '--root', guest, '--define', 'KRYON_HOSTED_UI',
             *modules, '-o', ir, guest / 'component.zi'])
        run([ZIRAN, 'bundle', '--root', ir, '--module-path', ir,
             '--define', 'KRYON_HOSTED_UI', '--entry', 'component:Frame',
             '-o', WORK / f'{name}.zx', ir / 'component.zir'])
        run([ZIRAN, 'bundle', '--root', guest, '--define', 'KRYON_HOSTED_UI',
             *modules, '--entry', 'component:Frame', '-o', guest / 'source.zx',
             guest / 'component.zi'])
        assert (WORK / f'{name}.zx').read_bytes() == (guest / 'source.zx').read_bytes(), name

    runtime = ROOT / 'build/zx-canvas/runtime/libziran.a'
    run(['make', '--no-print-directory', runtime, f'ZX_CANVAS_CC={EMCC}'])
    generated = WORK / 'c'
    run([ZIRAN, 'build', '--target=c', '--no-main', '--define', 'PLATFORM_WEB',
         '--define', 'BUNDLE_NATIVE_RENDERER', '--entry', 'component_browser_host:main',
         '--root', ROOT / 'tests', '--module-path', ROOT / 'src/backend',
         *UI_MODULE_ARGS, '--module-path', ZIRAN_ROOT / 'std',
         '-o', generated, ROOT / 'tests/component_browser_host.zi',
         ROOT / 'src/backend/canvas_raster.zi'])
    shell = WORK / 'shell.html'
    shell.write_text('''<!doctype html><html><head><meta charset="utf-8">
      <style>body{margin:0}#canvas-frame,canvas{width:320px;height:180px}canvas{display:block}</style>
      </head><body><div id="canvas-frame"><canvas id="canvas" width="320" height="180"></canvas></div>
      <pre id="result">running</pre><script>
      globalThis.componentProbe={}; globalThis.componentCommand=0; globalThis.componentPaused=false;
      var Module={print:console.log,printErr:console.error,
        onExit:code=>document.getElementById('result').textContent=code===0?'PASS':'FAIL '+code,
        onAbort:reason=>document.getElementById('result').textContent='ABORT '+reason};
      </script>{{{ SCRIPT }}}</body></html>''')
    embeds = []
    for name in guests:
        embeds += ['--embed-file', f'{WORK / (name + ".zx")}@/{name}.zx']
    run([EMCC, '-O1', '-I' + str(ZIRAN_ROOT / 'include'), '-iquote', generated,
         *sorted(generated.glob('*.c')), runtime,
         '--js-library', ZIRAN_ROOT / 'web/ziran_web.js',
         '--embed-file', str(ROOT / 'assets/fonts/LiberationSans-Regular.ttf') + '@/kryon-font.ttf',
         *embeds, '-sEXPORTED_RUNTIME_METHODS=FS', '-sASYNCIFY', '-sALLOW_MEMORY_GROWTH=1',
         '-sSTACK_SIZE=16777216', '-sASYNCIFY_STACK_SIZE=1048576',
         '-sEXIT_RUNTIME=1', '-sENVIRONMENT=web', '--shell-file', shell, '-o', OUTPUT])


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        pass


def pixels(browser, x=0, y=0, width=320, height=180):
    return browser.evaluate(f'''(() => {{
      const c=document.getElementById('canvas'); const scale=c.width/c.clientWidth;
      const data=c.getContext('2d').getImageData({x}*scale,{y}*scale,{width}*scale,{height}*scale).data;
      let hash=2166136261; for(const byte of data) hash=Math.imul(hash^byte,16777619)>>>0;
      return hash;
    }})()''')


def pause(browser, value):
    browser.evaluate('componentPaused=' + str(value).lower())
    browser.until('componentProbe.paused===' + str(value).lower(), 'host pause acknowledgement')


def command(browser, number):
    browser.evaluate(f'componentCommand={number}')
    browser.until(f'componentProbe.commandDone==={number}', f'command {number}')
    probe = browser.evaluate('componentProbe')
    assert probe['commandOK'] and (number in (8, 9) or probe['atomicOK']), probe
    return probe


def exercise(url):
    # Reuse the Canvas2D test's CDP transport; never touches a desktop display.
    canvas_browser.WORK = WORK
    canvas_browser.ENV = ENV
    browser = canvas_browser.Browser()
    try:
        browser.command('Page.addScriptToEvaluateOnNewDocument', source=r'''
          globalThis.componentServices={reads:0,writes:0,execCopy:0,measures:0};
          if(globalThis.Clipboard) for(const name of ['readText','writeText','write']) {
            const original=Clipboard.prototype[name]; if(!original)continue;
            Clipboard.prototype[name]=function(...args) {
              componentServices[name==='readText'?'reads':'writes']++;
              return original.apply(this,args);
            };
          }
          const execute=Document.prototype.execCommand;
          Document.prototype.execCommand=function(name,...args) {
            if(name==='copy'||name==='cut')componentServices.execCopy++;
            return execute.call(this,name,...args);
          };
          const measure=CanvasRenderingContext2D.prototype.measureText;
          CanvasRenderingContext2D.prototype.measureText=function(...args) {
            componentServices.measures++; return measure.apply(this,args);
          };
        ''')
        browser.viewport(320, 180)
        browser.open(url)
        browser.until('componentProbe.frames>=3 && componentProbe.a==="A 7" && componentProbe.b==="B 41"',
                      'two live ZX components', seconds=60)
        assert browser.stats()['webglRequests'] == 0, 'production host requested WebGL'
        baseline = pixels(browser)
        browser.mouse(40, 32)
        browser.until('componentProbe.a==="A 8" && componentProbe.focus===2', 'real browser click/focus')
        assert pixels(browser) != baseline, 'browser input did not change rendered pixels'
        pause(browser, True)
        before = pixels(browser)
        b_before = pixels(browser, 140, 0, 160, 48)
        for number, name in enumerate(('migrated clipboard render', 'ABI', 'schema', 'resource', 'init', 'truncated'), 1):
            # Queue an actual key event before each attempted replacement.
            virtual = 80 + number
            browser.tap_key(chr(virtual), 'Key' + chr(virtual), virtual)
            browser.until(f'componentProbe.lastKey==={virtual}', 'real pending browser key')
            probe = command(browser, number)
            assert probe['a'] == 'A 8' and probe['b'] == 'B 41' and probe['focus'] == 2, probe
            assert probe['oldValid'] and probe['bValid'] and probe['nodes'] == 6, probe
            assert pixels(browser) == before, name + ' changed live browser pixels'
            services = browser.evaluate('componentServices')
            assert services['writes'] == services['execCopy'] == services['reads'] == 0, services
            assert browser.stats()['webglRequests'] == 0
        swapped = command(browser, 7)
        assert not swapped['oldValid'] and swapped['bValid'], swapped
        assert pixels(browser) == before, 'replacement emitted pixels before its first live frame'
        pause(browser, False)
        browser.until('componentProbe.a==="A 8 v2" && componentProbe.b==="B 41"', 'migrated replacement frame')
        assert browser.evaluate('componentProbe.focus') == 2
        assert pixels(browser) != before
        assert pixels(browser, 140, 0, 160, 48) == b_before, 'unrelated B pixels changed'
        browser.mouse(40, 32)
        browser.until('componentProbe.a==="A 10 v2"', 'replacement browser interaction')
        for letter in 'advance':
            browser.tap_key(letter, 'Key' + letter.upper(), ord(letter.upper()), letter)
        browser.until('componentProbe.b==="B 42" && componentProbe.a==="A 10 v2"',
                      'unrelated B actual browser text continuation')
        pause(browser, True)
        # Positive control: outside staging the same native editor reaches the
        # actual browser clipboard API/fallback under a real input gesture.
        browser.mouse(300, 150)
        command(browser, 8)
        browser.until('componentServices.writes+componentServices.execCopy>0', 'live clipboard positive control')
        final = browser.evaluate('({probe:componentProbe,services:componentServices,stats:canvasStats})')
        assert not browser.errors, browser.errors
        assert final['stats']['webglRequests'] == 0
        command(browser, 9)
        pause(browser, False)
        browser.until('componentProbe.inputs==="inputs ready"', 'six hosted input state records')
        browser.tap_key('u', 'KeyU', 85, 'u')
        browser.until('componentProbe.inputs==="inputs updated"', 'six actual browser input state round trips')
        pause(browser, True)
        input_pixels = pixels(browser)
        services_before = browser.evaluate('JSON.stringify([componentServices.reads,componentServices.writes,componentServices.execCopy])')
        rejected = command(browser, 10)
        assert rejected['oldValid'] and rejected['inputs'] == 'inputs updated', rejected
        assert pixels(browser) == input_pixels, 'input candidate changed active pixels'
        assert browser.evaluate('JSON.stringify([componentServices.reads,componentServices.writes,componentServices.execCopy])') == services_before, 'staged input candidate emitted services'
        accepted = command(browser, 11)
        assert not accepted['oldValid'], accepted
        assert pixels(browser) == input_pixels
        pause(browser, False)
        browser.until('componentProbe.inputs==="inputs updated v2"', 'six owned input state records migrated')
        assert not browser.errors, browser.errors
        browser.evaluate('componentCommand=12')
        browser.until('document.getElementById("result").textContent==="PASS"', 'clean stale-handle teardown')
        print('Browser ComponentHost Canvas2D PASS ' + json.dumps({
            'swap_ms': swapped['swapMs'], 'frames': final['probe']['frames'],
            'live_a_bytes': final['probe']['liveA'], 'live_b_bytes': final['probe']['liveB'],
            'staged_clipboard_writes': 0, 'hosted_widget_records': 12,
            'live_clipboard_calls': final['services']['writes'] + final['services']['execCopy'],
            'webgl_requests': final['stats']['webglRequests'],
        }, sort_keys=True))
    finally:
        browser.close()


def main():
    prepare()
    handler = functools.partial(QuietHandler, directory=str(WORK))
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), handler)
    thread = threading.Thread(target=server.serve_forever)
    thread.start()
    try:
        exercise(f'http://127.0.0.1:{server.server_port}/{OUTPUT.name}')
    finally:
        server.shutdown()
        thread.join()
        server.server_close()


if __name__ == '__main__':
    main()
