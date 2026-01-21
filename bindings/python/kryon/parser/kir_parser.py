"""
KIR Parser - Parse KIR (JSON) into Python DSL components
"""

import json
from pathlib import Path
from typing import Dict, Any, List, Optional
from ..dsl.components import (
    Component, Container, Row, Column, Center,
    Text, Button, Input, Checkbox, Dropdown, Textarea,
    Image, Canvas, NativeCanvas, Markdown, Sprite,
    TabGroup, TabBar, Tab, TabContent, TabPanel,
    Modal,
    Table, TableHead, TableBody, TableFoot, TableRow, TableCell, TableHeaderCell,
    Heading, Paragraph, Blockquote, CodeBlock, HorizontalRule,
    List, ListItem, Link,
    Span, Strong, Em, CodeInline, Small, Mark,
    CustomComponent, StaticBlock, ForLoop, ForEach, VarDecl, Placeholder,
    Flowchart, FlowchartNode, FlowchartEdge, FlowchartSubgraph, FlowchartLabel,
)
from ..core.types import ComponentType, Style, Layout, Color


class KIRParser:
    """
    Parse KIR (JSON) into Python DSL components

    Converts KIR JSON format to Python component trees for manipulation
    or code generation.
    """

    # Component class mapping
    COMPONENT_CLASSES = {
        "Container": Container,
        "Row": Row,
        "Column": Column,
        "Center": Center,
        "Text": Text,
        "Button": Button,
        "Input": Input,
        "Checkbox": Checkbox,
        "Dropdown": Dropdown,
        "Textarea": Textarea,
        "Image": Image,
        "Canvas": Canvas,
        "NativeCanvas": NativeCanvas,
        "Markdown": Markdown,
        "Sprite": Sprite,
        "TabGroup": TabGroup,
        "TabBar": TabBar,
        "Tab": Tab,
        "TabContent": TabContent,
        "TabPanel": TabPanel,
        "Modal": Modal,
        "Table": Table,
        "TableHead": TableHead,
        "TableBody": TableBody,
        "TableFoot": TableFoot,
        "TableRow": TableRow,
        "TableCell": TableCell,
        "TableHeaderCell": TableHeaderCell,
        "Heading": Heading,
        "Paragraph": Paragraph,
        "Blockquote": Blockquote,
        "CodeBlock": CodeBlock,
        "HorizontalRule": HorizontalRule,
        "List": List,
        "ListItem": ListItem,
        "Link": Link,
        "Span": Span,
        "Strong": Strong,
        "Em": Em,
        "CodeInline": CodeInline,
        "Small": Small,
        "Mark": Mark,
        "Custom": CustomComponent,
        "StaticBlock": StaticBlock,
        "ForLoop": ForLoop,
        "ForEach": ForEach,
        "VarDecl": VarDecl,
        "Placeholder": Placeholder,
        "Flowchart": Flowchart,
        "FlowchartNode": FlowchartNode,
        "FlowchartEdge": FlowchartEdge,
        "FlowchartSubgraph": FlowchartSubgraph,
        "FlowchartLabel": FlowchartLabel,
    }

    def parse(self, kir_dict: Dict[str, Any]) -> Component:
        """
        Parse KIR dictionary into a component

        Args:
            kir_dict: KIR dictionary (either root or component dict)

        Returns:
            Component instance
        """
        # Check if this is a root KIR document
        if "root" in kir_dict:
            return self._parse_component(kir_dict["root"])
        return self._parse_component(kir_dict)

    def parse_file(self, filepath: str) -> Component:
        """
        Parse a KIR file into a component

        Args:
            filepath: Path to .kir file

        Returns:
            Component instance
        """
        with open(filepath, 'r', encoding='utf-8') as f:
            kir_dict = json.load(f)

        return self.parse(kir_dict)

    def parse_json(self, json_str: str) -> Component:
        """
        Parse KIR JSON string into a component

        Args:
            json_str: KIR JSON string

        Returns:
            Component instance
        """
        kir_dict = json.loads(json_str)
        return self.parse(kir_dict)

    def _parse_component(self, comp_dict: Dict[str, Any]) -> Component:
        """
        Parse a component dictionary

        Args:
            comp_dict: Component dictionary

        Returns:
            Component instance
        """
        # Get component type
        type_str = comp_dict.get("type", "Container")
        component_class = self.COMPONENT_CLASSES.get(type_str, Component)

        # Convert camelCase properties to snake_case
        properties = self._convert_properties_to_snake_case(
            comp_dict.get("properties", {})
        )

        # Parse style
        style = self._parse_style(comp_dict.get("style", {}))

        # Parse layout
        layout = self._parse_layout(comp_dict.get("layout", {}))

        # Parse children
        children = [
            self._parse_component(child_dict)
            for child_dict in comp_dict.get("children", [])
        ]

        # Get events
        events = comp_dict.get("events", [])

        # Create component instance
        if component_class in (Text, Heading, Paragraph, Blockquote, CodeBlock,
                               ListItem, Link, Strong, Em, CodeInline, Small, Mark):
            # Components with text content
            component = component_class(
                id=comp_dict.get("id"),
                properties=properties,
                style=style,
                layout=layout,
                children=children,
            )
        elif component_class == Button:
            # Button component
            component = component_class(
                title=properties.pop("title", comp_dict.get("properties", {}).get("title", "")),
                id=comp_dict.get("id"),
                style=style,
                layout=layout,
                children=children,
            )
        elif component_class in (Input, Textarea):
            component = component_class(
                value=properties.pop("value", ""),
                placeholder=properties.pop("placeholder", ""),
                id=comp_dict.get("id"),
                style=style,
                layout=layout,
                children=children,
            )
        elif component_class == Heading:
            level = properties.pop("level", 1)
            text = properties.pop("text", "")
            component = component_class(
                text=text,
                level=level,
                id=comp_dict.get("id"),
                style=style,
                layout=layout,
                children=children,
            )
        else:
            # Generic component creation
            if component_class == Component:
                # Base Component class requires type argument
                component = component_class(
                    type=type_str,
                    id=comp_dict.get("id"),
                    properties=properties,
                    style=style,
                    layout=layout,
                    children=children,
                )
            else:
                # Subclasses set their own type
                component = component_class(
                    id=comp_dict.get("id"),
                    properties=properties,
                    style=style,
                    layout=layout,
                    children=children,
                )

        # Add events
        for event in events:
            component.on(event["type"], event.get("handler", ""))

        return component

    def _convert_properties_to_snake_case(self, props: Dict[str, Any]) -> Dict[str, Any]:
        """Convert camelCase property names to snake_case"""
        result = {}

        # Known property name mappings
        mappings = {
            "textContent": "text_content",
            "customData": "custom_data",
            "sourceModule": "source_module",
            "exportName": "export_name",
            "moduleRef": "module_ref",
            "componentRef": "component_ref",
            "componentProps": "component_props",
            "selectedIndex": "selected_index",
            "isOpen": "is_open",
        }

        for key, value in props.items():
            snake_key = mappings.get(key, None)
            if snake_key is None:
                # Generic camelCase to snake_case
                import re
                snake_key = re.sub('([A-Z]+)', r'_\1', key).lower().lstrip('_')

            result[snake_key] = value

        return result

    def _parse_style(self, style_dict: Dict[str, Any]) -> Optional[Style]:
        """Parse style dictionary into Style object"""
        if not style_dict:
            return None

        # Convert camelCase to snake_case
        snake_dict = self._convert_camel_to_snake(style_dict)

        # Parse color values
        for key in ["background_color", "color", "border_color"]:
            if key in snake_dict and isinstance(snake_dict[key], str):
                snake_dict[key] = Color.from_hex(snake_dict[key])

        return Style(**snake_dict)

    def _parse_layout(self, layout_dict: Dict[str, Any]) -> Optional[Layout]:
        """Parse layout dictionary into Layout object"""
        if not layout_dict:
            return None

        # Convert camelCase to snake_case
        snake_dict = self._convert_camel_to_snake(layout_dict)

        return Layout(**snake_dict)

    @staticmethod
    def _convert_camel_to_snake(d: Dict[str, Any]) -> Dict[str, Any]:
        """Convert all keys in dict from camelCase to snake_case"""
        import re
        result = {}
        for key, value in d.items():
            snake_key = re.sub('([A-Z]+)', r'_\1', key).lower().lstrip('_')
            result[snake_key] = value
        return result


def from_kir(kir_data: Dict[str, Any]) -> Component:
    """
    Convenience function to parse KIR dictionary into component

    Args:
        kir_data: KIR dictionary

    Returns:
        Component instance
    """
    parser = KIRParser()
    return parser.parse(kir_data)


def load_kir(filepath: str) -> Component:
    """
    Convenience function to load KIR file into component

    Args:
        filepath: Path to .kir file

    Returns:
        Component instance
    """
    parser = KIRParser()
    return parser.parse_file(filepath)
