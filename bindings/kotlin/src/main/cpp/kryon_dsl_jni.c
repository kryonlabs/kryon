#include <jni.h>
#include <android/log.h>
#include <stdlib.h>
#include <string.h>

// Include Kryon headers
#include "android_internal.h"
#include "ir_core.h"
#include "ir_builder.h"

#define LOG_TAG "KryonDSL_JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// ============================================================================
// DSL Build Context (extends KryonNativeContext)
// ============================================================================

#define MAX_COMPONENTS 1024
#define MAX_PARENT_STACK 64

typedef struct {
    IRComponent* components[MAX_COMPONENTS];  // Component registry
    uint32_t component_count;

    IRComponent* parent_stack[MAX_PARENT_STACK];  // Parent stack for tree building
    uint32_t stack_depth;

    IRComponent* root;  // Root component
    IRContext* ir_context;  // IR context for this build
} DSLBuildContext;

// Forward declaration of KryonNativeContext (defined in kryon_jni.c)
typedef struct {
    void* activity_ref;
    void* jvm;
    void* window;
    void* renderer;
    void* ir_renderer;
    bool initialized;
    bool surface_ready;
    DSLBuildContext* dsl_context;  // DSL build context
} KryonNativeContext;

// ============================================================================
// DSL Build Context Management
// ============================================================================

static DSLBuildContext* dsl_context_create(void) {
    DSLBuildContext* ctx = (DSLBuildContext*)calloc(1, sizeof(DSLBuildContext));
    if (!ctx) {
        LOGE("Failed to allocate DSL build context");
        return NULL;
    }

    // Create IR context for this build
    ctx->ir_context = ir_create_context();
    if (!ctx->ir_context) {
        LOGE("Failed to create IR context");
        free(ctx);
        return NULL;
    }

    ir_set_context(ctx->ir_context);

    ctx->component_count = 0;
    ctx->stack_depth = 0;
    ctx->root = NULL;

    LOGI("DSL build context created");
    return ctx;
}

static void dsl_context_destroy(DSLBuildContext* ctx) {
    if (!ctx) return;

    // IR context cleanup is handled by ir_destroy_context
    if (ctx->ir_context) {
        ir_destroy_context(ctx->ir_context);
    }

    free(ctx);
    LOGI("DSL build context destroyed");
}

static int dsl_register_component(DSLBuildContext* ctx, IRComponent* component) {
    if (ctx->component_count >= MAX_COMPONENTS) {
        LOGE("Component registry full");
        return -1;
    }

    ctx->components[ctx->component_count] = component;
    int id = ctx->component_count;
    ctx->component_count++;

    // If this is the first component, set it as root
    if (ctx->component_count == 1) {
        ctx->root = component;
        ctx->parent_stack[0] = component;
        ctx->stack_depth = 1;
    } else {
        // Add as child to current parent
        if (ctx->stack_depth > 0) {
            IRComponent* parent = ctx->parent_stack[ctx->stack_depth - 1];
            ir_add_child(parent, component);
        }

        // Push onto parent stack if it's a container type
        if (component->type == IR_COMPONENT_CONTAINER ||
            component->type == IR_COMPONENT_ROW ||
            component->type == IR_COMPONENT_COLUMN ||
            component->type == IR_COMPONENT_CENTER) {
            if (ctx->stack_depth < MAX_PARENT_STACK) {
                ctx->parent_stack[ctx->stack_depth] = component;
                ctx->stack_depth++;
            }
        }
    }

    return id;
}

static IRComponent* dsl_get_component(DSLBuildContext* ctx, int id) {
    if (id < 0 || id >= (int)ctx->component_count) {
        return NULL;
    }
    return ctx->components[id];
}

// Helper to parse hex color string to RGBA
static void parse_color(const char* color_str, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    if (!color_str || strlen(color_str) < 7) {
        *r = *g = *b = 0;
        *a = 255;
        return;
    }

    // Skip '#' if present
    const char* hex = (color_str[0] == '#') ? color_str + 1 : color_str;

    unsigned int rgba = 0;
    sscanf(hex, "%x", &rgba);

    if (strlen(hex) == 6) {
        // RGB format
        *r = (rgba >> 16) & 0xFF;
        *g = (rgba >> 8) & 0xFF;
        *b = rgba & 0xFF;
        *a = 255;
    } else if (strlen(hex) == 8) {
        // RGBA format
        *r = (rgba >> 24) & 0xFF;
        *g = (rgba >> 16) & 0xFF;
        *b = (rgba >> 8) & 0xFF;
        *a = rgba & 0xFF;
    }
}

// ============================================================================
// Component Creation JNI Methods
// ============================================================================

JNIEXPORT jint JNICALL
Java_com_kryon_dsl_ContainerBuilder_nativeCreateContainer(JNIEnv* env, jobject thiz, jlong handle) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) {
        LOGE("Invalid context in nativeCreateContainer");
        return -1;
    }

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = ir_create_component(IR_COMPONENT_CONTAINER);
    if (!component) {
        LOGE("Failed to create container component");
        return -1;
    }

    int id = dsl_register_component(dsl, component);
    LOGD("Created Container with ID: %d", id);
    return id;
}

JNIEXPORT jint JNICALL
Java_com_kryon_dsl_RowBuilder_nativeCreateRow(JNIEnv* env, jobject thiz, jlong handle) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return -1;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = ir_create_component(IR_COMPONENT_ROW);
    if (!component) return -1;

    int id = dsl_register_component(dsl, component);
    LOGD("Created Row with ID: %d", id);
    return id;
}

JNIEXPORT jint JNICALL
Java_com_kryon_dsl_ColumnBuilder_nativeCreateColumn(JNIEnv* env, jobject thiz, jlong handle) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return -1;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = ir_create_component(IR_COMPONENT_COLUMN);
    if (!component) return -1;

    int id = dsl_register_component(dsl, component);
    LOGD("Created Column with ID: %d", id);
    return id;
}

JNIEXPORT jint JNICALL
Java_com_kryon_dsl_CenterBuilder_nativeCreateCenter(JNIEnv* env, jobject thiz, jlong handle) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return -1;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = ir_create_component(IR_COMPONENT_CENTER);
    if (!component) return -1;

    int id = dsl_register_component(dsl, component);
    LOGD("Created Center with ID: %d", id);
    return id;
}

JNIEXPORT jint JNICALL
Java_com_kryon_dsl_TextBuilder_nativeCreateText(JNIEnv* env, jobject thiz, jlong handle) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return -1;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = ir_create_component(IR_COMPONENT_TEXT);
    if (!component) return -1;

    int id = dsl_register_component(dsl, component);
    LOGD("Created Text with ID: %d", id);
    return id;
}

JNIEXPORT jint JNICALL
Java_com_kryon_dsl_ButtonBuilder_nativeCreateButton(JNIEnv* env, jobject thiz, jlong handle) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return -1;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = ir_create_component(IR_COMPONENT_BUTTON);
    if (!component) return -1;

    int id = dsl_register_component(dsl, component);
    LOGD("Created Button with ID: %d", id);
    return id;
}

JNIEXPORT jint JNICALL
Java_com_kryon_dsl_InputBuilder_nativeCreateInput(JNIEnv* env, jobject thiz, jlong handle) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return -1;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = ir_create_component(IR_COMPONENT_INPUT);
    if (!component) return -1;

    int id = dsl_register_component(dsl, component);
    LOGD("Created Input with ID: %d", id);
    return id;
}

// ============================================================================
// Layout Property JNI Methods
// ============================================================================

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetWidth(JNIEnv* env, jobject thiz,
                                                    jlong handle, jint componentId, jfloat value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (component) {
        ir_set_width(component, IR_DIMENSION_PX, value);
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetHeight(JNIEnv* env, jobject thiz,
                                                     jlong handle, jint componentId, jfloat value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (component) {
        ir_set_height(component, IR_DIMENSION_PX, value);
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetPadding(JNIEnv* env, jobject thiz,
                                                      jlong handle, jint componentId,
                                                      jfloat top, jfloat right, jfloat bottom, jfloat left) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (component) {
        ir_set_padding(component, top, right, bottom, left);
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetMargin(JNIEnv* env, jobject thiz,
                                                     jlong handle, jint componentId,
                                                     jfloat top, jfloat right, jfloat bottom, jfloat left) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (component) {
        ir_set_margin(component, top, right, bottom, left);
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetGap(JNIEnv* env, jobject thiz,
                                                  jlong handle, jint componentId, jfloat value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (component && component->layout) {
        // Gap is set on flexbox layout
        component->layout->flex.gap = (uint32_t)value;
    }
}

// ============================================================================
// Color Property JNI Methods
// ============================================================================

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetBackground(JNIEnv* env, jobject thiz,
                                                         jlong handle, jint componentId, jstring color) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component || !component->style) return;

    const char* color_str = (*env)->GetStringUTFChars(env, color, NULL);
    if (color_str) {
        uint8_t r, g, b, a;
        parse_color(color_str, &r, &g, &b, &a);
        ir_set_background_color(component->style, r, g, b, a);
        (*env)->ReleaseStringUTFChars(env, color, color_str);
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetColor(JNIEnv* env, jobject thiz,
                                                    jlong handle, jint componentId, jstring color) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component || !component->style) return;

    const char* color_str = (*env)->GetStringUTFChars(env, color, NULL);
    if (color_str) {
        uint8_t r, g, b, a;
        parse_color(color_str, &r, &g, &b, &a);
        ir_set_font_color(component->style, r, g, b, a);
        (*env)->ReleaseStringUTFChars(env, color, color_str);
    }
}

// ============================================================================
// Flexbox Layout JNI Methods
// ============================================================================

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetJustifyContent(JNIEnv* env, jobject thiz,
                                                             jlong handle, jint componentId, jstring value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component || !component->layout) return;

    const char* value_str = (*env)->GetStringUTFChars(env, value, NULL);
    if (value_str) {
        IRAlignment alignment = IR_ALIGNMENT_START;
        if (strcmp(value_str, "center") == 0) alignment = IR_ALIGNMENT_CENTER;
        else if (strcmp(value_str, "end") == 0) alignment = IR_ALIGNMENT_END;
        else if (strcmp(value_str, "space-between") == 0) alignment = IR_ALIGNMENT_SPACE_BETWEEN;
        else if (strcmp(value_str, "space-around") == 0) alignment = IR_ALIGNMENT_SPACE_AROUND;

        ir_set_justify_content(component->layout, alignment);
        (*env)->ReleaseStringUTFChars(env, value, value_str);
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetAlignItems(JNIEnv* env, jobject thiz,
                                                         jlong handle, jint componentId, jstring value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component || !component->layout) return;

    const char* value_str = (*env)->GetStringUTFChars(env, value, NULL);
    if (value_str) {
        IRAlignment alignment = IR_ALIGNMENT_START;
        if (strcmp(value_str, "center") == 0) alignment = IR_ALIGNMENT_CENTER;
        else if (strcmp(value_str, "end") == 0) alignment = IR_ALIGNMENT_END;
        else if (strcmp(value_str, "stretch") == 0) alignment = IR_ALIGNMENT_STRETCH;

        ir_set_align_items(component->layout, alignment);
        (*env)->ReleaseStringUTFChars(env, value, value_str);
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetFlexDirection(JNIEnv* env, jobject thiz,
                                                            jlong handle, jint componentId, jstring value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component || !component->layout) return;

    const char* value_str = (*env)->GetStringUTFChars(env, value, NULL);
    if (value_str) {
        uint8_t direction = 0;  // Row
        if (strcmp(value_str, "column") == 0) direction = 1;

        ir_set_flex_properties(component->layout, 0, 0, direction);
        (*env)->ReleaseStringUTFChars(env, value, value_str);
    }
}

// ============================================================================
// Typography JNI Methods
// ============================================================================

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetFontSize(JNIEnv* env, jobject thiz,
                                                       jlong handle, jint componentId, jfloat size) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (component && component->style) {
        ir_set_font_size(component->style, size);
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetFontWeight(JNIEnv* env, jobject thiz,
                                                         jlong handle, jint componentId, jint weight) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (component && component->style) {
        ir_set_font_weight(component->style, (uint16_t)weight);
    }
}

// ============================================================================
// Positioning JNI Methods
// ============================================================================

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetPosition(JNIEnv* env, jobject thiz,
                                                       jlong handle, jint componentId, jstring value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component || !component->style) return;

    const char* value_str = (*env)->GetStringUTFChars(env, value, NULL);
    if (value_str) {
        // Position handling - store in style flags or custom field
        // For now, just log it
        LOGD("Set position: %s for component %d", value_str, componentId);
        (*env)->ReleaseStringUTFChars(env, value, value_str);
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetLeft(JNIEnv* env, jobject thiz,
                                                   jlong handle, jint componentId, jfloat value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (component && component->style) {
        component->style->absolute_x = value;
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetTop(JNIEnv* env, jobject thiz,
                                                  jlong handle, jint componentId, jfloat value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (component && component->style) {
        component->style->absolute_y = value;
    }
}

// ============================================================================
// Border JNI Methods
// ============================================================================

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativeSetBorder(JNIEnv* env, jobject thiz,
                                                     jlong handle, jint componentId,
                                                     jfloat width, jstring color, jfloat radius) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component || !component->style) return;

    const char* color_str = (*env)->GetStringUTFChars(env, color, NULL);
    if (color_str) {
        uint8_t r, g, b, a;
        parse_color(color_str, &r, &g, &b, &a);
        ir_set_border(component->style, width, r, g, b, a, (uint8_t)radius);
        (*env)->ReleaseStringUTFChars(env, color, color_str);
    }
}

// ============================================================================
// Component-Specific JNI Methods
// ============================================================================

JNIEXPORT void JNICALL
Java_com_kryon_dsl_TextBuilder_nativeSetText(JNIEnv* env, jobject thiz,
                                              jlong handle, jint componentId, jstring value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component) return;

    const char* text = (*env)->GetStringUTFChars(env, value, NULL);
    if (text) {
        ir_set_text_content(component, text);
        (*env)->ReleaseStringUTFChars(env, value, text);
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ButtonBuilder_nativeSetButtonText(JNIEnv* env, jobject thiz,
                                                      jlong handle, jint componentId, jstring value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component) return;

    const char* text = (*env)->GetStringUTFChars(env, value, NULL);
    if (text) {
        ir_set_text_content(component, text);
        (*env)->ReleaseStringUTFChars(env, value, text);
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_InputBuilder_nativeSetPlaceholder(JNIEnv* env, jobject thiz,
                                                      jlong handle, jint componentId, jstring value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component) return;

    const char* placeholder = (*env)->GetStringUTFChars(env, value, NULL);
    if (placeholder) {
        // Store placeholder in custom_data for now
        ir_set_custom_data(component, placeholder);
        (*env)->ReleaseStringUTFChars(env, value, placeholder);
    }
}

JNIEXPORT void JNICALL
Java_com_kryon_dsl_InputBuilder_nativeSetInputValue(JNIEnv* env, jobject thiz,
                                                     jlong handle, jint componentId, jstring value) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component) return;

    const char* text = (*env)->GetStringUTFChars(env, value, NULL);
    if (text) {
        ir_set_text_content(component, text);
        (*env)->ReleaseStringUTFChars(env, value, text);
    }
}

// ============================================================================
// Event Handler Registration
// ============================================================================

JNIEXPORT void JNICALL
Java_com_kryon_dsl_InputBuilder_nativeRegisterTextChangeCallback(JNIEnv* env, jobject thiz,
                                                                   jlong handle, jint componentId,
                                                                   jobject callback) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component) return;

    // Create a global reference to the callback
    jobject globalCallback = (*env)->NewGlobalRef(env, callback);
    if (globalCallback) {
        LOGI("Registered onChanged handler for Input component %d", componentId);

        // Create text change event
        IREvent* event = ir_create_event(IR_EVENT_TEXT_CHANGE, "text_change_handler", NULL);
        if (event) {
            ir_add_event(component, event);
        }
    }
}

JNIEXPORT jint JNICALL
Java_com_kryon_dsl_ButtonBuilder_nativeRegisterClickCallback(JNIEnv* env, jobject thiz,
                                                               jlong handle, jint componentId,
                                                               jobject callback) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return -1;

    DSLBuildContext* dsl = ctx->dsl_context;
    ir_set_context(dsl->ir_context);

    IRComponent* component = dsl_get_component(dsl, componentId);
    if (!component) {
        LOGE("Failed to find component %d for onClick handler", componentId);
        return -1;
    }

    // Create a global reference to the callback (Kotlin lambda)
    jobject globalCallback = (*env)->NewGlobalRef(env, callback);
    if (!globalCallback) {
        LOGE("Failed to create global reference for callback");
        return -1;
    }

    // Store the callback reference in the component's custom_data temporarily
    // In a full implementation, we'd have a proper callback registry
    // For now, we'll just log that the handler was registered
    LOGI("Registered onClick handler for component %d", componentId);

    // Create an event for this component
    IREvent* event = ir_create_event(IR_EVENT_CLICK, "click_handler", NULL);
    if (event) {
        ir_add_event(component, event);
        LOGI("Added click event to component %d", componentId);
    }

    // Return a callback ID (for now, just use the component ID)
    return componentId;
}

// ============================================================================
// Parent Stack Management
// ============================================================================

JNIEXPORT void JNICALL
Java_com_kryon_dsl_ComponentBuilder_nativePopParent(JNIEnv* env, jobject thiz, jlong handle) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) return;

    DSLBuildContext* dsl = ctx->dsl_context;
    if (dsl->stack_depth > 1) {  // Never pop the root
        dsl->stack_depth--;
        LOGD("Popped parent stack, depth now: %d", dsl->stack_depth);
    }
}

// ============================================================================
// DSL Session Management (called from KryonActivity)
// ============================================================================

// Called at start of setContent()
JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeBeginDSLBuild(JNIEnv* env, jobject thiz, jlong handle) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx) {
        LOGE("Invalid context in nativeBeginDSLBuild");
        return;
    }

    // Clean up previous DSL context if exists
    if (ctx->dsl_context) {
        dsl_context_destroy(ctx->dsl_context);
    }

    // Create new DSL build context
    ctx->dsl_context = dsl_context_create();
    LOGI("DSL build session started");
}

// Called at end of setContent() to finalize the tree
JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeFinalizeContent(JNIEnv* env, jobject thiz, jlong handle) {
    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    if (!ctx || !ctx->dsl_context) {
        LOGE("Invalid context in nativeFinalizeContent");
        return;
    }

    DSLBuildContext* dsl = ctx->dsl_context;

    if (!dsl->root) {
        LOGE("No root component in DSL build");
        return;
    }

    // Set the root in the IR context
    ir_set_context(dsl->ir_context);
    ir_set_root(dsl->root);

    // Pass the component tree to the renderer
    if (ctx->ir_renderer) {
        LOGI("Setting component tree in renderer");
        android_ir_renderer_set_root(ctx->ir_renderer, dsl->root);
        android_ir_renderer_render(ctx->ir_renderer);
    } else {
        LOGI("Renderer not initialized yet, will render when surface is ready");
        // Component tree will be rendered when surface is created
    }

    LOGI("DSL build finalized with %d components", dsl->component_count);
    LOGI("Root component type: %d", dsl->root->type);
}
