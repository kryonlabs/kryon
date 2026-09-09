"""Regression tests for the single TextProps-value source guard."""

import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("text_guard", ROOT / "scripts/check-clean-text-api.py")
GUARD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GUARD)


class TextAPIGuardTest(unittest.TestCase):
    def test_accepts_explicit_props_values(self):
        for source in (
            'Text((TextProps){.text="Hello"});',
            'TextProps heading = {.text="Hello"}; Text(heading);',
            'heading: TextProps = (TextProps){.text="Hello"}; Text(heading);',
            'void draw(TextProps props) { Text(props); }',
            'void Text(TextProps props);',
        ):
            with self.subTest(source=source):
                self.assertEqual(GUARD.violations_in_source(source, Path("fixture.kry")), [])

    def test_rejects_legacy_or_untyped_arguments(self):
        for source in (
            'Text("Hello", 1, 2, 16, WHITE);',
            'Text(heading);',
            'heading: int = 2; Text(heading);',
            'TextProps heading; Text(heading, 2);',
            'TextProps *heading; Text(heading);',
            '// heading: TextProps\nText(heading);',
            'const char *s="heading: TextProps"; Text(heading);',
            'Text(heading); heading: TextProps;',
            'TextProps heading(); Text(heading);',
            'TextColored("Hello", WHITE);',
        ):
            with self.subTest(source=source):
                self.assertTrue(GUARD.violations_in_source(source, Path("fixture.kry")))


if __name__ == "__main__":
    unittest.main()
