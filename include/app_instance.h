#ifndef KRYON_APP_INSTANCE_H
#define KRYON_APP_INSTANCE_H

/* Kryon applications run as a single instance by default. Call this before
 * InitWindow when concurrent windows are intentional. */
void SetSingleInstance(int enabled);
int SingleInstanceEnabled(void);
/* Nonzero when InitWindow refused to create a window because another
 * instance with the same title-key already holds the lock. Frame APIs
 * become no-ops in that state; apps should check this right after
 * InitWindow and exit (or forward to the running instance). */
int InstanceRejected(void);

#endif
