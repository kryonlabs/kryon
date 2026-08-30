#ifndef KRYON_FRAME_H
#define KRYON_FRAME_H

void BeginFrame(void);
void EndFrame(void);
void SyncFrame(void);
int GetFrameWidth(void);
int GetFrameHeight(void);
float GetFrameScale(void);

typedef void (*KryonPostFrameCallback)(void *userdata);

int SchedulePostFrameCallback(KryonPostFrameCallback callback, void *userdata);

#endif /* KRYON_FRAME_H */
