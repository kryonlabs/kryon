#ifndef UI_INSTANCE_H
#define UI_INSTANCE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Generated declarations borrow zero-initialized state from the current render
 * host. Type names have static lifetime. A live instance's address is stable
 * until the host expires it or closes; repeated keys share state only within
 * the same type and host. */
void *InstanceState(const char *type, uint64_t key, size_t size);

#ifdef __cplusplus
}
#endif
#endif
