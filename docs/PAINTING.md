# Painting and picture effects

## Vector path coverage

`vector_path` rasterizes the portable `KVP1` filled-path format and round
polyline strokes into caller-owned 8-bit coverage. `VectorPathFill` accepts an
affine `VectorTransform`, so hosts can render either a complete image or an
integer-translated tile with identical pixels. `VectorPolylineStroke` uses the
same transform, target, and `VectorAdd` / `VectorErase` coverage arithmetic.
File helpers are bounds-checked; callers retain ownership of all bytes and
coverage storage.

Calls share bounded scratch storage and must be serialized. Targets may be up
to 8192 by 16384 pixels; invalid input or exhausted scratch returns `false`.

### Text as pixels

`#import "kryon/text_pixels"` exposes `TextPixels(value, font, color, width,
height, pixels) -> bool`: the host's own text raster letters VALUE at FONT
size in COLOR, centred on a transparent WIDTH by HEIGHT picture of straight
RGBA in PIXELS (at least WIDTH * HEIGHT * 4 bytes), shaped exactly as its text
on screen. Use it where text must become a texture, such as lettering on a 3D
surface. The desktop letters with Cairo and Pango (`RasterTextPixels`), the
web with the page's canvas in its text face; it is false before the desktop
raster exists and on Android.
Use the single public scene-image surface:

```zi
wood := LoadImagePixels("assets/wood.png")
options: SceneTextureOptions = .{
    projection=.SceneTextureBox,
    filter=.SceneTextureLinear,
    wrap=.SceneTextureRepeat,
    scale=.{3.0,3.0},
}
material := ImageMaterial(*scene, *wood,
    LitMaterial(.{255,255,255,255}, 0.72, 0.0), options)
VecFree(wood.pixels)
unused RoundedBox(*scene, 1.8, 0.08, 0.32, 0.01, material)
```

`ImageMaterial(scene, image, base, options) -> SceneMaterial` validates and
copies a complete `std/image.Image`; the source can be freed immediately.
Equal images, including equal explicitly prepared mip levels, share one owned
scene texture. The scene releases logical storage in `SceneClear`/`SceneFree`.
Native GPU contexts upload the immutable copy once, reuse it without scanning
the image every frame, and release/recreate the upload with context lifecycle.

`SceneTextureOptions` contains `projection`, `filter`, `wrap`, `scale`, `offset`
and `mip_level`. Projection is `SceneTextureUV` or `SceneTextureBox`; box mapping
chooses a face from the dominant object-space normal, so moving, rotating or
scaling a bookshelf part does not make its grain swim. Filtering is
`SceneTextureNearest` or `SceneTextureLinear`, and wrapping is
`SceneTextureClamp` or `SceneTextureRepeat`. A zero scale component defaults to
1. `mip_level` is an explicit nonnegative level prepared once with a box filter;
its fixed selection makes native CPU and GPU results deterministic. Both
renderers filter sRGB texels in linear light and use the same clamp/repeat and
top-row/V-bottom rules. `SceneTextureCoordinate` exposes the exact object-space
coordinate transform for procedural geometry and tests.

`ImageMaterial` attaches scene textures. Shader samplers use programmable
linear-RGBA values; convert sRGB first when using those bindings.

### Light cookies

`LightCookie(scene, light, texture, scale, offset)` projects a scene texture
through a light's beam, like a gobo: dappled sun through leaves, window bars or
stained glass. `texture` is an existing scene texture index, usually
`ImageMaterial(...).texture`. The texel's linear RGB multiplies the light's
direct irradiance on every lit surface, receiver or not; the hemisphere light,
unlit and exact-colour materials are unaffected. The texture spans the light's
shadow box from `CastLightShadow` `scale` times, shifted by `offset` (in
repetitions), with bilinear filtering and repeat wrapping in linear light, so
the CPU and GPU renderers agree. A light needs a shadow box to carry a cookie;
`shadow_size` may be zero when no depth map is wanted. Only the first light
with a cookie projects one. Change `SceneLight.cookie_offset` between renders
to drift the pattern without rebuilding anything.

### Scene grade: a time-of-day look for pictures

`Scene.grade` (`SceneGrade`) lights scenes made of painted pictures, whose
unlit and exact-colour materials ignore scene lights and cookies. Every
material without `SceneMaterial.ungraded` keeps its colour C as
`(grey + (C - grey) * saturation) * tint`, plus C times the light of up to
four `lamps` (`SceneGradeLamp`: position, radius, sRGB colour, strength,
falling off as `1/(1+(d/radius)²)²`) and one `beam` (`SceneGradeBeam`): light
through an elliptical opening such as a round window. A surface point is in
the beam when the ray from it along `direction` crosses the ellipse `centre`
plus half axes `axis_u` and `axis_v`; `softness` fades the edge over that
fraction of the radius. Lit materials are graded after lighting. The grade
beam can receive real cast shadows: set `beam.shadows = true` and
`beam.shadow_light` to a directional light's index, configure its depth map
with `CastLightShadow`, and align the light's position/target with the beam's
direction. Nodes opt into casting and receiving with `SetShadows`. Only the
direct beam is occluded; ambient tint and local lamp light stay readable.
An absent shadow light leaves the beam unoccluded. This also works on unlit
painted surfaces, in both CPU and GPU rendering. Shadow maps respect material
opacity and texture alpha/cutoff, so foliage cards cast their painted
silhouettes instead of solid rectangles. `SetVisible` only affects the camera;
hidden nodes still cast shadows when enabled with `SetShadows`.
The grade applies before tone mapping. It is disabled by default; `SceneGradeNeutral()`
is enabled with white tint and saturation 1, leaving every colour exactly as
it was, and the renderers skip such a grade entirely, so a neutral hour costs
nothing per pixel. Change it between renders: it never rebuilds geometry and keeps
`FreezeGeometry`'s promise. The CPU and GPU renderers prepare it identically
(`PrepareGrade`) and agree within the GPU tolerance (`make scene-gpu-test`).
Mark skies and lamp glows `ungraded` so they keep their own colours.

### Daylight: sun, sky and grade by the hour

Import `kryon/daylight`. `DaylightAt(hour, latitude = -25.0)` (local solar
hours, wrapped to 0..24) returns `DaylightState`: the sun's direction for a
north-facing view (rising at 06:00 on +x/east, setting at 18:00 on -x/west,
north beyond the scene on -z), its height, day/full/golden weights,
linear sky colours, the sRGB grade `tint` and `saturation`, and 0..1 weights for
`lamps` (dusk to dawn), `beam` (sunlight throughout the day) and `stars`. The
path uses equinox declination and fixed 06:00/18:00 dawn/dusk, rather than
date/location-specific ephemerides. Negative latitude places the noon sun
in the northern sky; positive latitude places it south. `DaylightBeam`
admits light only from the outward side of the opening (the cross product
of its axes faces indoors), fading grazing rays. While
the sun is high (`full == 1`) the tint is exactly white and the saturation
exactly 1, so pictures keep their painted colours. `DaylightGrade(state)`
turns it into a `SceneGrade`; the app adds its lamps and beam, scaled by
`state.lamps` and `state.beam`. The app owns the clock, so it can follow the
local time or let a debug build set, fast-forward or rewind the hour.

`DaylightSky` is a storybook sky drawn by one material shader: a gradient from
horizon to zenith, the sun's glow and disc, two drifting cloud layers and
twinkling stars, from a 256-pixel tileable noise texture made in code at
`DaylightSkyInit` (no image files). Each frame call
`DaylightSkyUpdate(sky, state, seconds, aspect, horizon)` with the plane's
width over height and the horizon's height (0..1) on it; seconds drive the
clouds and stars. Draw it on a plane whose UV runs left to right and bottom to
top with `DaylightSkyMaterial(*sky.program, sky.uniforms[:], sky.samplers[:])`
(unlit, exact colour, ungraded); keep the sky at a stable address while it is
drawn and release it with `DaylightSkyFree`. `tests/daylight_test.zi` covers
the hours, the sky and the grade on native and portable builds (`make
daylight-test`).

Linux uses the existing system image loader and packaged WebP/AVIF adapters,
copying the original straight pixels before Cairo premultiplies them. Android reads relative APK asset paths
through the current NativeActivity's asset manager, or absolute filesystem
paths. It decodes PNG, WebP and AVIF in process with the linked codecs, on every
supported API level. Other formats, such as JPEG with its orientation, need the
activity's `decodeBundleImage(byte[])` method: it returns width, height and
then ARGB_8888 pixels, or null, and the droid template provides it. Without
it those formats fail empty and Kryon logs once that the method is missing.
Unsupported formats fail empty; this API does not promise WebAssembly AVIF
support.
These are CPU images and survive presentation context teardown.

`make image-pixels-test` verifies exact RGBA/alpha ownership and invalid inputs.
`make shader-host-test`, `make shader-web-test` and the explicit hardware target
`make shader-android-test` additionally execute the loader in their real hosts.
The Android console test covers absolute PNG files and the real offscreen native
window. `make android-pictures-device-test` decodes AVIF textures and checks the
missing bridge inside Android's VM on the USB Moto.

Custom native and Canvas hosts can import `kryon/window_runtime` for
`BeginWindowFrame`, `ClearWindowFrame(color)` and `EndWindowFrame`.
`CloseNativeWindow`, `WindowHidden`, `WindowMinimized` and `WindowClosing`
retain the backend's existing window semantics. These observations do not
poll input, change focus, pause work, or decide whether closing hides or quits.
Applications using retained frames keep their retained presentation lifecycle.
`WindowWidth` and `WindowHeight` report the custom desktop host's drawing
surface or Android's current native pixel extent. Desktop frames use the same
SDL/Cairo window opened by `Desktop.OpenDesktopSized`; resize takes effect at
`BeginWindowFrame`. Clearing replaces the whole Cairo surface, including alpha,
while preserving the caller's clip for subsequent drawing. Android frames lock,
paint and post NativeActivity pixels directly, and pause while the activity is
paused or has no window. `SyncWindowSurface` refreshes Android's shared surface
and touch transform. Closing requests activity shutdown, drains its lifecycle
commands, then frees its native pixels. Canvas keeps its page lifecycle.

Custom hosts import `kryon/window_icon` for
`SetWindowIcon(encoded: string, file_type: string) -> bool`. Encoded bytes
are borrowed for the call; native hosts decode through their shared image
codecs, apply JPEG orientation, and preserve full dimensions and straight
alpha. The native window must already exist. False means invalid or
unsupported data, a missing SDL window, or a browser/Android host where the
application icon belongs to the page or package manifest. Native AVIF uses the shared codec libraries.

Desktop image services include `RasterInvalidateImage(path) -> bool`,
`RasterInvalidateImagesIn(folder) -> s32`, and `RasterInvalidateImages() -> s32`.
They remove cached file pictures and their pending smooth copies; folder matching
respects path boundaries. Counts report entries removed. `ImagePreviewDecode`
uses the desktop image decoder's bounded, oriented RGBA output; release its
pixels with `ImagePreviewRelease`.

`FramePreviousSnapshot() -> FrameSurface` captures an owned copy of the previous
frame on desktop and Android. Call it after composing a new route and before
painting, draw it with `FrameTransition` using the full logical frame bounds
(including a negative camera origin), and call `FrameSurfaceRelease` when the
transition ends or is canceled. `FrameSurfaceCopy` owns an independent copy.
The Web host returns an invalid surface, so callers skip the fade there.

### Indexed animated surfaces and watercolor

Import `kryon/scene_skin` for `SceneIndexedMesh`, `SceneSkinCreate`,
`SceneSkinDeform` and `SceneSkinRelease`. Indexed surfaces copy positions,
normals, UVs and triangle indices into their owning `Scene`. A `SceneSkin`
retains bind positions and four normalized joint weights per vertex. Supply
final bind-to-pose `SkinAffine` transforms; deformation always starts from
bind data and uses inverse-transpose normal transforms, including nonuniform
scale. Invalid indices, weights and singular transforms are rejected before
changing visible geometry. Release the skin before reusing or freeing its
scene. Bone animation and inverse kinematics belong to the caller.

Import `kryon/watercolor` for `WatercolorMaterialCompile` and
`WatercolorFinish`. The portable material samples real paper and pigment
RGBA bitmaps and supplies soft directional light and a Fresnel rim. Uniform
0 is UV repetition, 1 light direction, 2 view direction, and 3 rim RGB with
zero W; samplers 0 and 1 are repeated linear paper and pigment images. Keep
the program, uniforms and images alive while rendering. Use an unlit scene
material when this program supplies the final lighting. The finishing pass
uses caller-owned retained scratch, brush radius 0–2, and brush, grain,
edge, pooling and contour strengths 0–1. Pooling darkens pigment within two
pixels of the silhouette; contour draws a graphite pencil line on the
silhouette and where neighbouring washes differ. It preserves source alpha
and excludes transparent samples from pigment filtering. CPU and translated GLSL share the material;
no window or platform graphics dependency is required.

Desktop `LoadImagePixels` accepts the host's optional
`ImagePixelsUsePathResolver` callback, so the same loader can read bundled
assets and disk files. A resolver borrows its input path and an output buffer
and returns a retained file path (or empty to keep the original). Loading and
resolver registration belong to one owner thread; the host retains resolved
files until decoding finishes. `ImagePixelsClearPathResolver` restores disk loading.
The image loader does not import a raster backend or open a window.
