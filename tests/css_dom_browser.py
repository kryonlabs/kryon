#!/usr/bin/env python3
"""Build a production DOM app and verify style IDs in private Chromium."""

import fcntl
import json
import os
from pathlib import Path
import re
import select
import shlex
import shutil
import subprocess
import tempfile
import time

from toolchain import ZIRAN, ZIRAN_ROOT, chromium_temp_directory


ROOT = Path(__file__).resolve().parents[1]
CSS_ROOT = Path(os.environ.get("CSS_ROOT", ROOT.parent / "packages/css"))
PROJECT = ROOT / "build/scratch/css-dom-project"
BROWSER_CSS = ROOT / "build/scratch/css-dom-browser.css"
ENV = dict(os.environ)
for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "GDK_DISPLAY",
             "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER", "DESKTOP_SESSION",
             "XDG_SESSION_ID", "XDG_SESSION_TYPE"):
    ENV.pop(name, None)
ENV["YUE_DESKTOP_RECOVERY"] = "0"
ENV.setdefault("EM_CACHE", str(ROOT / "build/emscripten-cache"))


def run(command, directory, timeout=300):
    printable = shlex.join(map(str, command))
    print(printable, flush=True)
    subprocess.run(list(map(str, command)), cwd=directory, env=ENV,
                   check=True, timeout=timeout)


def css_tool():
    tool = Path(os.environ.get("CSS_TOOL", CSS_ROOT / "build/tools/css_tool"))
    if not tool.is_file():
        run(["make", "-C", CSS_ROOT, "build/tools/css_tool"], ROOT)
    return tool


def build_project():
    shutil.rmtree(PROJECT, ignore_errors=True)
    (PROJECT / "src").mkdir(parents=True)
    with (PROJECT / "lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    (PROJECT / "ziran.toml").write_text(
        '[package]\nname = "css_dom"\nentry = "src/app.zi"\n'
        'module_roots = ["src"]\nbridge_modules = ["app"]\n\n'
        '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\nref = "master"\n\n'
        '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\nref = "master"\n\n'
        '[dependencies.css]\ngit = "https://github.com/kryonlabs/css.git"\nref = "master"\n\n'
        '[tool.kryon]\ndefault_profile = "web"\n\n'
        '[tool.kryon.profiles.web]\nbackend = "dom"\n'
    )
    (PROJECT / "ziran.local.toml").write_text(
        "[overrides]\n"
        f'ziran = "{ZIRAN_ROOT}"\n'
        f'kryon = "{ROOT}"\n'
        f'css = "{CSS_ROOT}"\n'
    )
    shutil.copyfile(ROOT / "tests/css_dom_app.zi", PROJECT / "src/app.zi")
    run([ZIRAN, "lock"], PROJECT)
    run([ZIRAN, "tool", "kryon", "build", "--profile", "web"], PROJECT)
    output = PROJECT / "build/css_dom-web.html"
    if not output.is_file() or output.stat().st_size < 100_000:
        raise AssertionError("the production DOM page was not generated")
    if not next((PROJECT / "build/generated/web/ir").glob("*_dom_run.zir"), None):
        raise AssertionError("the saved DOM runner IR was not generated")
    if not next((PROJECT / "build/generated/web/c").glob("*_dom_run.c"), None):
        raise AssertionError("the generated DOM runner C was not generated")
    return output


class Chromium:
    def __init__(self, profile):
        self.diagnostics = []
        self.received = b""
        self.sequence = 0
        self.session = None
        self.sockets = chromium_temp_directory()
        try:
            self.log = (profile / "chromium.log").open("wb")
            previous_tmpdir = ENV.get("TMPDIR")
            ENV["TMPDIR"] = self.sockets.name
            try:
                self.process = subprocess.Popen(
                    ["sh", "-c", 'exec 3<&0 4>&1 "$@" </dev/null >/dev/null',
                     "chromium", os.environ.get("CHROMIUM", "chromium"), "--headless",
                     "--no-sandbox", "--disable-gpu", "--disable-webgl",
                     "--disable-software-rasterizer", "--disable-gpu-compositing",
                     "--use-gl=disabled", "--disable-dev-shm-usage", "--no-first-run",
                     "--no-default-browser-check", "--remote-debugging-pipe",
                     "--user-data-dir=" + str(profile), "about:blank"],
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log,
                    env=ENV)
            finally:
                if previous_tmpdir is None:
                    ENV.pop("TMPDIR", None)
                else:
                    ENV["TMPDIR"] = previous_tmpdir
        except BaseException:
            if hasattr(self, "log"):
                self.log.close()
            self.sockets.cleanup()
            raise

    def command(self, method, **params):
        self.sequence += 1
        message = {"id": self.sequence, "method": method, "params": params}
        if self.session:
            message["sessionId"] = self.session
        self.process.stdin.write(json.dumps(message).encode() + b"\0")
        self.process.stdin.flush()
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            while b"\0" in self.received:
                raw, self.received = self.received.split(b"\0", 1)
                if not raw:
                    continue
                reply = json.loads(raw)
                if reply.get("method") == "Runtime.exceptionThrown":
                    details = reply["params"].get("exceptionDetails", {})
                    self.diagnostics.append(details.get("exception", {}).get("description")
                                            or details.get("text"))
                if reply.get("id") == self.sequence:
                    if "error" in reply:
                        raise RuntimeError(method + ": " + json.dumps(reply["error"]))
                    return reply.get("result", {})
            if select.select([self.process.stdout], [], [], .2)[0]:
                data = os.read(self.process.stdout.fileno(), 1 << 20)
                if not data:
                    raise RuntimeError("private Chromium exited; see " + str(self.log.name))
                self.received += data
        raise TimeoutError(method)

    def evaluate(self, expression):
        result = self.command("Runtime.evaluate", expression=expression,
                              returnByValue=True)
        if "exceptionDetails" in result:
            raise RuntimeError(json.dumps(result["exceptionDetails"])[:2000])
        return result.get("result", {}).get("value")

    def until(self, expression, description, seconds=20):
        deadline = time.monotonic() + seconds
        last = None
        while time.monotonic() < deadline:
            last = self.evaluate(expression)
            if last:
                return last
            if self.diagnostics:
                break
            time.sleep(.05)
        raise AssertionError(description + ": " + repr(last) + " " + repr(self.diagnostics))

    def close(self):
        try:
            self.command("Browser.close")
        except Exception:
            pass
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            self.process.wait(timeout=5)
        self.log.close()
        self.sockets.cleanup()


INITIAL = r"""
(() => {
  const root = document.querySelector('#kryon-dom-root');
  if (!root) return false;
  const button = label => [...root.querySelectorAll('button')]
    .find(node => node.textContent === label);
  const nodes = ['Save', 'Other', 'State', 'Focus', 'Disabled', 'Plain'].map(button);
  globalThis.__cssDomInitial = Object.fromEntries(nodes.map((node, index) => [index, node]));
  return !!(root && nodes.every(Boolean) && globalThis.__cssDomMinHeight === 64);
})()
"""


UPDATED = r"""
(() => {
  const root = document.querySelector('#kryon-dom-root');
  const button = label => [...root.querySelectorAll('button')]
    .find(node => node.textContent === label);
  const nodes = ['Other', 'Save updated', 'State', 'Focus', 'Disabled', 'Plain'].map(button);
  const initial = globalThis.__cssDomInitial || {};
  const save = nodes[1];
  return !!(root && nodes.every(Boolean) && initial[0] === save &&
    initial[1] === nodes[0] && initial[2] === nodes[2] &&
    initial[3] === nodes[3] && initial[4] === nodes[4] &&
    initial[5] === nodes[5] && initial[0].parentElement === save.parentElement);
})()
"""


VALUES = r"""
(() => {
  const root = document.querySelector('#kryon-dom-root');
  const button = label => [...root.querySelectorAll('button')]
    .find(node => node.textContent === label);
  const save = button('Save updated');
  const other = button('Other');
  const state = button('State');
  const focus = button('Focus');
  const disabled = button('Disabled');
  const plain = button('Plain');
  if (!save || !other || !state || !focus || !disabled || !plain) return null;
  const saveStyle = getComputedStyle(save);
  return {
    ids: {
      save: save.getAttribute('data-kryon-id'),
      other: other.getAttribute('data-kryon-id'),
      state: state.getAttribute('data-kryon-id'),
      focus: focus.getAttribute('data-kryon-id'),
      disabled: disabled.getAttribute('data-kryon-id')
    },
    plainId: plain.getAttribute('data-kryon-id'),
    plainClass: plain.getAttribute('data-kryon-class'),
    saveWidth: saveStyle.width,
    saveMinHeight: saveStyle.minHeight,
    saveFontSize: saveStyle.fontSize,
    saveLineRatio: Number(saveStyle.lineHeight.replace('px', '')) /
      Number(saveStyle.fontSize.replace('px', '')),
    saveFamily: saveStyle.fontFamily,
    saveBackground: saveStyle.backgroundColor,
    saveTransitionDuration: saveStyle.transitionDuration,
    saveTransitionTiming: saveStyle.transitionTimingFunction,
    otherSelectedBackground: getComputedStyle(other).backgroundColor,
    stateSelected: state.getAttribute('aria-selected'),
    stateBackground: getComputedStyle(state).backgroundColor,
    focusBackground: getComputedStyle(focus).backgroundColor,
    disabledBackground: getComputedStyle(disabled).backgroundColor,
    plainBackground: getComputedStyle(plain).backgroundColor,
    typed: {
      minHeight: globalThis.__cssDomMinHeight,
      lineHeight: globalThis.__cssDomLineHeight,
      typeface: globalThis.__cssDomTypeface,
      elevation: globalThis.__cssDomElevation,
      duration: globalThis.__cssDomMotionDuration,
      multiplier: globalThis.__cssDomMotionMultiplier
    }
  };
})()
"""


FOCUSED = r"""
(() => {
  const root = document.querySelector('#kryon-dom-root');
  const focus = [...root.querySelectorAll('button')]
    .find(node => node.textContent === 'Focus');
  if (!focus || document.activeElement !== focus) return false;
  return getComputedStyle(focus).backgroundColor === 'rgb(80, 96, 112)';
})()
"""


def verify(output):
    source = BROWSER_CSS.read_text()
    required = ['[aria-selected="true"]', ":focus", '[aria-disabled="true"]',
                "prefers-reduced-motion: reduce"]
    for selector in required:
        if selector not in source:
            raise AssertionError("browser export lacks " + selector)
    profile = PROJECT / "browser-profile"
    shutil.rmtree(profile, ignore_errors=True)
    profile.mkdir(parents=True)
    browser = Chromium(profile)
    try:
        target = next(item["targetId"] for item in
                      browser.command("Target.getTargets")["targetInfos"]
                      if item["type"] == "page")
        browser.session = browser.command("Target.attachToTarget", targetId=target,
                                           flatten=True)["sessionId"]
        browser.command("Runtime.enable")
        browser.command("Page.enable")
        browser.command("Emulation.setDeviceMetricsOverride", width=320,
                        height=520, deviceScaleFactor=1, mobile=False)
        browser.command("Page.navigate", url=output.as_uri())
        browser.until(INITIAL, "initial production DOM")
        browser.evaluate("globalThis.__cssDomUpdate = true")
        browser.until(UPDATED, "identity-preserving update")
        browser.command("Page.bringToFront")
        browser.evaluate("([...document.querySelectorAll('button')]" +
                         ".find(node => node.textContent === 'Focus').focus())")
        browser.until(FOCUSED, "focused state")
        values = browser.until(VALUES, "computed styles and typed values")
        if values["plainId"] is not None:
            raise AssertionError("zero style ID was exported: " + repr(values["plainId"]))
        if not values["plainClass"] or values["plainClass"] == "0":
            raise AssertionError("class baseline was not exported")
        ids = values["ids"]
        if set(ids) != {"save", "other", "state", "focus", "disabled"}:
            raise AssertionError("missing named style IDs: " + repr(ids))
        if len(set(ids.values())) != 5:
            raise AssertionError("style IDs are not distinct: " + repr(ids))
        for name, value in ids.items():
            if not re.fullmatch(r"-?(0|[1-9][0-9]*)", value or ""):
                raise AssertionError(name + " is not signed decimal: " + repr(value))
            if f'[data-kryon-id="{value}"]' not in source:
                raise AssertionError(f"export lacks selector for {name} {value}")
        expected = {
            "saveWidth": "180px", "saveMinHeight": "64px",
            "saveFontSize": "17px",
            "saveBackground": "rgb(16, 32, 48)",
            "saveTransitionDuration": "0.123s",
            "saveTransitionTiming": "linear",
            "otherSelectedBackground": "rgb(48, 32, 16)",
            "stateSelected": "true",
            "stateBackground": "rgb(64, 80, 96)",
            "disabledBackground": "rgb(96, 112, 128)",
            "plainBackground": "rgb(17, 17, 17)"
        }
        for name, wanted in expected.items():
            if values[name] != wanted:
                raise AssertionError(f"{name}: expected {wanted!r}, got {values[name]!r}")
        if abs(values["saveLineRatio"] - 1.4) > .01:
            raise AssertionError("line-height ratio: " + repr(values["saveLineRatio"]))
        if "Kryon Css Test" not in values["saveFamily"]:
            raise AssertionError("font family: " + repr(values["saveFamily"]))
        typed = values["typed"]
        if (typed["minHeight"] != 64 or abs(typed["lineHeight"] - 1.4) > .001 or
                typed["typeface"] != "Kryon Css Test" or typed["elevation"] != 3 or
                typed["duration"] != 123 or typed["multiplier"] != 1.25):
            raise AssertionError("native typed values: " + repr(typed))
        if browser.diagnostics:
            raise AssertionError("browser exceptions: " + repr(browser.diagnostics))
    finally:
        browser.close()


def main():
    tool = css_tool()
    BROWSER_CSS.parent.mkdir(parents=True, exist_ok=True)
    run([tool, "export", ROOT / "tests/css_dom.css", BROWSER_CSS], ROOT)
    verify(build_project())
    print("CSS production DOM: style IDs, classes, states, geometry, fonts, motion and identity PASS")


if __name__ == "__main__":
    main()
