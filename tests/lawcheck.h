#ifndef KRYON_TESTS_LAWCHECK_H
#define KRYON_TESTS_LAWCHECK_H

#include <stddef.h>
#include <stdint.h>

typedef struct LawCheck {
    const char *current_law;
    int checks;
    int failures;
} LawCheck;

void lawcheck_begin(LawCheck *law, const char *name);
void lawcheck_require(LawCheck *law, int ok, const char *expr,
                      const char *file, int line);
int lawcheck_finish(LawCheck *law);

#define LAW_BEGIN(ctx, name) lawcheck_begin((ctx), (name))
#define REQUIRE(ctx, expr) \
    lawcheck_require((ctx), !!(expr), #expr, __FILE__, __LINE__)
#define REQUIRE_FLOAT_BETWEEN(ctx, value, low, high) \
    REQUIRE((ctx), (value) >= (low) && (value) <= (high))
#define FOR_INT(name, first, last) \
    for(int name = (first); name <= (last); name++)

/* Deterministic LCG shared by randomized laws; fixed seeds keep law runs
 * reproducible across machines and CI. */
static inline uint32_t
law_next_u32(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static inline float
law_float_between(uint32_t *state, float low, float high)
{
    float unit = (float)(law_next_u32(state) & 0xffffu) / 65535.0f;
    return low + unit * (high - low);
}

#endif
