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
  assert(article.style.display === "grid", "KSS display not applied");
  assert(article.style.gridTemplateAreas === '"main"', "KSS grid area not applied");
  assert(article.style.borderCollapse === "collapse", "KSS table style not applied");
  assert(kryon.webDOMObject(target, "article-ref").element === article, "DOM object lookup failed");
  assert(article.kryObject.element === article, "element Kry object getter failed");
  assert(article.kryMatches("Section[webRef='article-ref']"), "KSS selector match failed");
  const removeInstalledStyle = kryon.installWebStyleSheet(kryon.parseWebStyleSheet(\`
    Button.primary {
      background: rgb(12, 34, 56);
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
  const button = kryon.webDOMQuery(target, "Section > Button.primary");
  assert(button?.element?.id === "save", "child selector query failed");
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
  removeInstalledStyle();
  assert(!document.querySelector('style[data-kry-style="browser-install"]'),
    "installed CSS cleanup failed");
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
