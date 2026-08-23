#ifndef SCENE_PROPERTY_H
#define SCENE_PROPERTY_H

/*
 * Property bridge between the runtime scene tree (Node in scene_tree.h) and
 * the editor-facing property model (PropertySpec/Value in kryon_property.h).
 *
 * Each runtime node kind registers a property spec table: an ordered list of
 * named, typed properties with min/max/step metadata. The getters/setters read
 * and write the runtime Node fields (local transform, props structs) through
 * this abstraction, so an IDE inspector can edit every field generically
 * instead of special-casing x/y/w/h bounds.
 */

#include "scene_tree.h"
#include "kryon_property.h"

/* Register the property spec table for a node kind. The table must be static
 * (its lifetime is program-long). Call once per kind at startup. */
void SceneRegisterProperties(NodeKind kind,
                                const PropertySpec *specs, int count);

/* Property access callbacks for application-defined kinds (ids from
 * NodeRegisterCustomKind). The spec index selects the field; read/write
 * the kinds props struct through the nodes props pointer. */
typedef PropertyValue (*ScenePropertyGetFn)(Scene *scene,
                                                   NodeId node, int index);
typedef int (*ScenePropertySetFn)(Scene *scene, NodeId node,
                                     int index, PropertyValue value);

/* Register an application-defined kinds spec table plus getter/setter.
 * Indices 0..2 are reserved for the shared transform fields (position,
 * rotation, scale) and are handled generically; callbacks see index >= 3.
 * Either callback may be NULL (that direction becomes read-only zeroes). */
int SceneRegisterCustomKind(NodeKind kind,
                               const PropertySpec *specs, int count,
                               ScenePropertyGetFn get,
                               ScenePropertySetFn set);

/* Look up the spec table for a kind. Returns the count via *out_count; returns
 * NULL if the kind has no registered properties. */
const PropertySpec *ScenePropertySpecs(NodeKind kind, int *out_count);

/* Read a property by spec index on a runtime node. The kinds spec at `index`
 * determines which field is read. Returns a zero value on bad index/node. */
PropertyValue SceneNodeGetProperty(Scene *scene, NodeId node,
                                           int index);

/* Write a property by spec index on a runtime node. Returns 1 on success, 0 if
 * the index/node/value kind is invalid. Marks the node dirty so the next tick
 * recomputes its world transform. */
int SceneNodeSetProperty(Scene *scene, NodeId node, int index,
                            PropertyValue value);

/* Convenience: read/write by property id string. Index-based is preferred in
 * hot paths; these are for editor lookups by name. */
PropertyValue SceneNodeGetPropertyByName(Scene *scene,
                                                 NodeId node,
                                                 const char *property_id);
int SceneNodeSetPropertyByName(Scene *scene, NodeId node,
                                  const char *property_id,
                                  PropertyValue value);

/* Register the built-in kind property tables (Node2D, Camera2D, Sprite2D). */
void SceneRegisterBuiltinProperties(void);

#endif
