#ifndef K2G_LOWER_H
#define K2G_LOWER_H

#include "kir.h"

/*
 * k2g_lower - Kir -> Go backend.
 *
 * Emits one .go file per module into out_dir, all in one Go package. The
 * generated code calls the native Go Kryon package API. It does not thread a
 * cgo bridge or runtime object through generated functions.
 *
 * v1 scope: the declarative app subset translates fully — state blocks,
 * app metadata (-> generated main unless --no-main), frames, widget calls,
 * and scalar expressions (compound literals, casts, state refs). Imperative
 * constructs are best-effort; anything unsupported is emitted as a
 * commented TODO line instead of silently miscompiling.
 */
int k2g_lower(const KirProgram *const *progs, int prog_count,
              const char *root, const char *out_dir, const char *pkg,
              int no_main);

#endif
