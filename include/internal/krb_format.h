/**
 * @file krb_format.h
 * @brief Kryon Binary Format (KRB) Reader/Writer
 * 
 * Complete KRB file format implementation with validation, endianness handling,
 * and versioning support for cross-platform binary UI definitions.
 * 
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_INTERNAL_KRB_FORMAT_H
#define KRYON_INTERNAL_KRB_FORMAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonKrbFile KryonKrbFile;
typedef struct KryonKrbHeader KryonKrbHeader;
typedef struct KryonKrbElement KryonKrbElement;
typedef struct KryonKrbProperty KryonKrbProperty;
typedef struct KryonKrbReader KryonKrbReader;
typedef struct KryonKrbWriter KryonKrbWriter;

// =============================================================================
// KRB FILE FORMAT CONSTANTS
// =============================================================================

#define KRYON_KRB_MAGIC 0x4B52594E  // "KRYN" in big-endian
#define KRYON_KRB_VERSION_MAJOR 1
#define KRYON_KRB_VERSION_MINOR 0
#define KRYON_KRB_VERSION_PATCH 0

#define KRYON_KRB_MAX_NESTING_DEPTH 256
#define KRYON_KRB_MAX_ELEMENTS 65536
#define KRYON_KRB_MAX_PROPERTIES 32768
#define KRYON_KRB_MAX_STRING_LENGTH 65535

// Compression types
typedef enum {
    KRYON_KRB_COMPRESSION_NONE = 0,
    KRYON_KRB_COMPRESSION_LZ4 = 1,
    KRYON_KRB_COMPRESSION_ZSTD = 2
} KryonKrbCompressionType;

// Element types (matching KRY specification)
typedef enum {
    KRYON_ELEMENT_CONTAINER = 0x01,
    KRYON_ELEMENT_ROW = 0x02,
    KRYON_ELEMENT_COLUMN = 0x03,
    KRYON_ELEMENT_GRID = 0x04,
    KRYON_ELEMENT_STACK = 0x05,
    KRYON_ELEMENT_TEXT = 0x10,
    KRYON_ELEMENT_IMAGE = 0x11,
    KRYON_ELEMENT_BUTTON = 0x12,
    KRYON_ELEMENT_INPUT = 0x13,
    KRYON_ELEMENT_TEXTAREA = 0x14,
    KRYON_ELEMENT_CHECKBOX = 0x15,
    KRYON_ELEMENT_RADIO = 0x16,
    KRYON_ELEMENT_SELECT = 0x17,
    KRYON_ELEMENT_SLIDER = 0x18,
    KRYON_ELEMENT_PROGRESS = 0x19,
    KRYON_ELEMENT_LIST = 0x20,
    KRYON_ELEMENT_TREE = 0x21,
    KRYON_ELEMENT_TABLE = 0x22,
    KRYON_ELEMENT_CARD = 0x23,
    KRYON_ELEMENT_TAB = 0x24,
    KRYON_ELEMENT_MODAL = 0x25,
    KRYON_ELEMENT_DRAWER = 0x26,
    KRYON_ELEMENT_DROPDOWN = 0x27,
    KRYON_ELEMENT_MENU = 0x28,
    KRYON_ELEMENT_NAVBAR = 0x29,
    KRYON_ELEMENT_SIDEBAR = 0x2A,
    KRYON_ELEMENT_HEADER = 0x2B,
    KRYON_ELEMENT_FOOTER = 0x2C,
    KRYON_ELEMENT_SECTION = 0x2D,
    KRYON_ELEMENT_ARTICLE = 0x2E,
    KRYON_ELEMENT_ASIDE = 0x2F,
    // Extended elements up to 0xFF
    KRYON_ELEMENT_CUSTOM = 0xFF
} KryonElementType;

// Property types
typedef enum {
    KRYON_PROPERTY_STRING = 0x01,
    KRYON_PROPERTY_INTEGER = 0x02,
    KRYON_PROPERTY_FLOAT = 0x03,
    KRYON_PROPERTY_BOOLEAN = 0x04,
    KRYON_PROPERTY_COLOR = 0x05,
    KRYON_PROPERTY_SIZE = 0x06,
    KRYON_PROPERTY_POSITION = 0x07,
    KRYON_PROPERTY_MARGIN = 0x08,
    KRYON_PROPERTY_PADDING = 0x09,
    KRYON_PROPERTY_BORDER = 0x0A,
    KRYON_PROPERTY_FONT = 0x0B,
    KRYON_PROPERTY_ARRAY = 0x0C,
    KRYON_PROPERTY_OBJECT = 0x0D,
    KRYON_PROPERTY_REFERENCE = 0x0E
} KryonPropertyType;

// =============================================================================
// KRB FILE STRUCTURES
// =============================================================================

/**
 * @brief KRB file header
 */
struct KryonKrbHeader {
    uint32_t magic;                     ///< Magic number (KRYN)
    uint16_t version_major;             ///< Major version
    uint16_t version_minor;             ///< Minor version
    uint16_t version_patch;             ///< Patch version
    uint16_t flags;                     ///< Format flags
    uint32_t element_count;             ///< Number of elements
    uint32_t property_count;            ///< Number of properties
    uint32_t string_table_size;         ///< Size of string table
    uint32_t data_size;                 ///< Size of element data
    uint32_t checksum;                  ///< CRC32 checksum
    KryonKrbCompressionType compression; ///< Compression type
    uint32_t uncompressed_size;         ///< Uncompressed data size
    uint8_t reserved[32];               ///< Reserved bytes
};

/**
 * @brief KRB property value
 */
typedef union {
    char *string_value;      ///< String value
    int64_t int_value;       ///< Integer value  
    double float_value;      ///< Float value
    bool bool_value;         ///< Boolean value
    uint32_t color_value;    ///< Color value (RGBA)
    struct {                 ///< Size value
        float width;
        float height;
    } size_value;
    struct {                 ///< Position value
        float x;
        float y;
    } position_value;
    struct {                 ///< Margin/Padding value
        float top;
        float right;
        float bottom;
        float left;
    } spacing_value;
    uint32_t reference_id;   ///< Reference to another element
} KryonPropertyValue;

/**
 * @brief KRB property
 */
struct KryonKrbProperty {
    uint16_t name_id;               ///< String table index for property name
    KryonPropertyType type;         ///< Property type
    KryonPropertyValue value;       ///< Property value
    uint32_t flags;                 ///< Property flags
};

/**
 * @brief KRB element
 */
struct KryonKrbElement {
    uint32_t id;                    ///< Unique element ID
    KryonElementType type;          ///< Element type
    uint16_t name_id;               ///< String table index for element name (optional)
    uint32_t parent_id;             ///< Parent element ID (0 for root)
    uint16_t property_count;        ///< Number of properties
    uint16_t child_count;           ///< Number of child elements
    KryonKrbProperty *properties;   ///< Array of properties
    uint32_t *child_ids;            ///< Array of child element IDs
    uint32_t flags;                 ///< Element flags
};

/**
 * @brief KRB file structure
 */
struct KryonKrbFile {
    KryonKrbHeader header;          ///< File header
    char **string_table;            ///< String table
    uint32_t string_count;          ///< Number of strings
    KryonKrbElement *elements;      ///< Array of elements
    uint8_t *raw_data;              ///< Raw file data (for memory mapping)
    size_t file_size;               ///< Total file size
    bool owns_data;                 ///< Whether file owns the data
};

// =============================================================================
// KRB READER
// =============================================================================

/**
 * @brief KRB file reader
 */
struct KryonKrbReader {
    FILE *file;                     ///< Input file handle
    const uint8_t *data;            ///< Memory buffer (if reading from memory)
    size_t data_size;               ///< Size of memory buffer
    size_t position;                ///< Current read position
    bool big_endian;                ///< Whether file uses big-endian format
    KryonKrbFile *krb_file;         ///< Loaded KRB file
    char error_message[256];        ///< Last error message
};

/**
 * @brief Create KRB reader for file
 * @param filename Path to KRB file
 * @return Pointer to reader, or NULL on failure
 */
KryonKrbReader *kryon_krb_reader_create_file(const char *filename);

/**
 * @brief Create KRB reader for memory buffer
 * @param data Pointer to KRB data
 * @param size Size of data
 * @return Pointer to reader, or NULL on failure
 */
KryonKrbReader *kryon_krb_reader_create_memory(const uint8_t *data, size_t size);

/**
 * @brief Destroy KRB reader
 * @param reader Reader to destroy
 */
void kryon_krb_reader_destroy(KryonKrbReader *reader);

/**
 * @brief Read and parse KRB file
 * @param reader The reader
 * @return Pointer to parsed KRB file, or NULL on failure
 */
KryonKrbFile *kryon_krb_reader_parse(KryonKrbReader *reader);

/**
 * @brief Get last error message
 * @param reader The reader
 * @return Error message string
 */
const char *kryon_krb_reader_get_error(const KryonKrbReader *reader);

// =============================================================================
// KRB WRITER
// =============================================================================

/**
 * @brief KRB file writer
 */
struct KryonKrbWriter {
    FILE *file;                     ///< Output file handle
    uint8_t *buffer;                ///< Memory buffer (if writing to memory)
    size_t buffer_size;             ///< Size of memory buffer
    size_t buffer_capacity;         ///< Capacity of memory buffer
    bool big_endian;                ///< Whether to use big-endian format
    KryonKrbCompressionType compression; ///< Compression type
    char error_message[256];        ///< Last error message
};

/**
 * @brief Create KRB writer for file
 * @param filename Path to output KRB file
 * @return Pointer to writer, or NULL on failure
 */
KryonKrbWriter *kryon_krb_writer_create_file(const char *filename);

/**
 * @brief Create KRB writer for memory buffer
 * @param initial_capacity Initial buffer capacity
 * @return Pointer to writer, or NULL on failure
 */
KryonKrbWriter *kryon_krb_writer_create_memory(size_t initial_capacity);

/**
 * @brief Destroy KRB writer
 * @param writer Writer to destroy
 */
void kryon_krb_writer_destroy(KryonKrbWriter *writer);

/**
 * @brief Write KRB file
 * @param writer The writer
 * @param krb_file KRB file to write
 * @return true on success, false on failure
 */
bool kryon_krb_writer_write(KryonKrbWriter *writer, const KryonKrbFile *krb_file);

/**
 * @brief Set compression type
 * @param writer The writer
 * @param compression Compression type
 */
void kryon_krb_writer_set_compression(KryonKrbWriter *writer, KryonKrbCompressionType compression);

/**
 * @brief Get written data (for memory writers)
 * @param writer The writer
 * @param out_size Output for data size
 * @return Pointer to written data
 */
const uint8_t *kryon_krb_writer_get_data(const KryonKrbWriter *writer, size_t *out_size);

/**
 * @brief Get last error message
 * @param writer The writer
 * @return Error message string
 */
const char *kryon_krb_writer_get_error(const KryonKrbWriter *writer);

// =============================================================================
// KRB FILE MANAGEMENT
// =============================================================================

/**
 * @brief Create empty KRB file
 * @return Pointer to new KRB file, or NULL on failure
 */
KryonKrbFile *kryon_krb_file_create(void);

/**
 * @brief Destroy KRB file
 * @param krb_file KRB file to destroy
 */
void kryon_krb_file_destroy(KryonKrbFile *krb_file);

/**
 * @brief Add string to string table
 * @param krb_file The KRB file
 * @param string String to add
 * @return String table index, or UINT16_MAX on failure
 */
uint16_t kryon_krb_file_add_string(KryonKrbFile *krb_file, const char *string);

/**
 * @brief Get string from string table
 * @param krb_file The KRB file
 * @param index String table index
 * @return String, or NULL if index invalid
 */
const char *kryon_krb_file_get_string(const KryonKrbFile *krb_file, uint16_t index);

/**
 * @brief Add element to KRB file
 * @param krb_file The KRB file
 * @param element Element to add (will be copied)
 * @return true on success, false on failure
 */
bool kryon_krb_file_add_element(KryonKrbFile *krb_file, const KryonKrbElement *element);

/**
 * @brief Get element by ID
 * @param krb_file The KRB file
 * @param id Element ID
 * @return Pointer to element, or NULL if not found
 */
KryonKrbElement *kryon_krb_file_get_element(const KryonKrbFile *krb_file, uint32_t id);

/**
 * @brief Get root elements (elements with parent_id = 0)
 * @param krb_file The KRB file
 * @param out_count Output for number of root elements
 * @return Array of root element pointers
 */
KryonKrbElement **kryon_krb_file_get_root_elements(const KryonKrbFile *krb_file, size_t *out_count);

// =============================================================================
// KRB ELEMENT UTILITIES  
// =============================================================================

/**
 * @brief Create new KRB element
 * @param id Element ID
 * @param type Element type
 * @param parent_id Parent element ID (0 for root)
 * @return Pointer to new element, or NULL on failure
 */
KryonKrbElement *kryon_krb_element_create(uint32_t id, KryonElementType type, uint32_t parent_id);

/**
 * @brief Destroy KRB element
 * @param element Element to destroy
 */
void kryon_krb_element_destroy(KryonKrbElement *element);

/**
 * @brief Add property to element
 * @param element The element
 * @param property Property to add (will be copied)
 * @return true on success, false on failure
 */
bool kryon_krb_element_add_property(KryonKrbElement *element, const KryonKrbProperty *property);

/**
 * @brief Get property by name
 * @param element The element
 * @param krb_file KRB file (for string table lookup)
 * @param name Property name
 * @return Pointer to property, or NULL if not found
 */
KryonKrbProperty *kryon_krb_element_get_property(const KryonKrbElement *element, 
                                                const KryonKrbFile *krb_file, 
                                                const char *name);

/**
 * @brief Add child element ID
 * @param element The element
 * @param child_id Child element ID
 * @return true on success, false on failure
 */
bool kryon_krb_element_add_child(KryonKrbElement *element, uint32_t child_id);

// =============================================================================
// KRB PROPERTY UTILITIES
// =============================================================================

/**
 * @brief Create string property
 * @param name_id String table index for property name
 * @param value String value
 * @return Property structure
 */
KryonKrbProperty kryon_krb_property_string(uint16_t name_id, const char *value);

/**
 * @brief Create integer property
 * @param name_id String table index for property name
 * @param value Integer value
 * @return Property structure
 */
KryonKrbProperty kryon_krb_property_int(uint16_t name_id, int64_t value);

/**
 * @brief Create float property
 * @param name_id String table index for property name
 * @param value Float value
 * @return Property structure
 */
KryonKrbProperty kryon_krb_property_float(uint16_t name_id, double value);

/**
 * @brief Create boolean property
 * @param name_id String table index for property name
 * @param value Boolean value
 * @return Property structure
 */
KryonKrbProperty kryon_krb_property_bool(uint16_t name_id, bool value);

/**
 * @brief Create color property
 * @param name_id String table index for property name
 * @param rgba Color value (RGBA format)
 * @return Property structure
 */
KryonKrbProperty kryon_krb_property_color(uint16_t name_id, uint32_t rgba);

// =============================================================================
// VALIDATION UTILITIES
// =============================================================================

/**
 * @brief Validate KRB file structure
 * @param krb_file The KRB file
 * @param out_error Output buffer for error message (can be NULL)
 * @param error_size Size of error buffer
 * @return true if valid, false if invalid
 */
bool kryon_krb_file_validate(const KryonKrbFile *krb_file, char *out_error, size_t error_size);

/**
 * @brief Calculate checksum for KRB file data
 * @param data Pointer to data
 * @param size Size of data
 * @return CRC32 checksum
 */
uint32_t kryon_krb_calculate_checksum(const uint8_t *data, size_t size);

/**
 * @brief Get element type name
 * @param type Element type
 * @return Element type name string
 */
const char *kryon_krb_element_type_name(KryonElementType type);

/**
 * @brief Get property type name
 * @param type Property type
 * @return Property type name string
 */
const char *kryon_krb_property_type_name(KryonPropertyType type);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_KRB_FORMAT_H