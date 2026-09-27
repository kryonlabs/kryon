# Kryon Labs editorial assets

Created for the approved website redesign on 2026-09-19.

- `workshop.webp`: original ink-and-watercolor hero, generated with the built-in
  `image_gen` tool using the approved page mockup as a reference. Exact prompt:
  `workshop-prompt.txt`. WebP export: 1536 × 1024, quality 84.
- Site-root `og.png`: current 1200 × 630 Kryon Labs social card rendered from
  the site-root `og.svg`, using the new mark in `../brand/mark.svg`. The earlier
  generated card prompt remains in `social-card-prompt.txt` for reference.
- Manrope and Newsreader are served locally. Their OFL license texts are included.
- Application screenshots and metadata come from the Kryon showcase registry via
  the existing `scripts/update-showcase.py` build step.

Original generated images and the approved mockup are retained in
`/mnt/storage/Projects/waozi-design-proposals/2026-09-19/kryonlabs/` on the design
workstation. Image-generation originals are also retained in the tool's default
output directory. No compiler assets are loaded by the homepage.
