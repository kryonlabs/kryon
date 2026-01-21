"""
KIR Serializer - Serialize components to KIR files
"""

import json
from pathlib import Path
from typing import Dict, Any
from .kir_generator import KIRGenerator


class KIRSerializer:
    """Serialize components to KIR JSON format"""

    def __init__(self, indent: int = 2):
        """
        Initialize serializer

        Args:
            indent: JSON indentation level
        """
        self.indent = indent
        self.generator = KIRGenerator()

    def serialize(self, component, pretty: bool = True) -> str:
        """
        Serialize a component to KIR JSON string

        Args:
            component: Component to serialize
            pretty: Whether to format JSON prettily

        Returns:
            JSON string
        """
        # Convert to KIR dictionary
        kir_dict = self.generator.generate(component)

        # Wrap in root structure
        root = {
            "version": "2.0",
            "metadata": {
                "format": "KIR",
                "language": "python",
            },
            "root": kir_dict,
        }

        # Serialize to JSON
        if pretty:
            return json.dumps(root, indent=self.indent, ensure_ascii=False)
        else:
            return json.dumps(root, ensure_ascii=False)

    def serialize_to_file(self, component, filepath: str):
        """
        Serialize a component and write to a .kir file

        Args:
            component: Component to serialize
            filepath: Output file path
        """
        kir_json = self.serialize(component)

        path = Path(filepath)
        path.parent.mkdir(parents=True, exist_ok=True)

        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(kir_json)


def save_kir(component, filepath: str, pretty: bool = True):
    """
    Convenience function to save a component to a KIR file

    Args:
        component: Component to save
        filepath: Output file path
        pretty: Whether to format JSON prettily
    """
    serializer = KIRSerializer()
    serializer.serialize_to_file(component, filepath)
