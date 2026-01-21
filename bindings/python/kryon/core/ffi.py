"""
Kryon FFI bindings using cffi
Provides low-level access to libkryon C library
"""

import cffi
from pathlib import Path
import sys

# ============================================================================
# cffi Definitions
# ============================================================================

ffi = cffi.FFI()

# Core IR types and functions from ir_core.h, ir_component_factory.h, ir_serialization.h
ffi.cdef("""
// ============================================================================
// Component Types
// ============================================================================
typedef enum {
    IR_COMPONENT_CONTAINER = 0,
    IR_COMPONENT_TEXT = 1,
    IR_COMPONENT_BUTTON = 2,
    IR_COMPONENT_INPUT = 3,
    IR_COMPONENT_CHECKBOX = 4,
    IR_COMPONENT_DROPDOWN = 5,
    IR_COMPONENT_TEXTAREA = 6,
    IR_COMPONENT_ROW = 7,
    IR_COMPONENT_COLUMN = 8,
    IR_COMPONENT_CENTER = 9,
    IR_COMPONENT_IMAGE = 10,
    IR_COMPONENT_CANVAS = 11,
    IR_COMPONENT_NATIVE_CANVAS = 12,
    IR_COMPONENT_MARKDOWN = 13,
    IR_COMPONENT_SPRITE = 14,
    IR_COMPONENT_TAB_GROUP = 15,
    IR_COMPONENT_TAB_BAR = 16,
    IR_COMPONENT_TAB = 17,
    IR_COMPONENT_TAB_CONTENT = 18,
    IR_COMPONENT_TAB_PANEL = 19,
    IR_COMPONENT_MODAL = 20,
    IR_COMPONENT_TABLE = 21,
    IR_COMPONENT_TABLE_HEAD = 22,
    IR_COMPONENT_TABLE_BODY = 23,
    IR_COMPONENT_TABLE_FOOT = 24,
    IR_COMPONENT_TABLE_ROW = 25,
    IR_COMPONENT_TABLE_CELL = 26,
    IR_COMPONENT_TABLE_HEADER_CELL = 27,
    IR_COMPONENT_HEADING = 28,
    IR_COMPONENT_PARAGRAPH = 29,
    IR_COMPONENT_BLOCKQUOTE = 30,
    IR_COMPONENT_CODE_BLOCK = 31,
    IR_COMPONENT_HORIZONTAL_RULE = 32,
    IR_COMPONENT_LIST = 33,
    IR_COMPONENT_LIST_ITEM = 34,
    IR_COMPONENT_LINK = 35,
    IR_COMPONENT_SPAN = 36,
    IR_COMPONENT_STRONG = 37,
    IR_COMPONENT_EM = 38,
    IR_COMPONENT_CODE_INLINE = 39,
    IR_COMPONENT_SMALL = 40,
    IR_COMPONENT_MARK = 41,
    IR_COMPONENT_CUSTOM = 42,
    IR_COMPONENT_STATIC_BLOCK = 43,
    IR_COMPONENT_FOR_LOOP = 44,
    IR_COMPONENT_FOR_EACH = 45,
    IR_COMPONENT_VAR_DECL = 46,
    IR_COMPONENT_PLACEHOLDER = 47,
    IR_COMPONENT_FLOWCHART = 48,
    IR_COMPONENT_FLOWCHART_NODE = 49,
    IR_COMPONENT_FLOWCHART_EDGE = 50,
    IR_COMPONENT_FLOWCHART_SUBGRAPH = 51,
    IR_COMPONENT_FLOWCHART_LABEL = 52
} IRComponentType;

// Forward declarations
typedef struct IRComponent IRComponent;
typedef struct IRStyle IRStyle;
typedef struct IRLayout IRLayout;
typedef struct IREvent IREvent;
typedef struct IRLogic IRLogic;
typedef struct IRReactiveManifest IRReactiveManifest;
typedef struct IRSourceStructures IRSourceStructures;
typedef struct IRSourceMetadata IRSourceMetadata;

// ============================================================================
// Component Creation and Destruction
// ============================================================================
IRComponent* ir_create_component(IRComponentType type);
IRComponent* ir_create_component_with_id(IRComponentType type, uint32_t id);
void ir_destroy_component(IRComponent* component);

// ============================================================================
// Tree Management
// ============================================================================
void ir_add_child(IRComponent* parent, IRComponent* child);
void ir_remove_child(IRComponent* parent, IRComponent* child);
void ir_insert_child(IRComponent* parent, IRComponent* child, uint32_t index);
IRComponent* ir_get_child(IRComponent* component, uint32_t index);
IRComponent* ir_find_component_by_id(IRComponent* root, uint32_t id);

// ============================================================================
// Component Properties
// ============================================================================
void ir_set_text_content(IRComponent* component, const char* text);
void ir_set_custom_data(IRComponent* component, const char* data);

// ============================================================================
// Style Management
// ============================================================================
IRStyle* ir_create_style(void);
void ir_destroy_style(IRStyle* style);
void ir_set_style(IRComponent* component, IRStyle* style);
IRStyle* ir_get_style(IRComponent* component);

void ir_set_width(IRStyle* style, int type, float value);
void ir_set_height(IRStyle* style, int type, float value);
void ir_set_background_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_set_border(IRStyle* style, float width, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t style_type);
void ir_set_margin(IRStyle* style, float top, float right, float bottom, float left);
void ir_set_padding(IRStyle* style, float top, float right, float bottom, float left);

// ============================================================================
// Layout
// ============================================================================
IRLayout* ir_create_layout(void);
void ir_destroy_layout(IRLayout* layout);

// ============================================================================
// Serialization
// ============================================================================
char* ir_serialize_json_complete(
    IRComponent* root,
    IRReactiveManifest* manifest,
    void* logic_block,
    IRSourceMetadata* source_metadata,
    IRSourceStructures* source_structures
);

IRComponent* ir_deserialize_json(const char* json_string);
IRComponent* ir_read_json_file(const char* filename);

bool ir_write_json_file(IRComponent* root, IRReactiveManifest* manifest, const char* filename);

// ============================================================================
// Type Conversion
// ============================================================================
const char* ir_component_type_to_string(IRComponentType type);
IRComponentType ir_string_to_component_type(const char* str);
""")

# ============================================================================
# Library Loading
# ============================================================================

def find_library():
    """
    Find libkryon_ir.so in standard installation paths

    Search order:
    1. KRYON_LIB_PATH environment variable
    2. ~/.local/lib/libkryon_ir.so
    3. /usr/local/lib/libkryon_ir.so
    4. /usr/lib/libkryon_ir.so
    5. ../../build/libkryon_ir.so (relative to this file)
    """
    if "KRYON_LIB_PATH" in os.environ:
        return os.environ["KRYON_LIB_PATH"]

    search_paths = [
        Path.home() / ".local" / "lib" / "libkryon_ir.so",
        Path("/usr/local/lib/libkryon_ir.so"),
        Path("/usr/lib/libkryon_ir.so"),
        Path(__file__).parent.parent.parent.parent / "build" / "libkryon_ir.so",
    ]

    for path in search_paths:
        if path.exists():
            return str(path)

    raise RuntimeError(
        f"Could not find libkryon_ir.so. Tried:\n" +
        "\n".join(f"  - {p}" for p in search_paths) +
        "\n\nSet KRYON_LIB_PATH environment variable or build the IR library."
    )

class KryonLib:
    """Singleton wrapper for the loaded Kryon library"""
    _instance = None
    _lib = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._load_library()
        return cls._instance

    def _load_library(self):
        """Load the libkryon_ir shared library"""
        try:
            lib_path = find_library()
            KryonLib._lib = ffi.dlopen(lib_path)
        except Exception as e:
            raise RuntimeError(f"Failed to load libkryon_ir: {e}")

    @classmethod
    def get_lib(cls):
        """Get the loaded library instance"""
        if cls._lib is None:
            raise RuntimeError("Library not loaded. Create a KryonLib instance first.")
        return cls._lib

# Lazy-loaded library instance
_kryon_lib = None

def get_lib():
    """Get the Kryon library instance (lazy loading)"""
    global _kryon_lib
    if _kryon_lib is None:
        _kryon_lib = KryonLib().get_lib()
    return _kryon_lib

# Import os after it's used
import os

# Export convenience functions
def create_component(component_type: int):
    """Create a new IR component"""
    lib = get_lib()
    return lib.ir_create_component(component_type)

def destroy_component(component):
    """Destroy an IR component"""
    lib = get_lib()
    lib.ir_destroy_component(component)

def add_child(parent, child):
    """Add a child component to a parent"""
    lib = get_lib()
    lib.ir_add_child(parent, child)

def serialize_to_json(component):
    """Serialize a component tree to JSON string"""
    lib = get_lib()
    result = lib.ir_serialize_json_complete(component, ffi.NULL, ffi.NULL, ffi.NULL, ffi.NULL)
    if result != ffi.NULL:
        json_str = ffi.string(result).decode('utf-8')
        # Note: The C function allocates the string, but we don't have a free function exposed
        # In production, we'd need ir_free_string() or similar
        return json_str
    return None

def deserialize_from_json(json_str: str):
    """Deserialize a JSON string to a component tree"""
    lib = get_lib()
    return lib.ir_deserialize_json(json_str.encode('utf-8'))

def component_type_to_string(component_type: int) -> str:
    """Convert component type enum to string"""
    lib = get_lib()
    result = lib.ir_component_type_to_string(component_type)
    return ffi.string(result).decode('utf-8') if result else ""

def string_to_component_type(type_str: str) -> int:
    """Convert string to component type enum"""
    lib = get_lib()
    return lib.ir_string_to_component_type(type_str.encode('utf-8'))
