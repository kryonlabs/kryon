"""
KIR Generator - Converts Python DSL to KIR (JSON format)
"""

from typing import Dict, Any, List
from ..dsl.components import Component
from ..core.types import ComponentType, Style, Layout


class KIRGenerator:
    """
    Generate KIR (JSON) from Python DSL components

    Converts Python component trees to KIR JSON format for serialization
    or code generation.
    """

    def __init__(self):
        """Initialize the generator"""
        self._component_id_counter = 1

    def generate(self, component: Component) -> Dict[str, Any]:
        """
        Generate KIR dictionary from a component

        Args:
            component: Root component

        Returns:
            KIR dictionary
        """
        self._component_id_counter = 1
        return self._generate_component(component)

    def _generate_component(self, component: Component) -> Dict[str, Any]:
        """
        Recursively generate KIR for a component

        Args:
            component: Component to convert

        Returns:
            KIR dictionary
        """
        result: Dict[str, Any] = {
            "type": component.type.to_string(),
        }

        # Add ID if not set
        if component.id is None:
            result["id"] = self._component_id_counter
            self._component_id_counter += 1
        else:
            result["id"] = component.id

        # Add properties
        if component.properties:
            result["properties"] = self._convert_properties_to_camel_case(component.properties)

        # Add style
        if component.style:
            result["style"] = component.style.to_kir_dict()

        # Add layout
        if component.layout:
            result["layout"] = component.layout.to_kir_dict()

        # Add children
        if component.children:
            result["children"] = [
                self._generate_component(child) for child in component.children
            ]

        # Add events
        if component.events:
            result["events"] = component.events

        return result

    @staticmethod
    def _convert_properties_to_camel_case(props: Dict[str, Any]) -> Dict[str, Any]:
        """
        Convert snake_case property names to camelCase

        Args:
            props: Properties dictionary

        Returns:
            Converted dictionary
        """
        result = {}

        # Known property name mappings
        mappings = {
            "text_content": "textContent",
            "custom_data": "customData",
            "source_module": "sourceModule",
            "export_name": "exportName",
            "module_ref": "moduleRef",
            "component_ref": "componentRef",
            "component_props": "componentProps",
            "selected_index": "selectedIndex",
            "is_open": "isOpen",
        }

        for key, value in props.items():
            camel_key = mappings.get(key, None)
            if camel_key is None and "_" in key:
                # Generic snake_case to camelCase
                parts = key.split("_")
                camel_key = parts[0] + "".join(p.capitalize() for p in parts[1:])
            elif camel_key is None:
                camel_key = key

            result[camel_key] = value

        return result


def to_kir(component: Component) -> Dict[str, Any]:
    """
    Convenience function to convert a component to KIR

    Args:
        component: Component to convert

    Returns:
        KIR dictionary
    """
    generator = KIRGenerator()
    return generator.generate(component)
