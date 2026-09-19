"""Optional real libatspi smoke test; pass the native fixture binary as argv[1]."""

import os
import subprocess
import sys
import time

import gi

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi


def main():
    environment = dict(os.environ, NO_AT_BRIDGE="0", KRYON_ACCESSIBILITY="1")
    process = subprocess.Popen(
        [sys.argv[1]], stdout=subprocess.PIPE, text=True, env=environment
    )
    try:
        assert process.stdout.readline().strip() == "READY"
        desktop = Atspi.get_desktop(0)
        app = None
        for _ in range(100):
            for index in range(desktop.get_child_count()):
                candidate = desktop.get_child_at_index(index)
                if candidate.get_name() == "Kryon Accessibility Test":
                    app = candidate
                    break
            if app is not None:
                break
            time.sleep(0.02)
        assert app is not None, "application missing from AT-SPI registry"
        window = app.get_child_at_index(0)
        assert window.get_child_count() == 3
        button, editor, password = [window.get_child_at_index(i) for i in range(3)]
        assert button.get_name() == "Run"
        assert Atspi.Text.get_text(editor, 0, -1) == "Ae\u0301Z"
        assert Atspi.Text.get_character_count(editor) == 4
        assert Atspi.Text.get_text(password, 0, -1) == ""
        assert Atspi.Action.do_action(button, 0)
        assert Atspi.EditableText.set_text_contents(editor, "\u754c")
        assert process.stdout.readline().strip() == "APPLIED"
        assert process.wait(timeout=5) == 0
        print("libatspi discovery, Unicode text, secure redaction and actions: PASS")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)


if __name__ == "__main__":
    main()
