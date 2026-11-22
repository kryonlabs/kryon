# 3D models

### glTF 2.0 models

Import `kryon/scene_gltf` to load `.gltf` (JSON with `.bin` or `data:` URI
buffers) and `.glb` files without any platform graphics dependency:

```ziran
#import "kryon/scene"
#import "kryon/scene_gltf"
#import "kryon/scene_skin"
#import "kryon/scene_render"

model := GltfReadFile("models/character.glb")
defer GltfModelRelease(*model)
if !model.valid { return }

scene: Scene
SceneInit(*scene)
instance: GltfSceneInstance
defer { GltfInstanceRelease(*instance); SceneFree(*scene) }

if !GltfSceneCreate(*scene, *model, *instance) { return }
if model.animations.count > 0 {
    unused GltfAnimateScene(*model, *instance, *scene, 0, seconds)
}
camera := PerspectiveCamera(0.9, 16.0 / 9.0, 0.1, 100.0)
picture := NewImage(width, height)
defer VecFree(picture.pixels)
unused RenderScene(*scene, camera, *picture)
```

`GltfReadBytes(bytes, base_dir)` reads the same formats from memory;
`base_dir` resolves relative buffer and image URIs, and stays empty for a GLB
whose binary chunk holds everything. A loaded `GltfModel` owns its buffers,
decoded images, materials, meshes, nodes, skins and animation samplers;
`GltfModelRelease` frees them.

The reader covers buffers, bufferViews and accessors for every component type,
normalized integers and sparse overrides. Primitives read `POSITION`, `NORMAL`
(averaged flat normals when absent), `TANGENT`, `TEXCOORD_0`, `TEXCOORD_1`,
`COLOR_0`, `JOINTS_0`, `WEIGHTS_0` and indices; triangle lists, strips and fans
are supported. Meshes read morph targets and default weights. Nodes read TRS or
a column-major matrix, children, meshes, skins and per-node morph weights.
Skins read joints and inverse bind matrices. Animations read translation,
rotation, scale and weights channels with `LINEAR`, `STEP` and `CUBICSPLINE`
interpolation and report their duration. Materials read
metallic-roughness factors, base colour, metallic-roughness, normal, occlusion
and emissive texture info, alpha modes, double-sided, `KHR_materials_unlit`,
samplers, textures and PNG/JPEG images from a URI, a `data:` URI or a
bufferView.

Malformed input never crashes: every accessor and sparse range is
bounds-checked, required arrays are type-checked, node hierarchies reject
cycles and multiple parents, and relative URIs must stay inside the model's
directory. A rejected document leaves `model.valid` false and `GltfError`
describes the first problem. Documents above 256 MiB, accessor counts above
16 million, non-triangle primitive modes, non-finite floats and required
extensions other than `KHR_materials_unlit` are rejected.

`GltfSceneCreate(scene, model, instance, scene_index = -1)` builds one
`SceneIndexedMesh` per primitive under group nodes for the document's node
hierarchy and creates a `SceneSkin` for every skinned primitive; skinned
primitives draw through the same retained indexed path as other scene meshes.
`GltfUpdateScene` recomputes node world transforms, morph blending and skin
poses. `GltfAnimateScene(model, instance, scene, animation, time, loop = true)`
samples an animation and updates the scene in one call; `loop` wraps `time`
modulo the animation duration, and a non-looping sample clamps to the last
keyframe. `GltfSampleAnimation` changes only the instance, `GltfSampleSkinPose`
returns the current bind-to-pose `SkinAffine` array for one skin, and
`GltfSetDualQuaternion` switches skinned primitives to volume-preserving dual
quaternion skinning. `GltfPoseReset` returns to the document's rest pose.

Material conversion maps base colour, metallic, roughness, emissive, alpha
mask/blend, double-sided and unlit into `SceneMaterial`, uploads the base
colour texture with its sampler, and flips V so glTF's top-down UVs match the
scene's bottom-up UVs. Metallic-roughness, normal, occlusion and emissive maps
stay available on `GltfMaterial` for callers that supply a custom shader
material; the standard material uses the base colour map. Use
`GltfMaterialToScene` to convert a material yourself.

`make scene-gltf-test` checks the reader and adapter against small fixtures
(GLTF and GLB, sparse and normalized accessors, morph targets, all
interpolation modes and malformed files), and `make scene-gltf-gpu-test`
renders an animated two-bone skin headless through the GLES scene path and
compares it with `tests/fixtures/gltf/skinned_reference.png`. Both run through
`build-queue --`. The committed Khronos sample models under
`tests/fixtures/gltf/khronos/` are CC-BY-4.0 or CC0-1.0 and keep their
upstream README files; models carrying a non-Khronos trademark are never
committed and run only from a local sample-assets checkout named by
`KRYON_GLTF_SAMPLE_ASSETS`.

### VRM avatars

Import `kryon/scene_vrm` for VRM 0.x avatars (`.vrm` files such as VRoid
exports) loaded through `scene_gltf`: the humanoid bone map, expressions,
eye lookAt and spring-bone physics for hair and clothes.

```ziran
#import "kryon/scene_gltf"
#import "kryon/scene_vrm"

model := GltfReadFile("models/avatar.vrm")
instance: GltfSceneInstance
unused GltfSceneCreate(*scene, *model, *instance)
avatar: VrmAvatar
defer VrmAvatarRelease(*avatar)
if VrmReadFile(*avatar, "models/avatar.vrm") {
    unused VrmSpringsReset(*avatar, *model, *instance)
}
// Each frame: pose the bones in instance.rotations, then
VrmSetFrame(*avatar, turn, position, scale)
VrmSpringsUpdate(*avatar, *model, *instance, seconds_since_last_frame)
unused VrmSetExpression(*avatar, *model, *instance, "blink", closed)
VrmLookAtSet(*avatar, *model, *instance, yaw_degrees, pitch_degrees)
unused GltfUpdateScene(*model, *instance, *scene)
```

`VrmReadFile(avatar, path)` and `VrmReadBytes(avatar, bytes)` read the `VRM`
extension of a GLB or glTF JSON document and return false when there is
none. `VrmBoneNode(avatar, "leftUpperArm")` gives the glTF node of a VRM
humanoid bone or -1; `avatar.bones` holds all of them in `VrmBoneName`
order. `VrmSetExpression` sets a blend shape group, found by its preset
(`blink`, `joy`, `a`, ...) or its name, to a weight from 0 to 1 in the
instance's morph weights; binary groups snap at one half.
`VrmLookAtSet` turns the eye bones of a `Bone` lookAt avatar by degrees to
the avatar's left and up, mapped through its lookAt range curves.

Spring bones follow the VRM 0.x reference: each joint's tail moves by Verlet
inertia less drag, a stiffness pull back to its rest direction and gravity,
keeps its length and is pushed out of the sphere colliders of the collider
groups its bone group names. Physics runs at a fixed 60 steps a second in
the world placed by `VrmSetFrame(avatar, turn, at, scale)`, so moving or
turning the whole model swings the hair as well. A step backwards or of more
than a quarter second, as after a seek, puts the springs back at rest, as
`VrmSpringsSettle` does. Node positions come from the instance's
translations, rotations and scales, so one `GltfUpdateScene` after posing
and springs is enough. VRM 1.0 (`VRMC_vrm`) files are not read.

`make scene-gltf-test` also checks the VRM layer against
`tests/fixtures/gltf/vrm_avatar.gltf`: the bone map, expressions, lookAt
curves, gravity, colliders, stability while the body moves at uneven frame
steps, and settling after a seek.
