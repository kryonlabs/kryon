#include "android_platform.h"
#include <string.h>
#include <stdio.h>

#ifdef __ANDROID__
#include <android/input.h>
#endif

// ============================================================================
// Input State Management
// ============================================================================

typedef struct {
    bool initialized;
    bool keyboard_visible;

    // Touch tracking
    struct {
        bool active;
        float x, y;
        float last_x, last_y;
        uint64_t last_event_time;
    } touch_points[KRYON_ANDROID_MAX_TOUCH_POINTS];

    // Event queue (circular buffer)
    kryon_android_event_t event_queue[64];
    size_t event_head;
    size_t event_tail;
    size_t event_count;

} kryon_android_input_state_t;

static kryon_android_input_state_t g_input_state = {0};

// ============================================================================
// Internal Helper Functions
// ============================================================================

static void push_event(const kryon_android_event_t* event) {
    if (g_input_state.event_count >= 64) {
        // Queue full, drop oldest event
        g_input_state.event_head = (g_input_state.event_head + 1) % 64;
        g_input_state.event_count--;
        KRYON_ANDROID_LOG_WARN("Event queue overflow, dropping old event\n");
    }

    g_input_state.event_queue[g_input_state.event_tail] = *event;
    g_input_state.event_tail = (g_input_state.event_tail + 1) % 64;
    g_input_state.event_count++;
}

static bool pop_event(kryon_android_event_t* event) {
    if (g_input_state.event_count == 0) {
        return false;
    }

    *event = g_input_state.event_queue[g_input_state.event_head];
    g_input_state.event_head = (g_input_state.event_head + 1) % 64;
    g_input_state.event_count--;
    return true;
}

static int32_t find_touch_point_by_id(int32_t pointer_id) {
    for (int32_t i = 0; i < KRYON_ANDROID_MAX_TOUCH_POINTS; i++) {
        if (g_input_state.touch_points[i].active) {
            // For now, we'll use array index as pointer ID
            // In a real implementation, we'd track the actual pointer ID
            return i;
        }
    }
    return -1;
}

static int32_t allocate_touch_point(void) {
    for (int32_t i = 0; i < KRYON_ANDROID_MAX_TOUCH_POINTS; i++) {
        if (!g_input_state.touch_points[i].active) {
            g_input_state.touch_points[i].active = true;
            return i;
        }
    }
    return -1;
}

static void release_touch_point(int32_t index) {
    if (index >= 0 && index < KRYON_ANDROID_MAX_TOUCH_POINTS) {
        g_input_state.touch_points[index].active = false;
    }
}

// ============================================================================
// Input Event Processing
// ============================================================================

#ifdef __ANDROID__
static bool process_motion_event(AInputEvent* event) {
    int32_t action = AMotionEvent_getAction(event);
    int32_t action_masked = action & AMOTION_EVENT_ACTION_MASK;
    int32_t pointer_index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                            AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    size_t pointer_count = AMotionEvent_getPointerCount(event);
    uint64_t event_time = AMotionEvent_getEventTime(event) / 1000000; // ns to ms

    kryon_android_event_t kryon_event = {0};
    kryon_event.type = KRYON_ANDROID_EVENT_TOUCH;

    switch (action_masked) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN: {
            // Touch down
            int32_t slot = allocate_touch_point();
            if (slot < 0) {
                KRYON_ANDROID_LOG_WARN("No free touch point slots\n");
                return false;
            }

            float x = AMotionEvent_getX(event, pointer_index);
            float y = AMotionEvent_getY(event, pointer_index);
            float pressure = AMotionEvent_getPressure(event, pointer_index);
            float size = AMotionEvent_getSize(event, pointer_index);

            g_input_state.touch_points[slot].x = x;
            g_input_state.touch_points[slot].y = y;
            g_input_state.touch_points[slot].last_x = x;
            g_input_state.touch_points[slot].last_y = y;
            g_input_state.touch_points[slot].last_event_time = event_time;

            kryon_event.data.touch.pointer_id = slot;
            kryon_event.data.touch.x = x;
            kryon_event.data.touch.y = y;
            kryon_event.data.touch.pressure = pressure;
            kryon_event.data.touch.size = size;
            kryon_event.data.touch.action = KRYON_ANDROID_TOUCH_DOWN;
            kryon_event.data.touch.timestamp = event_time;

            push_event(&kryon_event);

            KRYON_ANDROID_LOG_DEBUG("Touch DOWN: id=%d, pos=(%.1f, %.1f)\n", slot, x, y);
            return true;
        }

        case AMOTION_EVENT_ACTION_MOVE: {
            // Touch move (can have multiple pointers)
            for (size_t i = 0; i < pointer_count && i < KRYON_ANDROID_MAX_TOUCH_POINTS; i++) {
                if (!g_input_state.touch_points[i].active) continue;

                float x = AMotionEvent_getX(event, i);
                float y = AMotionEvent_getY(event, i);
                float pressure = AMotionEvent_getPressure(event, i);
                float size = AMotionEvent_getSize(event, i);

                // Only generate event if position changed significantly
                float dx = x - g_input_state.touch_points[i].last_x;
                float dy = y - g_input_state.touch_points[i].last_y;
                if (dx * dx + dy * dy > 1.0f) {  // 1px threshold
                    g_input_state.touch_points[i].x = x;
                    g_input_state.touch_points[i].y = y;
                    g_input_state.touch_points[i].last_x = x;
                    g_input_state.touch_points[i].last_y = y;
                    g_input_state.touch_points[i].last_event_time = event_time;

                    kryon_event.data.touch.pointer_id = (int32_t)i;
                    kryon_event.data.touch.x = x;
                    kryon_event.data.touch.y = y;
                    kryon_event.data.touch.pressure = pressure;
                    kryon_event.data.touch.size = size;
                    kryon_event.data.touch.action = KRYON_ANDROID_TOUCH_MOVE;
                    kryon_event.data.touch.timestamp = event_time;

                    push_event(&kryon_event);
                }
            }
            return true;
        }

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP: {
            // Touch up
            int32_t pointer_id = AMotionEvent_getPointerId(event, pointer_index);
            int32_t slot = find_touch_point_by_id(pointer_id);

            if (slot < 0) {
                KRYON_ANDROID_LOG_WARN("Touch UP for unknown pointer\n");
                return false;
            }

            float x = AMotionEvent_getX(event, pointer_index);
            float y = AMotionEvent_getY(event, pointer_index);
            float pressure = AMotionEvent_getPressure(event, pointer_index);
            float size = AMotionEvent_getSize(event, pointer_index);

            kryon_event.data.touch.pointer_id = slot;
            kryon_event.data.touch.x = x;
            kryon_event.data.touch.y = y;
            kryon_event.data.touch.pressure = pressure;
            kryon_event.data.touch.size = size;
            kryon_event.data.touch.action = KRYON_ANDROID_TOUCH_UP;
            kryon_event.data.touch.timestamp = event_time;

            push_event(&kryon_event);

            release_touch_point(slot);

            KRYON_ANDROID_LOG_DEBUG("Touch UP: id=%d, pos=(%.1f, %.1f)\n", slot, x, y);
            return true;
        }

        case AMOTION_EVENT_ACTION_CANCEL: {
            // Touch cancelled - release all touch points
            for (int32_t i = 0; i < KRYON_ANDROID_MAX_TOUCH_POINTS; i++) {
                if (g_input_state.touch_points[i].active) {
                    kryon_event.data.touch.pointer_id = i;
                    kryon_event.data.touch.x = g_input_state.touch_points[i].x;
                    kryon_event.data.touch.y = g_input_state.touch_points[i].y;
                    kryon_event.data.touch.pressure = 0.0f;
                    kryon_event.data.touch.size = 0.0f;
                    kryon_event.data.touch.action = KRYON_ANDROID_TOUCH_CANCEL;
                    kryon_event.data.touch.timestamp = event_time;

                    push_event(&kryon_event);

                    release_touch_point(i);
                }
            }

            KRYON_ANDROID_LOG_DEBUG("Touch CANCEL\n");
            return true;
        }

        default:
            return false;
    }
}

static bool process_key_event(AInputEvent* event) {
    int32_t action = AKeyEvent_getAction(event);
    int32_t key_code = AKeyEvent_getKeyCode(event);
    int32_t meta_state = AKeyEvent_getMetaState(event);
    uint64_t event_time = AKeyEvent_getEventTime(event) / 1000000; // ns to ms

    kryon_android_event_t kryon_event = {0};
    kryon_event.type = KRYON_ANDROID_EVENT_KEY;
    kryon_event.data.key.key_code = key_code;
    kryon_event.data.key.meta_state = (uint32_t)meta_state;
    kryon_event.data.key.timestamp = event_time;

    switch (action) {
        case AKEY_EVENT_ACTION_DOWN:
            kryon_event.data.key.action = KRYON_ANDROID_KEY_DOWN;
            break;
        case AKEY_EVENT_ACTION_UP:
            kryon_event.data.key.action = KRYON_ANDROID_KEY_UP;
            break;
        case AKEY_EVENT_ACTION_MULTIPLE:
            kryon_event.data.key.action = KRYON_ANDROID_KEY_REPEAT;
            break;
        default:
            return false;
    }

    push_event(&kryon_event);

    KRYON_ANDROID_LOG_DEBUG("Key event: code=%d, action=%d\n", key_code, action);
    return true;
}

bool kryon_android_process_input_event(AInputEvent* event) {
    if (!event) return false;

    int32_t event_type = AInputEvent_getType(event);

    switch (event_type) {
        case AINPUT_EVENT_TYPE_MOTION:
            return process_motion_event(event);

        case AINPUT_EVENT_TYPE_KEY:
            return process_key_event(event);

        default:
            KRYON_ANDROID_LOG_WARN("Unknown input event type: %d\n", event_type);
            return false;
    }
}
#endif // __ANDROID__

// ============================================================================
// Public API Implementation
// ============================================================================

bool kryon_android_poll_events(kryon_android_event_t* events, size_t* count, size_t max_events) {
    if (!events || !count) {
        KRYON_ANDROID_LOG_ERROR("Invalid parameters\n");
        return false;
    }

    *count = 0;

    while (*count < max_events && g_input_state.event_count > 0) {
        if (pop_event(&events[*count])) {
            (*count)++;
        }
    }

    return true;
}

// ============================================================================
// Soft Keyboard (IME) Control
// ============================================================================

void kryon_android_show_keyboard(void) {
    KRYON_ANDROID_LOG_INFO("Show keyboard requested\n");

#ifdef __ANDROID__
    // Get JNI environment
    JNIEnv* env = kryon_android_get_jni_env();
    if (!env) {
        KRYON_ANDROID_LOG_ERROR("Failed to get JNI environment\n");
        return;
    }

    jobject activity = kryon_android_get_activity_object();
    if (!activity) {
        KRYON_ANDROID_LOG_ERROR("Failed to get activity object\n");
        return;
    }

    // Get InputMethodManager
    jclass activity_class = (*env)->GetObjectClass(env, activity);
    jmethodID get_system_service = (*env)->GetMethodID(env, activity_class,
        "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");

    jstring service_name = (*env)->NewStringUTF(env, "input_method");
    jobject imm = (*env)->CallObjectMethod(env, activity, get_system_service, service_name);
    (*env)->DeleteLocalRef(env, service_name);

    if (!imm) {
        KRYON_ANDROID_LOG_ERROR("Failed to get InputMethodManager\n");
        return;
    }

    // Call showSoftInput
    jclass imm_class = (*env)->GetObjectClass(env, imm);
    jmethodID show_soft_input = (*env)->GetMethodID(env, imm_class,
        "showSoftInput", "(Landroid/view/View;I)Z");

    // Get current window's decor view
    jmethodID get_window = (*env)->GetMethodID(env, activity_class,
        "getWindow", "()Landroid/view/Window;");
    jobject window = (*env)->CallObjectMethod(env, activity, get_window);

    jclass window_class = (*env)->GetObjectClass(env, window);
    jmethodID get_decor_view = (*env)->GetMethodID(env, window_class,
        "getDecorView", "()Landroid/view/View;");
    jobject decor_view = (*env)->CallObjectMethod(env, window, get_decor_view);

    (*env)->CallBooleanMethod(env, imm, show_soft_input, decor_view, 0);

    g_input_state.keyboard_visible = true;
#else
    g_input_state.keyboard_visible = true;
#endif
}

void kryon_android_hide_keyboard(void) {
    KRYON_ANDROID_LOG_INFO("Hide keyboard requested\n");

#ifdef __ANDROID__
    // Get JNI environment
    JNIEnv* env = kryon_android_get_jni_env();
    if (!env) {
        KRYON_ANDROID_LOG_ERROR("Failed to get JNI environment\n");
        return;
    }

    jobject activity = kryon_android_get_activity_object();
    if (!activity) {
        KRYON_ANDROID_LOG_ERROR("Failed to get activity object\n");
        return;
    }

    // Get InputMethodManager
    jclass activity_class = (*env)->GetObjectClass(env, activity);
    jmethodID get_system_service = (*env)->GetMethodID(env, activity_class,
        "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");

    jstring service_name = (*env)->NewStringUTF(env, "input_method");
    jobject imm = (*env)->CallObjectMethod(env, activity, get_system_service, service_name);
    (*env)->DeleteLocalRef(env, service_name);

    if (!imm) {
        KRYON_ANDROID_LOG_ERROR("Failed to get InputMethodManager\n");
        return;
    }

    // Call hideSoftInputFromWindow
    jclass imm_class = (*env)->GetObjectClass(env, imm);
    jmethodID hide_soft_input = (*env)->GetMethodID(env, imm_class,
        "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z");

    // Get current window's token
    jmethodID get_window = (*env)->GetMethodID(env, activity_class,
        "getWindow", "()Landroid/view/Window;");
    jobject window = (*env)->CallObjectMethod(env, activity, get_window);

    jclass window_class = (*env)->GetObjectClass(env, window);
    jmethodID get_decor_view = (*env)->GetMethodID(env, window_class,
        "getDecorView", "()Landroid/view/View;");
    jobject decor_view = (*env)->CallObjectMethod(env, window, get_decor_view);

    jclass view_class = (*env)->GetObjectClass(env, decor_view);
    jmethodID get_window_token = (*env)->GetMethodID(env, view_class,
        "getWindowToken", "()Landroid/os/IBinder;");
    jobject token = (*env)->CallObjectMethod(env, decor_view, get_window_token);

    (*env)->CallBooleanMethod(env, imm, hide_soft_input, token, 0);

    g_input_state.keyboard_visible = false;
#else
    g_input_state.keyboard_visible = false;
#endif
}

bool kryon_android_is_keyboard_visible(void) {
    return g_input_state.keyboard_visible;
}

// ============================================================================
// Permissions Management (Stubs for now)
// ============================================================================

bool kryon_android_request_permission(kryon_android_permission_t permission) {
    KRYON_ANDROID_LOG_INFO("Permission request: %d\n", permission);
    // TODO: Implement via JNI ActivityCompat.requestPermissions()
    (void)permission;
    return false;
}

bool kryon_android_has_permission(kryon_android_permission_t permission) {
    // TODO: Implement via JNI ContextCompat.checkSelfPermission()
    (void)permission;
    return false;
}

// ============================================================================
// Network Status (Stubs for now)
// ============================================================================

bool kryon_android_is_network_available(void) {
    // TODO: Implement via JNI ConnectivityManager
    return true;
}

bool kryon_android_is_wifi_connected(void) {
    // TODO: Implement via JNI ConnectivityManager
    return false;
}

// ============================================================================
// Share Intent (Stub)
// ============================================================================

bool kryon_android_share_text(const char* text, const char* subject) {
    KRYON_ANDROID_LOG_INFO("Share text: %s\n", text);
    // TODO: Implement via JNI Intent.ACTION_SEND
    (void)text;
    (void)subject;
    return false;
}
