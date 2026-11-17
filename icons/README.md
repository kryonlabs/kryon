# Bundled icon sheets

`ui.webp` and `pfp.webp` are lossless WebP sheets with 64 × 64 source
rectangles. The other five sheets use PNG encoding. Each JSON manifest's
`image` field names its shipped sheet; icon IDs and source rectangles are
listed in the manifest.

Run `scripts/sync-icons.sh DEST_DIR [SHEET ...]` to copy the selected sheets
and manifests into an application's asset directory. With no sheet names,
the script copies all sheets. Package the filenames named by those manifests.

`scripts/import-mingcute-icons.py` and `scripts/generate-nav-icons.py` write
lossless WebP with exact transparent RGB preservation. Never use photo quality
settings for these icon sheets. The gallery's published `catalog-image.png`
is encoded as PNG from the WebP source.

`python3 tests/icon_assets_test.py build/scratch/icon-assets` verifies sync,
manifest paths, MIME type, dimensions and hashes of every decoded RGBA byte.
