# Image, tile and shape rendering

`Image.Props.clip_polygon` clips an image without stretching its content.
Supply 3–4096 points in normalized destination coordinates (0–1); the path
closes automatically. Empty leaves the image unchanged. Invalid coordinates,
too few points, or combining a polygon with a mesh draw nothing. Native,
hosted and browser images retain their own copy of the supplied points.

`Image.Props.mesh` warps the same image over a grid of `columns` × `rows`
screen points, row by row from the source's top left, each cell drawn as two
textured triangles: a cut-out puppet's painted arm can bend at its elbow, or
a picture can follow a curve. The bounds still clip it; fit, origin,
rotation and the style radius do not apply, and a grid with too few points
draws nothing. Points are copied when the image is queued, at most 4096 per
image (256 through a portable bundle's hosted `Image`).

`Image.Props.preview_path` names a small copy of the same picture that draws in
its place, with the image's own bounds, fit, mesh and clip, while the picture
loads or when it is unavailable, so something shows at once and the picture
replaces it when ready. A preview of part of a picture (a nonempty `source`,
such as a jigsaw piece) needs `Image.Props.full_size`, the picture's size in
pixels, to map that rectangle onto the preview; without it the image waits for
the picture. Hosted previews pass the same asset policy as their picture.
`image_showing.ImageShowingOf(session, image)` reports `ImageShowingNone`
(loading), `ImageShowingPreview`, `ImageShowingFull` or
`ImageShowingUnavailable`, natively and through a portable bundle's host, so
an app can wait for a picture before what belongs with it. Asking starts the
host loading the picture, which also fetches pictures shown soon.

`image_mesh.ImageMeshQuad(bounds, camera, points)` constructs a four-corner
mesh for the standard `Image.Props.mesh`. It uses the same 2D `Camera` as
`WorldToScreen`, with caller-owned space for four points; too little space
produces an empty mesh. Rotation and scale stay on the standard image surface.

`TileMapProps.pixel_snap` fits and positions whole-pixel cells. `nearest` selects
nearest-neighbour sampling for the whole map; `nearest_tiles` selects individual
tile indices while the rest remain smooth. Sampling survives the hosted codec.
A hosted map uses batched sampling only when its host advertises
`tile_map.sampling`; hosts without mesh support draw the same standard Images individually.
Pixel snapping uses bounds fitted by the bundle.

`WidgetPaletteUse` scopes semantic colors along with widget rules. Hosted
`AppThemeColor` calls and compositions using canvas, surface, text, border and
accent see the same palette. `WidgetPaletteRestore` restores the surrounding palette when calls nest,
including its text style and semantic tokens. The host frame unwinds palettes
a guest leaves open. Sixteen nested parents are retained; scopes beyond that
restore to the deepest retained palette, then unwind to the original host.
The outermost restore preserves the complete prior token set, including
host-specific colors and the absence of unset tokens.

Hosts call `PaintResourcesChanged()` when replacing raster resources without
changing widget props. Retained frames then compose and paint again, including
when a browser graphics context resets or an app updates its image assets.

`Shape.Draw(Shape.Props)`, `Circle(Shape.CircleProps)`, `Shape.Ring(Shape.RingProps)` and
`Shape.Triangle(Shape.TriangleProps)` draw lines, circles, radial gradients,
rings and triangles. Like `Box`, each
is decorative: it submits a node covering what it draws and queues one paint
command. Kryon cuts the shape into triangles, clips them to its parent's clip
and sends each colour's triangles to the host in one `RasterTriangles` call;
a radial fade is drawn as rings of one colour each. `Box.Props.fill_bottom`
with `has_fill_bottom` fades a box's fill from top to bottom.
Import `kryon/shape` to use these shapes, including in a portable hosted
bundle.
