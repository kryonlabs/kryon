/**
 * @file gesture_recognizer.c
 * @brief Kryon Gesture Recognition Implementation
 */

#include "internal/events.h"
#include "internal/memory.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// GESTURE STATE TRACKING
// =============================================================================

typedef struct {
    KryonGestureType type;
    KryonGestureState state;
    double start_time;
    double last_update_time;
    float start_x, start_y;
    float current_x, current_y;
    float velocity_x, velocity_y;
    float distance;
    float angle;
    uint32_t touch_count;
    uint32_t touch_ids[KRYON_MAX_TOUCHES];
    bool is_valid;
} KryonActiveGesture;

typedef struct {
    // Active gestures
    KryonActiveGesture active_gestures[KRYON_MAX_ACTIVE_GESTURES];
    size_t active_gesture_count;
    
    // Configuration
    KryonGestureConfig config;
    
    // Recognition state
    double last_tap_time;
    float last_tap_x, last_tap_y;
    uint32_t consecutive_taps;
    
    // Multi-touch state
    float pinch_start_distance;
    float rotation_start_angle;
    
    // Callbacks
    KryonGestureCallback callbacks[KRYON_GESTURE_TYPE_COUNT];
    void* callback_contexts[KRYON_GESTURE_TYPE_COUNT];
    
} KryonGestureRecognizer;

static KryonGestureRecognizer g_gesture_recognizer = {0};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static float calculate_distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

static float calculate_angle(float x1, float y1, float x2, float y2) {
    return atan2f(y2 - y1, x2 - x1);
}

static float normalize_angle(float angle) {
    while (angle > M_PI) angle -= 2 * M_PI;
    while (angle < -M_PI) angle += 2 * M_PI;
    return angle;
}

static KryonActiveGesture* find_gesture_by_type(KryonGestureType type) {
    for (size_t i = 0; i < g_gesture_recognizer.active_gesture_count; i++) {
        if (g_gesture_recognizer.active_gestures[i].type == type) {
            return &g_gesture_recognizer.active_gestures[i];
        }
    }
    return NULL;
}

static KryonActiveGesture* create_gesture(KryonGestureType type) {
    if (g_gesture_recognizer.active_gesture_count >= KRYON_MAX_ACTIVE_GESTURES) {
        return NULL;
    }
    
    KryonActiveGesture* gesture = &g_gesture_recognizer.active_gestures[g_gesture_recognizer.active_gesture_count];
    memset(gesture, 0, sizeof(KryonActiveGesture));
    
    gesture->type = type;
    gesture->state = KRYON_GESTURE_STATE_POSSIBLE;
    gesture->start_time = kryon_platform_get_time();
    gesture->last_update_time = gesture->start_time;
    gesture->is_valid = true;
    
    g_gesture_recognizer.active_gesture_count++;
    return gesture;
}

static void remove_gesture(KryonActiveGesture* gesture) {
    if (!gesture) return;
    
    size_t index = gesture - g_gesture_recognizer.active_gestures;
    if (index >= g_gesture_recognizer.active_gesture_count) return;
    
    // Shift remaining gestures
    for (size_t i = index + 1; i < g_gesture_recognizer.active_gesture_count; i++) {
        g_gesture_recognizer.active_gestures[i - 1] = g_gesture_recognizer.active_gestures[i];
    }
    
    g_gesture_recognizer.active_gesture_count--;
}

static void dispatch_gesture_event(KryonActiveGesture* gesture) {
    if (!gesture || !g_gesture_recognizer.callbacks[gesture->type]) return;
    
    KryonGestureEvent event = {0};
    event.type = gesture->type;
    event.state = gesture->state;
    event.timestamp = gesture->last_update_time;
    event.location.x = gesture->current_x;
    event.location.y = gesture->current_y;
    event.start_location.x = gesture->start_x;
    event.start_location.y = gesture->start_y;
    event.velocity.x = gesture->velocity_x;
    event.velocity.y = gesture->velocity_y;
    event.distance = gesture->distance;
    event.angle = gesture->angle;
    event.touch_count = gesture->touch_count;
    
    g_gesture_recognizer.callbacks[gesture->type](&event, g_gesture_recognizer.callback_contexts[gesture->type]);
}

// =============================================================================
// GESTURE RECOGNITION LOGIC
// =============================================================================

static void recognize_tap(const KryonInputEvent* input_event) {
    if (input_event->type == KRYON_INPUT_EVENT_TOUCH_DOWN) {
        double current_time = input_event->timestamp;
        float x = input_event->data.touch.x;
        float y = input_event->data.touch.y;
        
        // Check for multi-tap
        bool is_multi_tap = false;
        if (current_time - g_gesture_recognizer.last_tap_time < g_gesture_recognizer.config.multi_tap_interval) {
            float tap_distance = calculate_distance(x, y, g_gesture_recognizer.last_tap_x, g_gesture_recognizer.last_tap_y);
            if (tap_distance < g_gesture_recognizer.config.tap_max_distance) {
                is_multi_tap = true;
                g_gesture_recognizer.consecutive_taps++;
            }
        }
        
        if (!is_multi_tap) {
            g_gesture_recognizer.consecutive_taps = 1;
        }
        
        KryonActiveGesture* gesture = create_gesture(KRYON_GESTURE_TYPE_TAP);
        if (gesture) {
            gesture->start_x = gesture->current_x = x;
            gesture->start_y = gesture->current_y = y;
            gesture->touch_count = 1;
            gesture->touch_ids[0] = input_event->data.touch.id;
        }
        
        g_gesture_recognizer.last_tap_time = current_time;
        g_gesture_recognizer.last_tap_x = x;
        g_gesture_recognizer.last_tap_y = y;
        
    } else if (input_event->type == KRYON_INPUT_EVENT_TOUCH_UP) {
        KryonActiveGesture* gesture = find_gesture_by_type(KRYON_GESTURE_TYPE_TAP);
        if (gesture && gesture->touch_ids[0] == input_event->data.touch.id) {
            double duration = input_event->timestamp - gesture->start_time;
            float distance = calculate_distance(gesture->start_x, gesture->start_y,
                                              input_event->data.touch.x, input_event->data.touch.y);
            
            if (duration < g_gesture_recognizer.config.tap_max_duration &&
                distance < g_gesture_recognizer.config.tap_max_distance) {
                
                gesture->state = KRYON_GESTURE_STATE_RECOGNIZED;
                gesture->current_x = input_event->data.touch.x;
                gesture->current_y = input_event->data.touch.y;
                gesture->last_update_time = input_event->timestamp;
                
                dispatch_gesture_event(gesture);
            }
            
            remove_gesture(gesture);
        }
    }
}

static void recognize_long_press(const KryonInputEvent* input_event) {
    if (input_event->type == KRYON_INPUT_EVENT_TOUCH_DOWN) {
        KryonActiveGesture* gesture = create_gesture(KRYON_GESTURE_TYPE_LONG_PRESS);
        if (gesture) {
            gesture->start_x = gesture->current_x = input_event->data.touch.x;
            gesture->start_y = gesture->current_y = input_event->data.touch.y;
            gesture->touch_count = 1;
            gesture->touch_ids[0] = input_event->data.touch.id;
        }
        
    } else if (input_event->type == KRYON_INPUT_EVENT_TOUCH_MOVE) {
        KryonActiveGesture* gesture = find_gesture_by_type(KRYON_GESTURE_TYPE_LONG_PRESS);
        if (gesture && gesture->touch_ids[0] == input_event->data.touch.id) {
            float distance = calculate_distance(gesture->start_x, gesture->start_y,
                                              input_event->data.touch.x, input_event->data.touch.y);
            
            if (distance > g_gesture_recognizer.config.long_press_max_distance) {
                gesture->state = KRYON_GESTURE_STATE_FAILED;
                remove_gesture(gesture);
            }
        }
        
    } else if (input_event->type == KRYON_INPUT_EVENT_TOUCH_UP) {
        KryonActiveGesture* gesture = find_gesture_by_type(KRYON_GESTURE_TYPE_LONG_PRESS);
        if (gesture && gesture->touch_ids[0] == input_event->data.touch.id) {
            remove_gesture(gesture);
        }
    }
}

static void recognize_swipe(const KryonInputEvent* input_event) {
    if (input_event->type == KRYON_INPUT_EVENT_TOUCH_DOWN) {
        KryonActiveGesture* gesture = create_gesture(KRYON_GESTURE_TYPE_SWIPE);
        if (gesture) {
            gesture->start_x = gesture->current_x = input_event->data.touch.x;
            gesture->start_y = gesture->current_y = input_event->data.touch.y;
            gesture->touch_count = 1;
            gesture->touch_ids[0] = input_event->data.touch.id;
        }
        
    } else if (input_event->type == KRYON_INPUT_EVENT_TOUCH_MOVE) {
        KryonActiveGesture* gesture = find_gesture_by_type(KRYON_GESTURE_TYPE_SWIPE);
        if (gesture && gesture->touch_ids[0] == input_event->data.touch.id) {
            float prev_x = gesture->current_x;
            float prev_y = gesture->current_y;
            
            gesture->current_x = input_event->data.touch.x;
            gesture->current_y = input_event->data.touch.y;
            gesture->last_update_time = input_event->timestamp;
            
            // Calculate velocity
            double time_delta = input_event->timestamp - gesture->last_update_time;
            if (time_delta > 0) {
                gesture->velocity_x = (gesture->current_x - prev_x) / time_delta;
                gesture->velocity_y = (gesture->current_y - prev_y) / time_delta;
            }
            
            gesture->distance = calculate_distance(gesture->start_x, gesture->start_y,
                                                 gesture->current_x, gesture->current_y);
            gesture->angle = calculate_angle(gesture->start_x, gesture->start_y,
                                           gesture->current_x, gesture->current_y);
            
            if (gesture->state == KRYON_GESTURE_STATE_POSSIBLE &&
                gesture->distance > g_gesture_recognizer.config.swipe_min_distance) {
                gesture->state = KRYON_GESTURE_STATE_BEGAN;
                dispatch_gesture_event(gesture);
            } else if (gesture->state == KRYON_GESTURE_STATE_BEGAN) {
                gesture->state = KRYON_GESTURE_STATE_CHANGED;
                dispatch_gesture_event(gesture);
            }
        }
        
    } else if (input_event->type == KRYON_INPUT_EVENT_TOUCH_UP) {
        KryonActiveGesture* gesture = find_gesture_by_type(KRYON_GESTURE_TYPE_SWIPE);
        if (gesture && gesture->touch_ids[0] == input_event->data.touch.id) {
            float velocity_magnitude = sqrtf(gesture->velocity_x * gesture->velocity_x + 
                                           gesture->velocity_y * gesture->velocity_y);
            
            if (gesture->distance > g_gesture_recognizer.config.swipe_min_distance &&
                velocity_magnitude > g_gesture_recognizer.config.swipe_min_velocity) {
                gesture->state = KRYON_GESTURE_STATE_ENDED;
                gesture->last_update_time = input_event->timestamp;
                dispatch_gesture_event(gesture);
            }
            
            remove_gesture(gesture);
        }
    }
}

static void recognize_pan(const KryonInputEvent* input_event) {
    if (input_event->type == KRYON_INPUT_EVENT_TOUCH_DOWN) {
        KryonActiveGesture* gesture = create_gesture(KRYON_GESTURE_TYPE_PAN);
        if (gesture) {
            gesture->start_x = gesture->current_x = input_event->data.touch.x;
            gesture->start_y = gesture->current_y = input_event->data.touch.y;
            gesture->touch_count = 1;
            gesture->touch_ids[0] = input_event->data.touch.id;
        }
        
    } else if (input_event->type == KRYON_INPUT_EVENT_TOUCH_MOVE) {
        KryonActiveGesture* gesture = find_gesture_by_type(KRYON_GESTURE_TYPE_PAN);
        if (gesture && gesture->touch_ids[0] == input_event->data.touch.id) {
            gesture->current_x = input_event->data.touch.x;
            gesture->current_y = input_event->data.touch.y;
            gesture->last_update_time = input_event->timestamp;
            
            gesture->distance = calculate_distance(gesture->start_x, gesture->start_y,
                                                 gesture->current_x, gesture->current_y);
            
            if (gesture->state == KRYON_GESTURE_STATE_POSSIBLE &&
                gesture->distance > g_gesture_recognizer.config.pan_min_distance) {
                gesture->state = KRYON_GESTURE_STATE_BEGAN;
                dispatch_gesture_event(gesture);
            } else if (gesture->state >= KRYON_GESTURE_STATE_BEGAN) {
                gesture->state = KRYON_GESTURE_STATE_CHANGED;
                dispatch_gesture_event(gesture);
            }
        }
        
    } else if (input_event->type == KRYON_INPUT_EVENT_TOUCH_UP) {
        KryonActiveGesture* gesture = find_gesture_by_type(KRYON_GESTURE_TYPE_PAN);
        if (gesture && gesture->touch_ids[0] == input_event->data.touch.id) {
            if (gesture->state >= KRYON_GESTURE_STATE_BEGAN) {
                gesture->state = KRYON_GESTURE_STATE_ENDED;
                gesture->last_update_time = input_event->timestamp;
                dispatch_gesture_event(gesture);
            }
            
            remove_gesture(gesture);
        }
    }
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_gesture_init(const KryonGestureConfig* config) {
    memset(&g_gesture_recognizer, 0, sizeof(g_gesture_recognizer));
    
    if (config) {
        g_gesture_recognizer.config = *config;
    } else {
        // Default configuration
        g_gesture_recognizer.config.tap_max_duration = 0.3; // 300ms
        g_gesture_recognizer.config.tap_max_distance = 20.0f;
        g_gesture_recognizer.config.multi_tap_interval = 0.5; // 500ms
        g_gesture_recognizer.config.long_press_duration = 1.0; // 1000ms
        g_gesture_recognizer.config.long_press_max_distance = 15.0f;
        g_gesture_recognizer.config.swipe_min_distance = 50.0f;
        g_gesture_recognizer.config.swipe_min_velocity = 100.0f;
        g_gesture_recognizer.config.pan_min_distance = 10.0f;
        g_gesture_recognizer.config.pinch_min_distance = 20.0f;
        g_gesture_recognizer.config.rotation_min_angle = 0.1f; // ~5.7 degrees
    }
    
    return true;
}

void kryon_gesture_shutdown(void) {
    memset(&g_gesture_recognizer, 0, sizeof(g_gesture_recognizer));
}

void kryon_gesture_update(double delta_time) {
    double current_time = kryon_platform_get_time();
    
    // Update active gestures and check for timeouts
    for (size_t i = 0; i < g_gesture_recognizer.active_gesture_count; i++) {
        KryonActiveGesture* gesture = &g_gesture_recognizer.active_gestures[i];
        
        // Check for long press recognition
        if (gesture->type == KRYON_GESTURE_TYPE_LONG_PRESS &&
            gesture->state == KRYON_GESTURE_STATE_POSSIBLE) {
            double duration = current_time - gesture->start_time;
            
            if (duration >= g_gesture_recognizer.config.long_press_duration) {
                gesture->state = KRYON_GESTURE_STATE_RECOGNIZED;
                gesture->last_update_time = current_time;
                dispatch_gesture_event(gesture);
            }
        }
    }
}

bool kryon_gesture_process_input(const KryonInputEvent* input_event) {
    if (!input_event) return false;
    
    // Only process touch events for gestures
    if (input_event->type < KRYON_INPUT_EVENT_TOUCH_DOWN || 
        input_event->type > KRYON_INPUT_EVENT_TOUCH_CANCEL) {
        return false;
    }
    
    // Run recognition for different gesture types
    recognize_tap(input_event);
    recognize_long_press(input_event);
    recognize_swipe(input_event);
    recognize_pan(input_event);
    
    return true;
}

void kryon_gesture_set_callback(KryonGestureType type, KryonGestureCallback callback, void* context) {
    if (type < KRYON_GESTURE_TYPE_COUNT) {
        g_gesture_recognizer.callbacks[type] = callback;
        g_gesture_recognizer.callback_contexts[type] = context;
    }
}

void kryon_gesture_remove_callback(KryonGestureType type) {
    if (type < KRYON_GESTURE_TYPE_COUNT) {
        g_gesture_recognizer.callbacks[type] = NULL;
        g_gesture_recognizer.callback_contexts[type] = NULL;
    }
}

const KryonActiveGesture* kryon_gesture_get_active_gestures(size_t* count) {
    if (count) *count = g_gesture_recognizer.active_gesture_count;
    return g_gesture_recognizer.active_gestures;
}

KryonGestureConfig kryon_gesture_get_config(void) {
    return g_gesture_recognizer.config;
}

void kryon_gesture_set_config(const KryonGestureConfig* config) {
    if (config) {
        g_gesture_recognizer.config = *config;
    }
}