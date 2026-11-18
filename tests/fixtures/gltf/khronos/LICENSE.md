# Khronos glTF-Sample-Assets Licenses

The sample models committed in this directory come from
[KhronosGroup/glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets):

- **`Box`**: © 2017 Cesium, licensed under
  [Creative Commons Attribution 4.0 International (CC-BY-4.0)](https://creativecommons.org/licenses/by/4.0/).
- **`RiggedSimple`**: © 2017 Cesium, licensed under
  [Creative Commons Attribution 4.0 International (CC-BY-4.0)](https://creativecommons.org/licenses/by/4.0/).
- **`RiggedFigure`**: © 2017 Cesium, licensed under
  [Creative Commons Attribution 4.0 International (CC-BY-4.0)](https://creativecommons.org/licenses/by/4.0/).
- **`AnimatedMorphCube`**: © 2017 Microsoft (donated by Wallabyway), dedicated
  to the public domain under
  [CC0 1.0 Universal (CC0-1.0)](https://creativecommons.org/publicdomain/zero/1.0/).

Each model subdirectory retains its upstream `README.md` with full provenance
and attribution.

`BoxTextured` and `CesiumMan` are intentionally not committed to this
repository because Khronos `Models-issues.md` tags them with `[Issues:
non-Khronos mark]` (`LicenseRef-LegalMark-Cesium.txt`) due to the Cesium logo
texture. When present locally in `build/scratch/gltf-sample-assets/` (or
`KRYON_GLTF_SAMPLE_ASSETS`), `tests/scene_gltf_test.sh` exercises them
alongside the committed fixtures.
