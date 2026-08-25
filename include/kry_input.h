#ifndef KRY_INPUT_H
#define KRY_INPUT_H

/*
 * Shared input front-end for kryons owned input surface.
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
 * KryonBackendRaw_* hooks below and everything else on the surface under
 * their own public names as usual. The generated raylib wrappers and the
 * generated null backend emit exactly these hooks
 * (tools/generate-kryon-compat.sh).
 */

#include "kryon_compat.generated.h"

/* Keyboard state. Raw keycodes match the public KeyboardKey constants. */
bool KryonBackendRaw_IsKeyPressed(int key);
bool KryonBackendRaw_IsKeyPressedRepeat(int key);
bool KryonBackendRaw_IsKeyDown(int key);
bool KryonBackendRaw_IsKeyReleased(int key);

/* Key and char queues; dequeue semantics - each call consumes one event and
 * returns 0 when the queue is empty, like the public queries. */
int KryonBackendRaw_GetKeyPressed(void);
int KryonBackendRaw_GetCharPressed(void);

/* Mouse buttons; button indices match the public MouseButton constants. */
bool KryonBackendRaw_IsMouseButtonPressed(int button);
bool KryonBackendRaw_IsMouseButtonDown(int button);
bool KryonBackendRaw_IsMouseButtonReleased(int button);
bool KryonBackendRaw_IsMouseButtonUp(int button);

/* Mouse position/motion/wheel in window coordinates. */
int KryonBackendRaw_GetMouseX(void);
int KryonBackendRaw_GetMouseY(void);
Vector2 KryonBackendRaw_GetMousePosition(void);
Vector2 KryonBackendRaw_GetMouseDelta(void);
float KryonBackendRaw_GetMouseWheelMove(void);
Vector2 KryonBackendRaw_GetMouseWheelMoveV(void);

#endif /* KRY_INPUT_H */
