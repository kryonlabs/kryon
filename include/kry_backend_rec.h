#ifndef KRY_BACKEND_REC_H
#define KRY_BACKEND_REC_H

/*
 * Recording backend: logs the KryBackend call stream a cartridge makes
 * (clear/rect/text/clip/texture) to a FILE while delegating every call to
 * an inner backend. Recording over kry_sw gives one pass that yields both
 * pixels and a deterministic call log — the two things engine conformance
 * is compared on (plan 11, phase 1).
 *
 * Query calls (mouse/width/time/theme) are answered by the inner backend
 * and not logged. Input injection goes through the inner backend.
 */

#include "kry_backend.h"

#include <stdio.h>

typedef struct KryBackendRec {
    FILE *log;
    const KryBackend *inner;
    long calls;
    KryBackend backend;
} KryBackendRec;

/* inner == NULL uses the null backend. log == NULL records nothing but
 * still counts calls. */
const KryBackend *KryBackendRecBackend(KryBackendRec *rec, FILE *log,
                                       const KryBackend *inner);

long KryBackendRecCalls(const KryBackendRec *rec);

#endif
