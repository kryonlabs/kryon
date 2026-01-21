"""
Python DSL Components for Kryon UI Framework
Provides Pythonic classes for all 67 component types
"""

from dataclasses import dataclass, field
from typing import Optional, List, Dict, Any, Union
from ..core.types import ComponentType, Style, Layout, Color, Dimension

# ============================================================================
# Base Component Class
# ============================================================================

@dataclass
class Component:
    """
    Base component class for all Kryon components

    Attributes:
        type: Component type (ComponentType enum or string)
        id: Optional component ID
        properties: Component-specific properties
        style: Style properties
        layout: Layout properties
        children: Child components
        events: Event handlers
    """
    type: Union[ComponentType, str]
    id: Optional[int] = None
    properties: Dict[str, Any] = field(default_factory=dict)
    style: Optional[Style] = None
    layout: Optional[Layout] = None
    children: List['Component'] = field(default_factory=list)
    events: List[Dict[str, str]] = field(default_factory=list)

    def __post_init__(self):
        # Convert string type to ComponentType if needed
        if isinstance(self.type, str):
            self.type = ComponentType.from_string(self.type)

    def add_child(self, child: 'Component'):
        """Add a child component"""
        self.children.append(child)
        return self

    def add_children(self, *children: 'Component'):
        """Add multiple child components"""
        self.children.extend(children)
        return self

    def on(self, event_type: str, handler: str):
        """Add an event handler"""
        self.events.append({"type": event_type, "handler": handler})
        return self

    def to_kir(self) -> Dict[str, Any]:
        """
        Convert component to KIR dictionary format

        Returns:
            Dictionary in KIR JSON format
        """
        result: Dict[str, Any] = {
            "type": self.type.to_string(),
            "id": self.id,
        }

        # Add properties
        if self.properties:
            result["properties"] = self._convert_properties_to_camel_case(self.properties)

        # Add style
        if self.style:
            result["style"] = self.style.to_kir_dict()

        # Add layout
        if self.layout:
            result["layout"] = self.layout.to_kir_dict()

        # Add children
        if self.children:
            result["children"] = [child.to_kir() for child in self.children]

        # Add events
        if self.events:
            result["events"] = self.events

        return result

    @staticmethod
    def _convert_properties_to_camel_case(props: Dict[str, Any]) -> Dict[str, Any]:
        """Convert snake_case property names to camelCase"""
        result = {}
        for key, value in props.items():
            # Handle common property conversions
            camel_key = key
            if key == "text_content":
                camel_key = "textContent"
            elif key == "custom_data":
                camel_key = "customData"
            elif key == "source_module":
                camel_key = "sourceModule"
            elif key == "export_name":
                camel_key = "exportName"
            elif key == "module_ref":
                camel_key = "moduleRef"
            elif key == "component_ref":
                camel_key = "componentRef"
            elif key == "component_props":
                camel_key = "componentProps"
            elif "_" in key:
                # Generic snake_case to camelCase conversion
                parts = key.split("_")
                camel_key = parts[0] + "".join(p.capitalize() for p in parts[1:])
            result[camel_key] = value
        return result


# ============================================================================
# Layout Components
# ============================================================================

class Container(Component):
    """Generic container component"""

    def __init__(
        self,
        **kwargs
    ):
        super().__init__(type=ComponentType.CONTAINER, **kwargs)


class Row(Component):
    """Horizontal layout container (flex-direction: row)"""

    def __init__(
        self,
        gap: Optional[float] = None,
        justify_content: Optional[str] = None,
        align_items: Optional[str] = None,
        **kwargs
    ):
        layout = Layout(
            flex_direction="row",
            gap=gap,
            justify_content=justify_content,
            align_items=align_items,
        )
        if "layout" in kwargs:
            # Merge with provided layout
            existing_layout = kwargs.pop("layout")
            for field in existing_layout.__dataclass_fields__:
                if getattr(existing_layout, field) is not None:
                    setattr(layout, field, getattr(existing_layout, field))
        kwargs["layout"] = layout
        super().__init__(type=ComponentType.ROW, **kwargs)


class Column(Component):
    """Vertical layout container (flex-direction: column)"""

    def __init__(
        self,
        gap: Optional[float] = None,
        justify_content: Optional[str] = None,
        align_items: Optional[str] = None,
        **kwargs
    ):
        layout = Layout(
            flex_direction="column",
            gap=gap,
            justify_content=justify_content,
            align_items=align_items,
        )
        if "layout" in kwargs:
            # Merge with provided layout
            existing_layout = kwargs.pop("layout")
            for field_name in existing_layout.__dataclass_fields__:
                if getattr(existing_layout, field_name) is not None:
                    setattr(layout, field_name, getattr(existing_layout, field_name))
        kwargs["layout"] = layout
        super().__init__(type=ComponentType.COLUMN, **kwargs)


class Center(Component):
    """Center-aligned container"""

    def __init__(self, **kwargs):
        layout = Layout(
            justify_content="center",
            align_items="center",
        )
        if "layout" in kwargs:
            existing_layout = kwargs.pop("layout")
            for field_name in existing_layout.__dataclass_fields__:
                if getattr(existing_layout, field_name) is not None:
                    setattr(layout, field_name, getattr(existing_layout, field_name))
        kwargs["layout"] = layout
        super().__init__(type=ComponentType.CENTER, **kwargs)


# ============================================================================
# Basic UI Components
# ============================================================================

class Text(Component):
    """Text display component"""

    def __init__(
        self,
        text: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["textContent"] = text
        super().__init__(type=ComponentType.TEXT, properties=properties, **kwargs)


class Button(Component):
    """Clickable button component"""

    def __init__(
        self,
        title: str = "",
        on_click: Optional[str] = None,
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["title"] = title
        super().__init__(type=ComponentType.BUTTON, properties=properties, **kwargs)
        if on_click:
            self.on("click", on_click)


class Input(Component):
    """Text input field"""

    def __init__(
        self,
        placeholder: str = "",
        value: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        if placeholder:
            properties["placeholder"] = placeholder
        if value:
            properties["value"] = value
        super().__init__(type=ComponentType.INPUT, properties=properties, **kwargs)


class Checkbox(Component):
    """Checkbox input component"""

    def __init__(
        self,
        checked: bool = False,
        label: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["checked"] = checked
        if label:
            properties["label"] = label
        super().__init__(type=ComponentType.CHECKBOX, properties=properties, **kwargs)


class Dropdown(Component):
    """Dropdown selection component"""

    def __init__(
        self,
        options: Optional[List[str]] = None,
        selected_index: int = 0,
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        if options:
            properties["options"] = options
        properties["selectedIndex"] = selected_index
        super().__init__(type=ComponentType.DROPDOWN, properties=properties, **kwargs)


class Textarea(Component):
    """Multi-line text input"""

    def __init__(
        self,
        placeholder: str = "",
        value: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        if placeholder:
            properties["placeholder"] = placeholder
        if value:
            properties["value"] = value
        super().__init__(type=ComponentType.TEXTAREA, properties=properties, **kwargs)


# ============================================================================
# Display Components
# ============================================================================

class Image(Component):
    """Image display component"""

    def __init__(
        self,
        src: str = "",
        width: Optional[Union[str, int, float]] = None,
        height: Optional[Union[str, int, float]] = None,
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["src"] = src
        if width:
            properties.setdefault("width", width)
        if height:
            properties.setdefault("height", height)
        super().__init__(type=ComponentType.IMAGE, properties=properties, **kwargs)


class Canvas(Component):
    """Canvas drawing component"""

    def __init__(
        self,
        width: Union[str, int, float] = 300,
        height: Union[str, int, float] = 150,
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["width"] = width
        properties["height"] = height
        super().__init__(type=ComponentType.CANVAS, properties=properties, **kwargs)


class NativeCanvas(Component):
    """Native canvas component (platform-specific)"""

    def __init__(
        self,
        width: Union[str, int, float] = 300,
        height: Union[str, int, float] = 150,
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["width"] = width
        properties["height"] = height
        super().__init__(type=ComponentType.NATIVE_CANVAS, properties=properties, **kwargs)


class Markdown(Component):
    """Markdown rendering component"""

    def __init__(
        self,
        content: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["content"] = content
        super().__init__(type=ComponentType.MARKDOWN, properties=properties, **kwargs)


class Sprite(Component):
    """Sprite component for 2D graphics"""

    def __init__(
        self,
        src: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["src"] = src
        super().__init__(type=ComponentType.SPRITE, properties=properties, **kwargs)


# ============================================================================
# Tab Components
# ============================================================================

class TabGroup(Component):
    """Tab group container"""

    def __init__(
        self,
        selected_index: int = 0,
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["selectedIndex"] = selected_index
        super().__init__(type=ComponentType.TAB_GROUP, properties=properties, **kwargs)


class TabBar(Component):
    """Tab bar container"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.TAB_BAR, **kwargs)


class Tab(Component):
    """Individual tab"""

    def __init__(
        self,
        title: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["title"] = title
        super().__init__(type=ComponentType.TAB, properties=properties, **kwargs)


class TabContent(Component):
    """Tab content container"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.TAB_CONTENT, **kwargs)


class TabPanel(Component):
    """Tab panel (grouped tab content)"""

    def __init__(
        self,
        title: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["title"] = title
        super().__init__(type=ComponentType.TAB_PANEL, properties=properties, **kwargs)


# ============================================================================
# Modal Component
# ============================================================================

class Modal(Component):
    """Modal overlay component"""

    def __init__(
        self,
        is_open: bool = False,
        title: Optional[str] = None,
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["isOpen"] = is_open
        if title:
            properties["title"] = title
        super().__init__(type=ComponentType.MODAL, properties=properties, **kwargs)


# ============================================================================
# Table Components
# ============================================================================

class Table(Component):
    """Table container"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.TABLE, **kwargs)


class TableHead(Component):
    """Table header section"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.TABLE_HEAD, **kwargs)


class TableBody(Component):
    """Table body section"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.TABLE_BODY, **kwargs)


class TableFoot(Component):
    """Table footer section"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.TABLE_FOOT, **kwargs)


class TableRow(Component):
    """Table row"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.TABLE_ROW, **kwargs)


class TableCell(Component):
    """Table cell"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.TABLE_CELL, **kwargs)


class TableHeaderCell(Component):
    """Table header cell"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.TABLE_HEADER_CELL, **kwargs)


# ============================================================================
# Markdown Document Components
# ============================================================================

class Heading(Component):
    """Heading component (H1-H6)"""

    def __init__(
        self,
        text: str = "",
        level: int = 1,
        **kwargs
    ):
        if not 1 <= level <= 6:
            raise ValueError("Heading level must be between 1 and 6")
        properties = kwargs.pop("properties", {})
        properties["text"] = text
        properties["level"] = level
        super().__init__(type=ComponentType.HEADING, properties=properties, **kwargs)


class Paragraph(Component):
    """Paragraph component"""

    def __init__(
        self,
        text: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["textContent"] = text
        super().__init__(type=ComponentType.PARAGRAPH, properties=properties, **kwargs)


class Blockquote(Component):
    """Blockquote component"""

    def __init__(
        self,
        text: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["textContent"] = text
        super().__init__(type=ComponentType.BLOCKQUOTE, properties=properties, **kwargs)


class CodeBlock(Component):
    """Code block component"""

    def __init__(
        self,
        code: str = "",
        language: Optional[str] = None,
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["code"] = code
        if language:
            properties["language"] = language
        super().__init__(type=ComponentType.CODE_BLOCK, properties=properties, **kwargs)


class HorizontalRule(Component):
    """Horizontal rule/thematic break"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.HORIZONTAL_RULE, **kwargs)


class List(Component):
    """List component (ordered or unordered)"""

    def __init__(
        self,
        ordered: bool = False,
        start: int = 1,
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["ordered"] = ordered
        properties["start"] = start
        super().__init__(type=ComponentType.LIST, properties=properties, **kwargs)


class ListItem(Component):
    """List item component"""

    def __init__(
        self,
        text: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        if text:
            properties["textContent"] = text
        super().__init__(type=ComponentType.LIST_ITEM, properties=properties, **kwargs)


class Link(Component):
    """Hyperlink component"""

    def __init__(
        self,
        text: str = "",
        url: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["textContent"] = text
        properties["url"] = url
        super().__init__(type=ComponentType.LINK, properties=properties, **kwargs)


# ============================================================================
# Inline Markdown Components
# ============================================================================

class Span(Component):
    """Inline span container"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.SPAN, **kwargs)


class Strong(Component):
    """Bold/strong text"""

    def __init__(self, text: str = "", **kwargs):
        properties = kwargs.pop("properties", {})
        if text:
            properties["textContent"] = text
        super().__init__(type=ComponentType.STRONG, properties=properties, **kwargs)


class Em(Component):
    """Emphasized/italic text"""

    def __init__(self, text: str = "", **kwargs):
        properties = kwargs.pop("properties", {})
        if text:
            properties["textContent"] = text
        super().__init__(type=ComponentType.EM, properties=properties, **kwargs)


class CodeInline(Component):
    """Inline code"""

    def __init__(self, text: str = "", **kwargs):
        properties = kwargs.pop("properties", {})
        properties["textContent"] = text
        super().__init__(type=ComponentType.CODE_INLINE, properties=properties, **kwargs)


class Small(Component):
    """Small text"""

    def __init__(self, text: str = "", **kwargs):
        properties = kwargs.pop("properties", {})
        if text:
            properties["textContent"] = text
        super().__init__(type=ComponentType.SMALL, properties=properties, **kwargs)


class Mark(Component):
    """Highlighted/marked text"""

    def __init__(self, text: str = "", **kwargs):
        properties = kwargs.pop("properties", {})
        if text:
            properties["textContent"] = text
        super().__init__(type=ComponentType.MARK, properties=properties, **kwargs)


# ============================================================================
# Special Components
# ============================================================================

class CustomComponent(Component):
    """Custom user-defined component"""

    def __init__(
        self,
        name: str = "Custom",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["componentName"] = name
        super().__init__(type=ComponentType.CUSTOM, properties=properties, **kwargs)


class StaticBlock(Component):
    """Static block (compile-time code execution)"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.STATIC_BLOCK, **kwargs)


class ForLoop(Component):
    """For loop template (compile-time iteration)"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.FOR_LOOP, **kwargs)


class ForEach(Component):
    """ForEach (runtime dynamic list rendering)"""

    def __init__(
        self,
        items: str = "",
        item_name: str = "item",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        if items:
            properties["items"] = items
        if item_name:
            properties["itemName"] = item_name
        super().__init__(type=ComponentType.FOR_EACH, properties=properties, **kwargs)


class VarDecl(Component):
    """Variable declaration (const/let/var)"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.VAR_DECL, **kwargs)


class Placeholder(Component):
    """Template placeholder ({{name}})"""

    def __init__(
        self,
        name: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["name"] = name
        super().__init__(type=ComponentType.PLACEHOLDER, properties=properties, **kwargs)


# ============================================================================
# Flowchart Components (for Mermaid support)
# ============================================================================

class Flowchart(Component):
    """Flowchart container"""

    def __init__(self, **kwargs):
        super().__init__(type=ComponentType.FLOWCHART, **kwargs)


class FlowchartNode(Component):
    """Individual flowchart node"""

    def __init__(
        self,
        id: str = "",
        label: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["id"] = id
        properties["label"] = label
        super().__init__(type=ComponentType.FLOWCHART_NODE, properties=properties, **kwargs)


class FlowchartEdge(Component):
    """Connection between flowchart nodes"""

    def __init__(
        self,
        from_node: str = "",
        to_node: str = "",
        label: Optional[str] = None,
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["from"] = from_node
        properties["to"] = to_node
        if label:
            properties["label"] = label
        super().__init__(type=ComponentType.FLOWCHART_EDGE, properties=properties, **kwargs)


class FlowchartSubgraph(Component):
    """Grouped nodes (subgraph)"""

    def __init__(
        self,
        id: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["id"] = id
        super().__init__(type=ComponentType.FLOWCHART_SUBGRAPH, properties=properties, **kwargs)


class FlowchartLabel(Component):
    """Text label for nodes/edges"""

    def __init__(
        self,
        text: str = "",
        **kwargs
    ):
        properties = kwargs.pop("properties", {})
        properties["text"] = text
        super().__init__(type=ComponentType.FLOWCHART_LABEL, properties=properties, **kwargs)


# ============================================================================
# Export all component classes
# ============================================================================

__all__ = [
    # Base
    "Component",

    # Layout
    "Container", "Row", "Column", "Center",

    # Basic UI
    "Text", "Button", "Input", "Checkbox", "Dropdown", "Textarea",

    # Display
    "Image", "Canvas", "NativeCanvas", "Markdown", "Sprite",

    # Tabs
    "TabGroup", "TabBar", "Tab", "TabContent", "TabPanel",

    # Modal
    "Modal",

    # Tables
    "Table", "TableHead", "TableBody", "TableFoot",
    "TableRow", "TableCell", "TableHeaderCell",

    # Markdown
    "Heading", "Paragraph", "Blockquote", "CodeBlock", "HorizontalRule",
    "List", "ListItem", "Link",

    # Inline
    "Span", "Strong", "Em", "CodeInline", "Small", "Mark",

    # Special
    "CustomComponent", "StaticBlock", "ForLoop", "ForEach", "VarDecl", "Placeholder",

    # Flowchart
    "Flowchart", "FlowchartNode", "FlowchartEdge", "FlowchartSubgraph", "FlowchartLabel",
]
