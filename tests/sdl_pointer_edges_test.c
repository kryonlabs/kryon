#include "kryon.h"
#include "kry_input.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>

static void
frame(void)
{
    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();
}

static void
button_event(Uint32 window, Uint32 type, Uint8 button)
{
    SDL_Event event = {0};
    event.type = type;
    event.button.windowID = window;
    event.button.button = button;
    event.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
    event.button.x = 40;
    event.button.y = 40;
    assert(SDL_PushEvent(&event) == 1);
}

int
main(void)
{
    InitWindow(200, 160, "Pointer edge regression");
    SetTargetFPS(0);
    frame();
    SDL_Window *native_window = SDL_GL_GetCurrentWindow();
    assert(native_window != NULL);
    Uint32 window = SDL_GetWindowID(native_window);
    assert(window != 0);
    SDL_Event motion = {0};
    motion.type = SDL_MOUSEMOTION;
    motion.motion.windowID = window;
    motion.motion.x = 40;
    motion.motion.y = 40;
    assert(SDL_PushEvent(&motion) == 1);
    frame();
    button_event(window, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT);
    button_event(window, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT);
    frame();
    assert(!BackendRaw_IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
    assert(IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    assert(IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
    assert(!IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    BeginInterfaceFrame(200, 160, 1.0f);
    assert(HandleClick((Rectangle){20, 20, 60, 60}, 0, NULL));
    assert(!HandleClick((Rectangle){20, 20, 60, 60}, 0, NULL));
    EndInterfaceFrame();
    frame();
    assert(!IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    assert(!IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
    button_event(window, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT);
    motion.motion.x = 120;
    assert(SDL_PushEvent(&motion) == 1);
    button_event(window, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT);
    frame();
    BeginInterfaceFrame(200, 160, 1.0f);
    assert(!HandleClick((Rectangle){0, 0, 160, 100}, 0, NULL));
    EndInterfaceFrame();
    frame();
    button_event(window + 1000, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT);
    button_event(window + 1000, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT);
    frame();
    assert(!IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    assert(!IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
    CloseWindow();
    puts("SDL pointer edge tests passed");
    return 0;
}
