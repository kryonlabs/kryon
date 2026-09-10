#define _POSIX_C_SOURCE 200809L
#include "kryon.h"
#include "ui_style_internal.h"
#include "runtime/button.h"
#include "runtime/dropdown.h"
#include "runtime/material.h"
#include "runtime/paint.h"
#include "runtime/surface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct Counters {
    unsigned long long surface_calls;
    unsigned long long surface_visible;
    unsigned long long draw_calls;
    unsigned long long text_calls;
    unsigned long long chevron_calls;
    unsigned long long ring_calls;
    unsigned long long glow_layers;
    double area;
} Counters;

typedef void (*BenchFn)(Counters *, int);

typedef struct Result {
    const char *widget;
    const char *appearance;
    int iterations;
    double startup_us;
    double wall_us_per_frame;
    double cpu_us_per_frame;
    long rss_delta_kib;
    double layers_per_frame;
    double visible_layers_per_frame;
    double draw_calls_per_frame;
    unsigned long long glow_layers;
} Result;

static volatile unsigned long long sink;

static double
now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000000.0 + (double)t.tv_nsec / 1000.0;
}

static double
cpu_us(void)
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (double)ru.ru_utime.tv_sec * 1000000.0 + (double)ru.ru_utime.tv_usec +
           (double)ru.ru_stime.tv_sec * 1000000.0 + (double)ru.ru_stime.tv_usec;
}

static long
rss_kib(void)
{
#if defined(__linux__)
    FILE *f = fopen("/proc/self/statm", "r");
    long total_pages = 0;
    long resident_pages = 0;
    long page_kib;
    if(f != NULL) {
        if(fscanf(f, "%ld %ld", &total_pages, &resident_pages) == 2) {
            fclose(f);
            page_kib = sysconf(_SC_PAGESIZE) / 1024;
            if(page_kib <= 0)
                page_kib = 4;
            return resident_pages * page_kib;
        }
        fclose(f);
    }
#endif
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_maxrss;
}

static void
surface_counter(void *context, SurfaceDrawing drawing)
{
    Counters *c = (Counters *)context;
    c->surface_calls++;
    if(drawing.visible)
        c->surface_visible++;
    if(drawing.layer.blur > 0.0f || drawing.layer.inner_blur > 0.0f)
        c->glow_layers++;
    c->area += (double)drawing.area.width * (double)drawing.area.height;
    sink += (unsigned long long)drawing.visible + (unsigned long long)drawing.bounds.width;
}

static void
drawing_counter(void *context, Drawing drawing)
{
    Counters *c = (Counters *)context;
    c->draw_calls++;
    if(drawing.kind == DrawingText)
        c->text_calls++;
    else if(drawing.kind == DrawingChevron)
        c->chevron_calls++;
    else if(drawing.kind == DrawingRing) {
        c->ring_calls++;
        if(drawing.ring.glow_blur > 0.0f)
            c->glow_layers++;
    }
    sink += (unsigned long long)drawing.kind + (unsigned long long)drawing.bounds.width;
}

static void
paint_material(Counters *c, MaterialPaint paint)
{
    paint = PrepareMaterial(paint);
    for(int i = 0; i < MaterialLayerCount(paint.value.material); ++i)
        surface_counter(c, PaintMaterialLayer(paint, i));
}

static InteractionMotion
sample_motion(float hover, float press, float focus)
{
    InteractionMotion motion;
    memset(&motion, 0, sizeof(motion));
    motion.hover.value = hover;
    motion.hover.target = hover;
    motion.press.value = press;
    motion.press.target = press;
    motion.focus.value = focus;
    motion.focus.target = focus;
    return motion;
}

static ButtonFrame
make_button_frame(ButtonProps props, ButtonState state,
                  float hover, float press, float focus)
{
    Activation sample = {.hovered = hover > 0.0f, .pressed = press > 0.0f,
                         .focused = focus > 0.0f};
    ButtonInput input = ResolveButtonInput(props, sample);
    InteractionMotion motion = sample_motion(hover, press, focus);
    StyleFrame appearance = ui_button_style_frame(props, state, 1, hover, press, focus);
    ButtonFrame frame = BuildFrame(props, input, appearance, motion, (Rectangle){0},
                                   PackedColor(0x092039u, 255u), 1.0f,
                                   (int)appearance.value.font_size, 18);
    frame.appearance = ui_style_apply_effects_frame(frame.appearance);
    frame.material.value = frame.appearance.value;
    frame.material.fill = ui_style_apply_effects_fill(frame.material.fill);
    return frame;
}

static Style
make_dropdown_style(int role, int selected, ButtonState state)
{
    ButtonProps props;
    memset(&props, 0, sizeof(props));
    props.tone = ButtonToneNeutral;
    props.emphasis = ButtonEmphasisSoft;
    Style base = ResolveButtonStyle(props, state);
    props.tone = ButtonToneAccent;
    props.emphasis = role == 2 ? SelectionEmphasis(PackedColor(0x092039u, 255u))
                               : ButtonEmphasisFilled;
    Style accent = ResolveButtonStyle(props, ButtonStateNormal);
    StyleData data = Appearance(
        ui_pack_style_states((ControlStyle){.normal = base}).normal,
        ui_pack_style_states((ControlStyle){.normal = accent}).normal,
        PackedColor(0x092039u, 255u), role, state, selected != 0);
    return ui_style_apply_effects(ui_unpack_style(data));
}

static void
bench_text(Counters *c, int i)
{
    (void)i;
    Drawing text = {.kind = DrawingText, .bounds = {24, 24, 188, 24},
                    .text = "Human tomorrow", .font = 18,
                    .color = PackedColor(0xf5f8ffu, 255u)};
    drawing_counter(c, text);
}

static void
bench_button(Counters *c, int i)
{
    ButtonProps props;
    memset(&props, 0, sizeof(props));
    props.bounds = (Rectangle){20, 20, 168, 42};
    props.id = 100 + (i & 31);
    props.label = "Save changes";
    props.tone = ButtonToneAccent;
    props.emphasis = ButtonEmphasisFilled;
    props.size = ControlSizeMedium;
    ButtonFrame frame = make_button_frame(props, ButtonStateHover, 0.55f, 0.0f, 0.0f);
    SurfacePainter surface = {.context = c, .call = surface_counter};
    Painter painter = {.context = c, .call = drawing_counter};
    PaintButton(frame, 104.0f, (double)i * 16.67, 0, surface, painter);
}

static void
bench_text_input(Counters *c, int i)
{
    ButtonProps props;
    memset(&props, 0, sizeof(props));
    props.bounds = (Rectangle){20, 20, 260, 42};
    props.id = 300 + (i & 31);
    props.label = "";
    props.tone = ButtonToneNeutral;
    props.emphasis = ButtonEmphasisSoft;
    props.size = ControlSizeMedium;
    ButtonFrame frame = make_button_frame(props, ButtonStateFocus, 0.20f, 0.0f, 1.0f);
    paint_material(c, frame.material);
    drawing_counter(c, (Drawing){.kind = DrawingText, .bounds = {32, 30, 190, 20},
        .text = "freelancermap.de", .font = 18, .color = frame.foreground});
    drawing_counter(c, (Drawing){.kind = DrawingText, .bounds = {226, 30, 1, 20},
        .text = "|", .font = 18, .color = PackedColor(0x409cffu, 255u)});
}

static void
bench_dropdown(Counters *c, int i)
{
    ButtonProps props;
    memset(&props, 0, sizeof(props));
    props.bounds = (Rectangle){20, 20, 232, 42};
    props.id = 500 + (i & 31);
    props.label = "Inner Breeze";
    props.tone = ButtonToneNeutral;
    props.emphasis = ButtonEmphasisSoft;
    props.size = ControlSizeMedium;
    Style paint = make_dropdown_style(0, 0, ButtonStateHover);
    StyleFrame appearance = {.value = ui_style_apply_effects_data(
        ui_pack_style_states((ControlStyle){.normal = paint}).normal)};
    appearance.fill = ui_style_apply_effects_fill(FillState(
        appearance.value.fields, appearance.value.background,
        appearance.value.background_end));
    Activation sample = {.hovered = 1, .pressed = 0, .focused = 0};
    ButtonInput input = ResolveButtonInput(props, sample);
    ButtonFrame frame = BuildFrame(props, input, appearance, sample_motion(0.55f, 0.0f, 0.0f),
                                   (Rectangle){0}, PackedColor(0x092039u, 255u), 1.0f,
                                   (int)appearance.value.font_size, 18);
    frame.appearance = ui_style_apply_effects_frame(frame.appearance);
    frame.material.value = frame.appearance.value;
    frame.material.fill = ui_style_apply_effects_fill(frame.material.fill);
    paint_material(c, frame.material);
    drawing_counter(c, (Drawing){.kind = DrawingText, .bounds = {34, 30, 130, 20},
        .text = "Inner Breeze", .font = frame.font, .color = frame.foreground});
    drawing_counter(c, (Drawing){.kind = DrawingChevron, .bounds = {222, 35, 10, 8},
        .color = frame.foreground});
}

static Result
run_case(const char *widget, BenchFn fn, int fancy, int iterations)
{
    enum { WARMUP = 2000 };
    Counters counters;
    double startup_start, startup_us, wall_start, wall_end, cpu_start, cpu_end;
    long rss_before, rss_after;
    const char *appearance = fancy ? "glow" : "simple";

    memset(&counters, 0, sizeof(counters));
    SetFancyEffectsEnabled(fancy);
    startup_start = now_us();
    fn(&counters, 0);
    startup_us = now_us() - startup_start;

    memset(&counters, 0, sizeof(counters));
    for(int i = 0; i < WARMUP; ++i)
        fn(&counters, i);

    memset(&counters, 0, sizeof(counters));
    rss_before = rss_kib();
    cpu_start = cpu_us();
    wall_start = now_us();
    for(int i = 0; i < iterations; ++i)
        fn(&counters, i);
    wall_end = now_us();
    cpu_end = cpu_us();
    rss_after = rss_kib();

    Result result = {
        .widget = widget,
        .appearance = appearance,
        .iterations = iterations,
        .startup_us = startup_us,
        .wall_us_per_frame = (wall_end - wall_start) / (double)iterations,
        .cpu_us_per_frame = (cpu_end - cpu_start) / (double)iterations,
        .rss_delta_kib = rss_after - rss_before,
        .layers_per_frame = (double)counters.surface_calls / (double)iterations,
        .visible_layers_per_frame = (double)counters.surface_visible / (double)iterations,
        .draw_calls_per_frame = (double)counters.draw_calls / (double)iterations,
        .glow_layers = counters.glow_layers
    };
    return result;
}

static int
iterations_from_env(void)
{
    const char *value = getenv("KRYON_APPEARANCE_BENCH_ITERS");
    long parsed;
    if(value == NULL || *value == '\0')
        return 100000;
    parsed = strtol(value, NULL, 10);
    if(parsed < 1000)
        parsed = 1000;
    if(parsed > 5000000)
        parsed = 5000000;
    return (int)parsed;
}

static void
print_json(Result r)
{
    printf("{\"benchmark\":\"control_appearance\",\"widget\":\"%s\",\"appearance\":\"%s\",\"iterations\":%d,\"startup_us\":%.3f,\"wall_us_per_frame\":%.3f,\"cpu_us_per_frame\":%.3f,\"rss_delta_kib\":%ld,\"surface_layers_per_frame\":%.2f,\"visible_layers_per_frame\":%.2f,\"draw_calls_per_frame\":%.2f,\"glow_layers_total\":%llu}\n",
           r.widget, r.appearance, r.iterations, r.startup_us,
           r.wall_us_per_frame, r.cpu_us_per_frame, r.rss_delta_kib,
           r.layers_per_frame, r.visible_layers_per_frame,
           r.draw_calls_per_frame, r.glow_layers);
}

static void
print_markdown(Result *results, int count)
{
    puts("| Widget | Appearance | Startup us | CPU us/frame | Wall us/frame | RSS delta KiB | Surface layers/frame | Draw calls/frame | Glow layers/frame |");
    puts("|---|---:|---:|---:|---:|---:|---:|---:|---:|");
    for(int i = 0; i < count; ++i) {
        Result r = results[i];
        printf("| %s | %s | %.3f | %.3f | %.3f | %ld | %.2f | %.2f | %.2f |\n",
               r.widget, r.appearance, r.startup_us, r.cpu_us_per_frame,
               r.wall_us_per_frame, r.rss_delta_kib, r.layers_per_frame,
               r.draw_calls_per_frame,
               (double)r.glow_layers / (double)r.iterations);
    }
}

static int
validate_results(Result *results, int count)
{
    int failures = 0;
    for(int i = 0; i + 1 < count; i += 2) {
        Result fancy = results[i];
        Result simple = results[i + 1];
        if(strcmp(fancy.widget, simple.widget) != 0)
            failures++;
        if(strcmp(fancy.widget, "Text") == 0) {
            if(simple.layers_per_frame != 0.0 || simple.glow_layers != 0)
                failures++;
            continue;
        }
        if(!(simple.layers_per_frame < fancy.layers_per_frame))
            failures++;
        if(simple.glow_layers != 0)
            failures++;
        if(fancy.glow_layers == 0)
            failures++;
    }
    return failures;
}

int
main(void)
{
    int iterations = iterations_from_env();
    Result results[8];
    int n = 0;

    results[n++] = run_case("Text", bench_text, 1, iterations);
    results[n++] = run_case("Text", bench_text, 0, iterations);
    results[n++] = run_case("TextInput", bench_text_input, 1, iterations);
    results[n++] = run_case("TextInput", bench_text_input, 0, iterations);
    results[n++] = run_case("Button", bench_button, 1, iterations);
    results[n++] = run_case("Button", bench_button, 0, iterations);
    results[n++] = run_case("Dropdown", bench_dropdown, 1, iterations);
    results[n++] = run_case("Dropdown", bench_dropdown, 0, iterations);

    for(int i = 0; i < n; ++i)
        print_json(results[i]);
    print_markdown(results, n);
    return validate_results(results, n) == 0 ? 0 : 1;
}
