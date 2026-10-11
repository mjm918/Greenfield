# Greenfield — Founding a Tribe

An offline, single-player settlement game in C and raylib. Lead eight Ice Age survivors through the opening winter, build a permanent camp, discover farming, and welcome enough travelers to found a village. The main objective is designed for **45–60 minutes at 1×**; pause, 2× and 4× are available. Play continues after victory.

The game has a fixed 48 × 48 valley, five resources, eight worker roles, eleven building types, four workshop discoveries, farming, arrivals, warned raids and paid truces. Every resource batch is produced by a worker and carried to storage. Construction needs both materials and builder labor.

The game draws its 1440 × 900 logical interface and world into one **2880 × 1800** render target for smooth Retina output. Mipmapped, trilinear sprite filtering preserves detail across zoom levels, and bilinear output scaling avoids hard enlarged pixel blocks. The UI uses larger, antialiased Atkinson Hyperlegible text with bold headings. The existing illustrated assets, palette and stepped UI borders are retained. Original artwork is retained in **Reference Viewer**, with the social icons and “Ninety thousand acres” title removed.

## Build and open

macOS is the verified target. Requires CMake 3.20+, a C99 compiler and desktop OpenGL. The first configure downloads a checksum-pinned raylib source archive; the game makes no runtime network calls.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
open build/bin/Greenfield.app
```

The app loads its packaged `assets` directory beside the executable. It does not depend on your current working directory or installed system fonts. Other desktop targets retain raylib's build path, but have not been runtime-verified.

## Your first village

1. Place two **Shelters** near the hearth. The two starting builders complete them automatically.
2. Keep foragers, a hunter and a woodcutter working. Each person eats one food per minute; the hearth uses one wood every twenty seconds during winter. Citizens recover when fed and warm.
3. Build a **Workshop**, then give the unassigned citizen a crafter role. Crafters make tools when no discovery is active. Tools speed gathering; running out slows work without preventing recovery.
4. Discover better tools, insulation and food storage. Select shelters to upgrade them into insulated huts. Discover farming after the twelve-minute winter.
5. Put **Farm plots** on the fertile riverbank and assign farmers. They plant, wait for growth and carry the harvest to storage. Granaries reduce spoilage and accept food deliveries.
6. Add housing and maintain food reserves. The first two travelers arrive at minute 18, followed by pairs every six minutes when there are two free beds and a day's food for the current population. A missed arrival retries after one minute.
7. Keep guards near the rally flag or offer **40 food for a twelve-minute truce**. Raids give ninety seconds' warning, steal at most eighteen food, and retreat. A raid lasts at most six minutes, including a bounded surrender fallback for trapped raiders.

Victory requires surviving winter, twenty living people, a bed for everyone, a delivered harvest, three days of stored food, and resolving a raid through defense or truce. All citizens dying ends the game. v1 supports up to 64 citizen records and 160 buildings.

## Controls

| Input | Action |
| --- | --- |
| Left click | Place a building or inspect a citizen/building |
| Shift + place | Keep the current building selected |
| Space | Pause/resume |
| 1 / 2 / 4 | Simulation speed |
| B | Select shelter / cancel placement |
| W / R / O | Workforce / discoveries / objectives |
| Mouse wheel | Zoom toward the pointer |
| Right-drag / arrows | Pan |
| Home / M | Founding camp / whole-valley overview |
| Click minimap | Recenter |
| G / H | Grid / field guide |
| F5 / F9 | Save / load |
| F | Borderless fullscreen |
| Escape | Cancel placement, close guide, or open menu |

In Workforce, reduce one role before adding people to another. **Area** assigns work within nine tiles of a chosen center. **High** priority gives that role first access to shared work. **Rally guards** chooses the defended area; finishing a watch post moves the rally there. Clear areas restores whole-valley access.

## Saves

The single local slot on macOS is:

```text
~/Library/Application Support/Greenfield/village.gfv1
```

Manual saves and autosaves use the same slot. Autosave runs every two minutes of simulation time while playing, including after victory. Starting a new game leaves the prior slot intact until a save occurs. Closing the app does not force a save.

The format has a version, explicit little-endian fields, length and checksum. It includes routes, carried goods, paid construction, jobs, research, crops, health, seasonal time and rival state. Saving validates conservation and state first, writes and flushes a temporary file, then atomically renames it. Loading decodes and validates a separate candidate before replacing the current village. Incomplete, corrupt and incompatible saves do not overwrite the existing file or current game.

A failed load pauses autosave until a successful load, an explicit save, or a new game. Failed autosaves wait for the next two-minute interval before trying again. The UI displays the error and recovery action.

## Verification and captures

```sh
ctest --test-dir build --output-on-failure
./build/bin/Greenfield.app/Contents/MacOS/Greenfield --game-capture build/evidence/game-winter.png
./build/bin/Greenfield.app/Contents/MacOS/Greenfield --game-capture build/evidence/game-village.png --load build/campaign.gfv1
./build/bin/Greenfield.app/Contents/MacOS/Greenfield --reference --capture build/evidence/reference-manual.png
./build/bin/Greenfield.app/Contents/MacOS/Greenfield --time 1.25 --capture build/evidence/animated-manual.png
```

CTest covers simulation invariants, economic conservation, single charges, assignment and placement validation, path recovery, hunger/cold damage and recovery, farming and delivery, arrivals, defense, bounded theft/retreat, truce expiry, frame-rate/speed equivalence, save round trips and failed replacements. A command-driven campaign proves the victory conditions are achievable in the intended simulated duration, including recovery from shortage and a raid. The art check validates all 48 sprite cells, the full-resolution capture, detailed world edges and antialiased text. The existing `pixel_art` CTest name is retained for command compatibility. GPU capture tests require a graphical session.

For the separate **wall-clock 1× campaign**:

```sh
cd build
./game_tests --realtime-campaign > evidence/realtime-campaign.log
```

This is an automated simulation playthrough using the same validated command API as the UI, with one fixed tick every 100 ms. It intentionally exhausts food early, restores gathering, suffers theft, and continues to victory. It writes the ending save to `evidence/realtime-campaign.gfv1`. This is distinct from a human playtest; the wider range of player strategies still needs playtesting.

The preserved reference test compares every RGBA pixel to the cleaned source, requires zero changes outside the four repair regions, checks all nineteen reference avatars animate, verifies deterministic paused frames and confines greetings to the selected avatar. The original `ui.jpeg` is unchanged.

## Ownership and assets

- `src/game.h`: public commands, queries and the single authoritative `GameState`.
- `src/game.c`: initialization, command dispatch, fixed 100 ms update order and objectives. `src/sim/` owns the individual gameplay systems: world, citizens, jobs, economy, buildings, workshop, farming, survival, rivals, definitions and validation. UI never awards resources directly.
- `src/save.c`: one canonical serializer shared by persistence and deterministic state hashes. No raw C struct dumps or pointer fields.
- `src/game_view.c`: window/frame lifecycle and composition. `src/view/` owns assets, input, world drawing, widgets, HUD, panels, screens and save/command adapters, sharing one explicitly passed `ViewContext`.
- `src/reference.c` / `scene.fs`: preserved illustration viewer and its original localized animation/capture verification.
- `tests/campaign.c`: test-only player commands. It is not linked into the playable game and does not inject resources or complete objectives directly.

See [Code organization](docs/architecture.md) for module ownership, extension rules and focused test commands. Simulation checks live in `tests/sim/`; `simulation_baseline` preserves pre-refactor campaign hashes and version-1 save continuations, while `view_input` checks the production input adapters with controlled input. The public simulation API and save format are unchanged.

The existing C/raylib/CMake application, macOS bundle, asset-loading convention, reference camera/input and GPU verification were reused. New surfaces are the simulation, renderer, canonical local save and modular art, each needed to replace a fixed illustration with a changeable village. There is one resource ledger, command path, serializer and simulation tick; no parallel gameplay store or backend.

Runtime I/O is three local PNG atlas reads and two packaged TTF reads when opening gameplay, the existing reference asset reads when opening that viewer, and explicit/periodic local save writes or user-requested loads. The higher-resolution renderer replaces two low-resolution render targets with one larger target and generates sprite mipmaps once at load; it adds no filesystem writes or background work. There are no accounts, telemetry, network services or background workers.

The settlement atlas and both citizen atlases were made with the built-in imagegen tool. Exact prompts are saved in `assets/settlement-atlas-prompt.txt`, `assets/citizen-atlas-prompt.txt` and `assets/citizen-braids-atlas-prompt.txt`. The reference repairs retain their original prompt files. The former Romulus and Alagard font assets and their upstream credits remain in `assets/` for provenance. Gameplay packages [Atkinson Hyperlegible Regular and Bold](https://github.com/google/fonts/tree/main/ofl/atkinsonhyperlegible), copyright Braille Institute of America, under the SIL Open Font License; the bundled license is `assets/AtkinsonHyperlegible-LICENSE.txt`.
