#!/usr/bin/env python3
"""Press semantic DOM buttons while the page republishes them.

Drives only the private headless Chromium started here, over its DevTools
pipe, with no desktop connection. The browser sends no click when the element
a press landed on moves before the release, so the release on that same
button stands in for it: once, never beside the click, never on a release
elsewhere, after focus loss or on a replacement, and a raw page press released
over a button must still release the page's pointer.
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
for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "GDK_DISPLAY", "DBUS_SESSION_BUS_ADDRESS"):
    environment.pop(name, None)
browser = subprocess.Popen(
    ["sh", "-c", 'exec 3<&0 4>&1 "$@" </dev/null >/dev/null', "chromium",
     os.environ.get("CHROMIUM", "chromium"), "--headless", "--no-sandbox", "--disable-gpu",
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


def state():
    return evaluate("""({plain: __kryonPlainClicks, replaced: __kryonReplacedClicks,
        other: __kryonOtherClicks, down: __kryonPointerDown, moves: __plainMoves,
        native: __plainNativeClicks, frames: __kryonFrames})""")


def until(expression, what):
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        if evaluate("!!(" + expression + ")"):
            return
        time.sleep(0.03)
    raise AssertionError("Timed out: " + what + " " + json.dumps(state()) + " " + " ".join(map(str, diagnostics)))


def frames(count=4):
    """Let a few frames pass, so a wrongly resolved release would arrive."""
    start = evaluate("__kryonFrames")
    until("__kryonFrames >= %d" % (start + count), "%d frames" % count)


def flags(**values):
    evaluate(";".join("globalThis.__kryon%s = %s" % (name, json.dumps(value)) for name, value in values.items()))
    frames()


def at(label):
    box = evaluate("""(() => { const box = document.querySelector('[aria-label="%s"]')
        .getBoundingClientRect(); return [box.left + box.width / 2, box.top + box.height / 2]; })()""" % label)
    return {"x": round(box[0]), "y": round(box[1])}


EMPTY = {"x": 500, "y": 400}


def press(point, button="left"):
    command("Input.dispatchMouseEvent", type="mouseMoved", **point)
    command("Input.dispatchMouseEvent", type="mousePressed", button=button, clickCount=1,
            buttons=1 if button == "left" else 2, **point)


def release(point, button="left"):
    command("Input.dispatchMouseEvent", type="mouseMoved", button=button,
            buttons=1 if button == "left" else 2, **point)
    command("Input.dispatchMouseEvent", type="mouseReleased", button=button, clickCount=1, buttons=0, **point)
    frames()


def expect(what, before, **changes):
    now = state()
    for name, change in changes.items():
        if name == "down":
            continue
        if now[name] - before[name] != change:
            raise AssertionError(what + ": " + name + " changed by " + str(now[name] - before[name])
                                 + ", expected " + str(change) + " " + json.dumps({"before": before, "now": now}))
    if "down" in changes and now["down"] != changes["down"]:
        raise AssertionError(what + ": page pointer down " + json.dumps(now))
    print("ok", what)


def moved_plain():
    """Hold a press on Plain while the page really moves it ahead of Other."""
    press(at("Plain"))
    evaluate("globalThis.__kryonPlainFirst = true")
    until("""(() => { const plain = document.querySelector('[aria-label="Plain"]');
        const other = document.querySelector('[aria-label="Other"]');
        return (plain.compareDocumentPosition(other) & Node.DOCUMENT_POSITION_FOLLOWING) != 0; })()""",
          "Plain moved ahead of Other")
    frames(2)


try:
    target = next(item["targetId"] for item in command("Target.getTargets")["targetInfos"] if item["type"] == "page")
    session = command("Target.attachToTarget", targetId=target, flatten=True)["sessionId"]
    command("Runtime.enable")
    command("Page.enable")
    command("Emulation.setDeviceMetricsOverride", width=800, height=600, deviceScaleFactor=1, mobile=False)
    # Count the page's own moves of Plain and the clicks the browser sends it.
    command("Page.addScriptToEvaluateOnNewDocument", source="""
        globalThis.__plainMoves = 0; globalThis.__plainNativeClicks = 0;
        const plain = node => node && node.isConnected && node.getAttribute && node.getAttribute('aria-label') === 'Plain';
        for (const [owner, name] of [[Node.prototype, 'insertBefore'], [Element.prototype, 'moveBefore']]) {
          const original = owner[name];
          if (typeof original !== 'function') continue;
          owner[name] = function (node, ...rest) {
            if (plain(node)) globalThis.__plainMoves += 1;
            return original.call(this, node, ...rest);
          };
        }
        addEventListener('click', event => {
          if (event.target.closest && event.target.closest('[aria-label="Plain"]')) globalThis.__plainNativeClicks += 1;
        }, true);
    """)
    command("Page.navigate", url=html)
    until("""globalThis.__kryonPressReady && document.querySelector('[aria-label="Plain"]')
        && document.querySelector('[aria-label="Other"]') && document.querySelector('[aria-label="Editor"]')""",
          "page ready")
    frames()

    before = state()
    press(at("Plain")); release(at("Plain"))
    expect("a click activates once", before, plain=1, native=1, moves=0)

    before = state()
    press(at("Plain"))
    evaluate("globalThis.__kryonInsertAhead = true")
    until("""document.querySelector('[aria-label="Ahead"]')""", "a sibling ahead of Plain")
    frames(2)
    release(at("Plain"))
    expect("a sibling inserted ahead leaves the press in place", before, plain=1, native=1, moves=0)
    flags(InsertAhead=False)

    before = state()
    moved_plain()
    release(at("Plain"))
    after = state()
    if after["moves"] == before["moves"] or after["native"] != before["native"]:
        raise AssertionError("the browser kept its click across a real move, so the release was not tested: "
                             + json.dumps({"before": before, "after": after}))
    expect("a release on a moved button activates it once", before, plain=1, other=0)
    flags(PlainFirst=False)

    for label, point in (("no control", lambda: EMPTY), ("another button", lambda: at("Other")),
                         ("an editor", lambda: at("Editor"))):
        before = state()
        moved_plain()
        release(point())
        expect("a release over " + label + " cancels the moved press", before, plain=0, other=0, native=0)
        flags(PlainFirst=False)

    before = state()
    moved_plain()
    evaluate("dispatchEvent(new Event('blur'))")
    release(at("Plain"))
    expect("focus lost mid press cancels it", before, plain=0, other=0, native=0)
    # The page's pointer follows only a focused window.
    evaluate("dispatchEvent(new Event('focus'))")
    flags(PlainFirst=False)

    before = state()
    evaluate("""globalThis.__pressed = document.querySelector('[aria-label="Plain"]')""")
    press(at("Plain"))
    evaluate("globalThis.__kryonReplacePlain = true")
    until("""document.querySelector('[aria-label="Plain"]') !== __pressed""", "Plain replaced")
    frames(2)
    release(at("Plain"))
    expect("a replaced button activates nothing", before, plain=0, replaced=0, other=0)
    flags(ReplacePlain=False)

    before = state()
    press(EMPTY)
    frames()
    if not state()["down"]:
        raise AssertionError("a press on no control did not reach the page's pointer " + json.dumps(state()))
    release(at("Plain"))
    expect("a page press released over a button releases the page", before, plain=0, other=0, down=False)
    before = state()
    press(at("Plain")); release(at("Plain"))
    expect("input after that page press still activates", before, plain=1, native=1)

    before = state()
    press(at("Plain"), "right"); release(at("Plain"), "right")
    expect("a secondary press activates nothing", before, plain=0, native=0)

    before = state()
    evaluate("""document.querySelector('[aria-label="Plain"]').focus()""")
    for kind in ("keyDown", "keyUp"):
        command("Input.dispatchKeyEvent", type=kind, key="Enter", code="Enter",
                windowsVirtualKeyCode=13, nativeVirtualKeyCode=13, **({"text": "\r"} if kind == "keyDown" else {}))
    frames()
    expect("a keyboard click activates once", before, plain=1)

    if diagnostics:
        raise AssertionError("page errors: " + " ".join(map(str, diagnostics)))
    evaluate("globalThis.__kryonTestDone = true")
    print("Semantic DOM presses across republished buttons passed")
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
