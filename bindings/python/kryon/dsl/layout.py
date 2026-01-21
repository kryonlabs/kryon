"""
Layout utilities and helpers for Kryon components
"""

from ..core.types import Layout
from typing import Optional

# Re-export main class
__all__ = ["Layout"]


def row_layout(
    gap: Optional[float] = None,
    justify_content: Optional[str] = None,
    align_items: Optional[str] = None,
) -> Layout:
    """
    Create a row layout (horizontal)

    Args:
        gap: Gap between children
        justify_content: Horizontal alignment
        align_items: Vertical alignment

    Returns:
        Layout object
    """
    return Layout(
        flex_direction="row",
        gap=gap,
        justify_content=justify_content,
        align_items=align_items,
    )


def column_layout(
    gap: Optional[float] = None,
    justify_content: Optional[str] = None,
    align_items: Optional[str] = None,
) -> Layout:
    """
    Create a column layout (vertical)

    Args:
        gap: Gap between children
        justify_content: Vertical alignment
        align_items: Horizontal alignment

    Returns:
        Layout object
    """
    return Layout(
        flex_direction="column",
        gap=gap,
        justify_content=justify_content,
        align_items=align_items,
    )


def center_layout() -> Layout:
    """
    Create a centered layout

    Returns:
        Layout object
    """
    return Layout(
        justify_content="center",
        align_items="center",
    )


# Common layout presets
class LayoutPresets:
    """Common layout configurations"""

    @staticmethod
    def space_between() -> Layout:
        """Space between items"""
        return Layout(justify_content="space-between")

    @staticmethod
    def space_around() -> Layout:
        """Space around items"""
        return Layout(justify_content="space-around")

    @staticmethod
    def space_evenly() -> Layout:
        """Space evenly distributed"""
        return Layout(justify_content="space-evenly")

    @staticmethod
    def start() -> Layout:
        """Align to start"""
        return Layout(
            justify_content="flex-start",
            align_items="flex-start",
        )

    @staticmethod
    def end() -> Layout:
        """Align to end"""
        return Layout(
            justify_content="flex-end",
            align_items="flex-end",
        )


__all__ = ["Layout", "row_layout", "column_layout", "center_layout", "LayoutPresets"]
