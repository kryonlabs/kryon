#include "kryon_plan9.h"

static void
put16(unsigned char *p, int v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
}

void
main(void)
{
    enum { rate = 44100, seconds = 1, frames = rate * seconds };
    unsigned char *pcm;
    Wave wave;
    Sound sound;
    int i;
    int s;

    pcm = malloc(frames * 2 * 2);
    if(pcm == nil) {
        print("audio probe: malloc failed\n");
        exits("malloc");
    }
    for(i = 0; i < frames; i++) {
        s = ((i / 50) & 1) ? 12000 : -12000;
        put16(pcm + i * 4 + 0, s);
        put16(pcm + i * 4 + 2, s);
    }

    memset(&wave, 0, sizeof(wave));
    wave.frameCount = frames;
    wave.sampleRate = rate;
    wave.sampleSize = 16;
    wave.channels = 2;
    wave.data = pcm;

    InitAudioDevice();
    print("audio ready=%d\n", IsAudioDeviceReady() ? 1 : 0);
    if(!IsAudioDeviceReady()) {
        free(pcm);
        exits("noaudio");
    }

    sound = LoadSoundFromWave(wave);
    print("sound valid=%d frames=%ud\n",
          IsSoundValid(sound) ? 1 : 0, sound.frameCount);
    PlaySound(sound);
    print("playing after play=%d\n", IsSoundPlaying(sound) ? 1 : 0);
    sleep(250);
    print("playing after 250ms=%d\n", IsSoundPlaying(sound) ? 1 : 0);
    sleep(1200);
    print("playing after end=%d\n", IsSoundPlaying(sound) ? 1 : 0);

    UnloadSound(sound);
    CloseAudioDevice();
    free(pcm);
    exits(nil);
}
