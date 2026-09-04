#ifndef KRY_INPUT_H
#define KRY_INPUT_H

/*
 * Shared input front-end for the owned input surface.
 *
 * The public input queries (IsKeyPressed, GetMousePosition, GetCharPressed,
 * ...) are implemented once here, not per backend. Before a query reaches the
 * active graphics backend it is merged with the synthetic input injector
 * (kry_inject.h), the modal input override (BeginKryonInputOverride), and the
 * keyboard platform callbacks (SetKeyPlatformCallbacks). Every backend -
 * raylib, null, canvas - therefore gets identical injection, override, and
 * headless-test behavior for free.
 *
 * Backends do not implement the public input names; they implement the
 * BackendRaw_* hooks below and everything else on the surface under
 * their own public names as usual. The generated raylib wrappers and the
 * generated null backend emit exactly these hooks
 * (tools/generate-kryon-compat.sh).
 */

#include "kryon_compat.generated.h"

#define KRY_TEXT_COMPOSITION_MAX 256

typedef enum KryTextCompositionPhase {
    KRY_TEXT_COMPOSITION_START = 1,
    KRY_TEXT_COMPOSITION_UPDATE,
    KRY_TEXT_COMPOSITION_COMMIT,
    KRY_TEXT_COMPOSITION_CANCEL
} KryTextCompositionPhase;

typedef struct KryTextCompositionEvent {
    KryTextCompositionPhase phase;
    int cursor;
    int selection_length;
    char text[KRY_TEXT_COMPOSITION_MAX];
} KryTextCompositionEvent;

/* Platform adapters submit UTF-8 composition events here. Focused text
 * controls consume them during input routing. Events are copied, so callers
 * do not need to retain `text`. Returns zero only when the queue is full. */
int SubmitTextComposition(KryTextCompositionPhase phase, const char *text,
                          int cursor, int selection_length);
int PollTextComposition(KryTextCompositionEvent *event);
void ClearTextComposition(void);

/* Keyboard state. Raw keycodes match the public KeyboardKey constants. */
bool BackendRaw_IsKeyPressed(int key);
bool BackendRaw_IsKeyPressedRepeat(int key);
bool BackendRaw_IsKeyDown(int key);
bool BackendRaw_IsKeyReleased(int key);

/* Key and char queues; dequeue semantics - each call consumes one event and
 * returns 0 when the queue is empty, like the public queries. */
int BackendRaw_GetKeyPressed(void);
int BackendRaw_GetCharPressed(void);

/* Mouse buttons; button indices match the public MouseButton constants. */
bool BackendRaw_IsMouseButtonPressed(int button);
bool BackendRaw_IsMouseButtonDown(int button);
bool BackendRaw_IsMouseButtonReleased(int button);
bool BackendRaw_IsMouseButtonUp(int button);

/* Mouse position/motion/wheel in window coordinates. */
int BackendRaw_GetMouseX(void);
int BackendRaw_GetMouseY(void);
Vector2 BackendRaw_GetMousePosition(void);
Vector2 BackendRaw_GetMouseDelta(void);
float BackendRaw_GetMouseWheelMove(void);
Vector2 BackendRaw_GetMouseWheelMoveV(void);

#endif /* KRY_INPUT_H */
