#!/bin/sh
# Real browser inspector bridge test for Kryon Web Document rendering.
set -eu

root=${1:-.}
if ! command -v chromium >/dev/null 2>&1; then
    echo "web dom inspector browser test: chromium not found - skipping"
    exit 0
fi
if ! command -v node >/dev/null 2>&1; then
    echo "web dom inspector browser test: node not found - skipping"
    exit 0
fi

work=${TMPDIR:-/tmp}/kryon-web-dom-inspector-browser-test.$$
cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM
mkdir -p "$work/profile"

runtime_url=$(node -e 'const {pathToFileURL}=require("node:url"); console.log(pathToFileURL(process.argv[1]).href)' "$root/web/kryon-runtime.js")
html_url=$(node -e 'const {pathToFileURL}=require("node:url"); console.log(pathToFileURL(process.argv[1]).href)' "$work/index.html")

cat > "$work/index.html" <<EOF
<!doctype html>
<meta charset="utf-8">
<title>Kryon Web DOM Inspector Browser Test</title>
<div id="target"></div>
<script type="module">
import * as kryon from "$runtime_url";

function assert(condition, message) {
  if (!condition)
    throw new Error(message);
}

try {
  const rt = kryon.createRuntime();
  kryon.setWebStyleSheets(rt, kryon.parseWebStyleSheet(\`
    @pack inspector;
    Screen { display: grid; gap: 6; }
    Button.primary { background: #102030; color: #f8f8f8; }
    Button#save-action { background: #203040; }
    Progress { inline-size: 240; }
  \`));
  kryon.beginFrame(rt);
  kryon.widget(rt, "Screen", {}, null, {
    nodeName: "root",
    path: "Inspect/root",
    sourcePath: "inspect.kry",
    sourceLine: 1,
    sourceColumn: 1,
    sourceEndLine: 12,
    sourceEndColumn: 2
  });
  kryon.widget(rt, "Button", {
    label: "Save",
    class: "primary",
    dom_id: "save-action",
    web_ref: "save-ref",
    on_click: "save"
  }, null, {
    nodeName: "save",
    path: "Inspect/root/save",
    parentPath: "Inspect/root",
    sourcePath: "inspect.kry",
    sourceLine: 4,
    sourceColumn: 3,
    sourceEndLine: 6,
    sourceEndColumn: 4
  });
  kryon.widget(rt, "Progress", { min: 0, max: 10, value: 7, label: "Done" }, null, {
    nodeName: "done",
    path: "Inspect/root/done",
    parentPath: "Inspect/root",
    sourcePath: "inspect.kry",
    sourceLine: 8,
    sourceColumn: 3,
    sourceEndLine: 8,
    sourceEndColumn: 45
  });
  kryon.endFrame(rt);

  const target = document.getElementById("target");
  kryon.renderWebDocument(rt, target);
  const root = kryon.webDOMRoot(target);
  const button = kryon.findWebElement(target, "save-ref");
  assert(root && button, "mounted objects missing");

  assert(kryon.webDOMSourceMap(target).some((object) =>
    object.ref === "save-ref" && object.identity.sourceColumnRef === "inspect.kry:4:3"),
    "source map did not expose column ref");
  assert(root.kryObjectMap.get("save-ref")?.element === button,
    "root object map did not expose ref");
  assert(root.kryObjectMap.get("inspect.kry:4:3")?.element === button,
    "root object map did not expose source ref");

  const buttonSnapshot = kryon.webDOMSnapshot(target, "save-ref");
  assert(buttonSnapshot.identity.sourceRangeRef === "inspect.kry:4:3-6:4",
    "snapshot source range missing");
  assert(buttonSnapshot.eventRefs.click === "save", "snapshot event ref missing");
  assert(kryon.webDOMSnapshots(target, "Button.primary").length === 1,
    "selector snapshots did not filter button");

  const trace = kryon.webDOMStyleTrace(target, "save-ref");
  assert(trace.resolved.background === "#203040", "style trace winner value missing");
  assert(trace.matchedRules.length === 2, "style trace matched rule count missing");
  assert(trace.winners.background.selector.includes("#save-action"),
    "style trace winner selector missing");

  button.textContent = "Saved";
  kryon.webDOMSync(target);
  assert(kryon.webDOMAccessibilitySnapshot(target, "Button.primary").nodes[0].text === "Saved",
    "mounted accessibility snapshot did not read DOM text");
  assert(root.kryAccessibilitySnapshot("Progress").nodes[0].valueNow === "7",
    "mounted accessibility range fact missing");
  assert(kryon.webDOMSnapshotFromElement(button).ref === "save-ref",
    "snapshot from element did not round trip");
  assert(kryon.webDOMSnapshotFromEvent({ target: button }).eventRefs.click === "save",
    "snapshot from event did not expose event refs");

  document.body.dataset.result = "ok";
} catch (error) {
  document.body.dataset.result = "fail";
  document.body.dataset.error = error && error.stack ? error.stack : String(error);
}
</script>
EOF

out="$work/dom.txt"
chromium --headless=new --disable-gpu --no-sandbox --allow-file-access-from-files \
    --virtual-time-budget=3000 --user-data-dir="$work/profile" \
    --dump-dom "$html_url" > "$out"
if ! grep -q 'data-result="ok"' "$out"; then
    echo "web dom inspector browser test failed:" >&2
    grep 'data-error=' "$out" >&2 || cat "$out" >&2
    exit 1
fi
echo "web dom inspector browser test ok"
