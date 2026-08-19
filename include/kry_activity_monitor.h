#ifndef KRYON_ACTIVITY_MONITOR_H
#define KRYON_ACTIVITY_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

void KryActivityMonitorInit(void);
int KryActivityIsWayland(void);
int KryActivityAvailable(void);
long KryActivityGetIdleMilliseconds(void);
int KryActivitySetInputBlocked(int on);
int KryActivityInputBlocked(void);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_ACTIVITY_MONITOR_H */
