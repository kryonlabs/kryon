#ifndef KRY_INJECT_H
#define KRY_INJECT_H

/* Synthetic input injection for kryon's owned input surface.
 *
 * The generated backend wrappers (see tools/generate-kryon-compat.sh)
 * consult this module before the real platform input, so injected events
 * behave exactly like physical ones everywhere - including app code that
 * calls GetMousePosition or IsMouseButtonReleased directly. KryonInjectPump
 * runs once per frame from UpdateKeyPlatformState: pending edges become
 * visible for exactly one frame, and a tap's release fires on the next
 * pump. Everything here is pure state, no GPU or window required, which is
 * what makes the synthetic file system (kry_sfs.h) and the KryT* test
 * helpers drivable headless. */

#define KRY_INJECT_MAX_BUTTONS 8
#define KRY_INJECT_MAX_KEYS 512
#define KRY_INJECT_KEY_MAX 512
#define KRY_INJECT_CHAR_QUEUE 64

void KryonInjectMousePosition(float x, float y);
void KryonInjectMouseButton(int button, int down);
void KryonInjectKey(int key, int down);
void KryonInjectKeyTap(int key);
void KryonInjectText(const char *text);
void KryonInjectWheel(float move);
void KryonInjectTap(float x, float y);

/* Merge queued edges into the current frame. Called by the input sync
 * (UpdateKeyPlatformState); tests call it directly between frames. */
void KryonInjectPump(void);

/* Query side used by the generated wrappers. */
int KryonInjectMouseActive(void);
float KryonInjectMouseX(void);
float KryonInjectMouseY(void);
float KryonInjectMouseDeltaX(void);
float KryonInjectMouseDeltaY(void);
float KryonInjectWheelValue(void);
int KryonInjectMousePressed(int button);
int KryonInjectMouseReleased(int button);
int KryonInjectMouseButtonDown(int button);
int KryonInjectMouseButtonUp(int button);
int KryonInjectKeyPressed(int key);
int KryonInjectKeyReleased(int key);
int KryonInjectKeyDown(int key);
int KryonInjectCharPressed(void);
int KryonInjectKeyPressedCode(void);

void KryonInjectReset(void);

#endif
