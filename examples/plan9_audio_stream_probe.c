#include "kryon_plan9.h"

static void
attenuate(void *buffer, unsigned int frames)
{
    float *samples;
    unsigned int i;

    samples = (float *)buffer;
    for(i = 0; i < frames * 2; i++)
        samples[i] *= 0.5f;
}

void
main(void)
{
    AudioStream stream;
    short pcm[4410 * 2];
    int i;
    short s;

    for(i = 0; i < 4410; i++) {
        s = (i % 80) < 40 ? 9000 : -9000;
        pcm[i * 2 + 0] = s;
        pcm[i * 2 + 1] = s;
    }

    InitAudioDevice();
    print("audio ready=%d\n", IsAudioDeviceReady());

    SetAudioStreamBufferSizeDefault(1024);
    stream = LoadAudioStream(44100, 16, 2);
    print("stream valid=%d processed=%d\n",
          IsAudioStreamValid(stream), IsAudioStreamProcessed(stream));

    AttachAudioStreamProcessor(stream, attenuate);
    UpdateAudioStream(stream, pcm, 4410);
    print("stream processed after update=%d\n", IsAudioStreamProcessed(stream));

    PlayAudioStream(stream);
    print("playing after play=%d\n", IsAudioStreamPlaying(stream));
    sleep(50);
    PauseAudioStream(stream);
    print("playing after pause=%d\n", IsAudioStreamPlaying(stream));
    ResumeAudioStream(stream);
    print("playing after resume=%d\n", IsAudioStreamPlaying(stream));
    sleep(150);
    print("playing after drain=%d processed=%d\n",
          IsAudioStreamPlaying(stream), IsAudioStreamProcessed(stream));

    StopAudioStream(stream);
    print("playing after stop=%d\n", IsAudioStreamPlaying(stream));
    DetachAudioStreamProcessor(stream, attenuate);
    UnloadAudioStream(stream);
    CloseAudioDevice();
    exits(nil);
}
