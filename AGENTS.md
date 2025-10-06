# Repository Guidelines

## Project Structure & Module Organization
The repo anchors its C sources in `src/` with submodules `cli/`, `compiler/`, `core/`, `runtime/`, `renderers/`, and `shared/`. Public headers sit in `include/`, third-party libraries in `third-party/`, scripts in `scripts/`, while `examples/` and `web/` host sample KRY apps and the HTML renderer. Tests live under `tests/` with unit, integration, and shell-based compilation checks. Keep generated artifacts inside `build/` or `tests/build/`; avoid committing `web_output/`.

## Build, Test, and Development Commands
Run `./scripts/build.sh` for a release build or pass `--debug` for instrumentation and `--clean` to reset the build tree. Manual setup uses `cmake -S . -B build` followed by `cmake --build build`. Execute examples via `./scripts/run_example.sh hello-world raylib` or list available demos with `./scripts/run_examples.sh list`. Web targets compile through `./scripts/build_web.sh`.

## Coding Style & Naming Conventions
All C code follows the bundled `.clang-format` (LLVM base, 4-space indent, C99). Keep function and variable names in `kryon_snake_case`, macros in `ALL_CAPS`, and public API identifiers prefixed with `kryon_` or `KRYON_`. Header include order should match the categories defined in `.clang-format`; run `clang-format -i path/to/file.c` before committing. Static analysis is configured through `.clang-tidy`—invoke `clang-tidy` via your IDE or `cmake --build build --target clang-tidy`.

## Testing Guidelines
Unity-based unit tests live in `tests/unit/**`. After configuring CMake, run `ctest --output-on-failure` from `build/` for the aggregated suite. Critical compiler regressions are validated with `bash tests/test_for_compilation.sh`; update that script whenever you add new `@for` patterns. Prefer descriptive test names like `test_feature_behavior`. Document non-automated steps in `tests/README.md`.

## Commit & Pull Request Guidelines
Recent history favors short, imperative subject lines (`improve script vm`, `make basic arithmetics work`). Reference issues inline when relevant (e.g., `fix renderer crash refs #42`). For PRs include: 1) a concise change summary, 2) test evidence (`ctest` output or script logs), 3) screenshots for renderer-facing changes, and 4) callouts for follow-up work or known limitations.

## Security & Configuration Tips
Kryon renderers rely on external toolchains (raylib, HTML). Document any system packages you touch in the PR body and avoid checking secrets into sample configs. Use `KRYON_RENDERER=raylib` when scripting local runs, and keep web assets under `web/` to simplify review.
