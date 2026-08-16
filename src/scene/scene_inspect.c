/*
 * Live scene inspection server. See include/scene_inspect.h.
 *
 * The game thread serialises the tree into a JSON snapshot (SceneInspectPoll);
 * a small accept-loop thread hands the latest snapshot to every HTTP request.
 * A mutex guards only the snapshot buffer swap, so the scene is never read
 * from the socket thread.
 */

#include "scene_inspect.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define INSPECT_CLOSE closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define INSPECT_CLOSE close
#define INVALID_SOCKET -1
#endif

typedef struct InspectState {
    Scene *scene;
    int listen_fd;
    int running;
    int port;
    KryThread thread;
    KryMutex lock;
    char *snapshot;
    size_t snapshot_len;
    char *next;         /* snapshot being built by Poll (game thread only) */
    size_t next_len;
    size_t next_cap;
} InspectState;

static InspectState g_inspect;

/* --- snapshot builder --------------------------------------------------- */

static void
buf_append(InspectState *st, const char *text, size_t len)
{
    if(st->next_len + len + 1 > st->next_cap) {
        size_t cap = st->next_cap ? st->next_cap : 4096;

        while(cap < st->next_len + len + 1)
            cap *= 2;
        st->next = realloc(st->next, cap);
        if(st->next == NULL)
            return;
        st->next_cap = cap;
    }
    memcpy(st->next + st->next_len, text, len);
    st->next_len += len;
    st->next[st->next_len] = '\0';
}

static void
buf_str(InspectState *st, const char *text)
{
    buf_append(st, text, strlen(text));
}

static void
buff(InspectState *st, const char *fmt, ...)
{
    char tmp[256];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if(n > 0)
        buf_append(st, tmp, (size_t)n);
}

static void
buf_json_string(InspectState *st, const char *s)
{
    buf_str(st, "\"");
    for(const unsigned char *p = (const unsigned char *)(s ? s : "");
        *p != '\0'; p++) {
        if(*p == '"' || *p == '\\') {
            buf_str(st, "\\");
            buf_append(st, (const char *)p, 1);
        } else if(*p < 0x20) {
            buff(st, "\\u%04x", *p);
        } else {
            buf_append(st, (const char *)p, 1);
        }
    }
    buf_str(st, "\"");
}

static const char *
builtin_kind_name(int kind)
{
    switch(kind) {
    case NODE_ROOT: return "Root";
    case NODE_NODE2D: return "Node2D";
    case NODE_CAMERA2D: return "Camera2D";
    case NODE_SPRITE2D: return "Sprite2D";
    case NODE_ANIMATED_SPRITE2D: return "AnimatedSprite2D";
    case NODE_TILEMAP: return "TileMap";
    case NODE_COLLISION_SHAPE2D: return "CollisionShape2D";
    case NODE_AREA2D: return "Area2D";
    case NODE_BODY2D: return "Body2D";
    case NODE_TIMER: return "Timer";
    case NODE_AUDIO_SOURCE: return "AudioSource";
    case NODE_CUSTOM: return "Custom";
    default: return NULL;
    }
}

static void
buf_property_value(InspectState *st, PropertyValue v)
{
    switch(v.kind) {
    case PROPERTY_BOOL:
        buf_append(st, v.as.bool_value ? "true" : "false", v.as.bool_value ? 4 : 5);
        break;
    case PROPERTY_INT:
        buff(st, "%d", v.as.int_value);
        break;
    case PROPERTY_FLOAT:
        buff(st, "%g", (double)v.as.float_value);
        break;
    case PROPERTY_STRING:
        buf_json_string(st, v.as.string_value);
        break;
    case PROPERTY_COLOR:
        buff(st, "\"#%02x%02x%02x%02x\"", v.as.color_value.r,
            v.as.color_value.g, v.as.color_value.b, v.as.color_value.a);
        break;
    case PROPERTY_ENUM:
        buff(st, "%d", v.as.enum_index);
        break;
    case PROPERTY_VECTOR2:
        buff(st, "[%g,%g]", (double)v.as.vector2_value.x,
            (double)v.as.vector2_value.y);
        break;
    case PROPERTY_RECTANGLE:
        buff(st, "[%g,%g,%g,%g]", (double)v.as.rectangle_value.x,
            (double)v.as.rectangle_value.y, (double)v.as.rectangle_value.width,
            (double)v.as.rectangle_value.height);
        break;
    default:
        buf_str(st, "null");
        break;
    }
}

void
SceneInspectPoll(Scene *scene)
{
    InspectState *st = &g_inspect;
    int alive_count = 0;
    int i;

    if(scene == NULL)
        return;
    st->scene = scene;
    st->next_len = 0;

    for(i = 0; i < scene->count; i++)
        if(scene->nodes[i].flags & NODE_FLAG_ALIVE)
            alive_count++;

    buf_str(st, "{\"count\":");
    buff(st, "%d,\"nodes\":[", alive_count);
    for(i = 0; i < scene->count; i++) {
        const Node *n = &scene->nodes[i];
        const PropertySpec *specs;
        int spec_count = 0;
        const char *kind;

        if(!(n->flags & NODE_FLAG_ALIVE))
            continue;
        kind = builtin_kind_name(n->kind);
        if(kind == NULL)
            kind = NodeKindName(n->kind);
        if(kind == NULL)
            kind = "Unknown";
        if(alive_count-- == 0)
            break;
        buff(st, "%s{\"id\":%d,\"parent\":%d,\"name\":", i > 0 ? "," : "",
             n->id, n->parent);
        buf_json_string(st, n->name);
        buf_str(st, ",\"kind\":");
        buf_json_string(st, kind);
        buff(st, ",\"position\":[%g,%g]", (double)n->local.position.x,
             (double)n->local.position.y);
        buff(st, ",\"children\":[");
        {
            int child = n->first_child;
            int first = 1;

            while(child >= 0) {
                buff(st, "%s%d", first ? "" : ",", child);
                first = 0;
                child = scene->nodes[child].next_sibling;
            }
        }
        buf_str(st, "]");
        specs = ScenePropertySpecs(n->kind, &spec_count);
        if(specs != NULL && spec_count > 0) {
            int j;
            int shown = 0;

            buf_str(st, ",\"props\":[");
            for(j = 0; j < spec_count; j++) {
                PropertyValue v = SceneNodeGetProperty((Scene *)scene,
                                                       n->id, j);

                if(v.kind == PROPERTY_BOOL || v.kind == PROPERTY_INT ||
                   v.kind == PROPERTY_FLOAT || v.kind == PROPERTY_STRING ||
                   v.kind == PROPERTY_COLOR || v.kind == PROPERTY_ENUM ||
                   v.kind == PROPERTY_VECTOR2 ||
                   v.kind == PROPERTY_RECTANGLE) {
                    buff(st, "%s{\"id\":", shown > 0 ? "," : "");
                    buf_json_string(st, specs[j].id);
                    buf_str(st, ",\"label\":");
                    buf_json_string(st, specs[j].label);
                    buf_str(st, ",\"value\":");
                    buf_property_value(st, v);
                    buf_str(st, "}");
                    shown++;
                }
            }
            buf_str(st, "]");
        }
        buf_str(st, "}");
    }
    buf_str(st, "]}");

    KryMutexLock(&st->lock);
    free(st->snapshot);
    st->snapshot = st->next;
    st->snapshot_len = st->next_len;
    st->next = NULL;
    st->next_cap = 0;
    st->next_len = 0;
    KryMutexUnlock(&st->lock);
}

/* --- socket thread ------------------------------------------------------ */

static const char HTTP_HEAD[] =
    "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
    "Connection: close\r\n\r\n";

static void *
inspect_thread(void *userdata)
{
    InspectState *st = userdata;

    while(st->running) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        int client = accept(st->listen_fd, (struct sockaddr *)&addr, &addr_len);
        char request[1024];
        char *body = NULL;
        size_t body_total = 0;

        if(client < 0)
            continue;
        /* drain the request (browsers/curl send a GET first) */
        recv(client, request, sizeof(request), 0);
        KryMutexLock(&st->lock);
        if(st->snapshot != NULL && st->snapshot_len > 0) {
            size_t head_len = sizeof(HTTP_HEAD) - 1;

            body_total = head_len + st->snapshot_len;
            body = malloc(body_total);
            if(body != NULL) {
                memcpy(body, HTTP_HEAD, head_len);
                memcpy(body + head_len, st->snapshot, st->snapshot_len);
            }
        }
        KryMutexUnlock(&st->lock);
        if(body != NULL) {
            size_t sent = 0;

            while(sent < body_total) {
                long n = send(client, body + sent, body_total - sent, 0);

                if(n <= 0)
                    break;
                sent += (size_t)n;
            }
            free(body);
        } else {
            const char *empty =
                "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
                "Connection: close\r\n\r\n{}";

            send(client, empty, strlen(empty), 0);
        }
        INSPECT_CLOSE(client);
    }
    return NULL;
}

/* --- lifecycle ---------------------------------------------------------- */

int
SceneInspectServe(Scene *scene, int port)
{
    InspectState *st = &g_inspect;
    struct sockaddr_in addr;
#if defined(_WIN32)
    WSADATA wsa;
#endif

    SceneInspectStop();
    if(scene == NULL || port <= 0 || port > 65535)
        return 0;
#if defined(_WIN32)
    if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 0;
#endif
    st->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(st->listen_fd < 0)
        return 0;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((unsigned short)port);
    if(bind(st->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
       listen(st->listen_fd, 4) != 0) {
        INSPECT_CLOSE(st->listen_fd);
        st->listen_fd = -1;
        return 0;
    }
    KryMutexInit(&st->lock);
    st->scene = scene;
    st->port = port;
    st->running = 1;
    if(KryThreadStart(&st->thread, inspect_thread, st) == 0) {
        st->running = 0;
        INSPECT_CLOSE(st->listen_fd);
        st->listen_fd = -1;
        return 0;
    }
    KryThreadDetach(&st->thread);
    SceneInspectPoll(scene);
    return 1;
}

void
SceneInspectStop(void)
{
    InspectState *st = &g_inspect;

    if(st->listen_fd >= 0) {
        st->running = 0;
        INSPECT_CLOSE(st->listen_fd);   /* unblocks accept() */
        st->listen_fd = -1;
    }
    KryMutexLock(&st->lock);
    free(st->snapshot);
    st->snapshot = NULL;
    st->snapshot_len = 0;
    KryMutexUnlock(&st->lock);
}
