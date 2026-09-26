#include <stddef.h>
#include <stdio.h>

#ifdef GENERATED
#include "raylib_game.h"
#define WAVE_FRAME frame_count
#define WAVE_RATE sample_rate
#define WAVE_SAMPLE sample_size
#define SOUND_FRAME frame_count
#define STREAM_RATE sample_rate
#else
#include "raylib.h"
#define WAVE_FRAME frameCount
#define WAVE_RATE sampleRate
#define WAVE_SAMPLE sampleSize
#define SOUND_FRAME frameCount
#define STREAM_RATE sampleRate
#endif

int main(void)
{
    printf("%zu %zu %zu %zu %zu %zu %zu %zu %zu %zu\n",
           sizeof(Wave), offsetof(Wave, WAVE_FRAME),
           offsetof(Wave, WAVE_RATE), offsetof(Wave, WAVE_SAMPLE),
           offsetof(Wave, channels), offsetof(Wave, data),
           sizeof(AudioStream), offsetof(AudioStream, STREAM_RATE),
           sizeof(Sound), offsetof(Sound, SOUND_FRAME));
    return 0;
}
