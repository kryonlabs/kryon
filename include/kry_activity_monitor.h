#ifndef KRYON_ACTIVITY_MONITOR_H
#define KRYON_ACTIVITY_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

void ActivityMonitorInit(void);
int ActivityIsWayland(void);
int ActivityAvailable(void);
long ActivityGetIdleMilliseconds(void);
int ActivitySetInputBlocked(int on);
int ActivityInputBlocked(void);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_ACTIVITY_MONITOR_H */
