"""
Test Python DSL component creation
"""

import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import kryon
from kryon.dsl import (
    Container, Row, Column, Center,
    Text, Button, Input, Checkbox, Dropdown, Textarea,
    Image, Canvas, Markdown,
    Style, Layout, Color,
)


class TestBasicComponents:
    """Test basic component creation"""

    def test_text_component(self):
        """Test Text component"""
        text = Text(text="Hello, World!")
        assert text.type.to_string() == "Text"
        assert text.properties["textContent"] == "Hello, World!"

    def test_button_component(self):
        """Test Button component"""
        button = Button(title="Click Me")
        assert button.type.to_string() == "Button"
        assert button.properties["title"] == "Click Me"

    def test_input_component(self):
        """Test Input component"""
        input = Input(placeholder="Enter text...")
        assert input.type.to_string() == "Input"
        assert input.properties["placeholder"] == "Enter text..."


class TestLayoutComponents:
    """Test layout components"""

    def test_column_component(self):
        """Test Column component"""
        col = Column(
            gap=10,
            justify_content="center",
            children=[
                Text(text="Item 1"),
                Text(text="Item 2"),
            ]
        )
        assert col.type.to_string() == "Column"
        assert col.layout.gap == 10
        assert col.layout.justify_content == "center"
        assert len(col.children) == 2

    def test_row_component(self):
        """Test Row component"""
        row = Row(
            gap=20,
            children=[
                Text(text="Left"),
                Text(text="Right"),
            ]
        )
        assert row.type.to_string() == "Row"
        assert row.layout.gap == 20
        assert len(row.children) == 2


class TestStyle:
    """Test Style class"""

    def test_color_from_hex(self):
        """Test creating Color from hex string"""
        color = Color.from_hex("#ff0000")
        assert color.r == 255
        assert color.g == 0
        assert color.b == 0

    def test_style_creation(self):
        """Test Style creation"""
        style = Style(
            background_color="#ff0000",
            padding=16,
            font_size=14,
        )
        assert style.background_color.r == 255
        assert style.padding == 16
        assert style.font_size == 14

    def test_style_to_kir_dict(self):
        """Test Style to KIR dictionary conversion"""
        style = Style(
            background_color="#ff0000",
            padding=16,
        )
        kir_dict = style.to_kir_dict()

        assert "backgroundColor" in kir_dict
        assert "padding" in kir_dict


class TestLayout:
    """Test Layout class"""

    def test_layout_creation(self):
        """Test Layout creation"""
        layout = Layout(
            gap=10,
            justify_content="center",
            align_items="center",
        )
        assert layout.gap == 10
        assert layout.justify_content == "center"
        assert layout.align_items == "center"

    def test_layout_to_kir_dict(self):
        """Test Layout to KIR dictionary conversion"""
        layout = Layout(
            gap=10,
            justify_content="center",
        )
        kir_dict = layout.to_kir_dict()

        assert "gap" in kir_dict
        assert "justifyContent" in kir_dict


class TestComponentTree:
    """Test component tree building"""

    def test_add_child(self):
        """Test adding children"""
        col = Column()
        text = Text(text="Hello")
        col.add_child(text)

        assert len(col.children) == 1
        assert col.children[0] == text

    def test_add_children(self):
        """Test adding multiple children"""
        col = Column()
        col.add_children(
            Text(text="Item 1"),
            Text(text="Item 2"),
            Text(text="Item 3"),
        )

        assert len(col.children) == 3

    def test_nested_tree(self):
        """Test building nested component tree"""
        app = Column(
            children=[
                Row(
                    children=[
                        Text(text="Left"),
                        Text(text="Right"),
                    ]
                ),
                Button(title="Click"),
            ]
        )

        assert len(app.children) == 2
        assert app.children[0].type.to_string() == "Row"
        assert len(app.children[0].children) == 2


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
