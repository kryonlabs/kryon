#!/usr/bin/env python3
"""Read the prebuilt AVIF browser result using private Firefox Marionette.
Protocol: https://firefox-source-docs.mozilla.org/remote/marionette/Protocol.html
No owner browser/profile, display capture, Node or external Python packages.

The page reports the browser's AVIF capability explicitly. Firefox must
report support and decode the goldens. Firefox can no longer disable AVIF, so
the unsupported-browser run loads the same page with a shim that makes
createImageBitmap reject AVIF exactly as a browser without AVIF rejects any
undecodable image; the page must report no support, fail every AVIF decode
cleanly and still decode PNG.
"""
from contextlib import contextmanager
import functools
import http.server
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import time
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/scratch/avif-browser'
class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self,*args):pass

def receive(connection):
    size=b''
    while not size.endswith(b':'):
        byte=connection.recv(1)
        if not byte:raise RuntimeError('Firefox connection closed')
        size+=byte
    count=int(size[:-1]);data=b''
    if count>65536:raise RuntimeError('Unexpected large protocol response')
    while len(data)<count:
        block=connection.recv(count-len(data))
        if not block:raise RuntimeError('Firefox connection closed during response')
        data+=block
    return json.loads(data)

@contextmanager
def firefox(directory):
    """Private headless Firefox serving directory; yields (command, base URL)."""
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=directory))
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1',0));port=reserve.getsockname()[1]
    environment={k:v for k,v in os.environ.items() if k not in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER')}
    environment.update(MOZ_HEADLESS='1',LIBGL_ALWAYS_SOFTWARE='1',YUE_DESKTOP_RECOVERY='0')
    try:
        with tempfile.TemporaryDirectory(prefix='firefox-',dir=WORK) as temporary:
            profile=Path(temporary)
            prefs=[f'user_pref("marionette.port", {port});','user_pref("gfx.webrender.software", true);',
                   'user_pref("browser.shell.checkDefaultBrowser", false);']
            (profile/'user.js').write_text('\n'.join(prefs)+'\n')
            with (WORK/'firefox.log').open('wb') as log:
                process=subprocess.Popen(['firefox','--headless','--marionette','--no-remote','--profile',str(profile),'about:blank'],env=environment,stdout=log,stderr=subprocess.STDOUT)
                try:
                    # A fresh profile on a busy CI runner can take over 30 s
                    # before Marionette listens.
                    deadline=time.monotonic()+120;connection=None
                    while time.monotonic()<deadline and process.poll() is None:
                        try:connection=socket.create_connection(('127.0.0.1',port),timeout=1);break
                        except OSError:time.sleep(.1)
                    if connection is None:
                        log.flush();tail=(WORK/'firefox.log').read_bytes()[-4000:].decode(errors='replace')
                        raise AssertionError(f'Firefox startup exit {process.poll()}; {WORK}/firefox.log ends:\n{tail}')
                    with connection:
                        connection.settimeout(300); receive(connection);sequence=0
                        def command(method,parameters):
                            nonlocal sequence
                            sequence+=1;payload=json.dumps([0,sequence,method,parameters]).encode()
                            connection.sendall(str(len(payload)).encode()+b':'+payload)
                            reply=receive(connection)
                            assert reply[1]==sequence and reply[2] is None,reply
                            return reply[3]
                        command('WebDriver:NewSession',{'capabilities':{}})
                        yield command,f'http://127.0.0.1:{server.server_port}'
                        command('WebDriver:DeleteSession',{})
                finally:
                    process.terminate()
                    try:process.wait(timeout=10)
                    except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
    finally:server.shutdown();server.server_close();thread.join(timeout=5)

# A browser without AVIF: its own decoder rejects AVIF like any undecodable image.
UNSUPPORTED_SHIM='''<script>
(() => {
  const native = globalThis.createImageBitmap;
  globalThis.createImageBitmap = async function (source, ...rest) {
    if (source instanceof Blob) {
      const head = String.fromCharCode(...new Uint8Array(await source.slice(0, 32).arrayBuffer()));
      if (/ftypavi[fs]/.test(head)) throw new DOMException('The source image could not be decoded.', 'InvalidStateError');
    }
    return native.call(this, source, ...rest);
  };
})();
</script>'''

def page_result(page):
    with firefox(WORK) as (command,base):
        command('WebDriver:Navigate',{'url':f'{base}/{page}'})
        deadline=time.monotonic()+60;result=None
        while time.monotonic()<deadline:
            result=command('WebDriver:ExecuteScript',{'script':'return document.getElementById("result")?.textContent','args':[],'newSandbox':True,'sandbox':'default','line':1,'filename':'avif-test'})
            if result.get('value') not in (None,'pending'):break
            time.sleep(.1)
        return result

# Mirrors PageImageDecode: createImageBitmap, canvas draw and RGBA readback.
BENCHMARK_SCRIPT='''
const [cases, done] = [arguments[0], arguments[arguments.length - 1]];
(async () => {
  const rows = [];
  for (const [name, url, iterations] of cases) {
    const blob = await (await fetch(url)).blob();
    const samples = [];
    let width = 0, height = 0;
    for (let i = 0; i <= iterations; i++) {
      const start = performance.now();
      const bitmap = await createImageBitmap(blob);
      const canvas = new OffscreenCanvas(bitmap.width, bitmap.height);
      const context = canvas.getContext('2d');
      context.drawImage(bitmap, 0, 0);
      context.getImageData(0, 0, bitmap.width, bitmap.height);
      [width, height] = [bitmap.width, bitmap.height];
      bitmap.close();
      if (i) samples.push(performance.now() - start);
    }
    samples.sort((a, b) => a - b);
    rows.push({name, width, height, iterations, ms: samples[samples.length >> 1], min_ms: samples[0]});
  }
  done(rows);
})().catch(error => done({error: String(error)}));
'''

def benchmark(cases,work):
    """Firefox's own AVIF decoder timings for (name, path, iterations)."""
    directory=Path(work)/'browser-corpus';directory.mkdir(parents=True,exist_ok=True)
    for name,path,_ in cases:(directory/f'{name}.avif').write_bytes(Path(path).read_bytes())
    (directory/'index.html').write_text('<!doctype html><title>avif benchmark</title>')
    WORK.mkdir(parents=True,exist_ok=True)
    with firefox(directory) as (command,base):
        command('WebDriver:Navigate',{'url':f'{base}/index.html'})
        version=command('WebDriver:ExecuteScript',{'script':'return navigator.userAgent','args':[]})['value']
        command('WebDriver:SetTimeouts',{'script':600000})
        rows=command('WebDriver:ExecuteAsyncScript',{'script':BENCHMARK_SCRIPT,
            'args':[[[name,f'{base}/{name}.avif',iterations] for name,_,iterations in cases]]})['value']
    assert isinstance(rows,list),rows
    for row in rows:row.update(user_agent=version,peak_rss_kib=-1)
    return rows

def main():
    assert (WORK/'test.html').is_file(),'Build make avif-browser-test first'
    supported=page_result('test.html')
    assert supported=={'value':'PASS supported'},supported
    page=(WORK/'test.html').read_text()
    assert page.count('<head>')==1
    (WORK/'test-unsupported.html').write_text(page.replace('<head>','<head>'+UNSUPPORTED_SHIM,1))
    unsupported=page_result('test-unsupported.html')
    assert unsupported=={'value':'PASS unsupported'},unsupported
    print('AVIF Canvas/DOM actual Firefox decode: capability reported, 8/10/12-bit RGBA/gray, alpha, first frame and corpus sizes passed')
    print('AVIF unsupported browser (createImageBitmap rejects AVIF): capability false, every AVIF decode failed cleanly, PNG still decoded')
if __name__=='__main__':main()
