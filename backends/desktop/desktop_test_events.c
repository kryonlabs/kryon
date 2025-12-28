// For strdup on some systems
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef ENABLE_SDL3

#include "desktop_test_events.h"
#include "desktop_internal.h"
#include "../../third_party/cJSON/cJSON.h"
#include "../../ir/ir_builder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

TestEventQueue* test_queue_init_from_file(const char* filepath) {
    if (!filepath) return NULL;

    // Read JSON file
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "[test_events] Failed to open test events file: %s\n", filepath);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    // Parse JSON
    cJSON* root = cJSON_Parse(content);
    free(content);

    if (!root) {
        fprintf(stderr, "[test_events] Failed to parse JSON: %s\n", cJSON_GetErrorPtr());
        return NULL;
    }

    cJSON* events_array = cJSON_GetObjectItem(root, "events");
    if (!events_array || !cJSON_IsArray(events_array)) {
        fprintf(stderr, "[test_events] JSON missing 'events' array\n");
        cJSON_Delete(root);
        return NULL;
    }

    TestEventQueue* queue = calloc(1, sizeof(TestEventQueue));
    if (!queue) {
        cJSON_Delete(root);
        return NULL;
    }

    queue->count = cJSON_GetArraySize(events_array);
    queue->events = calloc(queue->count, sizeof(TestEvent));
    queue->enabled = true;
    queue->current_index = 0;
    queue->frames_waited = 0;

    if (!queue->events) {
        free(queue);
        cJSON_Delete(root);
        return NULL;
    }

    // Parse each event
    for (size_t i = 0; i < queue->count; i++) {
        cJSON* event = cJSON_GetArrayItem(events_array, (int)i);
        if (!event) continue;

        cJSON* type = cJSON_GetObjectItem(event, "type");
        if (!type || !cJSON_IsString(type)) continue;

        if (strcmp(type->valuestring, "wait") == 0) {
            queue->events[i].type = TEST_EVENT_WAIT;
            cJSON* frames = cJSON_GetObjectItem(event, "frames");
            queue->events[i].frames_to_wait = frames && cJSON_IsNumber(frames) ? frames->valueint : 1;

        } else if (strcmp(type->valuestring, "click") == 0) {
            queue->events[i].type = TEST_EVENT_CLICK;
            cJSON* comp_id = cJSON_GetObjectItem(event, "component_id");
            if (comp_id && cJSON_IsNumber(comp_id)) {
                queue->events[i].component_id = (uint32_t)comp_id->valueint;
                queue->events[i].x = -1; // Use component center
                queue->events[i].y = -1;
            } else {
                cJSON* x = cJSON_GetObjectItem(event, "x");
                cJSON* y = cJSON_GetObjectItem(event, "y");
                queue->events[i].component_id = 0;
                queue->events[i].x = x && cJSON_IsNumber(x) ? x->valueint : 0;
                queue->events[i].y = y && cJSON_IsNumber(y) ? y->valueint : 0;
            }

        } else if (strcmp(type->valuestring, "key_press") == 0) {
            queue->events[i].type = TEST_EVENT_KEY_PRESS;
            cJSON* key = cJSON_GetObjectItem(event, "key");
            if (key && cJSON_IsString(key)) {
                queue->events[i].key = SDL_GetKeyFromName(key->valuestring);
            }

        } else if (strcmp(type->valuestring, "text_input") == 0) {
            queue->events[i].type = TEST_EVENT_TEXT_INPUT;
            cJSON* text = cJSON_GetObjectItem(event, "text");
            if (text && cJSON_IsString(text)) {
                queue->events[i].text = strdup(text->valuestring);
            }

        } else if (strcmp(type->valuestring, "mouse_move") == 0) {
            queue->events[i].type = TEST_EVENT_MOUSE_MOVE;
            cJSON* x = cJSON_GetObjectItem(event, "x");
            cJSON* y = cJSON_GetObjectItem(event, "y");
            queue->events[i].x = x && cJSON_IsNumber(x) ? x->valueint : 0;
            queue->events[i].y = y && cJSON_IsNumber(y) ? y->valueint : 0;

        } else if (strcmp(type->valuestring, "screenshot") == 0) {
            queue->events[i].type = TEST_EVENT_SCREENSHOT;
            cJSON* path = cJSON_GetObjectItem(event, "path");
            if (path && cJSON_IsString(path)) {
                queue->events[i].screenshot_path = strdup(path->valuestring);
            }
        }
    }

    cJSON_Delete(root);
    printf("[test_events] Loaded %zu test events from %s\n", queue->count, filepath);
    return queue;
}

void test_queue_process(TestEventQueue* queue, DesktopIRRenderer* renderer) {
    if (!queue || !queue->enabled || queue->current_index >= queue->count) {
        return;
    }

    TestEvent* event = &queue->events[queue->current_index];

    // Handle wait events
    if (event->type == TEST_EVENT_WAIT) {
        queue->frames_waited++;
        if (queue->frames_waited >= event->frames_to_wait) {
            printf("[test_events] Wait complete (%d frames)\n", event->frames_to_wait);
            queue->frames_waited = 0;
            queue->current_index++;
        }
        return;
    }

    // Process immediate events
    switch (event->type) {
        case TEST_EVENT_CLICK: {
            SDL_Event click_event = {0};
            click_event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            click_event.button.button = SDL_BUTTON_LEFT;

            if (event->component_id > 0) {
                // Find component bounds
                IRComponent* comp = ir_find_component_by_id(renderer->last_root, event->component_id);
                if (comp && comp->rendered_bounds.width > 0 && comp->rendered_bounds.height > 0) {
                    click_event.button.x = comp->rendered_bounds.x + comp->rendered_bounds.width / 2.0f;
                    click_event.button.y = comp->rendered_bounds.y + comp->rendered_bounds.height / 2.0f;
                    printf("[test_events] Click component #%u at (%.0f, %.0f)\n",
                           event->component_id, click_event.button.x, click_event.button.y);
                } else {
                    fprintf(stderr, "[test_events] Component #%u not found or has no rendered_bounds\n",
                            event->component_id);
                    queue->current_index++;
                    return;
                }
            } else {
                click_event.button.x = (float)event->x;
                click_event.button.y = (float)event->y;
                printf("[test_events] Click at (%d, %d)\n", event->x, event->y);
            }

            SDL_PushEvent(&click_event);

            SDL_Event release_event = click_event;
            release_event.type = SDL_EVENT_MOUSE_BUTTON_UP;
            SDL_PushEvent(&release_event);
            break;
        }

        case TEST_EVENT_KEY_PRESS: {
            SDL_Event key_event = {0};
            key_event.type = SDL_EVENT_KEY_DOWN;
            key_event.key.key = event->key;
            printf("[test_events] Key press: %s\n", SDL_GetKeyName(event->key));
            SDL_PushEvent(&key_event);

            SDL_Event key_up = key_event;
            key_up.type = SDL_EVENT_KEY_UP;
            SDL_PushEvent(&key_up);
            break;
        }

        case TEST_EVENT_TEXT_INPUT: {
            if (event->text) {
                SDL_Event text_event = {0};
                text_event.type = SDL_EVENT_TEXT_INPUT;
                // Copy text into the event structure (const cast needed for SDL3 API)
                size_t len = strlen(event->text);
                if (len >= sizeof(text_event.text.text)) {
                    len = sizeof(text_event.text.text) - 1;
                }
                memcpy((char*)text_event.text.text, event->text, len);
                ((char*)text_event.text.text)[len] = '\0';
                printf("[test_events] Text input: %s\n", event->text);
                SDL_PushEvent(&text_event);
            }
            break;
        }

        case TEST_EVENT_MOUSE_MOVE: {
            SDL_Event move_event = {0};
            move_event.type = SDL_EVENT_MOUSE_MOTION;
            move_event.motion.x = (float)event->x;
            move_event.motion.y = (float)event->y;
            printf("[test_events] Mouse move to (%d, %d)\n", event->x, event->y);
            SDL_PushEvent(&move_event);
            break;
        }

        case TEST_EVENT_SCREENSHOT: {
            if (event->screenshot_path && renderer) {
                printf("[test_events] Taking screenshot: %s\n", event->screenshot_path);
#ifdef ENABLE_SDL3
                desktop_save_screenshot(renderer, event->screenshot_path);
#else
                printf("[test_events] Screenshot not supported for this renderer\n");
#endif
            }
            break;
        }

        default:
            break;
    }

    queue->current_index++;

    // Check if we've completed all events
    if (queue->current_index >= queue->count) {
        printf("[test_events] All test events completed\n");
        queue->enabled = false;
    }
}

void test_queue_free(TestEventQueue* queue) {
    if (!queue) return;

    for (size_t i = 0; i < queue->count; i++) {
        if (queue->events[i].screenshot_path) {
            free(queue->events[i].screenshot_path);
        }
        if (queue->events[i].text) {
            free(queue->events[i].text);
        }
    }

    free(queue->events);
    free(queue);
}

#endif // ENABLE_SDL3
