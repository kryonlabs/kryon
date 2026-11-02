# Bundled icon sheets

`ui.webp` and `pfp.webp` are lossless WebP sheets. Their decoded RGBA pixels,
dimensions, icon IDs and 64 × 64 source rectangles match the original PNGs.
The other five sheets retain their PNG encoding. Each JSON manifest's `image`
field names the shipped file.

Consumers updating Kryon must update their package lock and image packaging
lists together: replace `icons/ui.png` and `icons/pfp.png` with `icons/ui.webp`
and `icons/pfp.webp`, and update explicit references. Run
`scripts/sync-icons.sh DEST_DIR [SHEET ...]` to copy the official sheets and
manifests. This does not delete old consumer files; remove obsolete PNGs from
the consumer's packaging list when integrating. Applications pinned to an
older Kryon keep that version's paths until they update the package.

`scripts/import-mingcute-icons.py` and `scripts/generate-nav-icons.py` write
lossless WebP with exact transparent RGB preservation. Never use photo quality
settings for these icon sheets. The gallery's published `catalog-image.png`
is explicitly encoded as PNG from the WebP source.

`tests/ziran_icon_catalog_test.sh` verifies catalog paths, sync, MIME type,
dimensions and hashes of every decoded RGBA byte from the original sheets.
`tests/raylib_webp_decode_test.sh` also decodes the staged sheets through the
native renderer and compares every byte, from source and saved IR.
