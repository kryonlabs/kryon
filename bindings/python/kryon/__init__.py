"""
Kryon Python Bindings
A declarative, cross-platform UI framework

Example usage:
    import kryon

    app = kryon.Column(
        width="100%",
        height="100%",
        children=[
            kryon.Text(text="Hello, World!"),
            kryon.Button(title="Click Me")
        ]
    )

    # Save as KIR
    kryon.save_kir(app, "app.kir")

    # Load from KIR
    loaded = kryon.load_kir("app.kir")

    # Generate Python code
    code = kryon.generate_python(loaded.to_kir())
"""

# Export main DSL components
from kryon.dsl.components import (
    # Layout components
    Container,
    Row,
    Column,
    Center,

    # Basic UI components
    Text,
    Button,
    Input,
    Checkbox,
    Dropdown,
    Textarea,

    # Display components
    Image,
    Canvas,
    Markdown,
    Sprite,

    # Tab components
    TabGroup,
    TabBar,
    Tab,
    TabContent,
    TabPanel,

    # Modal
    Modal,

    # Table components
    Table,
    TableHead,
    TableBody,
    TableFoot,
    TableRow,
    TableCell,
    TableHeaderCell,

    # Markdown components
    Heading,
    Paragraph,
    Blockquote,
    CodeBlock,
    HorizontalRule,
    List,
    ListItem,
    Link,

    # Inline markdown components
    Span,
    Strong,
    Em,
    CodeInline,
    Small,
    Mark,

    # Flowchart components
    Flowchart,
    FlowchartNode,
    FlowchartEdge,
    FlowchartSubgraph,
    FlowchartLabel,

    # Base component
    Component,
)

# Export style and layout helpers
from kryon.dsl.styles import Style, Color
from kryon.dsl.layout import Layout

# Export codegen utilities
from kryon.codegen.kir_generator import KIRGenerator, to_kir
from kryon.codegen.serializer import KIRSerializer, save_kir

# Export utilities
from kryon.parser.kir_parser import KIRParser, from_kir, load_kir
from kryon.parser.python_generator import PythonGenerator, generate_python

# Export core types
from kryon.core.types import ComponentType, Dimension

__version__ = "0.1.0"
__all__ = [
    # Components
    "Container", "Row", "Column", "Center",
    "Text", "Button", "Input", "Checkbox", "Dropdown", "Textarea",
    "Image", "Canvas", "Markdown", "Sprite",
    "TabGroup", "TabBar", "Tab", "TabContent", "TabPanel",
    "Modal",
    "Table", "TableHead", "TableBody", "TableFoot", "TableRow", "TableCell", "TableHeaderCell",
    "Heading", "Paragraph", "Blockquote", "CodeBlock", "HorizontalRule", "List", "ListItem", "Link",
    "Span", "Strong", "Em", "CodeInline", "Small", "Mark",
    "Flowchart", "FlowchartNode", "FlowchartEdge", "FlowchartSubgraph", "FlowchartLabel",
    "Component",

    # Style and Layout
    "Style", "Color", "Layout",

    # Codegen
    "KIRGenerator", "to_kir",
    "KIRSerializer", "save_kir",

    # Parser
    "KIRParser", "from_kir", "load_kir",
    "PythonGenerator", "generate_python",

    # Types
    "ComponentType", "Dimension",
]

# Convenience functions - use existing functions
# (no need to redefine since they're already imported above)
