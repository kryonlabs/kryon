/**
 * @file events.c
 * @brief Kryon Core Event System Implementation
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 */

#include "internal/events.h"
#include "internal/memory.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// =============================================================================
// KEY CODE MAPPINGS
// =============================================================================

typedef struct {
    const char* name;
    int keyCode;
} KeyMapping;

static const KeyMapping keyMappings[] = {
    {"Escape", 256},
    {"Enter", 257},
    {"Tab", 258},
    {"Backspace", 259},
    {"Insert", 260},
    {"Delete", 261},
    {"Right", 262},
    {"Left", 263},
    {"Down", 264},
    {"Up", 265},
    {"PageUp", 266},
    {"PageDown", 267},
    {"Home", 268},
    {"End", 269},
    {"CapsLock", 280},
    {"ScrollLock", 281},
    {"NumLock", 282},
    {"PrintScreen", 283},
    {"Pause", 284},
    {"F1", 290},
    {"F2", 291},
    {"F3", 292},
    {"F4", 293},
    {"F5", 294},
    {"F6", 295},
    {"F7", 296},
    {"F8", 297},
    {"F9", 298},
    {"F10", 299},
    {"F11", 300},
    {"F12", 301},
    {"Space", 32},
    {NULL, 0}
};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static int getKeyCodeFromName(const char* keyName) {
    // Check special keys first
    for (int i = 0; keyMappings[i].name; i++) {
        if (strcmp(keyMappings[i].name, keyName) == 0) {
            return keyMappings[i].keyCode;
        }
    }
    
    // Single character keys (A-Z, 0-9, etc.)
    if (strlen(keyName) == 1) {
        char c = toupper(keyName[0]);
        if (c >= 'A' && c <= 'Z') {
            return c;
        }
        if (c >= '0' && c <= '9') {
            return c;
        }
    }
    
    return 0; // Unknown key
}

// =============================================================================
// GLOBAL EVENT REGISTRATION
// =============================================================================

bool kryon_event_register_keyboard_handler(KryonEventSystem* system,
                                           const char* shortcut,
                                           KryonEventHandler handler,
                                           void* userData) {
    if (!system || !shortcut || !handler) {
        return false;
    }
    
    // Register for all key down events and filter in handler
    kryon_event_add_listener(system, KRYON_EVENT_KEY_DOWN, handler, userData);
    return true;
}

bool kryon_event_register_mouse_handler(KryonEventSystem* system,
                                        const char* mouseEvent,
                                        KryonEventHandler handler,
                                        void* userData) {
    if (!system || !mouseEvent || !handler) {
        return false;
    }
    
    KryonEventType eventType;
    
    if (strcmp(mouseEvent, "LeftClick") == 0 || 
        strcmp(mouseEvent, "RightClick") == 0 || 
        strcmp(mouseEvent, "MiddleClick") == 0) {
        eventType = KRYON_EVENT_MOUSE_BUTTON_DOWN;
    } else if (strcmp(mouseEvent, "DoubleClick") == 0) {
        eventType = KRYON_EVENT_MOUSE_BUTTON_DOWN; // Filter by click count in handler
    } else {
        return false; // Unknown mouse event
    }
    
    kryon_event_add_listener(system, eventType, handler, userData);
    return true;
}

bool kryon_event_parse_keyboard_shortcut(const char* shortcut,
                                         int* keyCode,
                                         bool* ctrl,
                                         bool* shift,
                                         bool* alt,
                                         bool* meta) {
    if (!shortcut || !keyCode || !ctrl || !shift || !alt || !meta) {
        return false;
    }
    
    // Initialize modifiers
    *ctrl = false;
    *shift = false;
    *alt = false;
    *meta = false;
    *keyCode = 0;
    
    char* shortcutCopy = kryon_strdup(shortcut);
    if (!shortcutCopy) {
        return false;
    }
    
    // Parse modifiers
    char* token = strtok(shortcutCopy, "+");
    char* lastToken = NULL;
    
    while (token) {
        lastToken = token;
        
        if (strcmp(token, "Ctrl") == 0) {
            *ctrl = true;
        } else if (strcmp(token, "Shift") == 0) {
            *shift = true;
        } else if (strcmp(token, "Alt") == 0) {
            *alt = true;
        } else if (strcmp(token, "Cmd") == 0 || strcmp(token, "Meta") == 0) {
            *meta = true;
        } else {
            // This should be the key
            *keyCode = getKeyCodeFromName(token);
            break;
        }
        
        token = strtok(NULL, "+");
    }
    
    kryon_free(shortcutCopy);
    
    return *keyCode != 0;
}

bool kryon_event_matches_shortcut(const KryonKeyEvent* keyEvent,
                                  const char* shortcut) {
    if (!keyEvent || !shortcut) {
        return false;
    }
    
    int expectedKeyCode;
    bool expectedCtrl, expectedShift, expectedAlt, expectedMeta;
    
    if (!kryon_event_parse_keyboard_shortcut(shortcut, &expectedKeyCode,
                                           &expectedCtrl, &expectedShift,
                                           &expectedAlt, &expectedMeta)) {
        return false;
    }
    
    return keyEvent->keyCode == expectedKeyCode &&
           keyEvent->ctrlPressed == expectedCtrl &&
           keyEvent->shiftPressed == expectedShift &&
           keyEvent->altPressed == expectedAlt &&
           keyEvent->metaPressed == expectedMeta;
}

// =============================================================================
// EVENT SYSTEM CORE FUNCTIONS
// =============================================================================

KryonEventSystem* kryon_event_system_create(size_t queueCapacity) {
    KryonEventSystem* system = kryon_calloc(1, sizeof(KryonEventSystem));
    if (!system) {
        return NULL;
    }
    
    system->eventQueue = kryon_calloc(queueCapacity, sizeof(KryonEvent));
    if (!system->eventQueue) {
        kryon_free(system);
        return NULL;
    }
    
    system->queueCapacity = queueCapacity;
    system->queueSize = 0;
    system->queueHead = 0;
    system->queueTail = 0;
    system->listeners = NULL;
    
    return system;
}

void kryon_event_system_destroy(KryonEventSystem* system) {
    if (!system) {
        return;
    }
    
    // Free all listeners
    KryonEventListener* listener = system->listeners;
    while (listener) {
        KryonEventListener* next = listener->next;
        kryon_free(listener);
        listener = next;
    }
    
    kryon_free(system->eventQueue);
    kryon_free(system);
}

void kryon_event_add_listener(KryonEventSystem* system,
                              KryonEventType type,
                              KryonEventHandler handler,
                              void* userData) {
    if (!system || !handler) {
        return;
    }
    
    KryonEventListener* listener = kryon_calloc(1, sizeof(KryonEventListener));
    if (!listener) {
        return;
    }
    
    listener->eventType = type;
    listener->handler = handler;
    listener->userData = userData;
    listener->next = system->listeners;
    system->listeners = listener;
}

void kryon_event_remove_listener(KryonEventSystem* system,
                                 KryonEventType type,
                                 KryonEventHandler handler) {
    if (!system || !handler) {
        return;
    }
    
    KryonEventListener** current = &system->listeners;
    while (*current) {
        if ((*current)->eventType == type && (*current)->handler == handler) {
            KryonEventListener* toRemove = *current;
            *current = (*current)->next;
            kryon_free(toRemove);
            return;
        }
        current = &(*current)->next;
    }
}

bool kryon_event_push(KryonEventSystem* system, const KryonEvent* event) {
    if (!system || !event) {
        return false;
    }
    
    if (system->queueSize >= system->queueCapacity) {
        return false; // Queue full
    }
    
    system->eventQueue[system->queueTail] = *event;
    system->queueTail = (system->queueTail + 1) % system->queueCapacity;
    system->queueSize++;
    
    return true;
}

void kryon_event_process_all(KryonEventSystem* system) {
    if (!system) {
        return;
    }
    
    while (system->queueSize > 0) {
        KryonEvent event = system->eventQueue[system->queueHead];
        system->queueHead = (system->queueHead + 1) % system->queueCapacity;
        system->queueSize--;
        
        // Dispatch to listeners
        KryonEventListener* listener = system->listeners;
        while (listener) {
            if (listener->eventType == event.type || listener->eventType == KRYON_EVENT_CUSTOM) {
                if (listener->handler(&event, listener->userData)) {
                    event.handled = true;
                    break; // Stop propagation
                }
            }
            listener = listener->next;
        }
    }
}

bool kryon_event_poll(KryonEventSystem* system, KryonEvent* event) {
    if (!system || !event || system->queueSize == 0) {
        return false;
    }
    
    *event = system->eventQueue[system->queueHead];
    system->queueHead = (system->queueHead + 1) % system->queueCapacity;
    system->queueSize--;
    
    return true;
}

bool kryon_event_wait(KryonEventSystem* system, KryonEvent* event, uint32_t timeoutMs) {
    // For now, just do a simple poll - real implementation would use platform-specific waiting
    (void)timeoutMs; // Suppress unused parameter warning
    return kryon_event_poll(system, event);
}