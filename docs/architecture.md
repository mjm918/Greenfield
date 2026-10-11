# Code organization

Greenfield uses small C modules grouped by gameplay responsibility. There is one
simulation state, one command entry point, and one serializer. The view reads that
state and issues commands; it does not maintain a second model of the village.

## Boundaries

```text
main.c
  -> game_view.c -> view/* -> game.h (commands, queries, save/load)
  -> reference.c / scene.fs

game.c -> sim/* -> GameState
save.c -> GameValidate + the versioned GameState wire format
```

`game.h` is the public simulation contract. It contains the state types, stable
enum values, definitions, commands and queries. `game_view.h` is the presentation
entry point. Headers inside `sim/` and `view/` are private implementation contracts;
application consumers should not bypass `GameCommandApply` to call feature handlers.

The `greenfield_sim` target has no raylib dependency. Simulation code uses integer
ticks and the random generator stored in `GameState`, never rendering time, input,
filesystem access, or platform randomness. `save.c` alone handles persistence.

## Simulation owners

| File | Responsibility |
| --- | --- |
| `src/game.c` | Initialization, command dispatch, fixed-step scheduling, update order, objectives, shared event log and deterministic RNG |
| `src/sim/definitions.c` | Building/discovery definitions, costs, descriptions and display names |
| `src/sim/world.c` | Valley generation, walkability, grid pathfinding, reachable destinations and citizen movement |
| `src/sim/citizens.c` | Creation, deaths, arrivals, role assignments, work areas and priorities |
| `src/sim/jobs.c` | Job selection, routes/retries, priority order, work dispatch and job reset |
| `src/sim/economy.c` | Gathering, cargo, delivery and paired resource/ledger mutations |
| `src/sim/buildings.c` | Placement validation, occupancy, construction, housing, upgrades and proximity queries |
| `src/sim/workshop.c` | Discovery validation/progress and tool crafting |
| `src/sim/farming.c` | Planting, crop growth and harvesting |
| `src/sim/survival.c` | Hunger, warmth, health, seasonal warnings, spoilage and daily natural-resource renewal |
| `src/sim/rivals.c` | Raid warnings, raider movement, guard combat, theft, retreat and truces |
| `src/sim/validation.c` | Cross-system invariants and resource conservation |
| `src/save.c` | Canonical serializer, hashes, atomic saves and validated loads |

`GameCanPlace` is shared by placement previews and the build command. Construction
materials are charged only by the validated placement handler. Resource changes
go through the economy owner, except initial supply seeding in `GameInit` and
deserialization in `save.c`. There is still only the original conservation ledger.

Feature work handlers receive a citizen who has reached the job destination. Jobs
owns scheduling; each feature owns the actual work. Crafting still reserves its
materials when the job is selected, before travel. Harvest food still becomes
stored food only after delivery. These timings are part of the existing behavior.

The order in `GameTick` is intentional:

1. Increment the tick and apply survival.
2. Resolve deaths and lost cargo.
3. Process rivals/combat, then resolve deaths again.
4. Grow crops, then process workers by descending priority and citizen array order.
5. Process arrivals, then defeat/victory.

Changing this order, pathfinding neighbor order, array iteration order, or RNG call
order can change the game even when individual functions look equivalent.

## Presentation owners

`game_view.c` owns the window, frame loop, composition and one `ViewContext`.
It passes that context explicitly to private modules. No view module exports a
mutable global UI object. The context survives the Reference Viewer handoff only
to preserve the existing autosave-error protection, and is reset on view entry.

| File | Responsibility |
| --- | --- |
| `src/view/context.h` | View state, atlas types and logical coordinate constants |
| `src/view/assets.c` | Packaged asset loading/unloading, sprite bounds and sprite drawing |
| `src/view/world.c` | Isometric transforms, terrain, depth sorting, previews and citizen animation |
| `src/view/widgets.c` | Shared palette, text, buttons, borders, bars and hit checks |
| `src/view/hud.c` | Resource/status bar, warnings, map and construction palette |
| `src/view/panels.c` | Objectives, workforce, discoveries, rivals and selected-object panels |
| `src/view/screens.c` | Menu, guide, victory and defeat overlays |
| `src/view/input.c` | Keyboard/mouse actions, camera control and world picking |
| `src/view/actions.c` | UI-to-command adapter, local save path, save/load feedback and autosave protection |

Input uses logical 1440 × 900 coordinates. Rendering applies the existing 2×
rasterization scale at the world/composition boundary. Keep those coordinate
spaces distinct when adding controls or changing hit testing.

## Extending the game

- Add rules to the feature that owns them. Keep `game.c` limited to orchestration.
- Keep helpers `static` unless another real module needs them. Declare shared
  functions in the owning private header, not a growing catch-all header.
- Reuse the command entry point and economy operations. UI checks may explain
  rejection, but the simulation command must enforce eligibility and costs.
- Put a new panel beside the existing panels, and reuse widgets and the context.
- Split a module when responsibilities diverge; roughly 200–400 lines is a review
  signal, not a requirement to create extra files or abstractions.
- A new persistent field needs explicit serialization, validation, compatibility
  consideration and tests. This refactor does not change `game.h`, enum values,
  `SAVE_VERSION`, or `save.c`.

## Verification

The original simulation assertions are grouped under `tests/sim/`, with shared
fixtures in `test_support.c`. The existing test executable and campaign commands
remain available. Run an individual suite from the build directory:

```sh
./game_tests --suite economy
./game_tests --suite persistence
./game_tests --suite rivals
```

Other suites are `construction`, `survival`, `farming`, `determinism`, `validation`
and `sandbox`. Each creates its own state; save tests use separate disposable
filenames in the working directory. `tests/campaign.c` remains test-only.

`simulation_baseline` checks 54 full-state hashes captured from commit `b1fbbc9`,
through tick 30600. It also loads two saves written by that original implementation
and checks their continuation against the same checkpoints. See
[`tests/fixtures/README.md`](../tests/fixtures/README.md) for provenance. Investigate
baseline failures; do not regenerate expected values just to make a refactor pass.

`view_input` compiles the same view modules with controlled platform input. It
checks commands, selection state, modal blocking, speeds, camera controls and
save-error recovery without opening a window or touching the player's save.
GPU captures and art/reference checks remain separate and need a graphical session.

```sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Runtime asset reads, save/load operations, autosave cadence, and background work
are unchanged. The added files are internal source modules and test fixtures;
there are no new runtime dependencies, services, stores, or asynchronous jobs.
