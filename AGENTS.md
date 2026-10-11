# Repository Guidelines

## Project Structure & Module Organization

Greenfield is an offline, macOS-first RTS written in C99 with raylib.

- `src/main.c` launches gameplay or the reference viewer. `game.c` orchestrates simulation; `game_view.c` orchestrates rendering.
- `src/sim/` owns gameplay systems; `src/view/` owns input, HUD, assets, and animation. Public simulation types and queries live in `src/game.h`.
- `assets/` contains atlases, fonts, licenses, and generation briefs; `ui.jpeg` is the original reference. Edit these sources, not packaged copies under `build/`.
- `tests/` contains C test executables and campaign scripts; `tests/fixtures/` holds saved baselines. `docs/architecture.md` documents ownership; `docs/country-validation.md` records evidence.
- `CMakeLists.txt` defines targets and asset packaging; `cmake/` contains macOS bundle metadata.

## Build, Test, and Development Commands

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release  # Configure; fetch pinned raylib
cmake --build build --parallel               # Build application and tests
open build/bin/Greenfield.app                # Launch on macOS
ctest --test-dir build --output-on-failure    # Run all registered tests
ctest --test-dir build -R '^(simulation|wildlife|view_input)$' --output-on-failure
```

Requires CMake 3.20+, a C99 compiler, and desktop OpenGL. GPU tests require an unlocked graphical session. Capture and reference-verification commands are documented in `README.md`; output belongs in `build/evidence/`.

## Coding Style & Architecture

Use four-space indentation and braces on separate lines, matching neighboring code. Use `PascalCase` functions/types, module prefixes such as `Game`, `Economy`, and `View`, `camelCase` fields, and `UPPER_SNAKE_CASE` constants. Keep private helpers `static`. No formatter/linter configuration is checked in; compiler checks use `-Wall -Wextra -Wpedantic`.

Extend existing owners: one `GameState`, commands through `GameCommandApply`, resource accounting through `sim/economy.c`, persistence through `save.c`. Keep simulation independent of raylib, rendering time, and platform randomness. Add serialization and validation whenever persistent fields change; preserve atomic saves and existing player files. Retain readable Source Sans 3 text and smooth sprites.

## Testing Guidelines

CTest runs custom C assertion harnesses. Follow `test_*.c` naming for focused suites; core rules live in `rts_tests.c`. Cover affected invariants, rejected commands, determinism, and save/load behavior. Add view checks and screenshots for interaction changes. Explain intentional campaign-hash changes; preserve historical fixtures. No percentage coverage threshold is configured. Automated simulated-time campaigns are not human playtests.

## Commit & Pull Request Guidelines

History uses concise imperative subjects, sometimes prefixed `feat:` or `refactor:`. Keep commits focused and run `git diff --check`. PRs should explain the problem, affected owners, validation, and remaining limits; link relevant issues and include screenshots for visual changes. Document save-format or baseline changes explicitly. Preserve unrelated work and never commit player saves or build output.
