#ifndef KRYON_FRAME_H
#define KRYON_FRAME_H

void BeginFrame(void);
void EndFrame(void);
void SyncFrame(void);
int GetFrameWidth(void);
int GetFrameHeight(void);
float GetFrameScale(void);
void ConfigureFramePacing(int idle_fps, int active_fps);
void DisableFramePacing(void);
void SetFramePacingActive(int active);
void UpdateFramePacing(void);
int GetFramePacingTargetFPS(void);

typedef void (*KryonPostFrameCallback)(void *userdata);

int SchedulePostFrameCallback(KryonPostFrameCallback callback, void *userdata);

#endif /* KRYON_FRAME_H */
