# Kryon Labs editorial assets

Created for the approved website redesign on 2026-09-19.

- `workshop.webp`: original ink-and-watercolor hero, generated with the built-in
  `image_gen` tool using the approved page mockup as a reference. Exact prompt:
  `workshop-prompt.txt`. WebP export: 1536 × 1024, quality 84.
- Site-root `og.png`: generated social card, built-in `image_gen`, exact prompt
  `social-card-prompt.txt`. Export: 1200 × 800, 192-color optimized PNG. The
  original artwork has been preserved outside the deployment source.
- `hello.png`: actual 480 × 280 KRB software-renderer capture, not generated art.
  Recreate from the repository root with:

  ```sh
  build/linux-x86_64/bin/kryon-preview cartridge \
    --project "$PWD" --source docs/site/hello.kry \
    --output docs/site/assets/editorial/hello.png --width 480 --height 280
  ```

- Manrope and Newsreader are served locally. Their OFL license texts are included.
- Application screenshots and metadata come from the Kryon showcase registry via
  the existing `scripts/update-showcase.py` build step.

Original generated images and the approved mockup are retained in
`/mnt/storage/Projects/waozi-design-proposals/2026-09-19/kryonlabs/` on the design
workstation. Image-generation originals are also retained in the tool's default
output directory. No compiler assets are loaded by the homepage.
