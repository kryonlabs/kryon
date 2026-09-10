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

typedef void (*BenchFn)(Counters *, unsigned long long);

typedef struct Result {
    const char *widget;
    const char *appearance;
    unsigned long long iterations;
    double target_seconds;
    double wall_seconds;
    double startup_us;
    double wall_us_per_frame;
    double cpu_us_per_frame;
    double cpu_total_ms;
    double cpu_percent;
    double fps_throughput;
    double cpu_percent_at_60fps;
    double cpu_percent_at_60fps_1000;
    long rss_start_kib;
    long rss_end_kib;
    long rss_peak_kib;
    long rss_delta_kib;
    long rss_peak_delta_kib;
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

typedef struct InteractionSample {
    float hover;
    float press;
    float focus;
    ButtonState state;
} InteractionSample;

static float
ramp(unsigned long long position, unsigned long long start, unsigned long long end)
{
    if(position <= start)
        return 0.0f;
    if(position >= end)
        return 1.0f;
    return (float)(position - start) / (float)(end - start);
}

static InteractionSample
interaction_sample(unsigned long long i, unsigned long long offset)
{
    unsigned long long phase = (i + offset) % 360ull;
    InteractionSample sample;
    memset(&sample, 0, sizeof(sample));
    sample.state = ButtonStateNormal;

    if(phase < 60ull) {
        return sample;
    } else if(phase < 140ull) {
        sample.hover = ramp(phase, 60ull, 140ull);
    } else if(phase < 200ull) {
        sample.hover = 1.0f;
        sample.press = ramp(phase, 140ull, 200ull);
    } else if(phase < 240ull) {
        sample.hover = 1.0f;
        sample.press = 1.0f - ramp(phase, 200ull, 240ull);
        sample.focus = ramp(phase, 200ull, 240ull);
    } else if(phase < 320ull) {
        sample.hover = 1.0f - ramp(phase, 240ull, 320ull);
        sample.focus = 1.0f;
    } else {
        sample.focus = 1.0f - ramp(phase, 320ull, 360ull);
    }

    if(sample.press > 0.05f)
        sample.state = ButtonStatePressed;
    else if(sample.hover > 0.05f)
        sample.state = ButtonStateHover;
    else if(sample.focus > 0.05f)
        sample.state = ButtonStateFocus;
    return sample;
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
    if(role == 0)
        return ui_style_apply_effects(base);
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
bench_text(Counters *c, unsigned long long i)
{
    (void)i;
    Drawing text = {.kind = DrawingText, .bounds = {24, 24, 188, 24},
                    .text = "Human tomorrow", .font = 18,
                    .color = PackedColor(0xf5f8ffu, 255u)};
    drawing_counter(c, text);
}

static void
bench_button(Counters *c, unsigned long long i)
{
    ButtonProps props;
    memset(&props, 0, sizeof(props));
    props.bounds = (Rectangle){20, 20, 168, 42};
    props.id = 100 + (i & 31);
    props.label = "Save changes";
    props.tone = ButtonToneAccent;
    props.emphasis = ButtonEmphasisFilled;
    props.size = ControlSizeMedium;
    InteractionSample sample = interaction_sample(i, 0ull);
    ButtonFrame frame = make_button_frame(props, sample.state, sample.hover, sample.press, sample.focus);
    SurfacePainter surface = {.context = c, .call = surface_counter};
    Painter painter = {.context = c, .call = drawing_counter};
    PaintButton(frame, 104.0f, (double)i * 16.67, 0, surface, painter);
}

static void
bench_text_input(Counters *c, unsigned long long i)
{
    ButtonProps props;
    memset(&props, 0, sizeof(props));
    props.bounds = (Rectangle){20, 20, 260, 42};
    props.id = 300 + (i & 31);
    props.label = "";
    props.tone = ButtonToneNeutral;
    props.emphasis = ButtonEmphasisSoft;
    props.size = ControlSizeMedium;
    InteractionSample sample = interaction_sample(i, 73ull);
    ButtonFrame frame = make_button_frame(props, sample.state, sample.hover * 0.35f, sample.press * 0.25f, sample.focus);
    paint_material(c, frame.material);
    drawing_counter(c, (Drawing){.kind = DrawingText, .bounds = {32, 30, 190, 20},
        .text = "freelancermap.de", .font = 18, .color = frame.foreground});
    drawing_counter(c, (Drawing){.kind = DrawingText, .bounds = {226, 30, 1, 20},
        .text = "|", .font = 18, .color = PackedColor(0x409cffu, 255u)});
}

static void
bench_dropdown(Counters *c, unsigned long long i)
{
    ButtonProps props;
    memset(&props, 0, sizeof(props));
    props.bounds = (Rectangle){20, 20, 232, 42};
    props.id = 500 + (i & 31);
    props.label = "Inner Breeze";
    props.tone = ButtonToneNeutral;
    props.emphasis = ButtonEmphasisSoft;
    props.size = ControlSizeMedium;
    InteractionSample sample = interaction_sample(i, 149ull);
    int selected = ((i / 90ull) & 1ull) != 0ull;
    Style paint = make_dropdown_style(selected ? 2 : 0, selected, sample.state);
    StyleFrame appearance = {.value = ui_pack_style_states((ControlStyle){.normal = paint}).normal};
    appearance.fill = ui_style_apply_effects_fill(FillState(
        appearance.value.fields, appearance.value.background,
        appearance.value.background_end));
    Activation activation = {.hovered = sample.hover > 0.05f, .pressed = sample.press > 0.05f, .focused = sample.focus > 0.05f};
    ButtonInput input = ResolveButtonInput(props, activation);
    ButtonFrame frame = BuildFrame(props, input, appearance, sample_motion(sample.hover, sample.press, sample.focus),
                                   (Rectangle){0}, PackedColor(0x092039u, 255u), 1.0f,
                                   (int)appearance.value.font_size, 18);
    paint_material(c, frame.material);
    drawing_counter(c, (Drawing){.kind = DrawingText, .bounds = {34, 30, 130, 20},
        .text = "Inner Breeze", .font = frame.font, .color = frame.foreground});
    drawing_counter(c, (Drawing){.kind = DrawingChevron, .bounds = {222, 35, 10, 8},
        .color = frame.foreground});
}

static double
startup_sample_us(BenchFn fn, int fancy, unsigned long long startup_iteration)
{
    enum { STARTUP_BATCH = 256 };
    Counters counters;
    double start;

    memset(&counters, 0, sizeof(counters));
    SetFancyEffectsEnabled(fancy);
    start = now_us();
    for(int i = 0; i < STARTUP_BATCH; ++i)
        fn(&counters, startup_iteration);
    return (now_us() - start) / (double)STARTUP_BATCH;
}

static Result
run_case(const char *widget, BenchFn fn, int fancy, unsigned long long min_iterations,
         double target_seconds, unsigned long long startup_iteration)
{
    enum { WARMUP = 2000, STARTUP_SAMPLES = 32, CHECK_INTERVAL = 65536 };
    Counters counters;
    double startup_us, wall_start, wall_end, cpu_start, cpu_end, target_us;
    long rss_before, rss_after, rss_peak;
    unsigned long long iterations = 0;
    const char *appearance = fancy ? "glow" : "simple";

    startup_us = startup_sample_us(fn, fancy, startup_iteration);
    for(int i = 1; i < STARTUP_SAMPLES; ++i) {
        double sample = startup_sample_us(fn, fancy, startup_iteration);
        if(sample < startup_us)
            startup_us = sample;
    }

    memset(&counters, 0, sizeof(counters));
    SetFancyEffectsEnabled(fancy);
    for(int i = 0; i < WARMUP; ++i)
        fn(&counters, (unsigned long long)i);

    memset(&counters, 0, sizeof(counters));
    rss_before = rss_kib();
    rss_peak = rss_before;
    cpu_start = cpu_us();
    wall_start = now_us();
    target_us = target_seconds * 1000000.0;
    if(target_us < 0.0)
        target_us = 0.0;

    for(;;) {
        fn(&counters, iterations);
        iterations++;
        if((iterations % CHECK_INTERVAL) == 0ull) {
            long current_rss = rss_kib();
            double elapsed = now_us() - wall_start;
            if(current_rss > rss_peak)
                rss_peak = current_rss;
            if(iterations >= min_iterations && elapsed >= target_us)
                break;
        }
    }

    wall_end = now_us();
    cpu_end = cpu_us();
    rss_after = rss_kib();
    if(rss_after > rss_peak)
        rss_peak = rss_after;

    Result result = {
        .widget = widget,
        .appearance = appearance,
        .iterations = iterations,
        .target_seconds = target_seconds,
        .wall_seconds = (wall_end - wall_start) / 1000000.0,
        .startup_us = startup_us,
        .wall_us_per_frame = (wall_end - wall_start) / (double)iterations,
        .cpu_us_per_frame = (cpu_end - cpu_start) / (double)iterations,
        .cpu_total_ms = (cpu_end - cpu_start) / 1000.0,
        .cpu_percent = (wall_end > wall_start) ? (cpu_end - cpu_start) / (wall_end - wall_start) * 100.0 : 0.0,
        .fps_throughput = (wall_end > wall_start) ? (double)iterations * 1000000.0 / (wall_end - wall_start) : 0.0,
        .cpu_percent_at_60fps = (cpu_end - cpu_start) / (double)iterations * 60.0 / 1000000.0 * 100.0,
        .cpu_percent_at_60fps_1000 = (cpu_end - cpu_start) / (double)iterations * 60.0 / 1000000.0 * 100000.0,
        .rss_start_kib = rss_before,
        .rss_end_kib = rss_after,
        .rss_peak_kib = rss_peak,
        .rss_delta_kib = rss_after - rss_before,
        .rss_peak_delta_kib = rss_peak - rss_before,
        .layers_per_frame = (double)counters.surface_calls / (double)iterations,
        .visible_layers_per_frame = (double)counters.surface_visible / (double)iterations,
        .draw_calls_per_frame = (double)counters.draw_calls / (double)iterations,
        .glow_layers = counters.glow_layers
    };
    return result;
}

static unsigned long long
iterations_from_env(void)
{
    const char *value = getenv("KRYON_APPEARANCE_BENCH_MIN_ITERS");
    unsigned long long parsed;
    if(value == NULL || *value == '\0') {
        value = getenv("KRYON_APPEARANCE_BENCH_ITERS");
        if(value == NULL || *value == '\0')
            return 1000ull;
    }
    parsed = strtoull(value, NULL, 10);
    if(parsed < 1ull)
        parsed = 1ull;
    return parsed;
}

static double
seconds_from_env(void)
{
    const char *value = getenv("KRYON_APPEARANCE_BENCH_SECONDS");
    double parsed;
    if(value == NULL || *value == '\0')
        return 10.0;
    parsed = strtod(value, NULL);
    if(parsed < 0.1)
        parsed = 0.1;
    if(parsed > 60.0)
        parsed = 60.0;
    return parsed;
}

static void
print_json(Result r)
{
    printf("{\"benchmark\":\"control_appearance\",\"widget\":\"%s\",\"appearance\":\"%s\",\"target_seconds\":%.3f,\"wall_seconds\":%.3f,\"iterations\":%llu,\"startup_us\":%.3f,\"wall_us_per_frame\":%.3f,\"cpu_us_per_frame\":%.3f,\"cpu_total_ms\":%.3f,\"cpu_percent\":%.2f,\"fps_throughput\":%.2f,\"cpu_percent_at_60fps\":%.4f,\"cpu_percent_at_60fps_1000\":%.2f,\"rss_start_kib\":%ld,\"rss_end_kib\":%ld,\"rss_peak_kib\":%ld,\"rss_delta_kib\":%ld,\"rss_peak_delta_kib\":%ld,\"surface_layers_per_frame\":%.2f,\"visible_layers_per_frame\":%.2f,\"draw_calls_per_frame\":%.2f,\"glow_layers_total\":%llu}\n",
           r.widget, r.appearance, r.target_seconds, r.wall_seconds,
           r.iterations, r.startup_us, r.wall_us_per_frame,
           r.cpu_us_per_frame, r.cpu_total_ms, r.cpu_percent,
           r.fps_throughput, r.cpu_percent_at_60fps, r.cpu_percent_at_60fps_1000,
           r.rss_start_kib, r.rss_end_kib, r.rss_peak_kib,
           r.rss_delta_kib, r.rss_peak_delta_kib,
           r.layers_per_frame, r.visible_layers_per_frame,
           r.draw_calls_per_frame, r.glow_layers);
    fflush(stdout);
}

static void
print_markdown(Result *results, int count)
{
    puts("| Widget | Appearance | Seconds | Iterations | Startup us | CPU ms | Saturated CPU % | 60 FPS CPU % | 60 FPS x1000 CPU % | CPU us/frame | Wall us/frame | RSS start KiB | RSS end KiB | RSS peak KiB | Peak delta KiB | Surface layers/frame | Draw calls/frame | Glow layers/frame |");
    puts("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|");
    for(int i = 0; i < count; ++i) {
        Result r = results[i];
        printf("| %s | %s | %.3f | %llu | %.3f | %.3f | %.2f | %.4f | %.2f | %.3f | %.3f | %ld | %ld | %ld | %ld | %.2f | %.2f | %.2f |\n",
               r.widget, r.appearance, r.wall_seconds, r.iterations,
               r.startup_us, r.cpu_total_ms, r.cpu_percent,
               r.cpu_percent_at_60fps, r.cpu_percent_at_60fps_1000,
               r.cpu_us_per_frame, r.wall_us_per_frame,
               r.rss_start_kib, r.rss_end_kib, r.rss_peak_kib,
               r.rss_peak_delta_kib, r.layers_per_frame,
               r.draw_calls_per_frame,
               (double)r.glow_layers / (double)r.iterations);
    }
}

static int
validate_results(Result *results, int count)
{
    int failures = 0;
    for(int i = 0; i < count; ++i) {
        if(results[i].startup_us >= 2.0)
            failures++;
    }
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

static void
warm_benchmark_process(void)
{
    Counters counters;
    memset(&counters, 0, sizeof(counters));
    for(int i = 0; i < 128; ++i)
        (void)now_us();
    bench_text(&counters, 0);
    bench_button(&counters, 0);
    bench_text_input(&counters, 0);
    bench_dropdown(&counters, 0);
}

int
main(void)
{
    unsigned long long min_iterations = iterations_from_env();
    double target_seconds = seconds_from_env();
    Result results[8];
    int n = 0;

    warm_benchmark_process();
    results[n++] = run_case("Text", bench_text, 1, min_iterations, target_seconds, 0ull);
    print_json(results[n - 1]);
    results[n++] = run_case("Text", bench_text, 0, min_iterations, target_seconds, 0ull);
    print_json(results[n - 1]);
    results[n++] = run_case("TextInput", bench_text_input, 1, min_iterations, target_seconds, 287ull);
    print_json(results[n - 1]);
    results[n++] = run_case("TextInput", bench_text_input, 0, min_iterations, target_seconds, 287ull);
    print_json(results[n - 1]);
    results[n++] = run_case("Button", bench_button, 1, min_iterations, target_seconds, 0ull);
    print_json(results[n - 1]);
    results[n++] = run_case("Button", bench_button, 0, min_iterations, target_seconds, 0ull);
    print_json(results[n - 1]);
    results[n++] = run_case("Dropdown", bench_dropdown, 1, min_iterations, target_seconds, 211ull);
    print_json(results[n - 1]);
    results[n++] = run_case("Dropdown", bench_dropdown, 0, min_iterations, target_seconds, 211ull);
    print_json(results[n - 1]);

    print_markdown(results, n);
    return validate_results(results, n) == 0 ? 0 : 1;
}
