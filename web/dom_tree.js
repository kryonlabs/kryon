// Raw browser effects; document policy and tree traversal live in dom_tree.zi.
addToLibrary({
  js_dom_begin__deps: [],
  js_dom_begin__sig: "vidd",
  js_dom_begin: function(count, width, height) {
    var g = globalThis;
    var canvasState = g.__kryonCanvas;
    if (!canvasState || typeof document === "undefined") return;
    var dom = g.__kryonDom;
    if (!dom || !dom.root) {
      dom = g.__kryonDom = {nodes: new Map(), seen: null, frame: 0};
      var box = canvasState.layoutBox ? canvasState.layoutBox() : null;
      var root = document.createElement("div");
      root.id = "kryon-dom-root";
      root.setAttribute("data-kryon", "root");
      root.style.position = "absolute";
      root.style.inset = "0";
      root.style.overflow = "hidden";
      root.style.pointerEvents = "none";
      root.style.zIndex = "2";
      root.style.fontFamily = "system-ui, sans-serif";
      root.style.fontSize = "16px";
      root.style.lineHeight = "1.4";
      (box || document.body).appendChild(root);
      dom.root = root;
    }
    dom.count = count;
    dom.width = width;
    dom.height = height;
    dom.seen = new Set();
    dom.byIndex = [];
    dom.frame++;
  },

  js_dom_node__deps: ["$UTF8ToString"],
  js_dom_node__sig: "viidiiiddddiiiii",
  js_dom_node: function(index, parent, identity, widgetKind, semanticKind,
                         level, x, y, width, height, tag, role, label, url, flags) {
    var dom = globalThis.__kryonDom;
    if (!dom || !dom.root) return;
    var id = String(identity);
    dom.seen.add(id);
    var record = dom.nodes.get(id);
    var tagName = UTF8ToString(tag);
    var text = label ? UTF8ToString(label) : "";
    var makeElement = function () {
      var created = document.createElement(tagName);
      created.setAttribute("data-kryon-id", id);
      created.style.position = "absolute";
      created.style.boxSizing = "border-box";
      created.style.margin = "0";
      created.style.padding = "0";
      created.style.border = "0";
      created.style.background = "transparent";
      created.style.color = "transparent";
      return created;
    };
    dom.byIndex[index] = id;
    if (!record) {
      record = {element: makeElement(), parent: null};
      dom.nodes.set(id, record);
    } else if (record.element.tagName.toLowerCase() !== tagName) {
      var replacement = makeElement();
      record.element.parentNode.replaceChild(replacement, record.element);
      record.element = replacement;
      record.parent = null;
    }
    var element = record.element;
    element.setAttribute("data-kryon-index", String(index));
    element.setAttribute("data-kryon-kind", String(widgetKind));
    element.setAttribute("data-kryon-semantic", String(semanticKind));
    element.style.left = x + "px";
    element.style.top = y + "px";
    element.style.width = Math.max(0, width) + "px";
    element.style.height = Math.max(0, height) + "px";
    element.style.pointerEvents = (flags & 1) ? "auto" : "none";
    element.style.cursor = (flags & 1) ? "pointer" : "default";
    element.style.opacity = (flags & 2) ? "0.45" : "1";
    if (flags & 16) element.textContent = text;
    if (flags & 64) element.setAttribute("href", url ? UTF8ToString(url) : "");
    if (flags & 128) element.setAttribute("src", url ? UTF8ToString(url) : "");
    if (text) element.setAttribute("aria-label", text);
    else element.removeAttribute("aria-label");
    if (flags & 32) element.setAttribute("alt", text);
    var roleName = role ? UTF8ToString(role) : "";
    if (roleName) element.setAttribute("role", roleName);
    if (level) element.setAttribute("aria-level", String(level));
    if (flags & 2) element.setAttribute("aria-disabled", "true");
    else element.removeAttribute("aria-disabled");
    if (flags & 8) element.setAttribute("aria-selected", "true");
    else element.removeAttribute("aria-selected");
    if (flags & 256) element.disabled = true;
    else if (element.disabled) element.disabled = false;
    var parentRecord = parent >= 0 ? dom.nodes.get(dom.byIndex[parent]) : null;
    if (!parentRecord) parentRecord = {element: dom.root};
    if (record.parent !== parentRecord.element) {
      parentRecord.element.appendChild(element);
      record.parent = parentRecord.element;
    }
    parentRecord.element.appendChild(element);
  },

  js_dom_title__deps: ["$UTF8ToString"],
  js_dom_title__sig: "vi",
  js_dom_title: function(title) {
    if (typeof document !== "undefined") document.title = UTF8ToString(title);
  },

  js_dom_finish__deps: [],
  js_dom_finish__sig: "v",
  js_dom_finish: function() {
    var dom = globalThis.__kryonDom;
    if (!dom || !dom.root) return;
    dom.nodes.forEach(function(record, id) {
      if (!dom.seen.has(id)) {
        if (record.element.parentNode) record.element.parentNode.removeChild(record.element);
        dom.nodes.delete(id);
      }
    });
  },

  js_dom_close__deps: [],
  js_dom_close__sig: "v",
  js_dom_close: function() {
    var dom = globalThis.__kryonDom;
    if (!dom) return;
    globalThis.__kryonDomSnapshot = {
      title: document.title,
      nodes: []
    };
    dom.nodes.forEach(function(record, id) {
      var element = record.element;
      globalThis.__kryonDomSnapshot.nodes.push({
        id: id,
        parent: record.parent ? record.parent.getAttribute("data-kryon-id") : null,
        tag: element.tagName.toLowerCase(),
        text: element.textContent,
        semantic: element.getAttribute("data-kryon-semantic"),
        kind: element.getAttribute("data-kryon-kind"),
        href: element.getAttribute("href") || "",
        role: element.getAttribute("role") || "",
        disabled: element.getAttribute("aria-disabled") === "true"
      });
    });
    if (dom.root && dom.root.parentNode) dom.root.parentNode.removeChild(dom.root);
    globalThis.__kryonDom = null;
  }
});
