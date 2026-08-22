#define _POSIX_C_SOURCE 200809L
#include "kryon.h"
#include "kry_inject.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Text-input PRECISION benchmark: per-keystroke latency on a realistic
 * screen. Unlike text_input_perf_test.c (which rebuilds the tree every
 * sample to bound retained growth), this harness keeps one stable retained
 * tree across the whole run — exactly what a running application has — and
 * measures single events on it:
 *
 *   - tap a RANDOM field to focus it (real pointer routing, not SetUIFocus)
 *   - type ONE character, measure that frame, assert the letter landed
 *   - single backspace, measure, assert the text shrank
 *   - long-text fields (~200 chars) to expose O(n) edit costs
 *
 * A keystroke frame here is declare -> reconcile -> layout -> route ->
 * update; rendering is GPU-side and not measurable headless. Budgets are
 * calibrated to the measured baseline (see docs/site/benchmarks) and trip
 * CI on regression, because keystroke latency is the thing users feel.
 */
enum { FIELD_COUNT = 64, FIELD_SIZE = 512, WARMUP = 200, SAMPLES = 4000,
       FIELD_W = 600, FIELD_H = 36 };
typedef struct Field {
    char text[FIELD_SIZE];
    int cursor;
    int focused;
    int focus_id;
    float y;
} Field;
static Field fields[FIELD_COUNT];
static KeyID screen_key = 1;

static double now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec * 1e6 + (double)t.tv_nsec / 1e3; }
static int cmp_double(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }

/* xorshift RNG: seeded, reproducible runs */
static unsigned long rng_state = 0x9e3779b97f4a7c15UL;
static unsigned long rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static void frame(void)
{
    BeginUIFocus();
    BeginUI(screen_key);
    for(int i = 0; i < FIELD_COUNT; ++i)
        TextField((TextFieldProps){.bounds = {20, fields[i].y, FIELD_W, FIELD_H},
            .text = fields[i].text, .text_size = sizeof(fields[i].text),
            .cursor_position = &fields[i].cursor, .focused = &fields[i].focused,
            .max_codepoints = 500, .font = Text16,
            .focus_id = fields[i].focus_id});
    UIReconcileTree();
    UILayoutTree();
    UIRouteInput();
    UIUpdateTree();
    EndUIFocus();
}

static void drain_events(void) { UIEvent event; while(NextUIEvent(&event)) {} }

static void reset_fields(int long_text)
{
    static const char *short_seeds[] = { "user@host", "notes", "search", "abc" };
    KryonInjectReset();
    screen_key++;
    for(int i = 0; i < FIELD_COUNT; ++i) {
        if(long_text) {
            /* ~200 chars: repeated pattern, no multi-byte surprises */
            int n = 0;
            const char *pat = "the quick brown fox 0123456789 ";
            for(n = 0; n < 195; n += (int)strlen(pat))
                memcpy(fields[i].text + n, pat, strlen(pat));
            fields[i].text[195] = '\0';
        } else {
            snprintf(fields[i].text, sizeof(fields[i].text), "%s-%d",
                     short_seeds[i % 4], i);
        }
        fields[i].cursor = (int)strlen(fields[i].text);
        fields[i].focused = 0;
        fields[i].focus_id = 100 + i;
        fields[i].y = 10.0f + (float)i * (FIELD_H + 8);
    }
    frame();
    drain_events();
}

static int focus_random_field(int *index_out)
{
    int index = (int)(rng() % FIELD_COUNT);
    int attempts;
    for(attempts = 0; attempts < 4 && fields[index].focused; ++attempts)
        index = (int)(rng() % FIELD_COUNT);
    /* A real app drains its event queue every frame; drop anything left
     * over from the previous keystroke so the tap is the only event. */
    KryonInjectReset();
    KryonInjectTap(20.0f + 40.0f, fields[index].y + FIELD_H / 2.0f);
    KryonInjectPump();
    frame();
    drain_events();
    if(!fields[index].focused)
        SetUIFocus(fields[index].focus_id);   /* pointer miss fallback */
    *index_out = index;
    return fields[index].focused != 0;
}

typedef struct {
    const char *scenario;
    double *samples;
    int wrong;
    int focus_misses;
    long chars_typed;
    long backspaces;
} Run;

static void report(Run *r, int samples, long node_delta)
{
    double *s = r->samples;
    double mean = 0.0;
    int i;

    for(i = 0; i < samples; ++i)
        mean += s[i];
    mean /= samples;
    qsort(s, samples, sizeof(*s), cmp_double);
    printf("{\"runtime\":\"retained-core-c\",\"benchmark\":\"text-input-precision\","
           "\"scenario\":\"%s\",\"fields\":%d,\"samples\":%d,\"warmup\":%d,"
           "\"p50_us\":%.3f,\"p95_us\":%.3f,\"p99_us\":%.3f,\"max_us\":%.3f,"
           "\"mean_us\":%.3f,\"chars_typed\":%ld,\"backspaces\":%ld,"
           "\"focus_misses\":%d,\"mismatches\":%d,\"node_delta\":%ld}\n",
           r->scenario, FIELD_COUNT, samples, WARMUP,
           s[samples / 2], s[(samples * 95) / 100], s[(samples * 99) / 100],
           s[samples - 1], mean,
           r->chars_typed, r->backspaces, r->focus_misses, r->wrong, node_delta);
}

/* One scenario pass: every sample is a single keystroke frame on the stable
 * tree. Letters rotate; every 8 insertions two backspaces keep buffers from
 * filling, and backspace samples are timed too. */
static int run_typing(const char *name, int long_text, double char_budget_p99_us)
{
    static const char letters[] = "abcdefghijkmnpqrstuvwxyz023456789";
    double *samples = malloc(sizeof(*samples) * SAMPLES);
    Run run = {.scenario = name, .samples = samples};
    int i, sample = 0, insert_streak = 0, before_nodes = 0, after_nodes = 0;
    double worst = 0.0;
    char expected[FIELD_SIZE];

    if(samples == NULL)
        return 1;
    rng_state = 0x9e3779b97f4a7c15UL;
    reset_fields(long_text);
    UIGetTreeNodes(&before_nodes);
    for(i = -WARMUP; i < SAMPLES; ++i) {
        int index;
        char ch = letters[rng() % (sizeof(letters) - 1)];
        size_t before_len;
        double start, elapsed;

        if(!focus_random_field(&index))
            run.focus_misses++;
        before_len = strlen(fields[index].text);

        if(insert_streak >= 8) {
            /* timed backspace */
            fields[index].cursor = (int)before_len;
            memcpy(expected, fields[index].text, before_len + 1);
            expected[before_len - 1] = '\0';
            start = now_us();
            KryonInjectKeyTap(KEY_BACKSPACE);
            KryonInjectPump();
            frame();
            elapsed = now_us() - start;
            run.backspaces++;
            if(strcmp(fields[index].text, expected) != 0)
                run.wrong++;
            insert_streak = 0;
        } else {
            /* timed single character insert at the end */
            fields[index].cursor = (int)before_len;
            memcpy(expected, fields[index].text, before_len + 1);
            expected[before_len] = ch;
            expected[before_len + 1] = '\0';
            start = now_us();
            KryonInjectText((char[]){ch, '\0'});
            KryonInjectPump();
            frame();
            elapsed = now_us() - start;
            run.chars_typed++;
            insert_streak++;
            if(strcmp(fields[index].text, expected) != 0)
                run.wrong++;
        }
        drain_events();
        if(i >= 0) {
            samples[sample++] = elapsed;
            if(elapsed > worst)
                worst = elapsed;
        }
    }
    UIGetTreeNodes(&after_nodes);
    report(&run, sample, (long)after_nodes - before_nodes);
    i = samples[(SAMPLES * 99) / 100] > char_budget_p99_us || run.wrong > 0 ||
        run.focus_misses > SAMPLES / 100;
    free(samples);
    return i;
}

static int run_focus_only(double budget_p99_us)
{
    double *samples = malloc(sizeof(*samples) * SAMPLES);
    Run run = {.scenario = "random_focus", .samples = samples};
    int i, sample = 0, before_nodes = 0, after_nodes = 0;

    if(samples == NULL)
        return 1;
    rng_state = 0x123456789abcdefUL;
    reset_fields(0);
    UIGetTreeNodes(&before_nodes);
    for(i = -WARMUP; i < SAMPLES; ++i) {
        int index;
        double start, elapsed;

        start = now_us();
        if(!focus_random_field(&index))
            run.focus_misses++;
        elapsed = now_us() - start;
        drain_events();
        if(i >= 0)
            samples[sample++] = elapsed;
    }
    UIGetTreeNodes(&after_nodes);
    report(&run, sample, (long)after_nodes - before_nodes);
    i = samples[(SAMPLES * 99) / 100] > budget_p99_us || run.focus_misses > SAMPLES / 100;
    free(samples);
    return i;
}

int main(void)
{
    int failed = 0;
    /* budgets: p99 (not max -- one scheduler blip must not fail CI) with
     * ~10x headroom over the measured baseline (p99 ~9us on a dev machine;
     * CI runners are slower). A regression that makes a keystroke frame
     * relayout, re-measure every field, or allocate per character blows
     * straight through these. */
    failed |= run_typing("random_single_char", 0, 100.0);
    failed |= run_typing("random_single_char_long_text", 1, 150.0);
    failed |= run_focus_only(150.0);
    if(failed)
        fprintf(stderr, "text-input precision budgets exceeded\n");
    return failed;
}
