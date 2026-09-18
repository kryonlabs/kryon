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
compiled_module_url=""
k2js_bin=${K2JS:-}
if [ -z "$k2js_bin" ]; then
    platform=$(uname -s | tr '[:upper:]' '[:lower:]')
    arch=$(uname -m)
    if [ -x "$root/build/$platform-$arch/bin/k2js" ]; then
        k2js_bin="$root/build/$platform-$arch/bin/k2js"
    else
        k2js_bin=$(ls "$root"/build/"$platform"-*/bin/k2js \
            "$root"/build/*/bin/k2js 2>/dev/null | head -1 || true)
    fi
fi
if [ -n "$k2js_bin" ] && [ -x "$k2js_bin" ]; then
    mkdir -p "$work/compiled/src"
    cat > "$work/compiled/src/native_blocks.kry" <<'EOF'
#import "kryon.h"
NativeBlocks :: () #ui {
    Article story: {
        class = "feature"
        Text((TextProps){.text="Story"})
    }
    Figure chart: {
        Figcaption caption: {
            Text((TextProps){.text="Chart"})
        }
    }
    Fieldset profile: {
        Legend heading: {
            Text((TextProps){.text="Profile"})
        }
    }
    Video hero: {
        Source webm: {
            src = "intro.webm"
            type = "video/webm"
        }
        Track captions: {
            src = "captions.vtt"
            kind = "captions"
            srclang = "en"
            label = "English"
        }
    }
    Embed chartEmbed: {
        src = "chart.svg"
        type = "image/svg+xml"
    }
    Table cols: {
        TableColumnGroup metrics: {
            TableColumn quarter: {
                span = 1
            }
        }
    }
}
EOF
    "$k2js_bin" --no-main --root "$work/compiled" -o "$work/compiled/out" \
        "$work/compiled/src/native_blocks.kry"
    sed -i "s#\\.\\./kryon-runtime\\.js#$runtime_url#" \
        "$work/compiled/out/src/native_blocks.js"
    compiled_module_url=$(node -e 'const {pathToFileURL}=require("node:url"); console.log(pathToFileURL(process.argv[1]).href)' "$work/compiled/out/src/native_blocks.js")
else
    cat > "$work/compiled-noop.js" <<'EOF'
export const nativeBlocksUnavailable = true;
EOF
    compiled_module_url=$(node -e 'const {pathToFileURL}=require("node:url"); console.log(pathToFileURL(process.argv[1]).href)' "$work/compiled-noop.js")
fi

cat > "$work/index.html" <<EOF
<!doctype html>
<meta charset="utf-8">
<title>Kryon Web DOM Browser Test</title>
<div id="target"></div>
<script type="module">
import * as kryon from "$runtime_url";
import * as compiledNativeBlocks from "$compiled_module_url";

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
    aria_details: "Page/article/save",
    aria_errormessage: "Page/article/save",
    aria_flowto: "Page/article/save",
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
  kryon.widget(rt, "Icon", {}, null, {
    nodeName: "icon",
    path: "Page/article/icon",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Bullet", {}, null, {
    nodeName: "bullet",
    path: "Page/article/bullet",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Line", {}, null, {
    nodeName: "line",
    path: "Page/article/line",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Image", { asset_path: "hero.png", alt_text: "Hero image" }, null, {
    nodeName: "hero",
    path: "Page/article/hero",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Progress", { min: 0, max: 100, value: 64, label: "Upload" }, null, {
    nodeName: "upload",
    path: "Page/article/upload",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Toast", { text: "Saved" }, null, {
    nodeName: "status",
    path: "Page/article/status",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Input", {
    min: -5,
    max: 5,
    step: 0.25,
    value: 1.5,
    label: "Amount"
  }, null, {
    nodeName: "amount",
    path: "Page/article/amount",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Text", { text: "Amount", dom_for: "amount" }, null, {
    nodeName: "amountLabel",
    path: "Page/article/amountLabel",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Fieldset", { title: "Options" }, null, {
    nodeName: "options",
    path: "Page/article/options",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Checkbox", { checked: true, label: "Email opt in" }, null, {
    nodeName: "emailOptIn",
    path: "Page/article/options/email",
    parentPath: "Page/article/options"
  });
  kryon.widget(rt, "Fieldset", { title: "Locked", disabled: true }, null, {
    nodeName: "lockedOptions",
    path: "Page/article/lockedOptions",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "TextField", { label: "Locked field" }, null, {
    nodeName: "lockedField",
    path: "Page/article/lockedOptions/field",
    parentPath: "Page/article/lockedOptions"
  });
  kryon.widget(rt, "Column", { dom_tag: "label", web_ref: "newsletterLabel" }, null, {
    nodeName: "newsletterLabel",
    path: "Page/article/newsletterLabel",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Checkbox", { checked: false }, null, {
    nodeName: "newsletterOptIn",
    path: "Page/article/newsletterLabel/optIn",
    parentPath: "Page/article/newsletterLabel"
  });
  kryon.widget(rt, "ListBox", {}, null, {
    nodeName: "choices",
    path: "Page/article/choices",
    parentPath: "Page/article",
    ariaActiveDescendant: "Page/article/choices/beta"
  });
  kryon.widget(rt, "TextField", {
    aria_activedescendant: "Page/article/choices/beta",
    placeholder: "Find choice"
  }, null, {
    nodeName: "choiceSearch",
    path: "Page/article/choiceSearch",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Selectable", { text: "Beta", value: "b", selected: true }, null, {
    nodeName: "choiceBeta",
    path: "Page/article/choices/beta",
    parentPath: "Page/article/choices"
  });
  kryon.widget(rt, "Menu", {}, null, {
    nodeName: "contextMenu",
    path: "Page/article/contextMenu",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Button", { label: "Archive" }, null, {
    nodeName: "archiveItem",
    path: "Page/article/contextMenu/archive",
    parentPath: "Page/article/contextMenu"
  });
  kryon.widget(rt, "TabBar", {}, null, {
    nodeName: "tabs",
    path: "Page/article/tabs",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Button", { label: "Overview" }, null, {
    nodeName: "overviewTab",
    path: "Page/article/tabs/overview",
    parentPath: "Page/article/tabs"
  });
  kryon.widget(rt, "TreeView", {}, null, {
    nodeName: "outline",
    path: "Page/article/outline",
    parentPath: "Page/article"
  });
  kryon.widget(rt, "Selectable", { label: "Intro" }, null, {
    nodeName: "introNode",
    path: "Page/article/outline/intro",
    parentPath: "Page/article/outline"
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
      content-visibility: auto;
      contain-intrinsic-size: 320;
      contain-intrinsic-inline-size: 320;
      contain-intrinsic-block-size: 180;
      overflow-clip-margin: 12;
      container-type: inline-size;
      container-name: article;
      isolation: isolate;
      mix-blend-mode: multiply;
      column-count: 2;
      column-width: 180;
      column-rule: 1px solid #ccc;
      will-change: transform;
      view-transition-name: article-view;
    }
    Button.primary { color-scheme: light dark; }
    Section:has(> Button.primary) { outline-width: 2; }
    TextField:placeholder-shown { opacity: 0.72; }
    Checkbox:indeterminate { outline-offset: 7; }
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
  assert(article.getAttribute("aria-details") === "save", "aria-details relation not resolved");
  assert(article.getAttribute("aria-errormessage") === "save",
    "aria-errormessage relation not resolved");
  assert(article.getAttribute("aria-flowto") === "save", "aria-flowto relation not resolved");
  const articleSourceRef = kryon.webSourceRef("browser.kry", 3);
  const articleSourceColumnRef = kryon.webSourceRef("browser.kry", 3, 5);
  const articleSourceRangeRef = kryon.webSourceRangeRef("browser.kry", 3, 5, 11, 6);
  assert(articleSourceRangeRef === "browser.kry:3:5-11:6",
    "source range helper mismatch");
  assert(article.getAttribute("data-kry-source-range-ref") === articleSourceRangeRef,
    "source range attr missing");
  assert(article.dataset.krySourceRef === articleSourceRef, "source ref dataset missing");
  assert(article.dataset.krySourceColumnRef === articleSourceColumnRef,
    "source column ref dataset missing");
  assert(article.dataset.krySourceRangeRef === articleSourceRangeRef,
    "source range dataset missing");
  assert(article.style.display === "grid", "KSS display not applied");
  assert(article.style.gridTemplateAreas === '"main"', "KSS grid area not applied");
  assert(article.style.borderCollapse === "collapse", "KSS table style not applied");
  assert(article.style.contentVisibility === "auto", "KSS content visibility not applied");
  assert(article.style.containIntrinsicSize === "320px", "KSS contain intrinsic size not applied");
  assert(article.style.containIntrinsicInlineSize === "320px",
    "KSS contain intrinsic inline size not applied");
  assert(article.style.containIntrinsicBlockSize === "180px",
    "KSS contain intrinsic block size not applied");
  assert(article.style.overflowClipMargin === "12px", "KSS overflow clip margin not applied");
  assert(article.style.containerType === "inline-size", "KSS container type not applied");
  assert(article.style.containerName === "article", "KSS container name not applied");
  assert(article.style.isolation === "isolate", "KSS isolation not applied");
  assert(article.style.mixBlendMode === "multiply", "KSS mix blend mode not applied");
  assert(article.style.columnCount === "2", "KSS column count not applied");
  assert(article.style.columnWidth === "180px", "KSS column width not applied");
  assert(article.style.columnRule === "1px solid rgb(204, 204, 204)" ||
    article.style.columnRule === "1px solid #ccc", "KSS column rule not applied");
  assert(article.style.willChange === "transform", "KSS will-change not applied");
  assert(article.style.viewTransitionName === "article-view",
    "KSS view transition name not applied");
  assert(article.style.outlineWidth === "2px", "KSS :has style not applied");
  assert(kryon.webDOMQuery(target, "Section:has(> Button.primary)")?.ref === "article-ref",
    "browser DOM :has query failed");
  assert(kryon.webDOMQuery(target, "TextField:placeholder-shown")?.ref ===
    "Page/article/choiceSearch", "browser DOM placeholder-shown query failed");
  assert(kryon.webDOMSetState(target, "emailOptIn", "indeterminate", true),
    "browser DOM indeterminate state set failed");
  assert(kryon.webDOMQuery(target, "Checkbox:indeterminate")?.ref ===
    "Page/article/options/email", "browser DOM indeterminate query failed");
  assert(kryon.webDOMQuery(target, "Button:target") === null,
    "browser DOM target selector matched without hash");
  globalThis.location.hash = "save";
  assert(kryon.webDOMQuery(target, "Button:target")?.ref === "Page/article/save",
    "browser DOM target query failed");
  globalThis.location.hash = "";
  assert(kryon.webDOMObject(target, "article-ref").element === article, "DOM object lookup failed");
  const root = kryon.webDOMRoot(target);
  assert(root.kryIdentity("article-ref").domId === "article-id", "root identity lookup failed");
  assert(root.krySnapshot("article-ref").identity.ref === "article-ref",
    "root snapshot lookup failed");
  assert(root.krySnapshots("Section").map((snapshot) => snapshot.ref).includes("article-ref"),
    "root snapshots lookup failed");
  assert(article.kryStyleFacts.kind === "Section", "element style facts missing");
  assert(root.kryStyleFacts("article-ref").kind === "Section", "root style facts missing");
  assert(kryon.webDOMStyleFacts(target, "article-ref").kind === "Section",
    "mounted style facts missing");
  const articleTrace = kryon.webDOMStyleTrace(target, "article-ref");
  assert(articleTrace.resolved.display === "grid", "style trace resolved display missing");
  assert(articleTrace.winners.display.selector.includes("Section"),
    "style trace winner selector missing");
  assert(root.kryStyleTrace("article-ref").resolved.display === "grid",
    "root style trace missing");
  assert(article.kryStyleTrace.resolved.display === "grid", "element style trace missing");
  assert(kryon.webDOMObject(target, "article-ref").styleTrace.resolved.display === "grid",
    "object style trace missing");
  const uploadSnapshot = kryon.webDOMSnapshot(target, "Page/prices/priceCell");
  assert(uploadSnapshot.valueNow === "", "non-range snapshot should not expose valueNow");
  const mountedA11y = kryon.webDOMAccessibilitySnapshot(target);
  assert(mountedA11y.nodes.some((node) => node.path === "Page/article/upload" &&
    node.role === "progressbar" && node.valueNow === "64"),
    "mounted accessibility range node missing");
  assert(mountedA11y.nodes.some((node) => node.path === "Page/article/status" &&
    node.role === "status"),
    "mounted accessibility status node missing");
  const amount = kryon.findWebElement(target, "amount");
  const amountLabel = kryon.findWebElement(target, "amountLabel");
  assert(amount.tagName === "INPUT", "native Input tag missing");
  assert(amount.getAttribute("type") === "number", "native Input type missing");
  assert(amount.getAttribute("min") === "-5", "native Input min missing");
  assert(amount.getAttribute("max") === "5", "native Input max missing");
  assert(amount.getAttribute("step") === "0.25", "native Input step missing");
  assert(amount.getAttribute("value") === "1.5", "native Input value missing");
  assert(amount.getAttribute("aria-label") === "Amount",
    "native Input accessible label missing");
  const amountSnapshot = kryon.webDOMSnapshot(target, "amount");
  assert(amountSnapshot.valueNow === "1.5" && amountSnapshot.min === "-5" &&
    amountSnapshot.max === "5", "native Input snapshot range missing");
  assert(amountLabel.tagName === "LABEL", "htmlFor Text did not render native label");
  assert(amountLabel.getAttribute("for") === amount.id, "native label for attr did not resolve");
  assert(kryon.webDOMRelations(target, "amountLabel").labelFor.ref === "Page/article/amount",
    "htmlFor label relation missing");
  assert(mountedA11y.nodes.some((node) => node.path === "Page/article/amount" &&
    node.role === "spinbutton" && node.valueNow === "1.5"),
    "mounted accessibility numeric Input missing");
  assert(root.kryAccessibilitySnapshot("Button[role=tab]").nodes[0]?.role === "tab",
    "root mounted accessibility selector missing");
  assert(root.kryObjectMap.get(articleSourceRef)?.element === article,
    "root source object map lookup failed");
  assert(root.kryObjectMap.get(articleSourceColumnRef)?.element === article,
    "root source column object map lookup failed");
  assert(root.kryObjectMap.get(articleSourceRangeRef)?.element === article,
    "root source range object map lookup failed");
  assert(kryon.webDOMObjectAtSource(target, "browser.kry", 3)?.element === article,
    "DOM object source lookup failed");
  assert(kryon.webDOMObjectAtSource(target, "browser.kry", 3, 5)?.element === article,
    "DOM object source column lookup failed");
  assert(kryon.webDOMObjectAtSourceRange(target, "browser.kry", 3, 5)?.element === article,
    "DOM object source range lookup failed");
  assert(kryon.webDOMObjectOverlappingSourceRange(target, "browser.kry", 3, 6, 11, 5)
    ?.element === article, "DOM object source overlap lookup failed");
  assert(kryon.webDOMObjectsOverlappingSourceRange(target, "browser.kry", 12, 1, 12, 3)
    .length === 0, "DOM object source overlap returned unrelated nodes");
  assert(root.kryAtSource("browser.kry", 3, 5)?.element === article,
    "root source lookup failed");
  assert(root.kryAtSourceRange("browser.kry", 3, 5)?.element === article,
    "root source range lookup failed");
  assert(article.kryObject.element === article, "element Kry object getter failed");
  assert(article.kryMatches("Section[webRef='article-ref']"), "KSS selector match failed");
  assert(kryon.webDOMSnapshot(target, "article-ref").eventRefs.click === "article_click",
    "snapshot event refs missing click hook");
  assert(article.kryEventRefs.click === "article_click", "element event refs missing click hook");
  assert(root.kryEventRefs("article-ref").click === "article_click",
    "root event refs missing click hook");
  assert(kryon.webDOMEventRefs(target, "article-ref").click === "article_click",
    "mounted event refs missing click hook");
  const saveRelations = kryon.webDOMRelations(target, "save");
  assert(kryon.webDOMRelations(target, "article-ref").details.ref === "Page/article/save",
    "article details relation missing");
  assert(kryon.webDOMRelations(target, "article-ref").errorMessage.ref === "Page/article/save",
    "article error message relation missing");
  assert(kryon.webDOMRelations(target, "article-ref").flowTo.map((object) => object.ref).join(" ") === "Page/article/save",
    "article flow relation missing");
  assert(saveRelations.detailedBy.map((object) => object.ref).join(" ") === "article-ref",
    "save detailedBy relation missing");
  assert(saveRelations.errorFor.map((object) => object.ref).join(" ") === "article-ref",
    "save errorFor relation missing");
  assert(saveRelations.flowFrom.map((object) => object.ref).join(" ") === "article-ref",
    "save flowFrom relation missing");
  assert(kryon.webDOMRelationRefs(target, "article-ref").details === "Page/article/save",
    "article details relation refs missing");
  assert(kryon.webDOMSnapshot(target, "article-ref").relationRefs.errorMessage === "Page/article/save",
    "article snapshot error relation missing");
  assert(root.kryRelationRefs("article-ref").flowTo.join(" ") === "Page/article/save",
    "root flow relation refs missing");
  assert(kryon.webDOMRelations(target, "emailOptIn").groupOwner.ref === "Page/article/options",
    "fieldset group owner relation missing");
  assert(kryon.findWebElement(target, "options").firstElementChild?.tagName === "LEGEND",
    "fieldset native legend missing");
  assert(kryon.findWebElement(target, "options").firstElementChild?.textContent === "Options",
    "fieldset native legend text missing");
  assert(kryon.findWebElement(target, "emailOptIn").getAttribute("aria-label") === "Email opt in",
    "checkbox accessible label missing");
  assert(kryon.webDOMRelationRefs(target, "options").groupMembers
    .join(" ") === "Page/article/options/email",
    "fieldset group member relation refs missing");
  assert(kryon.webDOMSnapshot(target, "emailOptIn").relationRefs.groupOwner === "Page/article/options",
    "fieldset group owner snapshot missing");
  assert(kryon.findWebElement(target, "lockedOptions").getAttribute("disabled") === "",
    "disabled fieldset attribute missing");
  assert(kryon.findWebElement(target, "lockedOptions").firstElementChild?.tagName === "LEGEND",
    "disabled fieldset native legend missing");
  assert(kryon.findWebElement(target, "lockedOptions").firstElementChild?.textContent === "Locked",
    "disabled fieldset native legend text missing");
  assert(kryon.webDOMRelations(target, "lockedField").disabledOwner.ref ===
    "Page/article/lockedOptions", "disabled owner relation missing");
  assert(kryon.webDOMRelationRefs(target, "lockedOptions").disabledMembers
    .join(" ") === "Page/article/lockedOptions/field",
    "disabled member relation refs missing");
  assert(kryon.webDOMSnapshot(target, "lockedField").relationRefs.disabledOwner ===
    "Page/article/lockedOptions", "disabled owner snapshot missing");
  assert(kryon.webDOMRelations(target, "newsletterLabel").labelFor.ref ===
    "Page/article/newsletterLabel/optIn", "implicit labelFor relation missing");
  assert(kryon.webDOMRelations(target, "newsletterOptIn").labelledBy
    .map((object) => object.ref).join(" ") === "newsletterLabel",
    "implicit labelledBy relation missing");
  assert(kryon.webDOMSnapshot(target, "newsletterOptIn").relationRefs.labelledBy
    .join(" ") === "newsletterLabel", "implicit labelledBy snapshot missing");
  assert(kryon.webDOMRelations(target, "choiceBeta").activeDescendantOf
    .map((object) => object.ref).join(" ") === "Page/article/choices Page/article/choiceSearch",
    "active descendant reverse relation missing");
  assert(kryon.webDOMSnapshot(target, "choiceBeta").relationRefs.activeDescendantOf
    .join(" ") === "Page/article/choices Page/article/choiceSearch",
    "active descendant reverse relation refs missing");
  const icon = kryon.findWebElement(target, "icon");
  const bullet = kryon.findWebElement(target, "bullet");
  const line = kryon.findWebElement(target, "line");
  const hero = kryon.findWebElement(target, "hero");
  const upload = kryon.findWebElement(target, "upload");
  const status = kryon.findWebElement(target, "status");
  const choice = kryon.findWebElement(target, "choiceBeta");
  const menuItem = kryon.findWebElement(target, "archiveItem");
  const tab = kryon.findWebElement(target, "overviewTab");
  const treeItem = kryon.findWebElement(target, "introNode");
  assert(icon.tagName === "SPAN", "icon native span not rendered");
  assert(icon.getAttribute("role") === "img", "icon image role missing");
  assert(bullet.tagName === "LI", "bullet native list item not rendered");
  assert(kryon.webDOMSnapshot(target, "bullet").role === "listitem",
    "bullet listitem snapshot role missing");
  assert(line.tagName === "HR", "line native separator not rendered");
  assert(kryon.webDOMSnapshot(target, "line").role === "separator",
    "line separator snapshot role missing");
  assert(hero.tagName === "IMG", "image native img not rendered");
  assert(hero.getAttribute("src") === "hero.png", "image src attr missing");
  assert(hero.getAttribute("alt") === "Hero image", "image alt attr missing");
  assert(kryon.webDOMSnapshot(target, "hero").src === "hero.png",
    "image snapshot src missing");
  assert(kryon.webDOMSnapshot(target, "hero").asset === "hero.png",
    "image snapshot asset missing");
  assert(kryon.webDOMSnapshot(target, "hero").alt === "Hero image",
    "image snapshot alt missing");
  assert(kryon.webDOMAccessibilitySnapshot(target).nodes
    .find((node) => node.kind === "Image")?.label === "Hero image",
    "image accessibility alt label missing");
  assert(upload.tagName === "PROGRESS", "progress native element missing");
  assert(kryon.webDOMSnapshot(target, "upload").min === "0",
    "progress snapshot min missing");
  assert(kryon.webDOMSnapshot(target, "upload").max === "100",
    "progress snapshot max missing");
  assert(kryon.webDOMSnapshot(target, "upload").valueNow === "64",
    "progress snapshot valueNow missing");
  assert(status.tagName === "OUTPUT", "toast native output not rendered");
  assert(status.getAttribute("aria-live") === "polite",
    "toast live region default missing");
  assert(choice.tagName === "OPTION", "selectable native option not rendered");
  assert(choice.value === "b", "selectable option value missing");
  assert(choice.selected === true, "selectable option selected state missing");
  assert(kryon.webDOMSnapshot(target, "choiceBeta").role === "option",
    "selectable option snapshot role missing");
  assert(kryon.webDOMRelationRefs(target, "choices").collectionItems
    .join(" ") === "Page/article/choices/beta",
    "listbox collection item relation refs missing");
  assert(kryon.webDOMSnapshot(target, "choiceBeta").relationRefs.collectionOwner ===
    "Page/article/choices", "option collection owner snapshot missing");
  assert(kryon.webDOMRelations(target, "choiceBeta").selectedCollectionOwner.ref ===
    "Page/article/choices", "selected collection owner relation missing");
  assert(kryon.webDOMRelationRefs(target, "choices").selectedCollectionItems
    .join(" ") === "Page/article/choices/beta",
    "selected collection items relation refs missing");
  assert(kryon.webDOMSnapshot(target, "choiceBeta").relationRefs.selectedCollectionOwner ===
    "Page/article/choices", "selected collection owner snapshot missing");
  assert(kryon.webDOMRelationRefs(target, "choices").activeCollectionItems
    .join(" ") === "Page/article/choices/beta",
    "active collection item relation refs missing");
  assert(kryon.webDOMSnapshot(target, "choiceBeta").relationRefs.activeCollectionOwner ===
    "Page/article/choices", "active collection owner snapshot missing");
  assert(menuItem.tagName === "BUTTON", "menu item button not rendered");
  assert(menuItem.getAttribute("role") === "menuitem", "menu item role missing");
  assert(kryon.webDOMSnapshot(target, "archiveItem").role === "menuitem",
    "menu item snapshot role missing");
  assert(kryon.webDOMRelations(target, "contextMenu").collectionItems
    .map((object) => object.ref).join(" ") === "Page/article/contextMenu/archive",
    "menu collection item relation missing");
  assert(kryon.webDOMRelationRefs(target, "archiveItem").collectionOwner ===
    "Page/article/contextMenu", "menu item collection owner missing");
  assert(tab.tagName === "BUTTON", "tab button not rendered");
  assert(tab.getAttribute("role") === "tab", "tab button role missing");
  assert(kryon.webDOMSnapshot(target, "overviewTab").role === "tab",
    "tab button snapshot role missing");
  assert(kryon.webDOMRelationRefs(target, "tabs").collectionItems
    .join(" ") === "Page/article/tabs/overview",
    "tablist collection item relation refs missing");
  assert(kryon.webDOMSnapshot(target, "overviewTab").relationRefs.collectionOwner ===
    "Page/article/tabs", "tab collection owner snapshot missing");
  assert(treeItem.getAttribute("role") === "treeitem", "tree item role missing");
  assert(kryon.webDOMSnapshot(target, "introNode").role === "treeitem",
    "tree item snapshot role missing");
  assert(kryon.webDOMRelationRefs(target, "outline").collectionItems
    .join(" ") === "Page/article/outline/intro",
    "tree collection item relation refs missing");
  assert(kryon.webDOMSnapshot(target, "introNode").relationRefs.collectionOwner ===
    "Page/article/outline", "tree item collection owner snapshot missing");
  const removeInstalledStyle = kryon.installWebStyleSheet(kryon.parseWebStyleSheet(\`
    Button.primary {
      background-color: rgb(12, 34, 56);
    }
    Section:has(> Button.primary:not(.missing)) { outline-style: dashed; }
    Section[webRef="missing"][webRef="article-ref"] { outline-style: solid; }
    Section#missing#article-id { outline-style: groove; }
    Section:is([webRef="article-ref"]):is(.missing) { outline-style: dotted; }
    Section:where([webRef="article-ref"]):where(.missing) { outline-style: double; }
    @media all {
      Button.primary { border-top-width: 3px; }
    }
  \`), null, "browser-install");
  assert(typeof removeInstalledStyle === "function", "installed CSS cleanup missing");
  const installedStyle = document.querySelector('style[data-kry-style="browser-install"]');
  assert(installedStyle?.textContent.includes('@media all'), "conditional CSS group not installed");
  assert(getComputedStyle(article).outlineStyle === "dashed",
    "installed nested functional selector did not match in Chromium");
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
  assert(root.kryRelations("save").controlledBy
    .map((object) => object.ref).join(" ") === "article-ref",
    "root relation object lookup missing");
  assert(kryon.webDOMRelationRefs(target, "save").controlledBy.join(" ") === "article-ref",
    "reverse controlledBy relation refs missing");
  assert(root.kryRelationRefs("save").controlledBy.join(" ") === "article-ref",
    "root relation refs missing");
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
        event.krySnapshot?.identity?.ref || "",
        event.kryEventRefs?.click || "",
        event.kryRelationRefs?.controlledBy?.join(" ") || ""
      ].join(":"));
    });
  assert(typeof removeDelegated === "function", "delegated listener cleanup missing");
  nestedButtonSpan.dispatchEvent(new Event("kry-browser-delegated", { bubbles: true }));
  assert(delegatedLog[0] === "kry-browser-delegated:SPAN:Page/article/save:Page/article/save:Page/article/save::",
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
  assert(kryon.webDOMRelations(target, "priceHeader").headerFor
    .map((object) => object.ref).join(" ") === "Page/prices/priceCell",
    "table header reverse relation missing");
  assert(kryon.webDOMSnapshot(target, "priceHeader").relationRefs.headerFor
    .join(" ") === "Page/prices/priceCell",
    "table header reverse snapshot missing");
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
  assert(kryon.webDOMRelations(target, "contact").formControls
    .map((object) => object.ref).join(" ") === "Page/externalEmail Page/contact/email",
    "form controls reverse relation missing");
  assert(kryon.webDOMRelationRefs(target, "contact").formControls
    .join(" ") === "Page/externalEmail Page/contact/email",
    "form controls reverse relation refs missing");
  assert(kryon.webDOMSnapshot(target, "contact").relationRefs.formControls
    .join(" ") === "Page/externalEmail Page/contact/email",
    "form controls reverse snapshot missing");
  const contactValues = kryon.webFormValues(target, "contact");
  assert(contactValues.email === "hello@example.test", "nested form value missing");
  assert(contactValues.external_email === "outside@example.test", "owned form value missing");
  unbindPrimary();
  assert(bindLog.includes("cleanup:Page/article/save"), "webDOMBind cleanup callback missing");
  removeInstalledStyle();
  assert(!document.querySelector('style[data-kry-style="browser-install"]'),
    "installed CSS cleanup failed");

  if (!compiledNativeBlocks.nativeBlocksUnavailable) {
    const compiledTarget = document.createElement("div");
    document.body.appendChild(compiledTarget);
    const compiledRt = kryon.createRuntime({
      webStyleSheets: kryon.parseWebStyleSheet(\`
        Article.feature { display: grid; color: #102030; }
        Figure:has(> Figcaption) { margin-block: 12; }
      \`)
    });
    kryon.beginFrame(compiledRt);
    compiledNativeBlocks.NativeBlocks_NativeBlocks(compiledRt,
      compiledNativeBlocks.createState(), {});
    kryon.endFrame(compiledRt);
    kryon.renderWebDocument(compiledRt, compiledTarget);
    const compiledStory = kryon.findWebElement(compiledTarget, "NativeBlocks/story");
    const compiledCaption = kryon.findWebElement(compiledTarget, "NativeBlocks/chart/caption");
    const compiledLegend = kryon.findWebElement(compiledTarget, "NativeBlocks/profile/heading");
    const compiledSource = kryon.findWebElement(compiledTarget, "NativeBlocks/hero/webm");
    const compiledTrack = kryon.findWebElement(compiledTarget, "NativeBlocks/hero/captions");
    const compiledEmbed = kryon.findWebElement(compiledTarget, "NativeBlocks/chartEmbed");
    const compiledCol = kryon.findWebElement(compiledTarget, "NativeBlocks/cols/metrics/quarter");
    assert(compiledStory?.tagName === "ARTICLE",
      "compiled named Article did not render native article");
    assert(compiledStory.dataset.kryName === "story",
      "compiled Article name metadata missing");
    assert(compiledStory.dataset.kryPath === "NativeBlocks/story",
      "compiled Article path metadata missing");
    assert(compiledStory.dataset.krySourceRef === "src/native_blocks.kry:3",
      "compiled Article source ref missing");
    assert(compiledStory.style.display === "grid",
      "compiled Article KSS style missing");
    assert(compiledStory.kryMatches("Article.feature"),
      "compiled Article selector match failed");
    assert(compiledStory.kryChildren[0]?.node?.text === "Story",
      "compiled Article child text missing");
    assert(kryon.webDOMStyleTrace(compiledTarget, "NativeBlocks/story")
      .resolved.display === "grid",
      "compiled Article style trace missing");
    assert(kryon.webDOMQuery(compiledTarget, "Figure:has(> Figcaption)")?.ref ===
      "NativeBlocks/chart", "compiled Figure :has query failed");
    assert(compiledCaption?.tagName === "FIGCAPTION",
      "compiled Figcaption native tag missing");
    assert(compiledLegend?.tagName === "LEGEND",
      "compiled Legend native tag missing");
    assert(kryon.webDOMRelations(compiledTarget, "NativeBlocks/profile/heading")
      .legendOwner.ref === "NativeBlocks/profile",
      "compiled legend relation missing");
    assert(compiledSource?.tagName === "SOURCE",
      "compiled Source native tag missing");
    assert(compiledSource.getAttribute("type") === "video/webm",
      "compiled Source type attribute missing");
    assert(compiledTrack?.tagName === "TRACK",
      "compiled Track native tag missing");
    assert(compiledTrack.getAttribute("srclang") === "en",
      "compiled Track srclang attribute missing");
    assert(compiledEmbed?.tagName === "EMBED",
      "compiled Embed native tag missing");
    assert(compiledEmbed.getAttribute("type") === "image/svg+xml",
      "compiled Embed type attribute missing");
    assert(compiledCol?.tagName === "COL",
      "compiled TableColumn native tag missing");
    assert(compiledCol.getAttribute("span") === "1",
      "compiled TableColumn span attribute missing");
    assert(kryon.webDOMRelations(compiledTarget, "NativeBlocks/chart/caption")
      .captionOwner.ref === "NativeBlocks/chart",
      "compiled caption relation missing");
  }

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
  // Authored text and native children coexist through repeated updates.
  const textTarget = document.createElement("div");
  document.body.appendChild(textTarget);
  const textRt = kryon.createRuntime();
  const drawTextParent = (text, child = true) => {
    kryon.beginFrame(textRt);
    kryon.widget(textRt, "Ruby", {text}, null, {path: "Text/ruby"});
    if (child) {
      kryon.widget(textRt, "RubyText", {text: "kan"}, null,
        {path: "Text/ruby/reading", parentPath: "Text/ruby"});
    }
    kryon.endFrame(textRt);
    kryon.renderWebDocument(textRt, textTarget);
    return kryon.findWebElement(textTarget, "Text/ruby");
  };
  const ruby = drawTextParent("漢");
  const reading = ruby.querySelector("rt");
  assert(ruby.textContent === "漢kan", "parent text lost beside native child");
  assert(ruby.firstChild.nodeType === Node.TEXT_NODE, "parent text must precede children");
  drawTextParent("字");
  assert(ruby.textContent === "字kan" && ruby.querySelector("rt") === reading,
    "parent text update replaced a mounted child");
  drawTextParent("");
  assert(ruby.textContent === "kan" && ruby.firstChild === reading,
    "clearing parent text removed its child");
  drawTextParent("語");
  assert(ruby.textContent === "語kan" && ruby.querySelector("rt") === reading,
    "restoring parent text replaced its child");
  drawTextParent("単", false);
  assert(ruby.textContent === "単" && ruby.children.length === 0,
    "removing children lost parent text");
  drawTextParent("漢");
  assert(ruby.textContent === "漢kan", "adding children duplicated parent text");

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
  const chainRuntime = kryon.createRuntime();
  kryon.beginFrame(chainRuntime);
  const chainSpecs = [
    ["Chain", "", "chain-outer"],
    ["Chain/a", "Chain", "chain-branch"],
    ["Chain/a/b", "Chain/a", "chain-branch"],
    ["Chain/a/b/leaf", "Chain/a/b", "chain-leaf"],
    ["Chain/anchor", "Chain", "sib-anchor"],
    ["Chain/first", "Chain", "sib-branch"],
    ["Chain/other", "Chain", "sib-other"],
    ["Chain/second", "Chain", "sib-branch"],
    ["Chain/leaf", "Chain", "sib-leaf"]
  ];
  for (const [path, parentPath, className] of chainSpecs)
    kryon.widget(chainRuntime, "Column", {class: className}, null, {path, parentPath});
  kryon.endFrame(chainRuntime);
  const chainSheet = kryon.parseWebStyleSheet(\`
    .chain-outer > .chain-branch .chain-leaf { outline-style: dashed; }
    .sib-anchor + .sib-branch ~ .sib-leaf { outline-style: dotted; }
    .chain-outer:has(> .chain-branch .chain-leaf) { outline-style: solid; }
    .sib-anchor:has(+ .sib-branch ~ .sib-leaf) { outline-style: double; }
  \`);
  kryon.setWebStyleSheets(chainRuntime, chainSheet);
  const chainTarget = document.createElement("div");
  document.body.appendChild(chainTarget);
  kryon.renderWebDocument(chainRuntime, chainTarget);
  const removeChainStyle = kryon.installWebStyleSheet(chainSheet, null, "chain-backtracking");
  for (const [path, expectedStyle] of [["Chain/a/b/leaf", "dashed"], ["Chain/leaf", "dotted"],
    ["Chain", "solid"], ["Chain/anchor", "double"]]) {
    const element = kryon.findWebElement(chainTarget, path);
    assert(kryon.webDOMStyleTrace(chainTarget, path).resolved["outline-style"] === expectedStyle,
      "runtime selector chain did not backtrack: " + path);
    element.style.removeProperty("outline-style");
    assert(getComputedStyle(element).outlineStyle === expectedStyle,
      "exported selector disagrees with runtime chain: " + path);
  }
  assert(kryon.webDOMQueryAll(chainTarget, ".chain-branch:has(.chain-outer .chain-leaf)").length === 0,
    "relative selector escaped its subject");
  removeChainStyle();
  kryon.beginFrame(chainRuntime);
  kryon.endFrame(chainRuntime);
  kryon.renderWebDocument(chainRuntime, chainTarget);
  chainTarget.remove();
  const nthRuntime = kryon.createRuntime();
  kryon.beginFrame(nthRuntime);
  kryon.widget(nthRuntime, "Column", {}, null, {path: "Nth"});
  for (let index = 0; index < 15; index++)
    kryon.widget(nthRuntime, "Button", {class: "nth-probe", label: "Position"}, null,
      {path: "Nth/" + index, parentPath: "Nth"});
  kryon.endFrame(nthRuntime);
  const nthSheet = kryon.parseWebStyleSheet(\`
    .nth-probe:not(:nth-child(1.5)) { outline-style: dashed; }
    .nth-probe:nth-child(1.5) { outline-style: solid; }
    .nth-probe:nth-last-child(1.5) { outline-style: solid; }
    .nth-probe:nth-of-type(1.5) { outline-style: solid; }
    .nth-probe:nth-last-of-type(1.5) { outline-style: solid; }
    .nth-probe:nth-child(odd// comment) { outline-style: dotted; }
  \`);
  kryon.setWebStyleSheets(nthRuntime, nthSheet);
  const nthTarget = document.createElement("div");
  document.body.appendChild(nthTarget);
  kryon.renderWebDocument(nthRuntime, nthTarget);
  const removeNthStyle = kryon.installWebStyleSheet(nthSheet, null, "nth-validation");
  for (let index = 0; index < 15; index++) {
    const path = "Nth/" + index;
    const element = kryon.findWebElement(nthTarget, path);
    const expected = index % 2 === 0 ? "dotted" : "dashed";
    assert(kryon.webDOMStyleTrace(nthTarget, path).resolved["outline-style"] === expected,
      "invalid nth formula matched at runtime: " + path);
    element.style.removeProperty("outline-style");
    assert(getComputedStyle(element).outlineStyle === expected,
      "invalid nth formula changed meaning in CSS export: " + path);
  }
  removeNthStyle();
  nthTarget.remove();
  const unitRuntime = kryon.createRuntime();
  kryon.beginFrame(unitRuntime);
  kryon.widget(unitRuntime, "Button", {class: "unit-probe", label: "Units"}, null,
    {path: "Units"});
  kryon.endFrame(unitRuntime);
  const unitSheet = kryon.parseWebStyleSheet(\`
    .unit-probe { foreground: #112233; radius: 0; opacity: 0;
      border: 2px solid #223344; font-size: 10; line-height: 2;
      font-weight: 400; letter-spacing: 3; }
  \`);
  kryon.setWebStyleSheets(unitRuntime, unitSheet);
  const unitTarget = document.createElement("div");
  document.body.appendChild(unitTarget);
  kryon.renderWebDocument(unitRuntime, unitTarget);
  const unitElement = kryon.findWebElement(unitTarget, "Units");
  const unitExpected = {
    color: "rgb(17, 34, 51)", "border-radius": "0px", opacity: "0",
    "border-top-width": "2px", "border-top-style": "solid",
    "font-size": "10px", "line-height": "20px", "font-weight": "400",
    "letter-spacing": "3px"
  };
  for (const [property, expected] of Object.entries(unitExpected))
    assert(getComputedStyle(unitElement).getPropertyValue(property) === expected,
      "inline shared CSS property mismatch: " + property);
  const removeUnitStyle = kryon.installWebStyleSheet(unitSheet, null, "css-unit-validation");
  unitElement.removeAttribute("style");
  for (const [property, expected] of Object.entries(unitExpected))
    assert(getComputedStyle(unitElement).getPropertyValue(property) === expected,
      "exported shared CSS property mismatch: " + property);
  removeUnitStyle();
  unitTarget.remove();
  const expansionRuntime = kryon.createRuntime();
  kryon.beginFrame(expansionRuntime);
  kryon.widget(expansionRuntime, "Button", {class: "expansion-probe", label: "Expansion"}, null,
    {path: "Expansion"});
  kryon.endFrame(expansionRuntime);
  const expansionSheet = kryon.parseWebStyleSheet(\`
    .expansion-probe { animation: expansion-probe 10s linear paused both; }
    @keyframes expansion-probe {
      from { padding-x: 0; padding-y: 2; margin-x: 3; margin-y: 4;
        offset-x: 0; offset-y: 5; transform: scale(2);
        background: #112233; background-end: #445566; }
      to { padding-x: 8; padding-y: 10; margin-x: 12; margin-y: 14;
        offset-x: 6; offset-y: 7; transform: scale(3);
        background: #112233; background-end: #445566; }
    }
  \`);
  const expansionTarget = document.createElement("div");
  document.body.appendChild(expansionTarget);
  kryon.renderWebDocument(expansionRuntime, expansionTarget);
  const removeExpansionStyle = kryon.installWebStyleSheet(expansionSheet, null, "css-expansion-validation");
  const expansionElement = kryon.findWebElement(expansionTarget, "Expansion");
  const animation = expansionElement.getAnimations()[0];
  assert(animation, "expanded keyframes did not create a browser animation");
  animation.pause();
  for (const [time, expected] of [[0, [0, 2, 3, 4, "matrix(2, 0, 0, 2, 0, 5)"]],
    [10000, [8, 10, 12, 14, "matrix(3, 0, 0, 3, 6, 7)"]]]) {
    animation.currentTime = time;
    const computed = getComputedStyle(expansionElement);
    for (const [property, value] of Object.entries({
      paddingLeft: expected[0], paddingRight: expected[0],
      paddingTop: expected[1], paddingBottom: expected[1],
      marginLeft: expected[2], marginRight: expected[2],
      marginTop: expected[3], marginBottom: expected[3]
    }))
      assert(computed[property] === value + "px", "keyframe axis expansion mismatch: " + property);
    assert(computed.transform === expected[4], "keyframe offset/transform expansion mismatch");
    assert(computed.backgroundImage.includes("linear-gradient"), "keyframe gradient missing");
  }
  animation.cancel();
  removeExpansionStyle();
  expansionTarget.remove();
  const inlineRuntime = kryon.createRuntime();
  kryon.beginFrame(inlineRuntime);
  kryon.widget(inlineRuntime, "Column", {class: "inline-parity"}, null, {path: "InlineParity"});
  kryon.endFrame(inlineRuntime);
  const inlineTarget = document.createElement("div");
  document.body.appendChild(inlineTarget);
  const inlineCases = [
    ["color:#445566; foreground:#112233; padding-left:9; padding-x:2; border-width:2; border-top-style:dashed;",
      {color:"rgb(17, 34, 51)", paddingLeft:"2px", paddingRight:"2px", borderTopStyle:"dashed", borderRightStyle:"solid"}],
    ["foreground:#112233; color:#445566; padding-x:2; padding-left:9; border-width:0;",
      {color:"rgb(68, 85, 102)", paddingLeft:"9px", paddingRight:"2px", borderTopWidth:"0px"}],
    ["background:#112233; background-end:#445566; background-image:none; --kry-offset-y:9px; offset-x:2;",
      {backgroundImage:"none", transform:"matrix(1, 0, 0, 1, 2, 9)"}],
    ["background:#112233; background-end:#445566; offset-x:0;",
      {backgroundImage:"linear-gradient(rgb(17, 34, 51), rgb(68, 85, 102))", transform:"matrix(1, 0, 0, 1, 0, 0)"}],
    ["opacity:0;", {opacity:"0", backgroundImage:"none", transform:"none", borderTopWidth:"0px"}]
  ];
  let previousInlineElement = null;
  for (const [declarations, expected] of inlineCases) {
    const inlineSheet = kryon.parseWebStyleSheet(".inline-parity {" + declarations + "}");
    kryon.setWebStyleSheets(inlineRuntime, inlineSheet);
    kryon.renderWebDocument(inlineRuntime, inlineTarget);
    const element = kryon.findWebElement(inlineTarget, "InlineParity");
    if (previousInlineElement)
      assert(element === previousInlineElement, "inline parity test did not exercise retained-node cleanup");
    previousInlineElement = element;
    const computed = getComputedStyle(element);
    for (const [property, value] of Object.entries(expected))
      assert(computed[property] === value, "inline parity mismatch: " + property + " got " + computed[property]);
    const removeInlineSheet = kryon.installWebStyleSheet(inlineSheet, null, "inline-parity-validation");
    const mirror = element.cloneNode(false);
    mirror.removeAttribute("style");
    document.body.appendChild(mirror);
    const exported = getComputedStyle(mirror);
    for (const [property, value] of Object.entries(expected))
      assert(exported[property] === value, "export parity mismatch: " + property + " got " + exported[property]);
    mirror.remove();
    removeInlineSheet();
  }
  inlineTarget.remove();
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
