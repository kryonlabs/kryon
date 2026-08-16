#ifndef KRYON_MEM_H
#define KRYON_MEM_H

/*
 * Memory diagnostics for downstream apps. Every report is a no-op unless
 * KRYON_MEM_DEBUG is set in the environment, so apps can call these at
 * interesting points unconditionally.
 *
 * KryonMemReport prints the current RSS/high-water marks from /proc (Linux)
 * plus the glibc allocator arena breakdown (malloc_stats) to stderr. Apps can
 * use KryonMemDebugEnabled() to gate their own heavier diagnostics.
 */

int KryonMemDebugEnabled(void);

/* Print a tagged memory snapshot to stderr. No-op without KRYON_MEM_DEBUG. */
void KryonMemReport(const char *tag);

#endif /* KRYON_MEM_H */
