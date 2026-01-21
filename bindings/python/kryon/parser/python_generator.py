"""
Python Code Generator - Generate Python DSL code from KIR
"""

from typing import Dict, Any, List, Set
from .kir_parser import KIRParser


class PythonGenerator:
    """
    Generate Python DSL code from KIR

    Converts KIR JSON to valid Python DSL code that can be executed
    to recreate the component tree.
    """

    def __init__(self, indent: int = 4):
        """
        Initialize generator

        Args:
            indent: Indentation spaces
        """
        self.indent = indent
        self.indent_level = 0
        self.parser = KIRParser()

    def generate(self, kir_data: Dict[str, Any]) -> str:
        """
        Generate Python DSL code from KIR

        Args:
            kir_data: KIR dictionary

        Returns:
            Python code string
        """
        # Parse KIR to component
        component = self.parser.parse(kir_data)

        # Generate imports
        imports = self._generate_imports()

        # Generate component code
        component_code = self._generate_component_code(component, "app")

        return imports + "\n" + component_code

    def generate_from_component(self, component) -> str:
        """
        Generate Python DSL code from a component

        Args:
            component: Component instance

        Returns:
            Python code string
        """
        imports = self._generate_imports()
        component_code = self._generate_component_code(component, "app")
        return imports + "\n" + component_code

    def _generate_imports(self) -> str:
        """Generate import statements"""
        return """import kryon
from kryon.dsl import (
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
)
from kryon.dsl import Style, Layout, Color
"""

    def _generate_component_code(self, component, var_name: str = "component") -> str:
        """Generate code for a component and its children"""
        lines = []

        # Get component class name
        class_name = component.type.to_string()

        # Generate arguments
        args = []
        kwargs = {}

        # Add ID
        if component.id is not None:
            kwargs["id"] = str(component.id)

        # Add properties
        for key, value in component.properties.items():
            kwargs[key] = repr(value)

        # Add style
        if component.style:
            style_code = self._generate_style_code(component.style)
            kwargs["style"] = style_code

        # Add layout
        if component.layout:
            layout_code = self._generate_layout_code(component.layout)
            kwargs["layout"] = layout_code

        # Build constructor call
        if kwargs:
            arg_str = ", ".join(f"{k}={v}" for k, v in kwargs.items())
            lines.append(f"{var_name} = {class_name}({arg_str})")
        else:
            lines.append(f"{var_name} = {class_name}()")

        # Add children
        if component.children:
            self.indent_level += 1
            indent = " " * (self.indent * self.indent_level)

            for i, child in enumerate(component.children):
                child_var = f"{var_name}_child_{i}"
                child_code = self._generate_component_code(child, child_var)
                lines.append("")
                lines.append(child_code)
                lines.append(f"{indent}{var_name}.add_child({child_var})")

            self.indent_level -= 1

        return "\n".join(lines)

    def _generate_style_code(self, style) -> str:
        """Generate code for Style object"""
        if style is None:
            return "None"

        args = []
        for key, value in style.__dict__.items():
            if value is not None:
                args.append(f"{key}={repr(value)}")

        if args:
            return f"Style({', '.join(args)})"
        return "Style()"

    def _generate_layout_code(self, layout) -> str:
        """Generate code for Layout object"""
        if layout is None:
            return "None"

        args = []
        for key, value in layout.__dict__.items():
            if value is not None:
                args.append(f"{key}={repr(value)}")

        if args:
            return f"Layout({', '.join(args)})"
        return "Layout()"


def generate_python(kir_data: Dict[str, Any]) -> str:
    """
    Convenience function to generate Python code from KIR

    Args:
        kir_data: KIR dictionary

    Returns:
        Python code string
    """
    generator = PythonGenerator()
    return generator.generate(kir_data)
