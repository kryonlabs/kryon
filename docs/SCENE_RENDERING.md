# Programmable scene rendering

## Programmable scene materials

Import `kryon/scene`, `kryon/scene_render` and `kryon/shader`. `RenderScene`
executes camera-projected, depth-tested 3D meshes into a `std/image.Image`.
`SceneCamera.shift_x` and `shift_y` optionally offset a perspective view in
normalized device coordinates, with zero preserving the symmetric projection.
Positive X moves the scene right; positive Y moves it up. An offset of 1 moves
it by half the viewport width or height. Position, target, field of view and
aspect stay fixed, so this pans a bounded crop without rotating or translating
the camera. The CPU and GPU apply the offset before homogeneous frustum
clipping; render only the requested viewport. Orthographic bounds retain their
existing meaning and do not use these perspective offsets. Async GPU completion
metadata retains the exact submitted offsets with its camera.
The shader implementation is portable Ziran on CPU, including on Android and
WebAssembly. It does not compile GLSL or require a GPU shader context.

`ShaderCompile(program: *ShaderProgram, code: []ShaderInstruction) ->
ShaderDiagnostic` validates and copies up to 128 instructions into owned,
bounded storage. `ok`, zero-based `instruction` (or -1 for program errors),
and static `message` describe failures. A rejected replacement preserves the
previous executable. Do not modify compiled storage directly.
`ShaderFree(*program)` invalidates it and is idempotent. Programs have no
external handles or allocations and survive presentation-context recreation.
`ShaderProgram.input_mask` records which initial inputs the validated code reads:
`SHADER_INPUT_COLOR`, `SHADER_INPUT_POSITION`, `SHADER_INPUT_NORMAL` and
`SHADER_INPUT_UV`. Reads after overwriting an input register do not require
that initial value. The renderer uses this metadata to avoid normalizing
unused shader normals; UV-only albedo programs retain identical output.

Each instruction has `op`, `dest`, `a`, `b`, `c`, `slot` and `value`.
Registers hold `ShaderValue.{x,y,z,w:float64}`. Registers 0–3 initially contain
linear base RGBA, world XYZ/1, world-normal XYZ/0, and UV/0/0. The renderer
perspective-correctly interpolates these values. Registers 4–31 start undefined;
validation rejects reads before writes and invalid destinations. Operations are
`ShaderLiteral`, `ShaderUniform`, `ShaderSample`, `ShaderMove`, `ShaderAdd`,
`ShaderSubtract`, `ShaderMultiply`, `ShaderDivide`, `ShaderMin`, `ShaderMax`,
`ShaderSin`, `ShaderCos`, `ShaderFract`, `ShaderDot`, `ShaderNormalize`,
`ShaderMix`, `ShaderDiscardLess`, and `ShaderOutput` (members of `ShaderOp`).
Arithmetic is component-wise; dot and normalization use XYZ; dot broadcasts its
result and normalization preserves W for nonzero XYZ (zero XYZ returns zero
RGBA). Mix uses A, B and C per component.
Divide by zero returns zero. DiscardLess discards when A.x < B.x. Output takes
A and must end the program. Nonfinite outputs discard. No unbounded loops.

Set `SceneMaterial.shader` to the program address, `shader_uniforms` to a
borrowed slice of up to 16 vec4 slots, and `shader_samplers` to borrowed slots
(up to 8). Owners must remain alive and unchanged during `RenderScene`;
values and pixel contents can change between renders without recompilation.
`ShaderBindings` reports missing slots, released programs, nonfinite uniforms
and invalid RGBA dimensions. `RenderScene` rejects invalid bindings before
drawing. `SceneFree` never frees borrowed programs, uniforms or sampler pixels.

`ShaderPrepare(*prepared, program, uniforms, samplers)` validates bindings once
and produces a bounded `ShaderPrepared` draw plan with folded constants and
unused instructions removed. Reprepare after changing the program, uniforms,
sampler bindings or sampled pixels. The plan owns its instructions/constants
and borrows sampler storage for the draw. Its `input_mask` describes the initial
inputs still needed after preparation.
`ShaderRunPrepared(*prepared, *scratch, inputs)` executes one pixel with exact
interpreter arithmetic. `ShaderRunBatch(*prepared, *scratch, inputs, outputs)`
executes up to `SHADER_BATCH` (32) pixels per instruction dispatch and returns
false for an unprepared plan, oversized input or insufficient output storage.
Use `ShaderScratch` for scalar execution and `ShaderBatchScratch` for batches;
each concurrent caller owns separate scratch. Programs, bindings and samplers
remain unchanged during execution. `RenderScene` prepares per mesh each frame
and batches its pixel shader calls, preserving changes between frames.

`ShaderSampler.{width,height,pixels,repeat}` samples straight linear RGBA bytes,
top row first. UV V=0 selects the bottom row. Sampling uses nearest-neighbor
clamp-to-edge by default, or repeat including negative coordinates. Supply
linear data when loading an sRGB image for a custom sampler. `ImageMaterial`
is the standard sRGB image path described below.

Shader output replaces base RGBA before standard lighting and tone mapping.
Use `UnlitMaterial` for direct colour, or `LitMaterial` for procedural surfaces
that keep scene lighting. Set `transparent=true` for fractional alpha;
`alpha_test` also applies to shader output. Discarded and zero-alpha fragments
write neither colour nor depth. Shadow maps still use mesh geometry; custom
shader discard does not cut holes in shadow casters.

The renderer clips triangles against all six homogeneous camera planes,
preserving attributes when triangles cross the eye or near plane.
Present its straight sRGB output with `BitmapImageCreate`/`BitmapImageUpdate`
and the standard `Image` widget; widget clipping and alpha remain in effect.
`RenderSceneRelease()` releases shared scratch buffers, recreated on the next
render. Render and release on one owning thread; this renderer is not reentrant.

Native builds with `POSIX_THREADS` prepare immutable mesh states and clipped
triangle commands once per frame, then render through `std/parallel_linux`.
The pool defaults to one lane per online logical processor, up to 64 lanes.
Set `ZIRAN_PAR_THREADS` to select a lane count. A value of `1` runs serially.
Each lane clears, draws and resolves its own full pixel rows in the original
opaque and sorted transparent order. One completion barrier joins the frame.
Conservative row bounds skip empty triangle regions while retaining the
original edge equations at all four sample positions.
Soft-shadow disk radii are initialized once. Each lane memoizes exact disk
rotations in a 512-entry cache for its frame; the light's radius and depth
comparisons are still evaluated per fragment. This uses about 44 KiB of
temporary stack per lane, without a full-frame cache or changed shadow samples.
`RenderSceneWorkerCount()` reports available pool lanes, including the caller;
it returns one before initialization and after `RenderSceneRelease()`, which
also closes the pool. Small draws and thread-creation failure remain serial.
Portable bundles and WebAssembly builds without this capability remain serial.
Borrowed programs, uniform values and sampler pixels must remain immutable
until `RenderScene` returns on the owning thread.

For expensive opaque materials with more than two projected surface layers,
the renderer first tests depth and material discard without lighting. It then
shades only candidates near the final opaque depth, preserving the original
draw order, float32 depth comparisons and four coverage samples. Conservative
depth ceilings temporarily reuse the existing sample color bytes; this adds
no frame-sized buffer. Empty samples regain the scene background before the
original sorted transparent pass. Low-overlap scenes retain the single pass.
Programs and alpha textures still run during visibility testing, so savings
depend on overlap and lighting cost.

`RenderSceneStatistics()` returns the last completed render's
`opaque_projected_layers` (summed clipped opaque triangle area divided by image
area), `depth_prepass`, `depth_fragments` and `shaded_fragments`. Fragment counts
are pixel-center material evaluations after coverage and depth testing, shared
by covered MSAA samples; they include evaluations that discard. The depth count
belongs to the optional visibility pass, and the shaded count includes opaque
and transparent color passes. Lanes accumulate private counts before joining.
Release resets the snapshot.

`RenderSceneMemoryBytes()` reports retained backing-buffer bytes for renderer
scratch, including transformed vertices and prepared draw commands. It excludes
the caller's scene, textures and output image, static color tables, allocator
overhead and worker stacks; it is not process RSS. Color and depth buffers grow
without geometric spare capacity. Sized buffers retain capacity across renders
and all reported scratch is freed by `RenderSceneRelease()`.
`RenderSceneMemoryUsage()` returns the same backing storage split into
`framebuffer_bytes`, `shadow_bytes`, `vertex_bytes`, `triangle_bytes`,
`draw_bytes`, `node_bytes` and `ordering_bytes`. These fields sum to
`RenderSceneMemoryBytes()`. Triangle commands refer to shared draw states and
do not copy material bindings. The four coverage positions are shared constants,
not stored in each command. Geometry reservation allows one eighth of source
size plus 32 records for clipping; additional growth uses the same bounded
headroom. Camera frames within retained capacities allocate no geometry storage.
Transparent depth-sort keys also retain capacity across frames and are included
in `ordering_bytes`.

Render at a bounded resolution and reuse the image while camera, geometry,
lights and material inputs remain unchanged. CPU rendering every UI frame
at full window resolution can be expensive.

`tests/shader_scene_test.zi` demonstrates mutable uniforms, sampling,
diagnostics, clipping, alpha and recreation. Focused targets are
`make shader-scene-test` and `make shader-host-test`, through `build-queue --`.

### Portable GLSL translation and optional native GPU rendering

`kryon/shader_glsl` translates the same `ShaderProgram` into a GLSL ES material
function. `ShaderGLSLTranslate(program)` returns `ShaderGLSLResult` with a
`diagnostic`, owned `source` text, and uniform/sampler counts and input mask.
Check `diagnostic.ok` before composing its `kryonMaterial` function into a host
shader. `ShaderGLSLBindings` validates the borrowed values for GPU execution.
Translation is portable Ziran; it does not create a graphics context. GPU
highp arithmetic has different precision from the CPU's binary64 arithmetic,
so GPU comparisons use documented pixel tolerances rather than exact hashes.

Native desktop applications can explicitly import `kryon/scene_gpu` and link
`-lEGL -lGLESv2 -lGLX -lm`. This optional host is separate from the core UI, portable
CPU renderer and WebAssembly path. It owns a surfaceless EGL/GLES3 context and
renders the existing `Scene`, materials and `SceneCamera` into a reusable
`std/image.Image` for ordinary Kryon Image presentation. A web build (`OS ==
.WEB`) links none of EGL or GL: `SceneGpuProbe` reports the GPU unavailable,
and `SceneGpuRenderOrCPU` draws with `RenderScene`.

Android code generation defines `ANDROID_BUILD` and uses the default EGL display
through `eglGetDisplay`, including at API 21. `kryon_android_app` supplies the
`EGL` and `GLESv3` system libraries; the Android path has no GLX imports.

Keep one `SceneGpuContext` at a stable address on its owning thread.
`SceneGpuProbe(context)` reports availability, diagnostic, renderer labels,
texture limits and MSAA support. The host requires highp fragment precision,
15 texture units and four MSAA samples; it reports unavailability when those
requirements cannot be met. `pcf_available` reports hardware bilinear depth
comparison. When the probe fails, the GPU fragment shader performs the four
depth comparisons and bilinear interpolation at each of the five shadow taps.
`SceneGpuRender(context, scene, camera, image)` preserves the requested image
dimensions and returns `SceneGpuFrame` with `ok`, `accelerated`, `diagnostic`
and submission/readback/conversion timings. Failure leaves the output untouched.
`SceneGpuRenderOrCPU` explicitly permits the full-quality CPU renderer on
failure, setting `accelerated=false`; inspect that flag when measuring GPU work.

`SceneResizeOpaqueInto(image, width, height, output)` in `kryon/scene_render`
fills an owned RGBA display cache from an opaque scene image. Native builds
use the available Lanczos kernels and can reuse the output allocation. Portable
bundles use `std/image.ResizeLanczos`. An app can opt to render fewer pixels,
resize the result once, then pan a native-size cache using its original crop
coordinates. Pass separate owning Images. Invalid sizes, nonopaque input or
allocation failure return `false` and preserve both images. For translucent
input, use `std/image.ResizeLanczos` for alpha-aware resizing.

`SceneGpuSetProfiling(context, true)` adds stage timings in `frame.profile`.
`shader_setup_ms` covers material program lookup, binding validation and shader
setup. `texture_upload_ms` measures the driver calls that upload scene textures.
For synchronous readback, `gpu_completion_ms` measures the explicit finish
before `pixel_transfer_ms` measures the read. For async readback it measures
the elapsed time from fence submission until a poll first sees it ready, so it
includes polling delay. `image_copy_ms` measures publication to the owning
Image, including any required pixel conversion. Profiling is off by default.
`direct_resolve` identifies a frame that skipped the conversion draw.

`SceneGpuCapabilities.async_available` separately probes pixel pack buffer
readback, fences and read mapping. `async_diagnostic` explains unavailability;
the synchronous GPU API remains available when only async support is missing.
`SceneGpuSubmit(context, scene, camera, width, height, request_id)` draws at
full quality into one of exactly two retained transfer slots. Use positive,
strictly increasing IDs until release. `SceneGpuAsyncResult.ok` means the
operation is valid: `queued` accepts the request, `busy` accepts nothing.
Scene, shader and texture storage is borrowed only during submission and can
be mutated afterward on the owner thread.

`SceneGpuPoll(context, image)` tests the oldest fence with timeout zero.
An empty/unready poll has `ok=true, completed=false` and leaves the image
untouched. A completion returns the exact submitted ID, camera, size, original
draw timings and submission-to-completion `latency_ms`; it never labels pending
work as the latest camera. The image must already have the completed size.
Poll copies only a signaled buffer, validates its read mapping and then updates
the reusable Image. One CPU staging buffer is shared with synchronous readback.
Synchronous render can safely interleave while both transfer slots are pending.
Async queues add at most `2 * width * height * 4` bytes of GPU transfer storage.
On ARMv7 PowerVR Rogue GE8320, each pending opaque frame owns the resolved
framebuffer until its fence signals. The renderer reads it directly and copies
the rows into the Image in top-to-bottom order. Translucent frames still use
straight-alpha conversion. The Android Pixmap presenter takes the same CPU
Image pixels in both cases.

`SceneGpuCancel` discards pending IDs and releases the transfer slots. Resize,
release and context loss also discard them. Cancellation/resize/release may
drain GPU work; poll does not wait for unfinished work. Applications must keep
polling while work is pending, including after input stops, and retain the
completed camera metadata separately from current input. Queueing may add
camera latency, so measure changed presented frames and event-to-pixel latency.
The native Desktop host exposes `DesktopRequestFrame()` for this purpose.
Requests aggregate until the next `BeginDesktopFrame`, keep the active frame
budget while pending, and affect only scheduling. Continue to pass the actual
retained-paint result to `DesktopFrameUnchanged`; scheduling work must never
claim that unchanged image pixels changed.

Geometry, material programs, textures and render targets retain storage across
frames. Each context also keeps up to eight linked material programs from
earlier frames. Scenes with identical shader instructions and material features
reuse the linked program. Scene and binding data are borrowed during the call.
Mutate them only on the owner between calls. When only nodes move, turn or
scale between frames, the renderer sends the affected vertices. Smooth-shaded
indexed meshes keep their element buffers and refresh only changed position, normal and color
streams. Box-projected UVs update with their source positions and normals.
Topology, material and visibility changes rebuild the geometry. `SceneGpuResources.geometry_uploads`
counts uploads and `geometry_vertices` counts the vertices those uploads covered.
`FreezeGeometry(scene)` (in `kryon/scene`) promises that meshes, materials and
vertices stay as they are, so the renderer stops comparing them with its last
upload on every frame and compares only the nodes. Scene calls that add or
change geometry take the promise back; after writing into the scene's vertex
arrays directly, clear `scene.geometry_frozen` yourself. `SceneGpuMemoryUsage` reports retained CPU/GPU
storage estimates, object counts, upload counts and completed GPU frames; this
is separate from process RSS and presentation-host storage.
The resource report includes live fences/pending frames, accepted submissions
and cancelled requests. Deleting a live slot drains it before releasing storage,
so repeated navigation cannot accumulate deferred transfer allocations.
`SceneGpuResize` resizes the retained target. `SceneGpuRelease` releases owned
objects and context and can be called repeatedly; a later probe/render recreates
them. It preserves a foreign host's current graphics context and does not
terminate the process-shared EGL display.

Run the explicit native targets `make shader-glsl-test` and
`make scene-gpu-test` through `build-queue --`. The ordinary public-surface
local tests also compile and execute scene, material and translator imports as
a separate package consumer in native and portable forms without linking EGL.
Run `build-queue -- python3 tests/scene_gpu_test.py --readback-profile` to
compare the two synchronous copy paths on a 2527×1423 opaque frame and verify
their pixel hashes. The reported renderer identifies whether the run used a
hardware or software GLES driver.

### Owned image pixels for scene textures

`#import "kryon/image_pixels"` exposes
`LoadImagePixels(path: string) -> std/image.Image`. Success satisfies
`ImageValid(*image)`; missing, invalid or unsupported files return an empty image.
The caller owns `image.pixels` and releases it with `VecFree(image.pixels)`.
Pixels are tightly packed, top-down, straight RGBA in the source's sRGB encoding.
On the web the page decodes the file's bytes with the browser's own codecs
(photos turned upright by Exif), so a canvas build links no image library.

Headless image-widget tests can use the ordinary `kryon/pixmap` provider for
`image_raster`'s `ImageWidth` and `ImageHeight` capabilities. Import both modules
and bind `image_raster:ImageWidth=pixmap:ImageWidth` and
`image_raster:ImageHeight=pixmap:ImageHeight` with `--bind` when compiling source
or generating saved IR. For a package consumer, prefix **both** module names
with Kryon's package ID from `ziran.lock` and an underscore, for example
`p…_image_raster:ImageWidth=p…_pixmap:ImageWidth`; the ID depends on the locked
revision. Saved IR already contains those bindings. `PixmapSetImage` registers
fixture dimensions; absent paths return `0`. This needs no window or borrowed
framebuffer. A desktop application uses the same capabilities from
`kryon/Cairo`, which loads its picture assets.
