#define _POSIX_C_SOURCE 200809L
#include "kryon.h"
#include "kry_inject.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* This is intentionally a retained-tree test rather than a TextField unit
 * microbenchmark: each sample declares every field, reconciles the tree,
 * routes real synthetic input, and updates the complete UI. Rendering is
 * separately timed by KRYON_FRAME_TRACE in a real application: the headless
 * test environment has no graphics context. */
enum { WARMUP = 250, SAMPLES = 3000, FIELD_COUNT = 4, FIELD_SIZE = 512 };
typedef struct Field {
    char text[FIELD_SIZE];
    int cursor;
    int focused;
    int focus_id;
    float y;
} Field;
static Field fields[FIELD_COUNT] = {
    { .focus_id = 101, .y = 20 }, { .focus_id = 102, .y = 70 },
    { .focus_id = 103, .y = 120 }, { .focus_id = 104, .y = 170 }
};
static UIKey screen_key = 1;

static double now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec*1e6+(double)t.tv_nsec/1e3; }
static int cmp_double(const void *a, const void *b) { double x=*(const double *)a,y=*(const double *)b; return (x>y)-(x<y); }

static void frame(void)
{
    int i;
    BeginUIFocus();
    BeginUI(screen_key);
    for(i = 0; i < FIELD_COUNT; ++i)
        TextField((TextFieldProps){.bounds={20,fields[i].y,600,40},
            .text=fields[i].text,.text_size=sizeof(fields[i].text),
            .cursor_position=&fields[i].cursor,.focused=&fields[i].focused,
            .max_codepoints=500,.font=UI_TEXT_16,.focus_id=fields[i].focus_id});
    UIReconcileTree();
    UILayoutTree();
    UIRouteInput();
    UIUpdateTree();
    EndUIFocus();
}

static void drain_events(void) { UIEvent event; while(NextUIEvent(&event)) {} }

static void reset_fields(void)
{
    int i;
    static const char *initial[] = { "freelancermap.de", "wao@example.com", "master password", "notes" };
    KryonInjectReset();
    screen_key++;
    for(i = 0; i < FIELD_COUNT; ++i) {
        snprintf(fields[i].text, sizeof(fields[i].text), "%s", initial[i]);
        fields[i].cursor = (int)strlen(fields[i].text);
        fields[i].focused = 0;
    }
    frame();
    drain_events();
}

static void focus_field(int index)
{
    /* Pointer-to-focus routing is covered by ui_tree_api_test.  Setting the
     * focus manager here isolates the measured keystroke frame itself. */
    SetUIFocus(fields[index].focus_id);
    frame();
    drain_events();
}

static int run_scenario(const char *name)
{
    double *samples = malloc(sizeof(*samples) * SAMPLES), total = 0;
    int i, before = 0, after = 0, wrong = 0;
    if(!samples) return 1;
    reset_fields(); UIGetTreeNodes(&before);
    for(i = -WARMUP; i < SAMPLES; ++i) {
        int field = (i + WARMUP) % FIELD_COUNT;
        double start, elapsed;
        reset_fields();
        focus_field(field);
        start = now_us();
        if(strcmp(name, "typing_burst") == 0) {
            KryonInjectText("abcdef0123456789"); KryonInjectPump(); frame();
            wrong += strstr(fields[field].text, "abcdef0123456789") == NULL;
        } else if(strcmp(name, "backspace") == 0) {
            size_t before_length = strlen(fields[field].text);
            KryonInjectKeyTap(KEY_BACKSPACE); KryonInjectPump(); frame();
            wrong += strlen(fields[field].text) + 1 != before_length;
        } else if(strcmp(name, "selection_replace") == 0) {
            SetSelection(fields[field].focus_id, 0, fields[field].cursor);
            KryonInjectText("replacement"); KryonInjectPump(); frame();
            wrong += strcmp(fields[field].text, "replacement") != 0;
        } else if(strcmp(name, "tab_traversal") == 0) {
            /* Traversal is resolved at EndUIFocus, then reflected in the
             * next declaration frame just as it is in an application. */
            KryonInjectKeyTap(KEY_TAB); KryonInjectPump(); frame(); frame();
            wrong += !fields[(field + 1) % FIELD_COUNT].focused;
        } else { /* an idle retained frame catches unbounded retained growth */
            frame();
        }
        elapsed = now_us() - start;
        drain_events();
        if(i >= 0) { samples[i] = elapsed; total += elapsed; }
    }
    UIGetTreeNodes(&after);
    qsort(samples, SAMPLES, sizeof(*samples), cmp_double);
    printf("{\"runtime\":\"retained-core-c\",\"scenario\":\"%s\",\"fields\":%d,\"samples\":%d,\"warmup\":%d,\"p50_us\":%.3f,\"p95_us\":%.3f,\"p99_us\":%.3f,\"max_us\":%.3f,\"mean_us\":%.3f,\"full_frame_budget_us\":4000,\"input_budget_p99_us\":1000,\"node_delta\":%d,\"mismatches\":%d}\n",
        name, FIELD_COUNT, SAMPLES, WARMUP, samples[SAMPLES/2],
        samples[(SAMPLES*95)/100], samples[(SAMPLES*99)/100], samples[SAMPLES-1],
        total/SAMPLES, after-before, wrong);
    i = samples[(SAMPLES*99)/100] >= 1000.0 || after != before || wrong;
    free(samples); return i;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    int failed = 0;
    failed |= run_scenario("typing_burst");
    failed |= run_scenario("backspace");
    failed |= run_scenario("selection_replace");
    failed |= run_scenario("tab_traversal");
    failed |= run_scenario("idle_frame");
    return failed ? 1 : 0;
}
