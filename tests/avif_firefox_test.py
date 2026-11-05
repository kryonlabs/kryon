#!/usr/bin/env python3
"""Read the prebuilt AVIF browser result using private Firefox Marionette.
Protocol: https://firefox-source-docs.mozilla.org/remote/marionette/Protocol.html
No owner browser/profile, display capture, Node or external Python packages.
"""
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

def main():
    assert (WORK/'test.html').is_file(),'Build make avif-browser-test first'
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=WORK))
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1',0));port=reserve.getsockname()[1]
    environment={k:v for k,v in os.environ.items() if k not in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER')}
    environment.update(MOZ_HEADLESS='1',LIBGL_ALWAYS_SOFTWARE='1',YUE_DESKTOP_RECOVERY='0')
    try:
        with tempfile.TemporaryDirectory(prefix='firefox-',dir=WORK) as temporary:
            profile=Path(temporary)
            (profile/'user.js').write_text(f'user_pref("marionette.port", {port});\nuser_pref("gfx.webrender.software", true);\nuser_pref("browser.shell.checkDefaultBrowser", false);\n')
            with (WORK/'firefox.log').open('wb') as log:
                process=subprocess.Popen(['firefox','--headless','--marionette','--no-remote','--profile',str(profile),'about:blank'],env=environment,stdout=log,stderr=subprocess.STDOUT)
                try:
                    deadline=time.monotonic()+30;connection=None
                    while time.monotonic()<deadline and process.poll() is None:
                        try:connection=socket.create_connection(('127.0.0.1',port),timeout=1);break
                        except OSError:time.sleep(.1)
                    assert connection is not None, f'Firefox startup exit {process.poll()}; see {WORK}/firefox.log'
                    with connection:
                        connection.settimeout(30); greeting=receive(connection);sequence=0
                        def command(method,parameters):
                            nonlocal sequence
                            sequence+=1;payload=json.dumps([0,sequence,method,parameters]).encode()
                            connection.sendall(str(len(payload)).encode()+b':'+payload)
                            reply=receive(connection)
                            assert reply[1]==sequence and reply[2] is None,reply
                            return reply[3]
                        command('WebDriver:NewSession',{'capabilities':{}})
                        command('WebDriver:Navigate',{'url':f'http://127.0.0.1:{server.server_port}/test.html'})
                        result=None
                        while time.monotonic()<deadline:
                            result=command('WebDriver:ExecuteScript',{'script':'return document.getElementById("result")?.textContent','args':[],'newSandbox':True,'sandbox':'default','line':1,'filename':'avif-test'})
                            if result.get('value') not in (None,'pending'):break
                            time.sleep(.1)
                        assert result=={'value':'PASS'},result
                        command('WebDriver:DeleteSession',{})
                finally:
                    process.terminate()
                    try:process.wait(timeout=10)
                    except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
    finally:server.shutdown();server.server_close();thread.join(timeout=5)
    print('AVIF Canvas/DOM actual Firefox decode: 8/10/12-bit RGBA/gray, alpha and first frame passed')
if __name__=='__main__':main()
