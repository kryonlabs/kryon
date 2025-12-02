## Nim bindings for IR Serialization functions

import ir_core

# Buffer management
proc ir_buffer_create*(initial_capacity: csize_t): ptr IRBuffer {.importc, cdecl, header: "ir_serialization.h".}
proc ir_buffer_create_from_file*(filename: cstring): ptr IRBuffer {.importc, cdecl, header: "ir_serialization.h".}
proc ir_buffer_destroy*(buffer: ptr IRBuffer) {.importc, cdecl, header: "ir_serialization.h".}
proc ir_buffer_write*(buffer: ptr IRBuffer; data: pointer; size: csize_t): bool {.importc, cdecl, header: "ir_serialization.h".}
proc ir_buffer_read*(buffer: ptr IRBuffer; data: pointer; size: csize_t): bool {.importc, cdecl, header: "ir_serialization.h".}
proc ir_buffer_seek*(buffer: ptr IRBuffer; position: csize_t): bool {.importc, cdecl, header: "ir_serialization.h".}
proc ir_buffer_tell*(buffer: ptr IRBuffer): csize_t {.importc, cdecl, header: "ir_serialization.h".}
proc ir_buffer_size*(buffer: ptr IRBuffer): csize_t {.importc, cdecl, header: "ir_serialization.h".}

# Validation functions
proc ir_validate_binary_format*(buffer: ptr IRBuffer): bool {.importc, cdecl, header: "ir_serialization.h".}
proc ir_validate_json_format*(json_string: cstring): bool {.importc, cdecl, header: "ir_serialization.h".}

# Binary serialization
proc ir_serialize_binary*(root: ptr IRComponent): ptr IRBuffer {.importc, cdecl, header: "ir_serialization.h".}
proc ir_deserialize_binary*(buffer: ptr IRBuffer): ptr IRComponent {.importc, cdecl, header: "ir_serialization.h".}
proc ir_write_binary_file*(root: ptr IRComponent; filename: cstring): bool {.importc, cdecl, header: "ir_serialization.h".}
proc ir_read_binary_file*(filename: cstring): ptr IRComponent {.importc, cdecl, header: "ir_serialization.h".}

# JSON serialization
proc ir_serialize_json*(root: ptr IRComponent): cstring {.importc, cdecl, header: "ir_serialization.h".}
proc ir_deserialize_json*(json_string: cstring): ptr IRComponent {.importc, cdecl, header: "ir_serialization.h".}
proc ir_write_json_file*(root: ptr IRComponent; filename: cstring): bool {.importc, cdecl, header: "ir_serialization.h".}
proc ir_read_json_file*(filename: cstring): ptr IRComponent {.importc, cdecl, header: "ir_serialization.h".}

# JSON v2 serialization (complete property coverage)
proc ir_serialize_json_v2*(root: ptr IRComponent): cstring {.importc, cdecl, header: "ir_serialization.h".}
proc ir_write_json_v2_file*(root: ptr IRComponent; filename: cstring): bool {.importc, cdecl, header: "ir_serialization.h".}

# JSON v2 deserialization
proc ir_deserialize_json_v2*(json_string: cstring): ptr IRComponent {.importc, cdecl, header: "ir_serialization.h".}
proc ir_read_json_v2_file*(filename: cstring): ptr IRComponent {.importc, cdecl, header: "ir_serialization.h".}

