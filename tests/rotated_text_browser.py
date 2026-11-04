#!/usr/bin/env python3
"""Verify real Canvas transforms and DOM text styles in private headless Chromium.

Uses only its DevTools pipe and never captures pixels.
"""

import json
import os
import re
from pathlib import Path
import select
import shutil
import subprocess
import sys
import time

html, profile = sys.argv[1], Path(sys.argv[2])
backend = sys.argv[3]
shutil.rmtree(profile, ignore_errors=True)
profile.mkdir(parents=True)
environment = dict(os.environ)
for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "GDK_DISPLAY", "DBUS_SESSION_BUS_ADDRESS",
             "SESSION_MANAGER"):
    environment.pop(name, None)
environment["YUE_DESKTOP_RECOVERY"] = "0"
browser = subprocess.Popen(
    ["sh", "-c", 'exec 3<&0 4>&1 "$@" </dev/null >/dev/null', "chromium",
     os.environ.get("CHROMIUM", "chromium"), "--headless", "--no-sandbox", "--disable-gpu",
     "--disable-webgl", "--use-gl=disabled",
     "--disable-dev-shm-usage", "--no-first-run", "--no-default-browser-check",
     "--remote-debugging-pipe", "--user-data-dir=" + str(profile), "about:blank"],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=(profile / "chromium.log").open("wb"), env=environment)
diagnostics = []
received = b""
sequence = 0
session = None


def command(method, **params):
    global received, sequence
    sequence += 1
    message = {"id": sequence, "method": method, "params": params}
    if session:
        message["sessionId"] = session
    browser.stdin.write(json.dumps(message).encode() + b"\0")
    browser.stdin.flush()
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        while b"\0" in received:
            raw, received = received.split(b"\0", 1)
            if not raw:
                continue
            reply = json.loads(raw)
            if reply.get('method')=='Runtime.consoleAPICalled' and reply['params'].get('type')=='error':
                diagnostics.append([arg.get('value',arg.get('description','')) for arg in reply['params']['args']])
            if reply.get("method") == "Runtime.exceptionThrown":
                detail = reply["params"].get("exceptionDetails", {})
                diagnostics.append((detail.get("exception") or {}).get("description") or detail.get("text"))
            if reply.get("id") == sequence:
                if "error" in reply:
                    raise RuntimeError(method + ": " + json.dumps(reply["error"]))
                return reply.get("result", {})
        if select.select([browser.stdout], [], [], 0.5)[0]:
            data = os.read(browser.stdout.fileno(), 1 << 20)
            if not data:
                raise RuntimeError("Private Chromium exited with code " + str(browser.poll()) + "; see " + str(profile / "chromium.log"))
            received += data
    raise TimeoutError(method)


def evaluate(expression):
    result = command("Runtime.evaluate", expression=expression, returnByValue=True)
    if "exceptionDetails" in result:
        raise RuntimeError(json.dumps(result["exceptionDetails"])[:2000])
    return result.get("result", {}).get("value")


try:
    target = next(item["targetId"] for item in command("Target.getTargets")["targetInfos"] if item["type"] == "page")
    session = command("Target.attachToTarget", targetId=target, flatten=True)["sessionId"]
    command("Runtime.enable")
    command("Page.enable")
    command("Emulation.setDeviceMetricsOverride", width=640, height=480, deviceScaleFactor=1, mobile=False)
    command("Page.addScriptToEvaluateOnNewDocument", source=r"""
      globalThis.__rotatedCalls=[];
      for (const Type of [globalThis.CanvasRenderingContext2D,globalThis.OffscreenCanvasRenderingContext2D]) {
        if (!Type) continue;
        const draw=Type.prototype.fillText;
        Type.prototype.fillText=function(text,...args) {
        if ((text==='Rotated' || text==='Horizontal') && __rotatedCalls.length<32) {
          const m=this.getTransform();
          __rotatedCalls.push({text,a:m.a,b:m.b,c:m.c,d:m.d,e:m.e,f:m.f,font:this.font});
        }
          return draw.call(this,text,...args);
        };
      }
      new MutationObserver(()=> {
        const root=document.getElementById('kryon-dom-paint');
        if(root) globalThis.__rotationDom=[...root.querySelectorAll('span')].filter(n=>n.textContent==='Rotated')
          .map(n=>({transform:n.style.transform,origin:n.style.transformOrigin,font:n.style.font,
            clip:n.style.clipPath,outer:n.parentElement.style.overflow}));
      }).observe(document,{childList:true,subtree:true,attributes:true});
    """)
    command("Page.navigate",url=html)
    command('Page.bringToFront')
    deadline=time.monotonic()+30
    while time.monotonic()<deadline:
        value=evaluate("({calls:globalThis.__rotatedCalls||[],dom:globalThis.__rotationDom||[]})")
        if (backend=='dom' and value['dom']) or (backend=='canvas' and
            all(any(c['text']==label for c in value['calls']) for label in ('Rotated','Horizontal'))):
            break
        time.sleep(.05)
    assert not diagnostics,diagnostics
    assert value['dom'] if backend=='dom' else value['calls'],(value,evaluate('({body:document.body.innerText.slice(0,1200),frames:globalThis.__fixtureFrames,paint:globalThis.__fixturePaint,visibility:document.visibilityState,ready:document.readyState,module:typeof Module,called:typeof Module!=="undefined"?Module.calledRun:null,main:typeof Module!=="undefined"?typeof Module._main:null,canvases:[...document.querySelectorAll("canvas")].map(n=>[n.width,n.height])})'))
    if backend=='dom':
        item=value['dom'][0]
        assert item['transform']=='rotate(27.5deg)',item
        assert item['origin']=='90px 20px',item
        assert item['clip'].startswith('polygon(') and item['outer']=='hidden',item
        assert float(re.search(r'([0-9.]+)px',item['font']).group(1))>=24,item
    else:
        import math
        rotated=next(c for c in value['calls'] if c['text']=='Rotated')
        flat=next(c for c in value['calls'] if c['text']=='Horizontal')
        assert rotated['font']==flat['font'] and float(re.search(r'([0-9.]+)px',rotated['font']).group(1))>=24,(rotated,flat)
        c=math.cos(math.radians(27.5));s=math.sin(math.radians(27.5))
        for actual,expected in [(rotated['a'],c),(rotated['b'],s),(rotated['c'],-s),(rotated['d'],c),
                                (rotated['e'],190-c*190+s*120),(rotated['f'],120-s*190-c*120),
                                (flat['a'],1),(flat['b'],0),(flat['c'],0),(flat['d'],1)]:
            assert abs(actual-expected)<.02,(rotated,flat,actual,expected)
    assert not diagnostics,diagnostics
    print(backend+': real renderer arbitrary angle, centre pivot, full font size and transform restoration PASS')
finally:
    try:
        command('Browser.close')
    except Exception:
        pass
    try:
        browser.wait(timeout=10)
    except subprocess.TimeoutExpired:
        browser.terminate();browser.wait(timeout=10)
