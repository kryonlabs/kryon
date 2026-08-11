#ifndef SCENE_PROPERTY_H
#define SCENE_PROPERTY_H

/*
 * Property bridge between the runtime scene tree (KryNode in scene_tree.h) and
 * the editor-facing property model (KryonPropertySpec/Value in kryon_property.h).
 *
 * Each runtime node kind registers a property spec table: an ordered list of
 * named, typed properties with min/max/step metadata. The getters/setters read
 * and write the runtime KryNode fields (local transform, props structs) through
 * this abstraction, so Krait's Inspector can edit every field generically
 * instead of special-casing x/y/w/h bounds.
 */

#include "scene_tree.h"
#include "kryon_property.h"

/* Register the property spec table for a node kind. The table must be static
 * (its lifetime is program-long). Call once per kind at startup. */
void KrySceneRegisterProperties(KryNodeKind kind,
                                const KryonPropertySpec *specs, int count);

/* Look up the spec table for a kind. Returns the count via *out_count; returns
 * NULL if the kind has no registered properties. */
const KryonPropertySpec *KryScenePropertySpecs(KryNodeKind kind, int *out_count);

/* Read a property by spec index on a runtime node. The kind's spec at `index`
 * determines which field is read. Returns a zero value on bad index/node. */
KryonPropertyValue KrySceneNodeGetProperty(KryScene *scene, KryNodeId node,
                                           int index);

/* Write a property by spec index on a runtime node. Returns 1 on success, 0 if
 * the index/node/value kind is invalid. Marks the node dirty so the next tick
 * recomputes its world transform. */
int KrySceneNodeSetProperty(KryScene *scene, KryNodeId node, int index,
                            KryonPropertyValue value);

/* Convenience: read/write by property id string. Index-based is preferred in
 * hot paths; these are for editor lookups by name. */
KryonPropertyValue KrySceneNodeGetPropertyByName(KryScene *scene,
                                                 KryNodeId node,
                                                 const char *property_id);
int KrySceneNodeSetPropertyByName(KryScene *scene, KryNodeId node,
                                  const char *property_id,
                                  KryonPropertyValue value);

/* Register the built-in kind property tables (Node2D, Camera2D, Sprite2D). */
void KrySceneRegisterBuiltinProperties(void);

#endif
