// Host string storage and UTF-8 traversal. Editing decisions come from .kry.
import * as policy from "./text_input.js";

const encoder = new TextEncoder();
export function textBytes(text) {
  return encoder.encode(text).length;
}

function offsets(text) {
  const result = [0];
  for (const character of text)
    result.push(result[result.length - 1] + textBytes(character));
  return result;
}

function indexAt(positions, cursor) {
  let index = 0;
  while (index + 1 < positions.length && positions[index + 1] <= cursor)
    index++;
  return index;
}

export function editTextEvent(input, state, props, event) {
  let text = String(state[props.textKey] || "");
  let chars = Array.from(text);
  let positions = offsets(text);
  let cursor = positions[indexAt(positions, Number(state[props.cursorKey] || 0))];
  const selected = input.selections.get(props.focusID);
  if (selected) {
    cursor = positions[indexAt(positions, selected.cursor)];
  }
  let anchor = selected ? positions[indexAt(positions, selected.anchor)] : cursor;
  const start = Math.min(anchor, cursor);
  const end = Math.max(anchor, cursor);
  const modifier = event.type === "shortcut";
  const key = Number(event.key);
  let changed = false;

  function replace(a, b, inserted) {
    const left = indexAt(positions, a);
    const right = indexAt(positions, b);
    const result = chars.slice(0, left).join("") + inserted + chars.slice(right).join("");
    changed ||= result !== text;
    text = result;
    chars = Array.from(text);
    positions = offsets(text);
    cursor = a + textBytes(inserted);
    anchor = cursor;
  }

  function insert(value) {
    let accepted = "";
    let count = 0;
    for (const character of value) {
      const decision = policy.TextInput_TextInsertDecisionFor(null, undefined, undefined,
        character.codePointAt(0), textBytes(character), textBytes(text) - (end - start) + textBytes(accepted),
        props.textSize, chars.length - (indexAt(positions, end) - indexAt(positions, start)),
        count, props.maxCodepoints, props.multiline);
      if (decision.stop)
        break;
      if (decision.accept) {
        accepted += character;
        count++;
      }
    }
    if (accepted)
      replace(start, end, accepted);
  }

  function wordTarget(direction) {
    let index = indexAt(positions, cursor);
    do {
      index += direction;
    } while (index > 0 && index < chars.length &&
      !policy.TextInput_TextWordBoundaryFor(null, undefined, undefined,
        chars[index - 1].codePointAt(0), chars[index].codePointAt(0)));
    return positions[Math.max(0, Math.min(chars.length, index))];
  }

  function lineTarget(direction, rows = 0) {
    let index = indexAt(positions, cursor);
    let first = index;
    while (first > 0 && chars[first - 1] !== "\n") {
      first--;
    }
    if (!rows && direction < 0) {
      return positions[first];
    }
    let last = index;
    while (last < chars.length && chars[last] !== "\n") {
      last++;
    }
    if (!rows) {
      return positions[last];
    }
    const column = index - first;
    while (rows-- > 0) {
      if (direction < 0) {
        if (first === 0) {
          break;
        }
        last = first - 1;
        first = last;
        while (first > 0 && chars[first - 1] !== "\n") {
          first--;
        }
      } else {
        if (last === chars.length) {
          break;
        }
        first = last + 1;
        last = first;
        while (last < chars.length && chars[last] !== "\n") {
          last++;
        }
      }
    }
    return positions[Math.min(last, first + column)];
  }

  const composition = policy.TextInput_TextCompositionInputDecisionFor(null, undefined, undefined,
    input.focus === props.focusID, props.readOnly);
  if (composition.cancel)
    input.preedit.delete(props.focusID);
  if (event.type === "composition") {
    if (composition.accept_events && event.owner === props.focusID) {
      const phase = policy.TextInput_TextCompositionPhaseDecisionFor(null, undefined, undefined, event.phase);
      if (phase.store_preedit) {
        input.preedit.set(props.focusID, event);
      }
      if (phase.commit) {
        insert(event.text);
        input.preedit.delete(props.focusID);
      }
      if (phase.cancel) {
        input.preedit.delete(props.focusID);
      }
    }
  } else if (event.type === "text") {
    if (policy.TextInput_TextNativeEditShouldRun(null, undefined, undefined, props.readOnly, false))
      insert(event.text);
  } else {
    const shortcut = policy.TextInput_TextShortcutInputFor(null, undefined, undefined,
      modifier, key === 65, key === 67, key === 88, key === 86);
    const command = shortcut.select_all ? policy.TextInput_TextContextCommandSelectAll()
      : shortcut.copy ? policy.TextInput_TextContextCommandCopy()
      : shortcut.cut ? policy.TextInput_TextContextCommandCut()
      : shortcut.paste ? policy.TextInput_TextContextCommandPaste() : 0;
    if (command) {
      const decision = policy.TextInput_TextEditCommandDecisionFor(null, undefined, undefined,
        command, start !== end, false, false, text.length > 0,
        !props.secure && (!shortcut.cut || !props.readOnly), !props.readOnly, !props.readOnly);
      if (decision.select_all) {
        const all = policy.TextInput_TextSelectionAll(null, undefined, undefined, textBytes(text));
        anchor = all.anchor;
        cursor = all.cursor;
      }
      if (decision.copy_selection)
        input.clipboard = chars.slice(indexAt(positions, start), indexAt(positions, end)).join("");
      if (decision.delete_selection && !decision.paste) {
        replace(start, end, "");
      }
      if (decision.paste) {
        insert(input.clipboard);
      }
    } else {
      const navigationKey = policy.TextInput_TextNavigationKeyFor(null, undefined, undefined,
        props.multiline, key === 263, key === 262, key === 268, key === 269,
        key === 265, key === 264, key === 266, key === 267);
      const navigation = policy.TextInput_TextNavigationDecisionFor(null, undefined, undefined,
        navigationKey, props.multiline, !!event.shift, modifier, props.secure, start !== end);
      if (navigation.consumed) {
        let target = cursor;
        if (navigation.collapse_selection_start) {
          target = start;
        }
        else if (navigation.collapse_selection_end) {
          target = end;
        }
        else if (navigation.document_edge) {
          target = navigation.document_edge < 0 ? 0 : textBytes(text);
        }
        else if (navigation.line_edge) {
          target = lineTarget(navigation.line_edge);
        }
        else if (navigation.word_direction) {
          target = wordTarget(navigation.word_direction);
        }
        else if (navigation.char_direction)
          target = positions[Math.max(0, Math.min(chars.length, indexAt(positions, cursor) + navigation.char_direction))];
        else if (navigation.vertical_direction) {
          target = lineTarget(navigation.vertical_direction, 1);
        }
        else if (navigation.page_direction) {
          target = lineTarget(navigation.page_direction, props.pageRows);
        }
        const selection = policy.TextInput_TextSelectionAfterMove(null, undefined, undefined,
          anchor, cursor, target, navigation.extend_selection);
        anchor = selection.anchor;
        cursor = selection.cursor;
      } else if (policy.TextInput_TextDeleteShortcutShouldRun(null, undefined, undefined,
        props.readOnly, key === 259, key === 261, 0)) {
        const action = key === 259 ? policy.TextInput_TextDeleteBackspace() : policy.TextInput_TextDeleteForward();
        const deletion = policy.TextInput_TextDeleteDecisionFor(null, undefined, undefined,
          action, modifier, props.secure, start !== end);
        let a = start, b = end;
        if (deletion.document_edge < 0) {
          a = 0;
        }
        if (deletion.document_edge > 0) {
          b = textBytes(text);
        }
        if (deletion.word_direction < 0) {
          a = wordTarget(-1);
        }
        if (deletion.word_direction > 0) {
          b = wordTarget(1);
        }
        if (deletion.char_direction < 0) {
          a = positions[Math.max(0, indexAt(positions, cursor) - 1)];
        }
        if (deletion.char_direction > 0) {
          b = positions[Math.min(chars.length, indexAt(positions, cursor) + 1)];
        }
        if (deletion.consumed) {
          replace(a, b, "");
        }
      } else if (key === 257 && policy.TextInput_TextNativeEditShouldRun(null, undefined, undefined, props.readOnly, modifier)) {
        if (props.multiline) {
          insert("\n");
        }
        else if (props.commitKey) {
          state[props.commitKey] = true;
        }
      } else if (key === 256 && policy.TextInput_TextEscapeShouldBlur(null, undefined, undefined, true, true, true)) {
        input.focus = 0;
        input.preedit.delete(props.focusID);
      }
    }
  }
  state[props.textKey] = text;
  state[props.cursorKey] = cursor;
  input.selections.set(props.focusID, { anchor, cursor });
  return changed;
}
