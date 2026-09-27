// Raw browser effects; document policy and tree traversal live in dom_tree.zi.
addToLibrary({
  $domTagFor: function(semanticKind, widgetKind, headingLevel, detail, label) {
    var text = label || "";
    var uri = detail || "";
    var level = Math.min(6, Math.max(1, headingLevel || 1));
    if (semanticKind === 1) return {tag: "main", text: true};
    if (semanticKind === 2) return {tag: "section", text: true};
    if (semanticKind === 3) return {tag: "h" + level, text: true, level: level};
    if (semanticKind === 4) return {tag: "p", text: true};
    if (semanticKind === 5) {
      return uri ? {tag: "a", text: true, href: uri} :
        {tag: "a", text: true, role: "link"};
    }
    if (semanticKind === 6) {
      return uri ? {tag: "img", alt: text, src: uri} :
        {tag: "div", role: "img", alt: undefined};
    }
    if (semanticKind === 7 || widgetKind === 8) return {tag: "button", text: true};
    if (semanticKind === 9) return {tag: "li", text: true, role: "treeitem"};
    if (semanticKind === 10) return {tag: "input", role: "combobox"};
    if (semanticKind === 11) return {tag: "option", text: true};
    if (semanticKind === 12) return {tag: "input"};
    if (semanticKind === 13) return {tag: "div", role: "tablist"};
    if (semanticKind === 14) return {tag: "button", text: true, role: "tab"};
    if (semanticKind === 15) return {tag: "textarea"};
    if (widgetKind === 2 || widgetKind === 15) return {tag: "p", text: true};
    if (widgetKind === 24) return {tag: "img", alt: text};
    if (widgetKind === 51) return {tag: "main", text: true};
    if (widgetKind === 52) return {tag: "section", text: true};
    if (widgetKind === 53) return {tag: "a", text: true, role: "link"};
    return {tag: "div"};
  },
  js_dom_begin__deps: ["$domTagFor"],
  js_dom_begin__sig: "vidd",
  js_dom_begin: function(count, width, height) {
    var g = globalThis;
    var canvasState = g.__kryonCanvas;
    if (!canvasState || typeof document === "undefined") return;
    var dom = g.__kryonDom;
    if (!dom || !dom.root) {
      dom = g.__kryonDom = {nodes: new Map(), seen: null, frame: 0, tagFor: domTagFor};
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
  js_dom_node__sig: "viiiiiiddddiii",
  js_dom_node: function(index, parent, identity, widgetKind, semanticKind,
                         headingLevel, x, y, width, height, label, detail, flags) {
    var dom = globalThis.__kryonDom;
    if (!dom || !dom.root) return;
    var id = String(identity);
    dom.seen.add(id);
    var record = dom.nodes.get(id);
    var decodedLabel = label ? UTF8ToString(label) : "";
    var decodedDetail = detail ? UTF8ToString(detail) : "";
    var spec = dom.tagFor(semanticKind, widgetKind, headingLevel,
        decodedDetail, decodedLabel);
    dom.byIndex[index] = id;
    if (!record) {
      var element = document.createElement(spec.tag);
      element.setAttribute("data-kryon-id", id);
      element.setAttribute("data-kryon-index", String(index));
      element.style.position = "absolute";
      element.style.boxSizing = "border-box";
      element.style.margin = "0";
      element.style.padding = "0";
      element.style.border = "0";
      element.style.background = "transparent";
      element.style.color = "transparent";
      element.style.pointerEvents = (flags & 1) ? "auto" : "none";
      record = {element: element, parent: null};
      dom.nodes.set(id, record);
    } else if (record.element.tagName.toLowerCase() !== spec.tag) {
      var replacement = document.createElement(spec.tag);
      replacement.setAttribute("data-kryon-id", id);
      replacement.setAttribute("data-kryon-index", String(index));
      record.element.parentNode.replaceChild(replacement, record.element);
      record.element = replacement;
      record.parent = null;
      record.element.style.position = "absolute";
      record.element.style.boxSizing = "border-box";
      record.element.style.margin = "0";
      record.element.style.padding = "0";
      record.element.style.border = "0";
      record.element.style.background = "transparent";
      record.element.style.color = "transparent";
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
    var text = decodedLabel;
    if (spec.text) element.textContent = text;
    if (spec.href !== undefined) element.setAttribute("href", spec.href);
    if (spec.src !== undefined) element.setAttribute("src", spec.src);
    if (text) element.setAttribute("aria-label", text);
    if (!text) element.removeAttribute("aria-label");
    if (spec.alt !== undefined) element.setAttribute("alt", spec.alt || "");
    if (spec.role) element.setAttribute("role", spec.role);
    if (spec.level) element.setAttribute("aria-level", String(spec.level));
    if (flags & 2) element.setAttribute("aria-disabled", "true");
    else element.removeAttribute("aria-disabled");
    if (flags & 8) element.setAttribute("aria-selected", "true");
    else element.removeAttribute("aria-selected");
    if (spec.tag === "button") element.disabled = !!((flags & 2) || (flags & 4));
    var parentRecord = parent >= 0 ? dom.nodes.get(dom.byIndex[parent]) : null;
    if (!parentRecord) parentRecord = {element: dom.root};
    if (record.parent !== parentRecord.element) {
      parentRecord.element.appendChild(element);
      record.parent = parentRecord.element;
    }
    element.style.order = "";
    parentRecord.element.appendChild(element);
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
    var page = dom.root.querySelector('[data-kryon-semantic="1"]');
    if (page) {
      document.title = page.getAttribute("aria-label") ||
        page.textContent || document.title;
    }
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
