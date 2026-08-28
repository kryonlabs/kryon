#ifndef KRY_INJECT_H
#define KRY_INJECT_H

/* Synthetic input injection for the owned input surface.
 *
 * The generated backend wrappers (see tools/generate-kryon-compat.sh)
 * consult this module before the real platform input, so injected events
 * behave exactly like physical ones everywhere - including app code that
 * calls GetMousePosition or IsMouseButtonReleased directly. InjectPump
 * runs once per frame from UpdateKeyPlatformState: pending edges become
 * visible for exactly one frame, and a taps release fires on the next
 * pump. Everything here is pure state, no GPU or window required, which is
 * what makes the synthetic file system (kry_sfs.h) and the KryT* test
 * helpers drivable headless. */

#define KRY_INJECT_MAX_BUTTONS 8
#define KRY_INJECT_MAX_KEYS 512
#define KRY_INJECT_KEY_MAX 512
#define KRY_INJECT_CHAR_QUEUE 64

void InjectMousePosition(float x, float y);
void InjectMouseButton(int button, int down);
void InjectKey(int key, int down);
void InjectKeyTap(int key);
void InjectLayoutKey(int codepoint, int down);
void InjectLayoutKeyTap(int codepoint);
void InjectText(const char *text);
void InjectWheel(float move);
void InjectTap(float x, float y);

/* Merge queued edges into the current frame. Called by the input sync
 * (UpdateKeyPlatformState); tests call it directly between frames. */
void InjectPump(void);

/* Query side used by the generated wrappers. */
int InjectMouseActive(void);
float InjectMouseX(void);
float InjectMouseY(void);
float InjectMouseDeltaX(void);
float InjectMouseDeltaY(void);
float InjectWheelValue(void);
int InjectMousePressed(int button);
int InjectMouseReleased(int button);
int InjectMouseButtonDown(int button);
int InjectMouseButtonUp(int button);
int InjectKeyPressed(int key);
int InjectKeyReleased(int key);
int InjectKeyDown(int key);
int InjectLayoutKeyPressed(int codepoint);
int InjectLayoutKeyReleased(int codepoint);
int InjectLayoutKeyDown(int codepoint);
int InjectCharPressed(void);
int InjectKeyPressedCode(void);

void InjectReset(void);

#endif
