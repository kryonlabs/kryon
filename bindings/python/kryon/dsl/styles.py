"""
Style utilities and helpers for Kryon components
"""

from ..core.types import Style, Color
from typing import Optional, Union

# Re-export main classes
__all__ = ["Style", "Color"]


def hex_color(hex_str: str) -> Color:
    """
    Create a Color from a hex string

    Args:
        hex_str: Hex color (#RGB or #RRGGBB or #RRGGBBAA)

    Returns:
        Color object
    """
    return Color.from_hex(hex_str)


def rgb_color(r: int, g: int, b: int, a: float = 1.0) -> Color:
    """
    Create a Color from RGB values

    Args:
        r: Red component (0-255)
        g: Green component (0-255)
        b: Blue component (0-255)
        a: Alpha component (0.0-1.0)

    Returns:
        Color object
    """
    return Color(r=r, g=g, b=b, a=a)


# Common color presets
class Colors:
    """Common color palette"""

    # Basic colors
    BLACK = Color(0, 0, 0)
    WHITE = Color(255, 255, 255)
    RED = Color(255, 0, 0)
    GREEN = Color(0, 255, 0)
    BLUE = Color(0, 0, 255)
    YELLOW = Color(255, 255, 0)
    CYAN = Color(0, 255, 255)
    MAGENTA = Color(255, 0, 255)

    # Grays
    GRAY_100 = Color(245, 245, 245)
    GRAY_200 = Color(229, 229, 229)
    GRAY_300 = Color(212, 212, 212)
    GRAY_400 = Color(163, 163, 163)
    GRAY_500 = Color(115, 115, 115)
    GRAY_600 = Color(82, 82, 82)
    GRAY_700 = Color(64, 64, 64)
    GRAY_800 = Color(38, 38, 38)
    GRAY_900 = Color(26, 26, 26)

    # Material design colors
    MATERIAL_RED = Color(244, 67, 54)
    MATERIAL_PINK = Color(233, 30, 99)
    MATERIAL_PURPLE = Color(156, 39, 176)
    MATERIAL_DEEP_PURPLE = Color(103, 58, 183)
    MATERIAL_INDIGO = Color(63, 81, 181)
    MATERIAL_BLUE = Color(33, 150, 243)
    MATERIAL_LIGHT_BLUE = Color(3, 169, 244)
    MATERIAL_CYAN = Color(0, 188, 212)
    MATERIAL_TEAL = Color(0, 150, 136)
    MATERIAL_GREEN = Color(76, 175, 80)
    MATERIAL_LIGHT_GREEN = Color(139, 195, 74)
    MATERIAL_LIME = Color(205, 220, 57)
    MATERIAL_YELLOW = Color(255, 235, 59)
    MATERIAL_AMBER = Color(255, 193, 7)
    MATERIAL_ORANGE = Color(255, 152, 0)
    MATERIAL_DEEP_ORANGE = Color(255, 87, 34)
    MATERIAL_BROWN = Color(121, 85, 72)
    MATERIAL_GREY = Color(158, 158, 158)
    MATERIAL_BLUE_GREY = Color(96, 125, 139)


__all__ = ["Style", "Color", "hex_color", "rgb_color", "Colors"]
