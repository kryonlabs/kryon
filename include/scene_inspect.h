#ifndef SCENE_INSPECT_H
#define SCENE_INSPECT_H

/*
 * Live scene inspection server.
 *
 * Serves a JSON snapshot of a runtime Scene over TCP so external tools
 * (editors, debuggers, curl) can watch a running applications node tree.
 * The snapshot is rebuilt on the game thread by SceneInspectPoll (call once
 * per frame); the socket thread only ever sends the latest completed
 * snapshot, so the tree is never walked concurrently with mutation.
 *
 * Each response is a minimal HTTP/1.0 reply with Content-Type
 * application/json, so browsers and curl work unmodified.
 */

#include "scene_tree.h"
#include "scene_property.h"

/* Start serving snapshots of `scene` on 127.0.0.1:`port`. Returns 1 on
 * success, 0 if the port could not be bound or the thread failed. Only one
 * server exists per process; a second call stops the first. */
int SceneInspectServe(Scene *scene, int port);

/* Rebuild the JSON snapshot from the scene. Call from the thread that owns
 * the scene, ideally once per frame. Cheap for small trees; the socket
 * thread never touches the scene itself. */
void SceneInspectPoll(Scene *scene);

/* Stop the server and release the snapshot buffer. */
void SceneInspectStop(void);

#endif
