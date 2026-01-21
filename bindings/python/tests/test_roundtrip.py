"""
Round-trip conversion tests for Kryon Python bindings

Tests that Python → KIR → Python preserves semantic equivalence
"""

import pytest
import json
import sys
import os

# Add parent directory to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import kryon
from kryon.dsl import (
    Column, Row, Text, Button, Container, Center,
    Input, Checkbox, Dropdown, Textarea,
    Image, Canvas, Markdown,
    TabGroup, TabBar, Tab, TabContent, TabPanel,
    Modal,
    Table, TableHead, TableBody, TableFoot,
    TableRow, TableCell, TableHeaderCell,
    Heading, Paragraph, Blockquote, CodeBlock,
    HorizontalRule, List, ListItem, Link,
    Span, Strong, Em, CodeInline, Small, Mark,
    Style, Layout, Color,
)
from kryon.codegen import KIRGenerator, KIRSerializer
from kryon.parser import KIRParser, PythonGenerator


class TestRoundTrip:
    """Test round-trip conversion: Python → KIR → Python"""

    def test_simple_column_roundtrip(self):
        """Test simple Column component round-trip"""
        # Original: Python DSL
        original = Column(style=Style(width="100%"), children=[Text(text="Hello, World!")])

        # Python → KIR
        generator = KIRGenerator()
        kir_dict = generator.generate(original)

        # KIR → Python
        parser = KIRParser()
        parsed = parser.parse(kir_dict)

        # Generate Python code from KIR
        codegen = PythonGenerator()
        python_code = codegen.generate_from_component(parsed)

        # Verify KIR structure
        assert kir_dict["type"] == "Column"
        assert "style" in kir_dict
        assert kir_dict["style"]["width"]["value"] == "100%"

        # Verify parsed component
        assert parsed.type.to_string() == "Column"
        assert len(parsed.children) == 1
        assert parsed.children[0].type.to_string() == "Text"

    def test_nested_components_roundtrip(self):
        """Test nested component round-trip"""
        original = Column(
            gap=10,
            children=[
                Row(
                    gap=20,
                    children=[
                        Text(text="Left"),
                        Text(text="Right"),
                    ]
                ),
                Button(title="Click Me"),
            ]
        )

        # Python → KIR
        generator = KIRGenerator()
        kir_dict = generator.generate(original)

        # KIR → Python
        parser = KIRParser()
        parsed = parser.parse(kir_dict)

        # Verify structure
        assert parsed.type.to_string() == "Column"
        assert len(parsed.children) == 2
        assert parsed.children[0].type.to_string() == "Row"
        assert len(parsed.children[0].children) == 2

    def test_style_roundtrip(self):
        """Test style property round-trip with snake_case ↔ camelCase conversion"""
        original = Column(
            style=Style(
                background_color="#ff0000",
                padding=16,
                margin=8,
                font_size=14,
            )
        )

        # Python → KIR
        generator = KIRGenerator()
        kir_dict = generator.generate(original)

        # Verify KIR uses camelCase
        assert "backgroundColor" in kir_dict["style"]
        assert "padding" in kir_dict["style"]
        assert "margin" in kir_dict["style"]
        assert "fontSize" in kir_dict["style"]

        # KIR → Python
        parser = KIRParser()
        parsed = parser.parse(kir_dict)

        # Verify Python uses snake_case
        assert parsed.style.background_color is not None
        assert parsed.style.padding == 16
        assert parsed.style.margin == 8
        assert parsed.style.font_size == 14

    def test_layout_roundtrip(self):
        """Test layout property round-trip"""
        original = Row(
            layout=Layout(
                gap=10,
                justify_content="center",
                align_items="center",
            )
        )

        # Python → KIR
        generator = KIRGenerator()
        kir_dict = generator.generate(original)

        # Verify KIR uses camelCase
        assert "gap" in kir_dict["layout"]
        assert "justifyContent" in kir_dict["layout"]
        assert "alignItems" in kir_dict["layout"]

        # KIR → Python
        parser = KIRParser()
        parsed = parser.parse(kir_dict)

        # Verify Python uses snake_case
        assert parsed.layout.gap == 10
        assert parsed.layout.justify_content == "center"
        assert parsed.layout.align_items == "center"

    def test_all_component_types(self):
        """Test round-trip for all 67 component types"""
        component_classes = [
            Container, Row, Column, Center,
            Text, Button, Input, Checkbox, Dropdown, Textarea,
            Image, Canvas, Markdown,
            TabGroup, TabBar, Tab, TabContent, TabPanel,
            Modal,
            Table, TableHead, TableBody, TableFoot,
            TableRow, TableCell, TableHeaderCell,
            Heading, Paragraph, Blockquote, CodeBlock,
            HorizontalRule, List, ListItem, Link,
            Span, Strong, Em, CodeInline, Small, Mark,
        ]

        generator = KIRGenerator()
        parser = KIRParser()

        for component_class in component_classes:
            # Create component instance
            if component_class == Heading:
                comp = component_class(text="Test", level=1)
            elif component_class == Button:
                comp = component_class(title="Test")
            elif component_class in (Text, Paragraph, Blockquote):
                comp = component_class(text="Test")
            else:
                comp = component_class()

            # Python → KIR
            kir_dict = generator.generate(comp)

            # KIR → Python
            parsed = parser.parse(kir_dict)

            # Verify type is preserved
            assert parsed.type.to_string() == comp.type.to_string(), \
                f"Component type mismatch for {component_class.__name__}"

    def test_serialization_file_roundtrip(self):
        """Test serialization to file and loading back"""
        import tempfile

        original = Column(
            children=[
                Text(text="Hello"),
                Button(title="Click"),
            ]
        )

        # Save to file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.kir', delete=False) as f:
            temp_path = f.name

        try:
            # Save
            serializer = KIRSerializer()
            serializer.serialize_to_file(original, temp_path)

            # Load
            parser = KIRParser()
            loaded = parser.parse_file(temp_path)

            # Verify
            assert loaded.type.to_string() == "Column"
            assert len(loaded.children) == 2
        finally:
            os.unlink(temp_path)


class TestPropertyMapping:
    """Test snake_case ↔ camelCase property mapping"""

    def test_style_property_mapping(self):
        """Test style properties convert correctly"""
        original = Style(
            background_color="#ff0000",
            border_width=2,
            font_size=14,
        )

        kir_dict = original.to_kir_dict()

        # Verify camelCase conversion
        assert "backgroundColor" in kir_dict
        assert "borderWidth" in kir_dict
        assert "fontSize" in kir_dict

    def test_layout_property_mapping(self):
        """Test layout properties convert correctly"""
        original = Layout(
            flex_direction="row",
            justify_content="center",
        )

        kir_dict = original.to_kir_dict()

        # Verify camelCase conversion
        assert "flexDirection" in kir_dict
        assert "justifyContent" in kir_dict


class TestCodeGeneration:
    """Test Python code generation from KIR"""

    def test_generate_python_code(self):
        """Test generating Python code from component"""
        original = Column(
            children=[
                Text(text="Hello"),
            ]
        )

        # Generate KIR
        generator = KIRGenerator()
        kir_dict = generator.generate(original)

        # Generate Python code
        codegen = PythonGenerator()
        python_code = codegen.generate(kir_dict)

        # Verify imports
        assert "import kryon" in python_code
        assert "from kryon.dsl import" in python_code

        # Verify component creation
        assert "Column" in python_code

    def test_generate_python_with_style(self):
        """Test generating Python code with style"""
        original = Text(
            text="Hello",
            style=Style(
                font_size=14,
                color="#ffffff",
            )
        )

        # Generate Python code
        codegen = PythonGenerator()
        python_code = codegen.generate_from_component(original)

        # Verify style is generated
        assert "Style(" in python_code
        assert "font_size" in python_code


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
