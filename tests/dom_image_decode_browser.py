#!/usr/bin/env python3
"""A DOM image texture is ready only once its img has decoded.

Drives only the private headless Chromium started here, over its DevTools
pipe, with no desktop connection. A mutation observer reads the page's imgs
as each frame is published, before the browser can run a load task: the size
the application was given must already be on screen, and so must an image
shown again after other textures loaded. A decode the browser rejects must
report the image as unavailable.
"""

import json
import os
from pathlib import Path
import select
import shutil
import subprocess
import sys
import time

html, profile = sys.argv[1], Path(sys.argv[2])
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
     "--use-gl=disabled", "--disable-dev-shm-usage", "--no-first-run", "--no-default-browser-check",
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
                raise RuntimeError("Chromium exited: " + (profile / "chromium.log").read_text(errors="replace")[-4000:])
            received += data
    raise TimeoutError(method)


def evaluate(expression):
    result = command("Runtime.evaluate", expression=expression, returnByValue=True)
    if "exceptionDetails" in result:
        raise RuntimeError(json.dumps(result["exceptionDetails"])[:2000])
    return result.get("result", {}).get("value")


def until(expression, what):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if evaluate("!!(" + expression + ")"):
            return
        time.sleep(0.03)
    raise AssertionError("Timed out: " + what + " " + json.dumps(evaluate("[__decodeSeen, __again, __blank]")) + " "
                         + " ".join(map(str, diagnostics)))


try:
    target = next(item["targetId"] for item in command("Target.getTargets")["targetInfos"] if item["type"] == "page")
    session = command("Target.attachToTarget", targetId=target, flatten=True)["sessionId"]
    command("Runtime.enable")
    command("Page.enable")
    command("Emulation.setDeviceMetricsOverride", width=800, height=600, deviceScaleFactor=1, mobile=False)
    # In the microtask after each published frame, record the first page img,
    # its next showing after other textures loaded, and any img shown blank.
    # The driver can also make the browser reject image decodes.
    command("Page.addScriptToEvaluateOnNewDocument", source="""
        globalThis.__decodeSeen = null; globalThis.__again = null; globalThis.__blank = [];
        new MutationObserver(() => {
          const root = document.querySelector('#kryon-dom-paint');
          const images = root ? [...root.querySelectorAll('img')] : [];
          for (const image of images) {
            if (!image.complete || image.naturalWidth === 0) __blank.push(image.src);
          }
          if (!__decodeSeen && images.length) {
            const image = globalThis.__pageImage = images[0];
            __decodeSeen = {width: globalThis.__kryonWidth, height: globalThis.__kryonHeight,
              complete: image.complete, naturalWidth: image.naturalWidth, naturalHeight: image.naturalHeight};
          }
          const page = images.find(image => image.src === globalThis.__pageImage?.src);
          if (globalThis.__kryonAgain && !__again && page) {
            // The second showing draws from the texture's canvas.
            const copy = [...root.querySelectorAll('canvas')].find(tile => tile.style.left === '320px');
            const pixel = copy && copy.getContext('2d').getImageData(copy.width / 2, copy.height / 2, 1, 1).data;
            __again = {complete: page.complete, naturalWidth: page.naturalWidth, same: page === __pageImage,
              images: images.filter(image => image.src === page.src).length, copy: pixel ? [...pixel] : null};
          }
        }).observe(document, {subtree: true, childList: true, attributes: true, attributeFilter: ['src']});
        const decode = HTMLImageElement.prototype.decode;
        HTMLImageElement.prototype.decode = function () {
          return globalThis.__kryonRejectDecode ? Promise.reject(new DOMException('rejected', 'EncodingError'))
            : decode.call(this);
        };
    """)
    command("Page.navigate", url=html)
    until("globalThis.__kryonFrames >= 3", "page ready")
    if evaluate("document.querySelector('#kryon-dom-paint img') !== null"):
        raise AssertionError("an image was painted before one was requested")

    evaluate("globalThis.__kryonLoad = true")
    until("__decodeSeen", "the page img")
    seen = evaluate("__decodeSeen")
    if seen["width"] != 300 or seen["height"] != 200:
        raise AssertionError("the frame that painted the image reported no size: " + json.dumps(seen))
    if not seen["complete"] or seen["naturalWidth"] != 300 or seen["naturalHeight"] != 200:
        raise AssertionError("the image reported ready before its img decoded: " + json.dumps(seen))
    print("ok a texture reports 300x200 only with its img decoded")
    if evaluate("__kryonBrokenWidth") != 0:
        raise AssertionError("an undecodable image reported a size")
    print("ok an undecodable image reports unavailable")

    evaluate("globalThis.__kryonOthers = true")
    until("""globalThis.__kryonOthersLoaded === 6 && !document.querySelector('#kryon-dom-paint')
        .contains(__pageImage)""", "other textures loaded with the page image hidden")
    evaluate("globalThis.__kryonAgain = true")
    until("__again", "the page image shown again")
    again = evaluate("__again")
    if not again["complete"] or again["naturalWidth"] != 300 or not again["same"] or again["images"] != 1:
        raise AssertionError("the image shown again was not its decoded img: " + json.dumps(again))
    if not again["copy"] or again["copy"][3] == 0:
        raise AssertionError("the second showing in one frame drew nothing: " + json.dumps(again))
    print("ok an image shown again after other textures loaded is its decoded img, and a second copy draws")

    frames = evaluate("__kryonFrames")
    evaluate("globalThis.__kryonRejectDecode = true; globalThis.__kryonReject = true")
    until("globalThis.__kryonFrames >= %d && globalThis.__kryonRejectedWidth !== undefined" % (frames + 3),
          "frames after a rejected decode")
    if evaluate("__kryonRejectedWidth") != 0:
        raise AssertionError("an image whose img decode failed reported a size")
    print("ok a rejected img decode reports unavailable and frames continue")

    blank = evaluate("__blank")
    if blank:
        raise AssertionError("imgs were published before they decoded: " + json.dumps(blank))
    if diagnostics:
        raise AssertionError("page errors: " + " ".join(map(str, diagnostics)))
    evaluate("globalThis.__kryonTestDone = true")
    print("Semantic DOM image textures wait for their img decode passed")
finally:
    try:
        command("Browser.close")
    except Exception:
        pass
    try:
        browser.wait(timeout=10)
    except subprocess.TimeoutExpired:
        browser.terminate()
        browser.wait(timeout=10)
