"""
Type definitions for Kryon Python bindings
"""

from enum import IntEnum
from typing import Optional, Dict, Any, List, Union
from dataclasses import dataclass, field

# ============================================================================
# Component Type Enum (matches IRComponentType in ir_types.h)
# ============================================================================

class ComponentType(IntEnum):
    """All 67 component types in Kryon"""

    # Basic UI Components
    CONTAINER = 0
    TEXT = 1
    BUTTON = 2
    INPUT = 3
    CHECKBOX = 4
    DROPDOWN = 5
    TEXTAREA = 6

    # Layout Components
    ROW = 7
    COLUMN = 8
    CENTER = 9

    # Display Components
    IMAGE = 10
    CANVAS = 11
    NATIVE_CANVAS = 12
    MARKDOWN = 13
    SPRITE = 14

    # Tab Components
    TAB_GROUP = 15
    TAB_BAR = 16
    TAB = 17
    TAB_CONTENT = 18
    TAB_PANEL = 19

    # Modal/Overlay
    MODAL = 20

    # Table Components
    TABLE = 21
    TABLE_HEAD = 22
    TABLE_BODY = 23
    TABLE_FOOT = 24
    TABLE_ROW = 25
    TABLE_CELL = 26
    TABLE_HEADER_CELL = 27

    # Markdown Components
    HEADING = 28
    PARAGRAPH = 29
    BLOCKQUOTE = 30
    CODE_BLOCK = 31
    HORIZONTAL_RULE = 32
    LIST = 33
    LIST_ITEM = 34
    LINK = 35

    # Inline Markdown Components
    SPAN = 36
    STRONG = 37
    EM = 38
    CODE_INLINE = 39
    SMALL = 40
    MARK = 41

    # Custom and Special
    CUSTOM = 42
    STATIC_BLOCK = 43
    FOR_LOOP = 44
    FOR_EACH = 45
    VAR_DECL = 46
    PLACEHOLDER = 47

    # Flowchart Components
    FLOWCHART = 48
    FLOWCHART_NODE = 49
    FLOWCHART_EDGE = 50
    FLOWCHART_SUBGRAPH = 51
    FLOWCHART_LABEL = 52

    @classmethod
    def from_string(cls, name: str) -> 'ComponentType':
        """Convert string name to ComponentType (case-insensitive)"""
        name_map = {
            'container': cls.CONTAINER,
            'text': cls.TEXT,
            'button': cls.BUTTON,
            'input': cls.INPUT,
            'checkbox': cls.CHECKBOX,
            'dropdown': cls.DROPDOWN,
            'textarea': cls.TEXTAREA,
            'row': cls.ROW,
            'column': cls.COLUMN,
            'center': cls.CENTER,
            'image': cls.IMAGE,
            'canvas': cls.CANVAS,
            'native_canvas': cls.NATIVE_CANVAS,
            'markdown': cls.MARKDOWN,
            'sprite': cls.SPRITE,
            'tabgroup': cls.TAB_GROUP,
            'tabgroup': cls.TAB_GROUP,  # Common alternative
            'tab_bar': cls.TAB_BAR,
            'tab': cls.TAB,
            'tabcontent': cls.TAB_CONTENT,
            'tabpanel': cls.TAB_PANEL,
            'modal': cls.MODAL,
            'table': cls.TABLE,
            'tablehead': cls.TABLE_HEAD,
            'tablebody': cls.TABLE_BODY,
            'tablefoot': cls.TABLE_FOOT,
            'tablerow': cls.TABLE_ROW,
            'tablecell': cls.TABLE_CELL,
            'tableheadercell': cls.TABLE_HEADER_CELL,
            'heading': cls.HEADING,
            'paragraph': cls.PARAGRAPH,
            'blockquote': cls.BLOCKQUOTE,
            'codeblock': cls.CODE_BLOCK,
            'horizontalrule': cls.HORIZONTAL_RULE,
            'list': cls.LIST,
            'listitem': cls.LIST_ITEM,
            'link': cls.LINK,
            'span': cls.SPAN,
            'strong': cls.STRONG,
            'em': cls.EM,
            'codeinline': cls.CODE_INLINE,
            'small': cls.SMALL,
            'mark': cls.MARK,
            'custom': cls.CUSTOM,
            'staticblock': cls.STATIC_BLOCK,
            'forloop': cls.FOR_LOOP,
            'foreach': cls.FOR_EACH,
            'vardecl': cls.VAR_DECL,
            'placeholder': cls.PLACEHOLDER,
            'flowchart': cls.FLOWCHART,
            'flowchartnode': cls.FLOWCHART_NODE,
            'flowchartedge': cls.FLOWCHART_EDGE,
            'flowchartsubgraph': cls.FLOWCHART_SUBGRAPH,
            'flowchartlabel': cls.FLOWCHART_LABEL,
        }
        return name_map.get(name.lower().replace('_', ''), cls.CONTAINER)

    def to_string(self) -> str:
        """Convert ComponentType to string name (PascalCase)"""
        # Convert UPPER_CASE to PascalCase (e.g., TABLE_HEAD -> TableHead)
        return ''.join(word.capitalize() for word in self.name.split('_'))

# ============================================================================
# Dimension Types
# ============================================================================

@dataclass
class Dimension:
    """CSS dimension (pixels, percentage, or auto)"""
    value: Union[int, float, str]
    unit: str = "px"  # 'px', '%', 'auto', 'min', 'max'

    def __post_init__(self):
        if isinstance(self.value, str):
            # Parse strings like "100px", "50%", "auto"
            if self.value == "auto":
                self.unit = "auto"
                self.value = 0
            elif self.value.endswith("%"):
                self.unit = "%"
                self.value = float(self.value[:-1])
            elif self.value.endswith("px"):
                self.unit = "px"
                self.value = float(self.value[:-3])
            else:
                # Try to parse as number
                try:
                    self.value = float(self.value)
                except ValueError:
                    self.unit = "custom"

    def to_kir_dict(self) -> Dict[str, Any]:
        """Convert to KIR dictionary format"""
        if self.unit == "auto":
            return {"value": "auto"}
        elif self.unit == "%":
            # Use int if whole number, else float
            val = int(self.value) if self.value == int(self.value) else self.value
            return {"value": f"{val}%"}
        else:
            val = int(self.value) if self.value == int(self.value) else self.value
            return {"value": f"{val}px"}

# ============================================================================
# Style Types
# ============================================================================

@dataclass
class Color:
    """RGBA color"""
    r: int
    g: int
    b: int
    a: float = 1.0

    @classmethod
    def from_hex(cls, hex_str: str) -> 'Color':
        """Parse hex color string (#RGB or #RRGGBB or #RRGGBBAA)"""
        hex_str = hex_str.lstrip('#')
        if len(hex_str) == 3:
            # #RGB -> #RRGGBB
            hex_str = ''.join(c*2 for c in hex_str)
            return cls(
                r=int(hex_str[0:2], 16),
                g=int(hex_str[2:4], 16),
                b=int(hex_str[4:6], 16),
                a=1.0
            )
        elif len(hex_str) == 6:
            return cls(
                r=int(hex_str[0:2], 16),
                g=int(hex_str[2:4], 16),
                b=int(hex_str[4:6], 16),
                a=1.0
            )
        elif len(hex_str) == 8:
            return cls(
                r=int(hex_str[0:2], 16),
                g=int(hex_str[2:4], 16),
                b=int(hex_str[4:6], 16),
                a=int(hex_str[6:8], 16) / 255.0
            )
        raise ValueError(f"Invalid hex color: {hex_str}")

    def to_hex(self) -> str:
        """Convert to hex color string"""
        if self.a == 1.0:
            return f"#{self.r:02x}{self.g:02x}{self.b:02x}"
        else:
            return f"#{self.r:02x}{self.g:02x}{self.b:02x}{int(self.a * 255):02x}"

    def to_kir_dict(self) -> str:
        """Convert to KIR format (hex string or rgba())"""
        if self.a == 1.0:
            return self.to_hex()
        else:
            return f"rgba({self.r}, {self.g}, {self.b}, {self.a})"

@dataclass
class Style:
    """Component style properties (snake_case Python API)"""
    # Dimensions
    width: Optional[Union[str, int, float, Dimension]] = None
    height: Optional[Union[str, int, float, Dimension]] = None
    min_width: Optional[Union[str, int, float, Dimension]] = None
    max_width: Optional[Union[str, int, float, Dimension]] = None
    min_height: Optional[Union[str, int, float, Dimension]] = None
    max_height: Optional[Union[str, int, float, Dimension]] = None

    # Colors
    background_color: Optional[Union[str, Color]] = None
    color: Optional[Union[str, Color]] = None
    border_color: Optional[Union[str, Color]] = None

    # Border
    border_width: Optional[float] = None
    border_radius: Optional[float] = None

    # Spacing
    margin: Optional[float] = None
    margin_top: Optional[float] = None
    margin_right: Optional[float] = None
    margin_bottom: Optional[float] = None
    margin_left: Optional[float] = None

    padding: Optional[float] = None
    padding_top: Optional[float] = None
    padding_right: Optional[float] = None
    padding_bottom: Optional[float] = None
    padding_left: Optional[float] = None

    # Typography
    font_size: Optional[float] = None
    font_family: Optional[str] = None
    font_weight: Optional[str] = None  # 'normal', 'bold', etc.
    font_style: Optional[str] = None  # 'normal', 'italic'
    line_height: Optional[float] = None
    text_align: Optional[str] = None  # 'left', 'center', 'right', 'justify'

    # Display
    visible: Optional[bool] = None
    opacity: Optional[float] = None
    overflow: Optional[str] = None  # 'visible', 'hidden', 'scroll'

    # Flex
    flex_grow: Optional[float] = None
    flex_shrink: Optional[float] = None
    flex_basis: Optional[Union[str, int, float, Dimension]] = None

    # Position
    position: Optional[str] = None  # 'relative', 'absolute'
    x: Optional[float] = None
    y: Optional[float] = None

    def __post_init__(self):
        """Convert string colors to Color objects"""
        if isinstance(self.background_color, str):
            self.background_color = Color.from_hex(self.background_color)
        if isinstance(self.color, str):
            self.color = Color.from_hex(self.color)
        if isinstance(self.border_color, str):
            self.border_color = Color.from_hex(self.border_color)

    def to_kir_dict(self) -> Dict[str, Any]:
        """Convert to KIR format (camelCase)"""
        result = {}

        # Helper to normalize dimension
        def norm_dim(value):
            if isinstance(value, Dimension):
                return value.to_kir_dict()
            elif isinstance(value, str):
                return Dimension(value).to_kir_dict()
            elif value is not None:
                return {"value": f"{value}px"}
            return None

        # Helper to normalize color
        def norm_color(value):
            if isinstance(value, Color):
                return value.to_kir_dict()
            elif isinstance(value, str):
                return Color.from_hex(value).to_kir_dict()
            return None

        # Dimensions
        if self.width is not None:
            result["width"] = norm_dim(self.width)
        if self.height is not None:
            result["height"] = norm_dim(self.height)
        if self.min_width is not None:
            result["minWidth"] = norm_dim(self.min_width)
        if self.max_width is not None:
            result["maxWidth"] = norm_dim(self.max_width)
        if self.min_height is not None:
            result["minHeight"] = norm_dim(self.min_height)
        if self.max_height is not None:
            result["maxHeight"] = norm_dim(self.max_height)

        # Colors
        if self.background_color is not None:
            result["backgroundColor"] = norm_color(self.background_color)
        if self.color is not None:
            result["color"] = norm_color(self.color)
        if self.border_color is not None:
            result["borderColor"] = norm_color(self.border_color)

        # Border
        if self.border_width is not None:
            result["borderWidth"] = self.border_width
        if self.border_radius is not None:
            result["borderRadius"] = self.border_radius

        # Spacing
        if self.margin is not None:
            result["margin"] = self.margin
        if self.margin_top is not None:
            result["marginTop"] = self.margin_top
        if self.margin_right is not None:
            result["marginRight"] = self.margin_right
        if self.margin_bottom is not None:
            result["marginBottom"] = self.margin_bottom
        if self.margin_left is not None:
            result["marginLeft"] = self.margin_left

        if self.padding is not None:
            result["padding"] = self.padding
        if self.padding_top is not None:
            result["paddingTop"] = self.padding_top
        if self.padding_right is not None:
            result["paddingRight"] = self.padding_right
        if self.padding_bottom is not None:
            result["paddingBottom"] = self.padding_bottom
        if self.padding_left is not None:
            result["paddingLeft"] = self.padding_left

        # Typography
        if self.font_size is not None:
            result["fontSize"] = self.font_size
        if self.font_family is not None:
            result["fontFamily"] = self.font_family
        if self.font_weight is not None:
            result["fontWeight"] = self.font_weight
        if self.font_style is not None:
            result["fontStyle"] = self.font_style
        if self.line_height is not None:
            result["lineHeight"] = self.line_height
        if self.text_align is not None:
            result["textAlign"] = self.text_align

        # Display
        if self.visible is not None:
            result["visible"] = self.visible
        if self.opacity is not None:
            result["opacity"] = self.opacity
        if self.overflow is not None:
            result["overflow"] = self.overflow

        # Flex
        if self.flex_grow is not None:
            result["flexGrow"] = self.flex_grow
        if self.flex_shrink is not None:
            result["flexShrink"] = self.flex_shrink
        if self.flex_basis is not None:
            result["flexBasis"] = norm_dim(self.flex_basis)

        # Position
        if self.position is not None:
            result["position"] = self.position
        if self.x is not None:
            result["x"] = self.x
        if self.y is not None:
            result["y"] = self.y

        return result

# ============================================================================
# Layout Types
# ============================================================================

@dataclass
class Layout:
    """Component layout properties (snake_case Python API)"""
    # Flex direction
    flex_direction: Optional[str] = None  # 'row', 'column'

    # Alignment
    justify_content: Optional[str] = None  # 'flex-start', 'center', 'flex-end', 'space-between', 'space-around'
    align_items: Optional[str] = None  # 'flex-start', 'center', 'flex-end', 'stretch'
    align_content: Optional[str] = None

    # Gap
    gap: Optional[float] = None
    row_gap: Optional[float] = None
    column_gap: Optional[float] = None

    # Sizing
    flex_wrap: Optional[str] = None  # 'nowrap', 'wrap', 'wrap-reverse'

    # Position
    top: Optional[float] = None
    right: Optional[float] = None
    bottom: Optional[float] = None
    left: Optional[float] = None

    def to_kir_dict(self) -> Dict[str, Any]:
        """Convert to KIR format (camelCase)"""
        result = {}

        if self.flex_direction is not None:
            result["flexDirection"] = self.flex_direction
        if self.justify_content is not None:
            result["justifyContent"] = self.justify_content
        if self.align_items is not None:
            result["alignItems"] = self.align_items
        if self.align_content is not None:
            result["alignContent"] = self.align_content
        if self.gap is not None:
            result["gap"] = self.gap
        if self.row_gap is not None:
            result["rowGap"] = self.row_gap
        if self.column_gap is not None:
            result["columnGap"] = self.column_gap
        if self.flex_wrap is not None:
            result["flexWrap"] = self.flex_wrap
        if self.top is not None:
            result["top"] = self.top
        if self.right is not None:
            result["right"] = self.right
        if self.bottom is not None:
            result["bottom"] = self.bottom
        if self.left is not None:
            result["left"] = self.left

        return result
