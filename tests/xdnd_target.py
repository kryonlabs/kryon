#!/usr/bin/env python3
"""A minimal XDND version 5 drop target for private-display tests.

Usage: xdnd_target.py X Y WIDTH HEIGHT OUTPUT.json [ACTION]
Maps a top-level window, answers every position with an accepting XdndStatus
for ACTION (XdndActionCopy by default), converts XdndSelection to
text/uri-list on drop, finishes the drag and writes what it received.
Run only on a private display; it never touches other clients' windows.
"""
import json
import select
import sys
import time

from Xlib import X, Xatom, display
from Xlib.protocol import event


def main():
    x, y, width, height = map(int, sys.argv[1:5])
    output = sys.argv[5]
    action_name = sys.argv[6] if len(sys.argv) > 6 else "XdndActionCopy"
    d = display.Display()
    screen = d.screen()
    atom = d.intern_atom
    window = screen.root.create_window(
        x, y, width, height, 0, screen.root_depth, X.InputOutput, X.CopyFromParent,
        background_pixel=screen.white_pixel, override_redirect=True,
        event_mask=X.PropertyChangeMask | X.StructureNotifyMask)
    window.change_property(atom("XdndAware"), Xatom.ATOM, 32, [5])
    window.set_wm_name("XDND test target")
    window.map()
    d.sync()
    result = {"window": window.id, "enters": 0, "positions": 0, "leaves": 0, "drops": 0,
              "types": [], "actions": [], "data": None, "targets": None, "source": None}

    def save():
        with open(output, "w") as stream:
            json.dump(result, stream)
    save()
    print("ready", flush=True)
    source = None
    deadline = time.monotonic() + 60
    pending_targets = False
    while time.monotonic() < deadline:
        if not d.pending_events():
            select.select([d.fileno()], [], [], 0.2)
            if not d.pending_events():
                continue
        ev = d.next_event()
        if ev.type == X.ClientMessage:
            kind = d.get_atom_name(ev.client_type)
            data = ev.data[1]
            if kind == "XdndEnter":
                result["enters"] += 1
                source = d.create_resource_object("window", data[0])
                result["source"] = data[0]
                types = [value for value in data[2:5] if value]
                if data[1] & 1:
                    listed = source.get_full_property(atom("XdndTypeList"), Xatom.ATOM)
                    types = list(listed.value) if listed else types
                result["types"] = [d.get_atom_name(value) for value in types]
                result["version"] = data[1] >> 24
            elif kind == "XdndPosition" and source is not None:
                result["positions"] += 1
                if data[4]:
                    result["actions"].append(d.get_atom_name(data[4]))
                reply = event.ClientMessage(window=source, client_type=atom("XdndStatus"),
                                            data=(32, [window.id, 1, 0, 0, atom(action_name)]))
                source.send_event(reply)
                d.flush()
            elif kind == "XdndLeave":
                result["leaves"] += 1
                save()
            elif kind == "XdndDrop" and source is not None:
                result["drops"] += 1
                window.convert_selection(atom("XdndSelection"), atom("TARGETS"),
                                         atom("XDND_TARGETS"), data[2])
                pending_targets = True
                d.flush()
        elif ev.type == X.SelectionNotify and ev.requestor == window:
            if pending_targets:
                pending_targets = False
                listed = window.get_full_property(atom("XDND_TARGETS"), Xatom.ATOM)
                result["targets"] = [d.get_atom_name(value) for value in listed.value] if listed else []
                window.convert_selection(atom("XdndSelection"), atom("text/uri-list"),
                                         atom("XDND_DATA"), X.CurrentTime)
                d.flush()
                continue
            value = window.get_full_property(atom("XDND_DATA"), X.AnyPropertyType) if ev.property else None
            result["data"] = value.value.decode("utf-8", "replace") if value else ""
            finished = event.ClientMessage(window=source, client_type=atom("XdndFinished"),
                                           data=(32, [window.id, 1, atom(action_name), 0, 0]))
            source.send_event(finished)
            d.flush()
            save()
            time.sleep(.2)
            return 0
    save()
    return 3


if __name__ == "__main__":
    sys.exit(main())
