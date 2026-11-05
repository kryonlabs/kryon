# AVIF fixtures

Generated with official libavif avifenc 1.2.1 (CPU libaom), using
`tests/generate_avif_fixtures.py`. The tiny source patterns and `.rgba`
goldens are generated independently from known pixels and Pillow geometry.
The `.txt` files record output width, height and tolerated byte error.
No fixture is fetched during the regression tests.

Cases cover 8/10/12-bit RGBA and grayscale, opaque/transparent/premultiplied
alpha, all four irot angles combined with both imir axes after clean aperture,
and animated first-frame behavior. Corrupted copies exercise invalid crop
fractions, truncation, malformed containers, axis and total pixel limits.

Regenerate with `AVIFENC=/path/to/avifenc python3 tests/generate_avif_fixtures.py`.

`jungle-title.avif` is a byte-for-byte copy of the owner's Jungle asset
`assets/jungle/title/layers/treehouse-portrait-background.avif` (1024x1536,
opaque 8-bit YUV420, q35, 83,822 bytes). SHA-256:
`3fa9fb6ee4c68d7426e48afbaa76565e6d50dce5cdb16d02ca2c94a21850b1e9`.
It caused SIGBUS/BUS_ADRALN in ARMv7 `predict_w16` at `cfl_neon.c:432` in
the Android debug app, a path not exercised by the tiny lossless fixtures.
Keep this binary unchanged; the generator above only regenerates tiny cases.
`jungle-title-samples.rgba` contains an 8x8 grid of independent Pillow 12.3.0
RGBA samples at each grid cell's center, with a three-byte-value tolerance for
YUV conversion differences. Tests also check the dimensions and every alpha
byte, and Android repeats the decode through the actual background worker.
