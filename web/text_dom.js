// Browser transport for the shared editor. DOM selection offsets are UTF-16;
// application cursor and selection offsets are UTF-8 bytes.
import {editTextEvent, textBytes} from "./text_edit.js";
import * as policy from "./text_input.js";

const editors = new WeakMap();
export function registerTextEditor(item, editor) {
  editors.set(item, editor);
}
export function hasTextEditor(node) {
  return editors.has(node);
}
export function connectTextEditor(item, node) {
  const editor = editors.get(item);
  if (editor) {
    editors.set(node, editor);
    node.value = String(editor.state[editor.props.textKey] || "");
    node.text = node.value;
    node.readOnly = editor.props.readOnly;
    node.state.disabled ||= editor.props.disabled;
  }
}
export function presentTextEditor(el) {
  const editor = editorFor(el);
  if (!editor || !editor.props.focusID || el.__kryTextComposing) {
    return;
  }
  if (editor.rt.Focus() === editor.props.focusID && !editor.props.disabled) {
    if (document.activeElement !== el) {
      el.focus();
    }
    showText(el, editor);
  } else if (document.activeElement === el) {
    el.blur();
  }
}
export function refreshTextEditor(el) {
  el.__kryRefreshTextEditor?.();
}
function utf16At(text, bytes) {
  let used = 0;
  let index = 0;
  for (const character of text) {
    if (used + textBytes(character) > bytes) {
      break;
    }
    used += textBytes(character);
    index += character.length;
  }
  return index;
}
function editorFor(el) {
  return editors.get(el.__kryDocNode);
}
function syncSelection(el, editor) {
  const text = String(editor.state[editor.props.textKey] || "");
  const start = textBytes(text.slice(0, el.selectionStart));
  const end = textBytes(text.slice(0, el.selectionEnd));
  const backward = el.selectionDirection === "backward";
  editor.rt.input.selections.set(editor.props.focusID,
    {anchor: backward ? end : start, cursor: backward ? start : end});
  editor.state[editor.props.cursorKey] = backward ? start : end;
}
function showText(el, editor) {
  const text = String(editor.state[editor.props.textKey] || "");
  el.value = text;
  const selection = editor.rt.input.selections.get(editor.props.focusID);
  const cursor = Number(editor.state[editor.props.cursorKey] || 0);
  const anchor = selection?.anchor ?? cursor;
  const end = selection?.cursor ?? cursor;
  el.setSelectionRange(utf16At(text, Math.min(anchor, end)),
    utf16At(text, Math.max(anchor, end)), anchor > end ? "backward" : "forward");
}
function apply(el, editor, event) {
  editTextEvent(editor.rt.input, editor.state, editor.props, event);
  showText(el, editor);
  if (editor.rt.Focus() === 0) {
    el.blur();
  }
}

export function bindTextEditorEvents(el) {
  if (el.__kryTextEvents) {
    return;
  }
  el.__kryTextEvents = true;
  let composing = false;
  el.__kryTextComposing = false;
  let compositionOwner = null;
  el.__kryRefreshTextEditor = () => {
    const editor = editorFor(el);
    if (composing && (!editor || editor.props.readOnly || editor.props.disabled ||
        editor.state !== compositionOwner?.state || editor.props.focusID !== compositionOwner?.props.focusID)) {
      compositionOwner?.rt.input.preedit.delete(compositionOwner.props.focusID);
      composing = false;
      el.__kryTextComposing = false;
      compositionOwner = null;
      if (editor) {
        showText(el, editor);
      }
    }
  };
  const listen = (name, callback) => el.addEventListener(name, event => {
    const editor = editorFor(el);
    if (!editor || (editor.props.disabled && name !== "blur")) {
      return;
    }
    callback(event, editor);
  }, true);
  listen("click", event => event.stopPropagation());
  listen("focus", (_event, editor) => {
    editor.rt.SetFocus(editor.props.focusID);
  });
  listen("blur", (_event, editor) => {
    if (composing) {
      apply(el, editor, {type: "composition", phase: 4, text: "", owner: editor.props.focusID});
    }
    composing = false;
    el.__kryTextComposing = false;
    compositionOwner = null;
    editor.rt.input.preedit.delete(editor.props.focusID);
    if (editor.rt.Focus() === editor.props.focusID) {
      editor.rt.SetFocus(0);
    }
  });
  for (const name of ["select", "pointerup", "keyup"]) {
    listen(name, (_event, editor) => {
      if (!composing) {
        syncSelection(el, editor);
      }
    });
  }
  listen("compositionstart", (_event, editor) => {
    if (editor.props.readOnly) {
      return;
    }
    syncSelection(el, editor);
    composing = true;
    el.__kryTextComposing = true;
    compositionOwner = editor;
  });
  listen("compositionupdate", (event, editor) => {
    if (!composing || (compositionOwner?.state !== editor.state || compositionOwner?.props.focusID !== editor.props.focusID)) {
      return;
    }
    editTextEvent(editor.rt.input, editor.state, editor.props,
      {type: "composition", phase: 2, text: event.data || "", owner: editor.props.focusID});
  });
  listen("compositionend", (event, editor) => {
    if (!composing || (compositionOwner?.state !== editor.state || compositionOwner?.props.focusID !== editor.props.focusID)) {
      return;
    }
    composing = false;
    el.__kryTextComposing = false;
    compositionOwner = null;
    apply(el, editor, {type: "composition", phase: event.data ? 3 : 4,
      text: event.data || "", owner: editor.props.focusID});
    event.stopImmediatePropagation();
  });
  listen("beforeinput", (event, editor) => {
    if (composing || event.isComposing) {
      return;
    }
    // Committed composition is applied by compositionend, once.
    if (event.inputType === "insertFromComposition") {
      event.preventDefault();
      return;
    }
    syncSelection(el, editor);
    let action = null;
    if (event.inputType.startsWith("insert")) {
      action = {type: "text", text: event.inputType === "insertLineBreak" ||
        event.inputType === "insertParagraph" ? "\n" : event.data || ""};
    } else if (event.inputType.startsWith("delete")) {
      action = {type: event.inputType.includes("Word") ? "shortcut" : "key",
        key: event.inputType.endsWith("Backward") ? 259 : 261};
    }
    if (action) {
      event.preventDefault();
      event.stopImmediatePropagation();
      apply(el, editor, action);
    }
  });
  listen("input", (event, editor) => {
    // Native composition owns the provisional DOM value, never committed state.
    if (!composing) {
      showText(el, editor);
    }
    event.stopImmediatePropagation();
  });
  const keys = {ArrowLeft: 263, ArrowRight: 262, ArrowUp: 265, ArrowDown: 264,
    PageUp: 266, PageDown: 267, Home: 268, End: 269,
    Backspace: 259, Delete: 261, Enter: 257, Escape: 256, a: 65};
  listen("keydown", (event, editor) => {
    if (composing && event.key !== "Escape") {
      return;
    }
    // The browser resolves visual rows, pointer hit testing and caret scrolling.
    // Tab follows the live DOM focus order, including controls outside the editor.
    const key = keys[event.key];
    if (!key || (event.key === "a" && !event.ctrlKey && !event.metaKey)) {
      return;
    }
    if (!composing) {
      syncSelection(el, editor);
    }
    const navigationKey = policy.TextInput_TextNavigationKeyFor(null, undefined, undefined,
      editor.props.multiline, key === 263, key === 262, key === 268, key === 269,
      key === 265, key === 264, key === 266, key === 267);
    const navigation = policy.TextInput_TextNavigationDecisionFor(null, undefined, undefined,
      navigationKey, editor.props.multiline, event.shiftKey, event.ctrlKey || event.metaKey,
      editor.props.secure, el.selectionStart !== el.selectionEnd);
    if (navigation.vertical_direction || navigation.page_direction || navigation.line_edge) {
      return;
    }
    if (composing && event.key === "Escape") {
      composing = false;
    el.__kryTextComposing = false;
      compositionOwner = null;
    }
    event.preventDefault();
    event.stopImmediatePropagation();
    apply(el, editor, {type: event.ctrlKey || event.metaKey ? "shortcut" : "key", key, shift: event.shiftKey});
  });
  for (const [name, key] of [["copy", 67], ["cut", 88], ["paste", 86]]) {
    listen(name, (event, editor) => {
      if (composing) {
        return;
      }
      syncSelection(el, editor);
      event.preventDefault();
      event.stopImmediatePropagation();
      if (name === "paste") {
        editor.rt.SetClipboardText(event.clipboardData?.getData("text/plain") || "");
      }
      if (name !== "paste") {
        editor.rt.SetClipboardText("");
      }
      apply(el, editor, {type: "shortcut", key});
      if (name !== "paste") {
        event.clipboardData?.setData("text/plain", editor.rt.ClipboardText());
      }
    });
  }
}
