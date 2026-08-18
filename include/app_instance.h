#ifndef KRYON_APP_INSTANCE_H
#define KRYON_APP_INSTANCE_H

/* Kryon applications run as a single instance by default. Call this before
 * InitWindow when concurrent windows are intentional. */
void SetSingleInstance(int enabled);
int SingleInstanceEnabled(void);

#endif
