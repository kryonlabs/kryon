/*
 * AudioSource: plays a sound effect or a streaming music track. Wraps raylibs
 * audio API (LoadSound/PlaySound, LoadMusicStream/PlayMusicStream). The handle
 * is loaded lazily on first play; music streams are updated each process tick.
 * The node draws nothing. InitAudioDevice must have been called by the app.
 */

#include "scene_tree.h"
#include "node2d_props.h"
#include <stdlib.h>

static void
kry_audio_source_ensure_loaded(AudioSourceProps *props)
{
    if(props == NULL || props->loaded || props->asset_path == NULL ||
       props->asset_path[0] == '\0')
        return;
    if(props->kind == KRY_AUDIO_SOUND) {
        Sound *s = malloc(sizeof(Sound));
        if(s == NULL)
            return;
        *s = LoadSound(props->asset_path);
        props->handle = s;
        props->loaded = 1;
    } else {
        Music *m = malloc(sizeof(Music));
        if(m == NULL)
            return;
        *m = LoadMusicStream(props->asset_path);
        m->looping = props->loop != 0;
        props->handle = m;
        props->loaded = 1;
    }
}

static void
kry_audio_source_process(Scene *scene, NodeId node, float dt)
{
    Node *n = NodeGet(scene, node);
    AudioSourceProps *props;
    (void)dt;
    if(n == NULL)
        return;
    props = (AudioSourceProps *)n->props;
    if(props == NULL || !props->playing || !props->loaded)
        return;
    if(props->kind == KRY_AUDIO_MUSIC && props->handle != NULL)
        UpdateMusicStream(*(Music *)props->handle);
}

static void
kry_audio_source_destroy(Scene *scene, Node *node)
{
    AudioSourceProps *props;
    (void)scene;
    props = (AudioSourceProps *)node->props;
    if(props == NULL)
        return;
    if(props->loaded && props->handle != NULL) {
        if(props->kind == KRY_AUDIO_SOUND)
            UnloadSound(*(Sound *)props->handle);
        else
            UnloadMusicStream(*(Music *)props->handle);
        free(props->handle);
    }
    free(props);
    node->props = NULL;
}

static const NodeOps kry_audio_source_ops = {
    NULL,
    kry_audio_source_process,
    NULL,
    NULL
};

void
kry_register_audio_source(void)
{
    NodeRegisterOps(NODE_AUDIO_SOURCE, &kry_audio_source_ops);
    NodeRegisterDestroy(NODE_AUDIO_SOURCE, kry_audio_source_destroy);
}

AudioSourceProps *
KryAudioSourcePropsAlloc(const char *asset_path, KryAudioKind kind)
{
    AudioSourceProps *p = calloc(1, sizeof(*p));
    if(p != NULL) {
        p->asset_path = asset_path;
        p->kind = kind;
        p->volume = 1.0f;
        p->pitch = 1.0f;
    }
    return p;
}

/* Public play/stop controls called by app code or k2c-generated builders. */
void
KryAudioSourcePlay(Scene *scene, NodeId node)
{
    Node *n = NodeGet(scene, node);
    AudioSourceProps *props;
    if(n == NULL)
        return;
    props = (AudioSourceProps *)n->props;
    if(props == NULL)
        return;
    kry_audio_source_ensure_loaded(props);
    if(!props->loaded || props->handle == NULL)
        return;
    if(props->kind == KRY_AUDIO_SOUND) {
        Sound *s = (Sound *)props->handle;
        SetSoundVolume(*s, props->volume);
        SetSoundPitch(*s, props->pitch);
        PlaySound(*s);
    } else {
        Music *m = (Music *)props->handle;
        SetMusicVolume(*m, props->volume);
        m->looping = props->loop != 0;
        PlayMusicStream(*m);
    }
    props->playing = 1;
}

void
KryAudioSourceStop(Scene *scene, NodeId node)
{
    Node *n = NodeGet(scene, node);
    AudioSourceProps *props;
    if(n == NULL)
        return;
    props = (AudioSourceProps *)n->props;
    if(props == NULL || !props->loaded || props->handle == NULL)
        return;
    if(props->kind == KRY_AUDIO_MUSIC)
        StopMusicStream(*(Music *)props->handle);
    props->playing = 0;
}
