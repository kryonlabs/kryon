"""
High-level library wrapper for Kryon
Provides a Pythonic API over the raw FFI bindings
"""

from .ffi import get_lib, ffi
from .types import ComponentType, Style, Layout, Color, Dimension

class KryonLibrary:
    """
    High-level wrapper for libkryon
    Provides convenient methods for working with the Kryon IR
    """

    def __init__(self):
        """Initialize the library (lazy loads on first use)"""
        self._lib = None

    @property
    def lib(self):
        """Get the underlying FFI library"""
        if self._lib is None:
            self._lib = get_lib()
        return self._lib

    def create_component(self, component_type: ComponentType):
        """
        Create a new IR component

        Args:
            component_type: Type of component to create

        Returns:
            Pointer to the created component
        """
        return self.lib.ir_create_component(int(component_type))

    def destroy_component(self, component):
        """
        Destroy an IR component and its children

        Args:
            component: Component to destroy
        """
        self.lib.ir_destroy_component(component)

    def add_child(self, parent, child):
        """
        Add a child component to a parent

        Args:
            parent: Parent component
            child: Child component to add
        """
        self.lib.ir_add_child(parent, child)

    def get_child(self, component, index: int):
        """
        Get a child component by index

        Args:
            component: Parent component
            index: Child index

        Returns:
            Child component or None
        """
        return self.lib.ir_get_child(component, index)

    def set_text_content(self, component, text: str):
        """
        Set the text content of a component

        Args:
            component: Component to modify
            text: Text content
        """
        self.lib.ir_set_text_content(component, text.encode('utf-8'))

    def serialize_to_json(self, component) -> str:
        """
        Serialize a component tree to JSON

        Args:
            component: Root component

        Returns:
            JSON string representation
        """
        result = self.lib.ir_serialize_json_complete(
            component,
            ffi.NULL,
            ffi.NULL,
            ffi.NULL,
            ffi.NULL
        )
        if result != ffi.NULL:
            json_str = ffi.string(result).decode('utf-8')
            return json_str
        return None

    def deserialize_from_json(self, json_str: str):
        """
        Deserialize a JSON string to a component tree

        Args:
            json_str: JSON string

        Returns:
            Root component
        """
        return self.lib.ir_deserialize_json(json_str.encode('utf-8'))

    def read_json_file(self, filepath: str):
        """
        Read a KIR file and deserialize it

        Args:
            filepath: Path to .kir file

        Returns:
            Root component
        """
        return self.lib.ir_read_json_file(filepath.encode('utf-8'))

    def write_json_file(self, component, filepath: str):
        """
        Serialize a component tree and write to a KIR file

        Args:
            component: Root component
            filepath: Output file path

        Returns:
            True on success, False on failure
        """
        return bool(self.lib.ir_write_json_file(
            component,
            ffi.NULL,
            filepath.encode('utf-8')
        ))

    def component_type_to_string(self, component_type: ComponentType) -> str:
        """
        Convert component type enum to string

        Args:
            component_type: Component type enum

        Returns:
            String representation (e.g., "Column", "Text")
        """
        result = self.lib.ir_component_type_to_string(int(component_type))
        return ffi.string(result).decode('utf-8') if result else ""

    def string_to_component_type(self, type_str: str) -> ComponentType:
        """
        Convert string to component type enum

        Args:
            type_str: Type string (e.g., "Column", "Text")

        Returns:
            ComponentType enum value
        """
        result = self.lib.ir_string_to_component_type(type_str.encode('utf-8'))
        return ComponentType(result)


# Global library instance
_kryon_lib = KryonLibrary()

def get_library() -> KryonLibrary:
    """Get the global Kryon library instance"""
    return _kryon_lib
