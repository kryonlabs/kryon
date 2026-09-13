#!/bin/sh
# Real browser smoke test for Kryon Web Document rendering.
set -eu

root=${1:-.}
if ! command -v chromium >/dev/null 2>&1; then
    echo "web dom browser test: chromium not found - skipping"
    exit 0
fi
if ! command -v node >/dev/null 2>&1; then
    echo "web dom browser test: node not found - skipping"
    exit 0
fi

work=${TMPDIR:-/tmp}/kryon-web-dom-browser-test.$$
cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM
mkdir -p "$work/profile"

runtime_url=$(node -e 'const {pathToFileURL}=require("node:url"); console.log(pathToFileURL(process.argv[1]).href)' "$root/web/kryon-runtime.js")
html_url=$(node -e 'const {pathToFileURL}=require("node:url"); console.log(pathToFileURL(process.argv[1]).href)' "$work/index.html")

cat > "$work/index.html" <<EOF
<!doctype html>
<meta charset="utf-8">
<title>Kryon Web DOM Browser Test</title>
<div id="target"></div>
<script type="module">
import * as kryon from "$runtime_url";

function assert(condition, message) {
  if (!condition)
    throw new Error(message);
}

try {
  const rt = kryon.createRuntime();
  kryon.beginFrame(rt);
  kryon.widget(rt, "Section", {
    dom_tag: "article",
    web_ref: "article-ref",
    dom_id: "article-id",
    data_tracking_id: "browser-1",
    attr_itemprop: "mainEntity",
    role: "region",
    aria_label: "Browser article",
    on_click: "article_click"
  }, null, {
    nodeName: "article",
    path: "Page/article",
    sourcePath: "browser.kry",
    sourceLine: 3,
    sourceColumn: 5,
    sourceEndLine: 11,
    sourceEndColumn: 6
  });
  kryon.widget(rt, "Button", {
    label: "Save",
    dom_id: "save",
    class: "primary"
  }, null, {
    nodeName: "save",
    path: "Page/article/save",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "TableView", {}, null, {
    nodeName: "prices",
    path: "Page/prices"
  });
  kryon.widget(rt, "TableCell", {
    text: "Price",
    scope: "col",
    web_ref: "priceHeader"
  }, null, {
    nodeName: "priceHeader",
    path: "Page/prices/priceHeader",
    parentPath: "Page/prices"
  });
  kryon.widget(rt, "TableCell", {
    text: "Item",
    scope: "row",
    web_ref: "itemHeader"
  }, null, {
    nodeName: "itemHeader",
    path: "Page/prices/itemHeader",
    parentPath: "Page/prices"
  });
  kryon.widget(rt, "TableCell", {
    text: "Quarter",
    scope: "colgroup",
    web_ref: "quarterHeader"
  }, null, {
    nodeName: "quarterHeader",
    path: "Page/prices/quarterHeader",
    parentPath: "Page/prices"
  });
  kryon.widget(rt, "TableCell", {
    text: "Region",
    scope: "rowgroup",
    web_ref: "regionHeader"
  }, null, {
    nodeName: "regionHeader",
    path: "Page/prices/regionHeader",
    parentPath: "Page/prices"
  });
  kryon.widget(rt, "TableCell", {
    text: "\$12",
    headers: "priceHeader itemHeader quarterHeader regionHeader"
  }, null, {
    nodeName: "priceCell",
    path: "Page/prices/priceCell",
    parentPath: "Page/prices"
  });
  kryon.widget(rt, "Column", {
    dom_tag: "form",
    dom_id: "contact-form",
    web_ref: "contact"
  }, null, {
    nodeName: "contact",
    path: "Page/contact"
  });
  kryon.widget(rt, "TextField", {
    text: "hello@example.test",
    dom_name: "email"
  }, null, {
    nodeName: "email",
    path: "Page/contact/email",
    parentPath: "Page/contact"
  });
  kryon.widget(rt, "TextField", {
    text: "outside@example.test",
    dom_name: "external_email",
    form: "contact"
  }, null, {
    nodeName: "externalEmail",
    path: "Page/externalEmail"
  });
  kryon.endFrame(rt);
  kryon.setWebStyleSheets(rt, kryon.parseWebStyleSheet(\`
    Section[webRef="article-ref"] {
      display: grid;
      grid-template-areas: "main";
      border-collapse: collapse;
    }
    Button.primary { color-scheme: light dark; }
  \`));

  const target = document.getElementById("target");
  kryon.renderWebDocument(rt, target);
  const article = kryon.findWebElement(target, "article-ref");
  assert(article instanceof HTMLElement, "article element missing");
  assert(article.tagName === "ARTICLE", "authored native tag not rendered");
  assert(article.id === "article-id", "dom_id not reflected");
  assert(article.dataset.kryWebRef === "article-ref", "web ref dataset missing");
  assert(article.getAttribute("data-kry-web-ref") === "article-ref", "web ref attribute missing");
  assert(article.getAttribute("data-tracking-id") === "browser-1", "data attr missing");
  assert(article.getAttribute("itemprop") === "mainEntity", "extra attr missing");
  assert(article.getAttribute("role") === "region", "role attr missing");
  assert(article.getAttribute("aria-label") === "Browser article", "aria attr missing");
  assert(article.getAttribute("data-kry-source-range-ref") === "browser.kry:3:5-11:6",
    "source range attr missing");
  assert(article.dataset.krySourceRef === "browser.kry:3", "source ref dataset missing");
  assert(article.dataset.krySourceColumnRef === "browser.kry:3:5",
    "source column ref dataset missing");
  assert(article.dataset.krySourceRangeRef === "browser.kry:3:5-11:6",
    "source range dataset missing");
  assert(article.style.display === "grid", "KSS display not applied");
  assert(article.style.gridTemplateAreas === '"main"', "KSS grid area not applied");
  assert(article.style.borderCollapse === "collapse", "KSS table style not applied");
  assert(kryon.webDOMObject(target, "article-ref").element === article, "DOM object lookup failed");
  const root = kryon.webDOMRoot(target);
  assert(root.kryObjectMap.get("browser.kry:3")?.element === article,
    "root source object map lookup failed");
  assert(root.kryObjectMap.get("browser.kry:3:5")?.element === article,
    "root source column object map lookup failed");
  assert(root.kryObjectMap.get("browser.kry:3:5-11:6")?.element === article,
    "root source range object map lookup failed");
  assert(kryon.webDOMObjectAtSource(target, "browser.kry", 3)?.element === article,
    "DOM object source lookup failed");
  assert(kryon.webDOMObjectAtSource(target, "browser.kry", 3, 5)?.element === article,
    "DOM object source column lookup failed");
  assert(kryon.webDOMObjectAtSourceRange(target, "browser.kry", 3, 5)?.element === article,
    "DOM object source range lookup failed");
  assert(root.kryAtSource("browser.kry", 3, 5)?.element === article,
    "root source lookup failed");
  assert(root.kryAtSourceRange("browser.kry", 3, 5)?.element === article,
    "root source range lookup failed");
  assert(article.kryObject.element === article, "element Kry object getter failed");
  assert(article.kryMatches("Section[webRef='article-ref']"), "KSS selector match failed");
  const removeInstalledStyle = kryon.installWebStyleSheet(kryon.parseWebStyleSheet(\`
    Button.primary {
      background-color: rgb(12, 34, 56);
    }
    @media all {
      Button.primary { border-top-width: 3px; }
    }
  \`), null, "browser-install");
  assert(typeof removeInstalledStyle === "function", "installed CSS cleanup missing");
  const installedStyle = document.querySelector('style[data-kry-style="browser-install"]');
  assert(installedStyle?.textContent.includes('@media all'), "conditional CSS group not installed");
  const eventLog = [];
  article.addEventListener("click", (event) => {
    eventLog.push(event.kryObject?.ref || "");
  });
  assert(kryon.webDOMDispatchEvent(target, "article-ref", "click"), "dispatch failed");
  assert(eventLog[0] === "article-ref", "decorated event did not expose Kry object");
  assert(kryon.webDOMSetAttribute(target, "article-ref", "aria-controls", "save"),
    "aria-controls mutation failed");
  assert(kryon.webDOMSync(target, "article-ref")?.ref === "article-ref",
    "article sync after aria-controls failed");
  assert(kryon.webDOMRelations(target, "save").controlledBy
    .map((object) => object.ref).join(" ") === "article-ref",
    "reverse controlledBy relation missing");
  assert(kryon.webDOMRemoveAttribute(target, "article-ref", "aria-controls"),
    "aria-controls removal failed");
  assert(kryon.webDOMSync(target, "article-ref")?.ref === "article-ref",
    "article sync after aria-controls removal failed");
  assert(kryon.webDOMRelations(target, "save").controlledBy.length === 0,
    "removed aria-controls still produced controlledBy relation");
  const button = kryon.webDOMQuery(target, "Section > Button.primary");
  assert(button?.element?.id === "save", "child selector query failed");
  const nestedButtonSpan = document.createElement("span");
  nestedButtonSpan.textContent = "nested";
  button.element.appendChild(nestedButtonSpan);
  assert(kryon.webDOMObjectFromElement(nestedButtonSpan)?.ref === "Page/article/save",
    "nested native element did not resolve to Kry object");
  assert(kryon.webDOMSnapshotFromElement(nestedButtonSpan)?.identity?.ref === "Page/article/save",
    "nested native element snapshot did not include Kry identity");
  const delegatedLog = [];
  const removeDelegated = kryon.webDOMAddDelegatedEventListener(target, "Button.primary",
    "kry-browser-delegated", (event, object) => {
      delegatedLog.push([
        event.type,
        event.target.tagName,
        object.ref,
        event.kryObject?.ref || "",
        event.krySnapshot?.identity?.ref || ""
      ].join(":"));
    });
  assert(typeof removeDelegated === "function", "delegated listener cleanup missing");
  nestedButtonSpan.dispatchEvent(new Event("kry-browser-delegated", { bubbles: true }));
  assert(delegatedLog[0] === "kry-browser-delegated:SPAN:Page/article/save:Page/article/save:Page/article/save",
    "delegated listener did not resolve nested target");
  assert(kryon.webDOMSnapshotFromEvent({ target: nestedButtonSpan })?.ref === "Page/article/save",
    "event snapshot did not resolve nested target");
  removeDelegated();
  nestedButtonSpan.dispatchEvent(new Event("kry-browser-delegated", { bubbles: true }));
  assert(delegatedLog.length === 1, "delegated listener cleanup failed");
  const bindLog = [];
  const unbindPrimary = kryon.webDOMBind(target, "Button.primary", {
    mount(object, detail) {
      bindLog.push("mount:" + object.ref + ":" + (detail.event?.type || "immediate"));
      object.element.addEventListener("click", () => bindLog.push("click:" + object.ref));
      return () => bindLog.push("cleanup:" + object.ref);
    },
    update(object, detail, previous) {
      bindLog.push("update:" + object.ref + ":" + (previous?.ref || "") + ":" + (detail.event?.type || ""));
    },
    unmount(object, detail) {
      bindLog.push("unmount:" + object.ref + ":" + (detail.event?.type || ""));
    }
  });
  assert(typeof unbindPrimary === "function", "webDOMBind cleanup missing");
  assert(bindLog[0] === "mount:Page/article/save:immediate", "webDOMBind mount missing");
  assert(kryon.webDOMSetAttribute(target, "Page/article/save", "data-runtime", "1"),
    "webDOMSetAttribute failed");
  assert(button.element.getAttribute("data-runtime") === "1", "runtime attr not set");
  assert(kryon.webDOMQuery(target, "[data-runtime='1']")?.ref === "Page/article/save",
    "runtime attr not queryable");
  assert(kryon.webDOMSetStyle(target, "Page/article/save", "--runtime-accent", "hotpink"),
    "webDOMSetStyle failed");
  assert(kryon.webDOMGetStyle(target, "Page/article/save", "--runtime-accent") === "hotpink",
    "runtime custom style not readable");
  assert(kryon.webDOMSetProperty(target, "Page/article/save", "disabled", true),
    "webDOMSetProperty failed");
  assert(button.element.disabled === true, "runtime property not set");
  assert((button.element.dataset.kryState || "").includes("disabled"),
    "property mutation did not sync state: " + (button.element.dataset.kryState || ""));
  assert(kryon.webDOMQuery(target, "Button:disabled")?.ref === "Page/article/save",
    "state selector did not see property mutation");
  assert(button.element.kryAddClass("browser-bound"), "element class helper failed");
  assert(kryon.webDOMQuery(target, "Button.browser-bound")?.element === button.element,
    "class helper mutation not queryable");
  assert(kryon.webDOMDispatchEvent(target, "Page/article/save", "click"), "bound button dispatch failed");
  assert(bindLog.includes("click:Page/article/save"), "webDOMBind listener did not receive click");
  assert(kryon.webDOMSetProperty(target, "Page/article/save", "disabled", false),
    "webDOMSetProperty disable reset failed");
  assert(!(button.element.dataset.kryState || "").includes("disabled"),
    "property mutation did not clear state: " + (button.element.dataset.kryState || ""));
  assert(getComputedStyle(button.element).backgroundColor === "rgb(12, 34, 56)",
    "installed CSS did not style Kryon element");
  const priceHeader = kryon.findWebElement(target, "priceHeader");
  const itemHeader = kryon.findWebElement(target, "itemHeader");
  const quarterHeader = kryon.findWebElement(target, "quarterHeader");
  const regionHeader = kryon.findWebElement(target, "regionHeader");
  const priceCell = kryon.findWebElement(target, "priceCell");
  assert(priceHeader.id && itemHeader.id && quarterHeader.id && regionHeader.id,
    "table headers did not receive native ids");
  const expectedHeaders = \`\${priceHeader.id} \${itemHeader.id} \${quarterHeader.id} \${regionHeader.id}\`;
  assert(priceCell.getAttribute("headers") === expectedHeaders,
    \`table headers were not resolved to native ids: \${priceCell.getAttribute("headers")} expected \${expectedHeaders}\`);
  const tableRelations = kryon.webDOMRelations(target, "priceCell");
  assert(tableRelations.headers.map((object) => object.ref).join(" ") === "priceHeader itemHeader quarterHeader regionHeader",
    "table relation refs missing");
  assert(tableRelations.columnHeaders.map((object) => object.ref).join(" ") === "priceHeader quarterHeader",
    "column header relation missing");
  assert(tableRelations.rowHeaders.map((object) => object.ref).join(" ") === "itemHeader regionHeader",
    "row header relation missing");
  assert(tableRelations.columnGroupHeaders[0]?.ref === "quarterHeader",
    "column group header relation missing");
  assert(tableRelations.rowGroupHeaders[0]?.ref === "regionHeader",
    "row group header relation missing");
  const contact = kryon.findWebElement(target, "contact");
  const email = kryon.findWebElement(target, "email");
  const externalEmail = kryon.findWebElement(target, "externalEmail");
  assert(contact.tagName === "FORM", "form native tag not rendered");
  assert(email.getAttribute("name") === "email", "nested input name missing");
  assert(externalEmail.getAttribute("name") === "external_email", "external input name missing");
  assert(externalEmail.getAttribute("form") === "contact-form", "form owner did not resolve to native id");
  const formRelations = kryon.webDOMRelations(target, "externalEmail");
  assert(formRelations.formOwner?.ref === "contact", "form owner relation missing");
  const contactValues = kryon.webFormValues(target, "contact");
  assert(contactValues.email === "hello@example.test", "nested form value missing");
  assert(contactValues.external_email === "outside@example.test", "owned form value missing");
  unbindPrimary();
  assert(bindLog.includes("cleanup:Page/article/save"), "webDOMBind cleanup callback missing");
  removeInstalledStyle();
  assert(!document.querySelector('style[data-kry-style="browser-install"]'),
    "installed CSS cleanup failed");

  const lifecycleTarget = document.createElement("div");
  document.body.appendChild(lifecycleTarget);
  const lifecycleEvents = [];
  const renderCounts = [];
  for (const type of ["kry-mount", "kry-update", "kry-unmount"]) {
    lifecycleTarget.addEventListener(type, (event) => {
      lifecycleEvents.push([type, event.kryObject?.ref || "", event.detail?.object?.element === event.target]);
    });
  }
  lifecycleTarget.addEventListener("kry-render", (event) => {
    renderCounts.push(event.detail?.objects?.length || 0);
  });
  const lifecycleRt = kryon.createRuntime();
  kryon.beginFrame(lifecycleRt);
  kryon.widget(lifecycleRt, "Screen", {}, null, { nodeName: "root", path: "Life/root" });
  kryon.widget(lifecycleRt, "Button", { label: "One", class: "primary" }, null, {
    nodeName: "action",
    path: "Life/root/action",
    parentPath: "Life/root",
    ref: "life-action"
  });
  kryon.endFrame(lifecycleRt);
  kryon.renderWebDocument(lifecycleRt, lifecycleTarget);
  const lifecycleButton = kryon.findWebElement(lifecycleTarget, "life-action");
  const directUnmountEvents = [];
  lifecycleButton.addEventListener("kry-unmount", (event) => {
    directUnmountEvents.push([event.kryObject?.ref || "", event.detail?.object?.element === lifecycleButton]);
  });
  const observedRefs = [];
  const removeLifecycleObserver = kryon.webDOMObserve(lifecycleTarget, "Button.primary",
    (objects, detail) => observedRefs.push([
      objects.map((object) => object.ref).join(" "),
      detail.event?.type || "immediate"
    ]));
  kryon.beginFrame(lifecycleRt);
  kryon.widget(lifecycleRt, "Screen", {}, null, { nodeName: "root", path: "Life/root" });
  kryon.widget(lifecycleRt, "Button", { label: "Two", class: "primary" }, null, {
    nodeName: "action",
    path: "Life/root/action",
    parentPath: "Life/root",
    ref: "life-action"
  });
  kryon.endFrame(lifecycleRt);
  kryon.renderWebDocument(lifecycleRt, lifecycleTarget);
  kryon.beginFrame(lifecycleRt);
  kryon.widget(lifecycleRt, "Screen", {}, null, { nodeName: "root", path: "Life/root" });
  kryon.endFrame(lifecycleRt);
  kryon.renderWebDocument(lifecycleRt, lifecycleTarget);
  removeLifecycleObserver();
  assert(renderCounts.join(" ") === "2 2 1", "render lifecycle counts missing");
  assert(lifecycleEvents.some((entry) => entry[0] === "kry-mount" && entry[1] === "life-action" && entry[2]),
    "mount lifecycle event missing");
  assert(lifecycleEvents.some((entry) => entry[0] === "kry-update" && entry[1] === "life-action" && entry[2]),
    "update lifecycle event missing");
  assert(directUnmountEvents.some((entry) => entry[0] === "life-action" && entry[1]),
    "direct unmount lifecycle event missing: " + JSON.stringify(directUnmountEvents));
  assert(observedRefs.some((entry) => entry[0] === "life-action" && entry[1] === "immediate"),
    "immediate observer result missing");
  assert(observedRefs.some((entry) => entry[0] === "life-action" && entry[1] === "kry-render"),
    "render observer result missing");
  assert(observedRefs.some((entry) => entry[0] === "" && entry[1] === "kry-render"),
    "empty observer result after unmount missing");
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
    echo "web dom browser test failed:" >&2
    grep 'data-error=' "$out" >&2 || cat "$out" >&2
    exit 1
fi
echo "web dom browser test ok"
