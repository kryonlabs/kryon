#!/bin/sh
set -eu

# The browser audio state machines run as native code against a fake
# WebAudio host with a clock the test controls, so positions, scheduling,
# and limits are checked exactly without a browser or a sound card.
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
ziran_dir=$(CDPATH= cd -- "$bin/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$bin/zi2c" --no-main --define PLATFORM_WEB --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$ziran_dir/std" \
    -o "$work/c" "$repo/tests/canvas_audio_engine_behavior.zi"
cat > "$work/c/main.c" <<'C'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canvas_audio_engine_behavior.h"

#define LIMIT 256
static double now;
static int context_open;
static double master;
static struct { int used, frames, rate, channels; } buffers[LIMIT];
static struct {
    int used, live; double field[6];
} sources[LIMIT];
static int last_source;
static struct { int used, interval; CanvasAudioTimer tick; } timers[LIMIT];

void fake_set_time(double seconds) { now = seconds; }
void fake_end_source(int32_t source)
{ if (source > 0 && source < LIMIT) sources[source].live = 0; }
int32_t fake_live_sources(void)
{
    int count = 0;
    for (int i = 0; i < LIMIT; i++) count += sources[i].used && sources[i].live;
    return count;
}
int32_t fake_source_live(int32_t source)
{ return source > 0 && source < LIMIT && sources[source].used && sources[source].live; }
double fake_source_field(int32_t source, int32_t field)
{ return source > 0 && source < LIMIT ? sources[source].field[field] : 0.0; }
int32_t fake_last_source(void) { return last_source; }
int32_t fake_timer_interval(int32_t handle)
{ return handle > 0 && handle < LIMIT ? timers[handle].interval : 0; }
void fake_fire_timer(int32_t handle)
{ if (handle > 0 && handle < LIMIT && timers[handle].used) timers[handle].tick(handle); }
int32_t fake_timer_count(void)
{
    int count = 0;
    for (int i = 0; i < LIMIT; i++) count += timers[i].used;
    return count;
}
int32_t fake_buffer_count(void)
{
    int count = 0;
    for (int i = 0; i < LIMIT; i++) count += buffers[i].used;
    return count;
}
int32_t fake_context_open(void) { return context_open; }
double fake_master_volume(void) { return master; }

int32_t js_wa_open(void) { context_open = 1; return 1; }
void js_wa_close(void)
{
    context_open = 0;
    memset(buffers, 0, sizeof(buffers));
    memset(sources, 0, sizeof(sources));
    memset(timers, 0, sizeof(timers));
}
void js_wa_resume(void) {}
double js_wa_time(void) { return now; }
double js_wa_rate(void) { return 48000.0; }
void js_wa_master(float volume) { master = volume; }
int32_t js_wa_buffer_pcm(int32_t handle, void *data, int32_t frames,
    int32_t rate, int32_t size, int32_t channels)
{
    (void)size;
    if (!data || handle <= 0 || handle >= LIMIT) return 0;
    buffers[handle].used = 1;
    buffers[handle].frames = frames;
    buffers[handle].rate = rate;
    buffers[handle].channels = channels;
    return 1;
}
int32_t js_wa_buffer_file(int32_t handle, uint8_t *path)
{ (void)handle; (void)path; return 0; }
int32_t js_wa_buffer_memory(int32_t handle, uint8_t *data, int32_t size)
{ (void)handle; (void)data; (void)size; return 0; }
double js_wa_buffer_read(int32_t handle, int32_t field)
{
    if (handle <= 0 || handle >= LIMIT || !buffers[handle].used) return 0.0;
    if (field == 0) return buffers[handle].frames;
    if (field == 1) return buffers[handle].rate;
    if (field == 2) return buffers[handle].channels;
    return (double)buffers[handle].frames / buffers[handle].rate;
}
void *js_wa_buffer_export(int32_t handle, int32_t *rate, int32_t *channels,
    int32_t *frames)
{ (void)handle; (void)rate; (void)channels; (void)frames; return NULL; }
void js_wa_buffer_drop(int32_t handle)
{ if (handle > 0 && handle < LIMIT) buffers[handle].used = 0; }
static int start(int32_t source, double offset, double loop, double rate,
    double volume, double pan, double when)
{
    if (source <= 0 || source >= LIMIT) return 0;
    sources[source].used = 1;
    sources[source].live = 1;
    sources[source].field[0] = offset;
    sources[source].field[1] = loop;
    sources[source].field[2] = rate;
    sources[source].field[3] = volume;
    sources[source].field[4] = pan;
    sources[source].field[5] = when;
    last_source = source;
    return 1;
}
int32_t js_wa_source_start(int32_t source, int32_t buffer, double offset,
    int32_t loop, double rate, double volume, double pan)
{
    if (buffer <= 0 || buffer >= LIMIT || !buffers[buffer].used) return 0;
    return start(source, offset, loop, rate, volume, pan, 0.0);
}
int32_t js_wa_source_pcm(int32_t source, void *data, int32_t frames,
    int32_t sample_rate, int32_t sample_size, int32_t channels, double when,
    double rate, double volume, double pan)
{
    (void)data; (void)frames; (void)sample_rate; (void)sample_size;
    (void)channels;
    return start(source, 0.0, 0.0, rate, volume, pan, when);
}
void js_wa_source_stop(int32_t source)
{ if (source > 0 && source < LIMIT) memset(&sources[source], 0, sizeof(sources[source])); }
void js_wa_source_set(int32_t source, double volume, double pan, double rate)
{
    if (source <= 0 || source >= LIMIT || !sources[source].used) return;
    sources[source].field[3] = volume;
    sources[source].field[4] = pan;
    sources[source].field[2] = rate;
}
int32_t js_wa_source_done(int32_t source)
{ return !(source > 0 && source < LIMIT && sources[source].used && sources[source].live); }
void js_wa_timer(int32_t handle, int32_t interval, CanvasAudioTimer tick)
{
    if (handle <= 0 || handle >= LIMIT) return;
    timers[handle].used = 1;
    timers[handle].interval = interval;
    timers[handle].tick = tick;
}
void js_wa_timer_stop(int32_t handle)
{ if (handle > 0 && handle < LIMIT) timers[handle].used = 0; }

int main(void)
{
    int result = Answer();
    if (result != 42) fprintf(stderr, "audio engine check %d failed\n", result);
    return result == 42 ? 0 : 1;
}
C
"${CC:-cc}" -std=c11 -Wall -Wextra -Wno-unused-function \
    -I"$ziran_dir/include" -I"$work/c" "$work"/c/*.c -lm -o "$work/test"
"$work/test"
echo "Kryon browser audio state passed"
