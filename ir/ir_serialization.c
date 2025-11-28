#define _GNU_SOURCE
#include "ir_serialization.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

// Binary format magic numbers
#define IR_MAGIC_NUMBER 0x4B5259   // "KRY" in hex
#define IR_ENDIANNESS_CHECK 0x12345678

// Binary Serialization Helper Functions
static bool write_uint8(IRBuffer* buffer, uint8_t value) {
    return ir_buffer_write(buffer, &value, sizeof(uint8_t));
}

static bool write_uint16(IRBuffer* buffer, uint16_t value) {
    return ir_buffer_write(buffer, &value, sizeof(uint16_t));
}

static bool write_uint32(IRBuffer* buffer, uint32_t value) {
    return ir_buffer_write(buffer, &value, sizeof(uint32_t));
}

static bool write_float32(IRBuffer* buffer, float value) {
    return ir_buffer_write(buffer, &value, sizeof(float));
}

static bool write_string(IRBuffer* buffer, const char* string) {
    if (!string) {
        write_uint32(buffer, 0);
        return true;
    }

    uint32_t length = strlen(string) + 1;  // Include null terminator
    write_uint32(buffer, length);
    return ir_buffer_write(buffer, string, length);
}

static bool read_uint8(IRBuffer* buffer, uint8_t* value) {
    return ir_buffer_read(buffer, value, sizeof(uint8_t));
}

static bool read_uint16(IRBuffer* buffer, uint16_t* value) {
    return ir_buffer_read(buffer, value, sizeof(uint16_t));
}

static bool read_uint32(IRBuffer* buffer, uint32_t* value) {
    return ir_buffer_read(buffer, value, sizeof(uint32_t));
}

static bool read_float32(IRBuffer* buffer, float* value) {
    return ir_buffer_read(buffer, value, sizeof(float));
}

static bool read_string(IRBuffer* buffer, char** string) {
    uint32_t length;
    if (!read_uint32(buffer, &length)) return false;

    if (length == 0) {
        *string = NULL;
        return true;
    }

    *string = malloc(length);
    if (!*string) return false;

    if (!ir_buffer_read(buffer, *string, length)) {
        free(*string);
        *string = NULL;
        return false;
    }

    return true;
}

// Buffer Management
IRBuffer* ir_buffer_create(size_t initial_capacity) {
    IRBuffer* buffer = malloc(sizeof(IRBuffer));
    if (!buffer) return NULL;

    buffer->data = malloc(initial_capacity);
    if (!buffer->data) {
        free(buffer);
        return NULL;
    }

    buffer->size = 0;
    buffer->capacity = initial_capacity;

    return buffer;
}

void ir_buffer_destroy(IRBuffer* buffer) {
    if (!buffer) return;

    if (buffer->data) {
        free(buffer->data);
    }
    free(buffer);
}

bool ir_buffer_write(IRBuffer* buffer, const void* data, size_t size) {
    if (!buffer || !data) return false;

    // Check if we need to expand the buffer
    if (buffer->size + size > buffer->capacity) {
        size_t new_capacity = buffer->capacity * 2;
        if (new_capacity < buffer->size + size) {
            new_capacity = buffer->size + size + 1024;  // Add some extra space
        }

        uint8_t* new_data = realloc(buffer->data, new_capacity);
        if (!new_data) return false;

        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }

    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;

    return true;
}

bool ir_buffer_read(IRBuffer* buffer, void* data, size_t size) {
    if (!buffer || !data) return false;

    if (buffer->size < size) return false;  // Not enough data

    memcpy(data, buffer->data, size);
    buffer->data += size;
    buffer->size -= size;

    return true;
}

bool ir_buffer_seek(IRBuffer* buffer, size_t position) {
    if (!buffer) return false;

    // This is a simplified implementation - in practice you'd need to track the original data pointer
    return false;  // Not implemented for this simple version
}

size_t ir_buffer_tell(IRBuffer* buffer) {
    return buffer ? buffer->size : 0;
}

size_t ir_buffer_size(IRBuffer* buffer) {
    return buffer ? buffer->size : 0;
}

// Serialization Helper Functions
static bool serialize_dimension(IRBuffer* buffer, IRDimension dimension) {
    if (!write_uint8(buffer, dimension.type)) return false;
    if (!write_float32(buffer, dimension.value)) return false;
    return true;
}

static bool deserialize_dimension(IRBuffer* buffer, IRDimension* dimension) {
    if (!read_uint8(buffer, (uint8_t*)&dimension->type)) return false;
    if (!read_float32(buffer, &dimension->value)) return false;
    return true;
}

static bool serialize_color(IRBuffer* buffer, IRColor color) {
    if (!write_uint8(buffer, color.type)) return false;
    if (!write_uint8(buffer, color.data.r)) return false;
    if (!write_uint8(buffer, color.data.g)) return false;
    if (!write_uint8(buffer, color.data.b)) return false;
    if (!write_uint8(buffer, color.data.a)) return false;
    return true;
}

static bool deserialize_color(IRBuffer* buffer, IRColor* color) {
    if (!read_uint8(buffer, (uint8_t*)&color->type)) return false;
    if (!read_uint8(buffer, &color->data.r)) return false;
    if (!read_uint8(buffer, &color->data.g)) return false;
    if (!read_uint8(buffer, &color->data.b)) return false;
    if (!read_uint8(buffer, &color->data.a)) return false;
    return true;
}

static bool serialize_style(IRBuffer* buffer, IRStyle* style) {
    if (!style) {
        write_uint8(buffer, 0);  // Null style marker
        return true;
    }

    write_uint8(buffer, 1);  // Non-null style marker

    // Serialize dimensions
    if (!serialize_dimension(buffer, style->width)) return false;
    if (!serialize_dimension(buffer, style->height)) return false;

    // Serialize colors
    if (!serialize_color(buffer, style->background)) return false;
    if (!serialize_color(buffer, style->border_color)) return false;

    // Serialize border
    if (!write_float32(buffer, style->border.width)) return false;
    if (!serialize_color(buffer, style->border.color)) return false;
    if (!write_uint8(buffer, style->border.radius)) return false;

    // Serialize spacing
    if (!write_float32(buffer, style->margin.top)) return false;
    if (!write_float32(buffer, style->margin.right)) return false;
    if (!write_float32(buffer, style->margin.bottom)) return false;
    if (!write_float32(buffer, style->margin.left)) return false;

    if (!write_float32(buffer, style->padding.top)) return false;
    if (!write_float32(buffer, style->padding.right)) return false;
    if (!write_float32(buffer, style->padding.bottom)) return false;
    if (!write_float32(buffer, style->padding.left)) return false;

    // Serialize typography
    if (!write_float32(buffer, style->font.size)) return false;
    if (!serialize_color(buffer, style->font.color)) return false;
    if (!write_uint8(buffer, style->font.bold)) return false;
    if (!write_uint8(buffer, style->font.italic)) return false;
    if (!write_string(buffer, style->font.family)) return false;

    // Serialize transform
    if (!write_float32(buffer, style->transform.translate_x)) return false;
    if (!write_float32(buffer, style->transform.translate_y)) return false;
    if (!write_float32(buffer, style->transform.scale_x)) return false;
    if (!write_float32(buffer, style->transform.scale_y)) return false;
    if (!write_float32(buffer, style->transform.rotate)) return false;

    // Serialize other properties
    if (!write_uint32(buffer, style->animation_count)) return false;
    if (!write_uint32(buffer, style->z_index)) return false;
    if (!write_uint8(buffer, style->visible)) return false;
    if (!write_float32(buffer, style->opacity)) return false;

    return true;
}

static bool deserialize_style(IRBuffer* buffer, IRStyle** style_ptr) {
    uint8_t has_style;
    if (!read_uint8(buffer, &has_style)) return false;

    if (has_style == 0) {
        *style_ptr = NULL;
        return true;
    }

    IRStyle* style = ir_create_style();
    if (!style) return false;

    // Deserialize dimensions
    if (!deserialize_dimension(buffer, &style->width)) goto error;
    if (!deserialize_dimension(buffer, &style->height)) goto error;

    // Deserialize colors
    if (!deserialize_color(buffer, &style->background)) goto error;
    if (!deserialize_color(buffer, &style->border_color)) goto error;

    // Deserialize border
    if (!read_float32(buffer, &style->border.width)) goto error;
    if (!deserialize_color(buffer, &style->border.color)) goto error;
    if (!read_uint8(buffer, &style->border.radius)) goto error;

    // Deserialize spacing
    if (!read_float32(buffer, &style->margin.top)) goto error;
    if (!read_float32(buffer, &style->margin.right)) goto error;
    if (!read_float32(buffer, &style->margin.bottom)) goto error;
    if (!read_float32(buffer, &style->margin.left)) goto error;

    if (!read_float32(buffer, &style->padding.top)) goto error;
    if (!read_float32(buffer, &style->padding.right)) goto error;
    if (!read_float32(buffer, &style->padding.bottom)) goto error;
    if (!read_float32(buffer, &style->padding.left)) goto error;

    // Deserialize typography
    if (!read_float32(buffer, &style->font.size)) goto error;
    if (!deserialize_color(buffer, &style->font.color)) goto error;
    if (!read_uint8(buffer, (uint8_t*)&style->font.bold)) goto error;
    if (!read_uint8(buffer, (uint8_t*)&style->font.italic)) goto error;
    if (!read_string(buffer, &style->font.family)) goto error;

    // Deserialize transform
    if (!read_float32(buffer, &style->transform.translate_x)) goto error;
    if (!read_float32(buffer, &style->transform.translate_y)) goto error;
    if (!read_float32(buffer, &style->transform.scale_x)) goto error;
    if (!read_float32(buffer, &style->transform.scale_y)) goto error;
    if (!read_float32(buffer, &style->transform.rotate)) goto error;

    // Deserialize other properties
    if (!read_uint32(buffer, &style->animation_count)) goto error;
    if (!read_uint32(buffer, &style->z_index)) goto error;
    if (!read_uint8(buffer, (uint8_t*)&style->visible)) goto error;
    if (!read_float32(buffer, &style->opacity)) goto error;

    *style_ptr = style;
    return true;

error:
    ir_destroy_style(style);
    return false;
}

static bool serialize_event(IRBuffer* buffer, IREvent* event) {
    if (!event) {
        write_uint8(buffer, 0);  // Null event marker
        return true;
    }

    write_uint8(buffer, 1);  // Non-null event marker

    if (!write_uint8(buffer, event->type)) return false;
    if (!write_string(buffer, event->logic_id)) return false;
    if (!write_string(buffer, event->handler_data)) return false;

    return true;
}

static bool deserialize_event(IRBuffer* buffer, IREvent** event_ptr) {
    uint8_t has_event;
    if (!read_uint8(buffer, &has_event)) return false;

    if (has_event == 0) {
        *event_ptr = NULL;
        return true;
    }

    IREvent* event = malloc(sizeof(IREvent));
    if (!event) return false;

    memset(event, 0, sizeof(IREvent));

    if (!read_uint8(buffer, (uint8_t*)&event->type)) goto error;
    if (!read_string(buffer, &event->logic_id)) goto error;
    if (!read_string(buffer, &event->handler_data)) goto error;

    *event_ptr = event;
    return true;

error:
    ir_destroy_event(event);
    return false;
}

static bool serialize_logic(IRBuffer* buffer, IRLogic* logic) {
    if (!logic) {
        write_uint8(buffer, 0);  // Null logic marker
        return true;
    }

    write_uint8(buffer, 1);  // Non-null logic marker

    if (!write_string(buffer, logic->id)) return false;
    if (!write_uint8(buffer, logic->type)) return false;
    if (!write_string(buffer, logic->source_code)) return false;

    return true;
}

static bool deserialize_logic(IRBuffer* buffer, IRLogic** logic_ptr) {
    uint8_t has_logic;
    if (!read_uint8(buffer, &has_logic)) return false;

    if (has_logic == 0) {
        *logic_ptr = NULL;
        return true;
    }

    IRLogic* logic = malloc(sizeof(IRLogic));
    if (!logic) return false;

    memset(logic, 0, sizeof(IRLogic));

    if (!read_string(buffer, &logic->id)) goto error;
    if (!read_uint8(buffer, (uint8_t*)&logic->type)) goto error;
    if (!read_string(buffer, &logic->source_code)) goto error;

    *logic_ptr = logic;
    return true;

error:
    ir_destroy_logic(logic);
    return false;
}

static bool serialize_component(IRBuffer* buffer, IRComponent* component) {
    if (!component) {
        write_uint8(buffer, 0);  // Null component marker
        return true;
    }

    write_uint8(buffer, 1);  // Non-null component marker

    // Serialize basic properties
    if (!write_uint32(buffer, component->id)) return false;
    if (!write_uint8(buffer, component->type)) return false;
    if (!write_string(buffer, component->tag)) return false;
    if (!write_string(buffer, component->text_content)) return false;
    if (!write_string(buffer, component->custom_data)) return false;

    // Serialize style
    if (!serialize_style(buffer, component->style)) return false;

    // Serialize events
    uint32_t event_count = 0;
    IREvent* event = component->events;
    while (event) {
        event_count++;
        event = event->next;
    }
    if (!write_uint32(buffer, event_count)) return false;

    event = component->events;
    while (event) {
        if (!serialize_event(buffer, event)) return false;
        event = event->next;
    }

    // Serialize logic
    uint32_t logic_count = 0;
    IRLogic* logic = component->logic;
    while (logic) {
        logic_count++;
        logic = logic->next;
    }
    if (!write_uint32(buffer, logic_count)) return false;

    logic = component->logic;
    while (logic) {
        if (!serialize_logic(buffer, logic)) return false;
        logic = logic->next;
    }

    // Serialize children
    if (!write_uint32(buffer, component->child_count)) return false;
    for (uint32_t i = 0; i < component->child_count; i++) {
        if (!serialize_component(buffer, component->children[i])) return false;
    }

    return true;
}

static bool deserialize_component(IRBuffer* buffer, IRComponent** component_ptr) {
    uint8_t has_component;
    if (!read_uint8(buffer, &has_component)) return false;

    if (has_component == 0) {
        *component_ptr = NULL;
        return true;
    }

    IRComponent* component = malloc(sizeof(IRComponent));
    if (!component) return false;

    memset(component, 0, sizeof(IRComponent));

    // Deserialize basic properties
    if (!read_uint32(buffer, &component->id)) goto error;
    if (!read_uint8(buffer, (uint8_t*)&component->type)) goto error;
    if (!read_string(buffer, &component->tag)) goto error;
    if (!read_string(buffer, &component->text_content)) goto error;
    if (!read_string(buffer, &component->custom_data)) goto error;

    // Deserialize style
    if (!deserialize_style(buffer, &component->style)) goto error;

    // Deserialize events
    uint32_t event_count;
    if (!read_uint32(buffer, &event_count)) goto error;

    IREvent* last_event = NULL;
    for (uint32_t i = 0; i < event_count; i++) {
        IREvent* event;
        if (!deserialize_event(buffer, &event)) goto error;

        if (i == 0) {
            component->events = event;
        } else {
            last_event->next = event;
        }
        last_event = event;
    }

    // Deserialize logic
    uint32_t logic_count;
    if (!read_uint32(buffer, &logic_count)) goto error;

    IRLogic* last_logic = NULL;
    for (uint32_t i = 0; i < logic_count; i++) {
        IRLogic* logic;
        if (!deserialize_logic(buffer, &logic)) goto error;

        if (i == 0) {
            component->logic = logic;
        } else {
            last_logic->next = logic;
        }
        last_logic = logic;
    }

    // Deserialize children
    if (!read_uint32(buffer, &component->child_count)) goto error;

    if (component->child_count > 0) {
        component->children = malloc(sizeof(IRComponent*) * component->child_count);
        if (!component->children) goto error;

        for (uint32_t i = 0; i < component->child_count; i++) {
            if (!deserialize_component(buffer, &component->children[i])) goto error;
            component->children[i]->parent = component;
        }
    }

    *component_ptr = component;
    return true;

error:
    ir_destroy_component(component);
    return false;
}

// Binary Serialization Functions
IRBuffer* ir_serialize_binary(IRComponent* root) {
    IRBuffer* buffer = ir_buffer_create(4096);  // Start with 4KB
    if (!buffer) return NULL;

    // Write header
    if (!write_uint32(buffer, IR_MAGIC_NUMBER)) goto error;
    if (!write_uint16(buffer, IR_FORMAT_VERSION_MAJOR)) goto error;
    if (!write_uint16(buffer, IR_FORMAT_VERSION_MINOR)) goto error;
    if (!write_uint32(buffer, IR_ENDIANNESS_CHECK)) goto error;

    // Serialize root component
    if (!serialize_component(buffer, root)) goto error;

    return buffer;

error:
    ir_buffer_destroy(buffer);
    return NULL;
}

IRComponent* ir_deserialize_binary(IRBuffer* buffer) {
    if (!buffer) return NULL;

    // Read and validate header
    uint32_t magic;
    uint16_t version_major, version_minor;
    uint32_t endianness_check;

    if (!read_uint32(buffer, &magic)) return NULL;
    if (!read_uint16(buffer, &version_major)) return NULL;
    if (!read_uint16(buffer, &version_minor)) return NULL;
    if (!read_uint32(buffer, &endianness_check)) return NULL;

    if (magic != IR_MAGIC_NUMBER) {
        printf("Error: Invalid magic number in IR file\n");
        return NULL;
    }

    if (version_major != IR_FORMAT_VERSION_MAJOR) {
        printf("Error: Unsupported IR format version %d.%d\n", version_major, version_minor);
        return NULL;
    }

    if (endianness_check != IR_ENDIANNESS_CHECK) {
        printf("Error: Endianness mismatch in IR file\n");
        return NULL;
    }

    // Deserialize root component
    IRComponent* root;
    if (!deserialize_component(buffer, &root)) return NULL;

    return root;
}

bool ir_write_binary_file(IRComponent* root, const char* filename) {
    IRBuffer* buffer = ir_serialize_binary(root);
    if (!buffer) return false;

    FILE* file = fopen(filename, "wb");
    if (!file) {
        ir_buffer_destroy(buffer);
        return false;
    }

    bool success = (fwrite(buffer->data, 1, buffer->size, file) == buffer->size);
    fclose(file);
    ir_buffer_destroy(buffer);

    return success;
}

IRComponent* ir_read_binary_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;

    // Get file size
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Create buffer
    IRBuffer* buffer = ir_buffer_create(file_size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    // Read file into buffer
    if (fread(buffer->data, 1, file_size, file) != file_size) {
        fclose(file);
        ir_buffer_destroy(buffer);
        return NULL;
    }
    buffer->size = file_size;
    fclose(file);

    // Deserialize
    IRComponent* root = ir_deserialize_binary(buffer);
    ir_buffer_destroy(buffer);

    return root;
}

// JSON Serialization Helper - SAFE version without strcpy
static void append_json(char** json, size_t* size, size_t* capacity, const char* str) {
    size_t len = strlen(str);
    while (*size + len + 1 > *capacity) {
        *capacity *= 2;
        char* new_json = realloc(*json, *capacity);
        if (!new_json) {
            fprintf(stderr, "Error: Failed to allocate memory for JSON serialization\n");
            return;
        }
        *json = new_json;
    }
    // Use memcpy instead of strcpy for safety
    memcpy(*json + *size, str, len);
    *size += len;
    (*json)[*size] = '\0';
}

#define MAX_JSON_DEPTH 64

static void serialize_component_json_recursive(IRComponent* component, char** json, size_t* size, size_t* capacity, int depth) {
    if (!component) return;

    // Depth limit to prevent stack overflow
    if (depth > MAX_JSON_DEPTH) {
        fprintf(stderr, "Error: JSON serialization depth limit exceeded (%d)\n", MAX_JSON_DEPTH);
        return;
    }

    char buffer[1024];
    char indent[256] = "";  // Increased size for safety

    // Build indent string safely
    int indent_len = 0;
    for (int i = 0; i < depth && indent_len < (int)sizeof(indent) - 3; i++) {
        indent[indent_len++] = ' ';
        indent[indent_len++] = ' ';
    }
    indent[indent_len] = '\0';

    // Start component object
    snprintf(buffer, sizeof(buffer), "%s{\n", indent);
    append_json(json, size, capacity, buffer);

    // ID and type
    snprintf(buffer, sizeof(buffer), "%s  \"id\": %u,\n", indent, component->id);
    append_json(json, size, capacity, buffer);

    snprintf(buffer, sizeof(buffer), "%s  \"type\": \"%s\",\n", indent, ir_component_type_to_string(component->type));
    append_json(json, size, capacity, buffer);

    // Text content
    if (component->text_content) {
        snprintf(buffer, sizeof(buffer), "%s  \"text\": \"%s\",\n", indent, component->text_content);
        append_json(json, size, capacity, buffer);
    }

    // Style (simplified - just visible and z_index)
    if (component->style) {
        snprintf(buffer, sizeof(buffer), "%s  \"style\": {\n", indent);
        append_json(json, size, capacity, buffer);

        snprintf(buffer, sizeof(buffer), "%s    \"visible\": %s,\n", indent, component->style->visible ? "true" : "false");
        append_json(json, size, capacity, buffer);

        snprintf(buffer, sizeof(buffer), "%s    \"z_index\": %u\n", indent, component->style->z_index);
        append_json(json, size, capacity, buffer);

        snprintf(buffer, sizeof(buffer), "%s  },\n", indent);
        append_json(json, size, capacity, buffer);
    }

    // Rendered bounds
    if (component->rendered_bounds.valid) {
        snprintf(buffer, sizeof(buffer), "%s  \"bounds\": { \"x\": %.2f, \"y\": %.2f, \"width\": %.2f, \"height\": %.2f },\n",
                indent, component->rendered_bounds.x, component->rendered_bounds.y,
                component->rendered_bounds.width, component->rendered_bounds.height);
        append_json(json, size, capacity, buffer);
    }

    // Children
    if (component->child_count > 0) {
        snprintf(buffer, sizeof(buffer), "%s  \"children\": [\n", indent);
        append_json(json, size, capacity, buffer);

        for (uint32_t i = 0; i < component->child_count; i++) {
            serialize_component_json_recursive(component->children[i], json, size, capacity, depth + 2);
            if (i < component->child_count - 1) {
                append_json(json, size, capacity, ",\n");
            } else {
                append_json(json, size, capacity, "\n");
            }
        }

        snprintf(buffer, sizeof(buffer), "%s  ]\n", indent);
        append_json(json, size, capacity, buffer);
    } else {
        // Remove trailing comma if no children
        if ((*json)[*size - 2] == ',') {
            (*json)[*size - 2] = '\n';
            (*size)--;
        }
    }

    snprintf(buffer, sizeof(buffer), "%s}", indent);
    append_json(json, size, capacity, buffer);
}

// JSON Serialization
char* ir_serialize_json(IRComponent* root) {
    if (!root) return strdup("null");

    size_t capacity = 4096;
    size_t size = 0;
    char* json = malloc(capacity);
    if (!json) return NULL;

    json[0] = '\0';

    serialize_component_json_recursive(root, &json, &size, &capacity, 0);

    append_json(&json, &size, &capacity, "\n");

    return json;
}

IRComponent* ir_deserialize_json(const char* json_string) {
    // This is a placeholder for JSON deserialization
    // In a full implementation, you'd parse the JSON string and build IR components
    return NULL;
}

// File I/O for JSON
bool ir_write_json_file(IRComponent* root, const char* filename) {
    char* json = ir_serialize_json(root);
    if (!json) return false;

    FILE* file = fopen(filename, "w");
    if (!file) {
        free(json);
        return false;
    }

    bool success = (fprintf(file, "%s", json) >= 0);
    fclose(file);
    free(json);

    return success;
}

IRComponent* ir_read_json_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* json = malloc(file_size + 1);
    if (!json) {
        fclose(file);
        return NULL;
    }

    if (fread(json, 1, file_size, file) != file_size) {
        free(json);
        fclose(file);
        return NULL;
    }
    json[file_size] = '\0';
    fclose(file);

    IRComponent* root = ir_deserialize_json(json);
    free(json);

    return root;
}

// Utility Functions
size_t ir_calculate_serialized_size(IRComponent* root) {
    IRBuffer* buffer = ir_serialize_binary(root);
    if (!buffer) return 0;

    size_t size = buffer->size;
    ir_buffer_destroy(buffer);
    return size;
}

void ir_print_component_info(IRComponent* component, int depth) {
    if (!component) return;

    // Print indentation
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    printf("%s (ID: %u)", ir_component_type_to_string(component->type), component->id);
    if (component->text_content) {
        printf(" Text: \"%s\"", component->text_content);
    }
    if (component->tag) {
        printf(" Tag: \"%s\"", component->tag);
    }
    printf("\n");

    // Print children recursively
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_print_component_info(component->children[i], depth + 1);
    }
}

void ir_print_buffer_info(IRBuffer* buffer) {
    if (!buffer) {
        printf("Buffer: NULL\n");
        return;
    }

    printf("Buffer Info:\n");
    printf("  Size: %zu bytes\n", buffer->size);
    printf("  Capacity: %zu bytes\n", buffer->capacity);
    printf("  Usage: %.1f%%\n", buffer->capacity > 0 ? (100.0 * buffer->size / buffer->capacity) : 0.0);
}

bool ir_validate_binary_format(IRBuffer* buffer) {
    if (!buffer || buffer->size < 16) return false;

    // Check magic number and version by attempting to read header
    size_t original_size = buffer->size;
    uint8_t* original_data = buffer->data;

    uint32_t magic;
    uint16_t version_major, version_minor;
    uint32_t endianness_check;

    if (!read_uint32(buffer, &magic)) return false;
    if (!read_uint16(buffer, &version_major)) return false;
    if (!read_uint16(buffer, &version_minor)) return false;
    if (!read_uint32(buffer, &endianness_check)) return false;

    // Restore buffer position
    buffer->data = original_data;
    buffer->size = original_size;

    return (magic == IR_MAGIC_NUMBER &&
            version_major == IR_FORMAT_VERSION_MAJOR &&
            endianness_check == IR_ENDIANNESS_CHECK);
}

bool ir_validate_json_format(const char* json_string) {
    // Simplified JSON validation - just check if it starts with {
    return json_string && json_string[0] == '{';
}