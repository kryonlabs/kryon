#ifndef KRYON_FRAME_H
#define KRYON_FRAME_H

void BeginFrame(void);
void EndFrame(void);
void SyncFrame(void);
int GetFrameWidth(void);
int GetFrameHeight(void);
float GetFrameScale(void);

#endif /* KRYON_FRAME_H */
