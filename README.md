# Greenfield — From Tribe to Country

An offline survival RTS in C and raylib. Lead eight Ice Age survivors through winter, establish a farming village, and develop a Bronze Age country. Direct workers and soldiers in a fully visible **96 × 96 valley** against one rival with its own economy. Continue in the same world after victory.

**[Play in your browser](https://mjm918.github.io/Greenfield/)** — keyboard and mouse recommended. The browser edition runs the same C game through WebAssembly and GPU-accelerated **WebGL 2** (not WebGPU).

The first village milestone targets **45–60 minutes at 1×**; country formation targets **120–180 minutes**. The two automated command campaigns currently reach those milestones at **53:10** and **124:25**. These measure simulation time, not a completed human or wall-clock playtest. See [validation evidence](docs/country-validation.md).

## Build and run

macOS is the primary target. Requires CMake 3.20+, a C99 compiler and desktop OpenGL. Initial configuration downloads checksum-pinned raylib; gameplay makes no network calls.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
open build/bin/Greenfield.app
```

Assets load beside the executable. The renderer keeps the smooth isometric artwork, mipmapped sprites, Source Sans 3 Regular/Semibold fonts and restrained pixel borders. The 1440 × 900 logical view renders at 2880 × 1800 for Retina output. **Reference Viewer** preserves the cleaned illustration without the social icons or “Ninety thousand acres” label.

The gameplay HUD has a 48px resource strip and compact floating controls. The world extends to the bottom edge: a 100px selection tray (50px when nothing is selected) and a separate minimap replace the full-width bottom band. Construction expands to 142px only while open. Empty queues and permanent instruction columns take no space; occupied queues appear above the tray. Select workers for construction and work commands, soldiers for combat commands, or buildings for their production and upgrade controls. Objectives, workforce, discoveries and diplomacy open in a dismissible ledger; no permanent sidebar remains. Hover controls for costs and requirements. Resource totals are compacted above 10,000; hover shows exact values.

## Browser build and GitHub Pages

Use [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) **4.0.23**, the version pinned in `.github/workflows/pages.yml`:

```sh
source /path/to/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-web --parallel
python3 -m http.server 8080 --directory build-web/bin
```

Open `http://localhost:8080` through an HTTP server, rather than opening the HTML file directly. A browser with WebGL 2 enabled is required. CMake packages the same gameplay atlases and fonts in `index.data`; `index.html`, `index.js` and `index.wasm` are the other deployment files. The animated reference viewer remains desktop-only.

Browser saves use the existing version-4 codec and one `country.gfv2` slot, mounted at `/greenfield-save` in IndexedDB. The slot is restored before the game starts. **F5** or **Save village** writes it, and **F9** or **Continue / Load** restores it. Wait for “Village saved in this browser” before closing the tab. Saving errors remain visible and pause autosave; a failed initial storage restore prevents writing over an unread save. Clearing site data removes the slot; private browsing or blocked storage can prevent persistence. Desktop and browser saves are separate, and there is no cloud synchronization. Use one game tab at a time.

The browser suspends progress while the tab is hidden and limits elapsed time after long stalls. The canvas fits the available window; **F** or the menu's **Fullscreen** button enlarges it. Desktop controls are retained; this is not a touch-control adaptation.

Pushes to `main` run native tests, build WebAssembly, exercise browser gameplay/save recovery, then deploy the generated files to GitHub Pages. Pull requests run the same validation without deployment. The repository's **Settings → Pages → Source** must be **GitHub Actions**. No build output or player saves belong in Git.

Run the same browser checks locally with Node.js 22:

```sh
npm install --prefix build/browser-tools playwright@1.56.1
build/browser-tools/node_modules/.bin/playwright install chromium
NODE_PATH="$PWD/build/browser-tools/node_modules" node tests/web_smoke.cjs
# Validate the published site in a fresh, isolated browser session:
WEB_URL=https://mjm918.github.io/Greenfield/ NODE_PATH="$PWD/build/browser-tools/node_modules" node tests/web_smoke.cjs
```

Screenshots and a JSON report go to `build/evidence/`. The checks cover actual menu/keyboard input, simulation progress, save/reload equality, storage failure and retry, resizing, fullscreen, and unavailable storage. Gameplay adds no network calls after the initial static downloads. Each save adds one IndexedDB synchronization; startup adds one restore, without a second serializer or gameplay store.

## Building a civilization

1. **Ice Age Tribe.** Select workers, press B, and build two shelters for eight beds. Starting workers gather automatically; select people to change their jobs or issue direct orders. Each citizen consumes one food per minute. During the twelve-minute opening winter, the hearth consumes one wood every twenty seconds. Shortages warn before damage; restored food and warmth allow recovery.
2. **Settled Tribe.** After surviving winter and housing everyone, fund the center upgrade in Objectives. Builders supply its labor. Construct a workshop and assign crafters. Research insulation, food storage and farming. Build a training ground to organize soldiers.
3. **Farming Village.** Have sixteen citizens and both farming and storage discoveries, then upgrade the center again. Farms unlock on fertile soil; farmers plant, wait for growth and deliver harvests. Automatic reseeding costs two wood. Research metallurgy to mine ore and operate a staffed smelter: four ore plus two wood makes two bronze.
4. **Bronze Age.** Have thirty-two citizens, an operating smelter and delivered bronze, then fund the Council Hall upgrade. Bronze equipment costs resources per soldier. Research crop yield and stonework for improved production and stronger buildings; existing buildings can be repaired to the higher health limit.
5. **Country.** Complete the Council Hall, house fifty living citizens, store three days of food, secure both outlined resource sites with nearby watch posts, and obtain a lasting treaty or the rival’s surrender. Hostile soldiers near a site contest it. Destroying the rival center forces surrender and withdrawal. Losing every citizen ends the campaign.

The original twenty-person village objective remains an intermediate milestone: winter survived, sufficient housing, a delivered harvest, three days of food, and a raid resolved through defense or truce.

Recruit adults at the center for twelve food, time and spare housing. Recruitment takes four minutes early, three minutes in the farming age, and 1m45s in Bronze. Housing loss pauses recruitment without deleting people. Each faction supports **96 living citizens**; dead slots are reused with generation-checked handles.

Resources remain shared by faction, accessed through physical deliveries. Food, wood, stone, hides, tools, ore and bronze compete for construction, survival, technology and army production. Hover a header resource to see campaign-average production/spending, reserved materials and food upkeep. Carrying goods does not make them spendable. Trees slowly regrow; exhausted mineral deposits provide reduced extraction for continued play.

Hover a tree, berry bush, animal or deposit to see its exact remaining supplies. Click it to keep the live count in a compact bottom card; Esc clears the selection. Harvested trees leave stumps, then saplings before their wood returns. Bushes show fewer berries at 200 food or below and no berries when empty. Berry bushes and hunting grounds regenerate **3 food per game day** (three minutes at 1×), down from 60; sustained gathering now outpaces renewal. Select workers and target another site when one is exhausted. Existing saves retain their amounts and use the slower rate on future days.

## Army and rival

Training converts existing citizens, retaining their health, housing and food needs. Demobilization returns them to work. Selected groups distribute training across available training grounds; each building has its own queue.

| Class | Strength | Weakness |
| --- | --- | --- |
| Shield spearman | Armor; counters slingers | Axemen |
| Slinger | Fast, ranged; counters axemen | Shield spearmen |
| Axeman | Counters spearmen; high building damage | Ranged fire while approaching |

Right-click enemies to attack, or use attack-move to engage along a route. Buildings can be damaged, repaired and destroyed. Palisades block routes; gates admit their own faction. Blueprints reserve placement but remain walkable until builders start construction.

The rival gathers, builds, researches, recruits and trains through the same commands and costs. It receives no resource grants or replacement troops. Organized raids give ninety seconds’ warning. Raiders carry at most eighteen goods each, taken from actual stocks, and must deliver them home. Raids retreat after substantial losses or four minutes. Killing workers and destroying production buildings reduce the rival’s actual economy.

A temporary truce costs the displayed food payment and lasts twelve minutes. A lasting treaty requires Bronze advancement, ten uninterrupted peaceful minutes, 200 food and 30 bronze. Payments transfer to the rival’s shared stores. Truces and treaties block combat for both factions.

## Controls

| Input | Action |
| --- | --- |
| Left click | Select a citizen or building |
| Left-drag empty ground | Grab and pan the world; selection is kept |
| Shift-drag | Select a group / add to the current selection |
| Shift + selection | Add or toggle selected citizens |
| Right click | Contextual move, gather, build, repair, work, deliver or attack |
| Shift + order | Append an order, up to sixteen |
| Ctrl + number / number | Store / recall a control group |
| A / D / L, then click | Attack-move / defend / plunder a depot |
| S / X / T | Stop / hold position / retreat to rally |
| Select center, right click | Set new recruits’ rally instruction |
| Space / F1 / F2 / F3 | Pause / 1× / 2× / 4× |
| Middle drag / arrows / wheel | Pan / pan / zoom |
| Minimap click / drag | Move the camera across the valley |
| Home / M / minimap click | Founding camp / whole valley / recenter |
| B / W / R / O | Worker construction palette / workforce / discoveries / objectives |
| Q | Rotate a placement preview, move preview, or selected friendly building |
| G / H / F | Grid / field guide / borderless fullscreen |
| F5 / F9 / Escape | Save / load / cancel placement, close palette or ledger, then menu |

Workforce acts on the selected citizens. Counts reflect actual roles; there is no independent allocation budget. Production and advancement can be cancelled for a refund. Materials are reserved once, spent when the item completes, or recorded as lost when its building is destroyed.

## Saves

The new single slot is `~/Library/Application Support/Greenfield/country.gfv2` on macOS (`~/.greenfield/country.gfv2` elsewhere). Old `.gfv1` files remain untouched and are not migrated. Manual saves and two-minute simulation-time autosaves use the same slot. A new game does not overwrite it until saving occurs. Closing does not force a save.

Version 4 reads existing country version-2 and version-3 saves without overwriting them on load. It adds building orientation to the version-3 citizen facing and wildlife state. It keeps the same `country.gfv2` slot; version-1 village saves remain unsupported. The codec explicitly serializes both factions, generation handles, orders, routes, cargo, reservations, queues, discoveries, advancement, weather, AI decisions, combat and treaties. Validation precedes save; a temporary file is flushed and synced before atomic rename. Load validates a separate candidate before replacing the current game. Invalid/interrupted saves preserve the last usable slot. Failed loading pauses autosave until deliberate saving, successful loading or a new game.

## Verification

```sh
ctest --test-dir build --output-on-failure
cd build
./game_tests
./game_campaign treaty
./game_campaign military
./game_capacity_tests
```

The campaign scripts use ordinary player commands, allow shortages and theft, and check timing, objectives and a mid-campaign save/load. The capacity test uses an explicitly constructed stress fixture, separate from campaign evidence. Version-4 checkpoints include animation and building orientation while retaining the country gameplay expectations; historical version-1 fixtures remain for provenance, not acceptance.

```sh
build/bin/Greenfield.app/Contents/MacOS/Greenfield --game-capture build/evidence/winter.png
build/bin/Greenfield.app/Contents/MacOS/Greenfield --game-capture build/evidence/country.png --load build/evidence/treaty-campaign.gfv2
build/bin/Greenfield.app/Contents/MacOS/Greenfield --game-capture build/evidence/group.png --load build/evidence/military-campaign.gfv2 --capture-panel 4 --capture-selection
build/bin/Greenfield.app/Contents/MacOS/Greenfield --game-capture build/evidence/overview.png --load build/evidence/capacity.gfv2 --capture-overview --benchmark-frames 300
build/bin/Greenfield.app/Contents/MacOS/Greenfield --reference --capture build/evidence/reference.png
build/bin/Greenfield.app/Contents/MacOS/Greenfield --verify build/evidence
```

Capture options also include `--capture-panel 0..3` for ledger tabs (`4` closes the ledger), `--capture-build 0..12` for worker construction, `--capture-building 0..12` for an existing building, and `--capture-soldiers` for an existing army. Benchmark mode advances the loaded test state at 1× simulation time per sixty frames, disables frame limiting and reports rendering time after ten warm-up frames. It does not autosave. GPU tests require a graphical session. See [architecture](docs/architecture.md) for ownership and extension rules.

## Assets and runtime activity

The existing C/raylib/CMake foundation, macOS bundle, reference viewer, simulation modules, command boundary, economy ledger, atomic serializer and three original atlases are reused. New owners handle direct orders, production queues, progression, shared combat and deterministic rival decisions. There is one authoritative `GameState`; no second economy, scripted-raider system or gameplay store remains.

The new transparent `bronze-atlas.png` supplies the training ground, smelter and two civic-center stages. Its generation brief is recorded alongside the original atlas prompt files. Soldier equipment is drawn over the existing walking/working/combat animation frames. [Source Sans 3](https://github.com/adobe-fonts/source-sans) replaces the two gameplay fonts under the SIL Open Font License. The packaged license and pinned-source checksums are recorded in `assets/font-credits.md`. Historical font files remain in the repository.

Runtime I/O adds one packaged PNG read and a larger payload in the existing save operations. No accounts, services, networking, telemetry, new runtime dependencies or background workers are introduced. GPU mipmaps and batches stay in memory.

### Facing and wildlife

Workers face the resource or building they are using, including rear-facing poses. Foraging uses hands; mining uses a pick; construction uses a hammer; farming uses a hoe. Deer roam around their hunting grounds, pause, walk and run away from nearby citizens. Their movement, facing and animation follow fixed simulation ticks and survive saving/loading. Clicking a moving deer still orders hunting at its resource site. Hunting yields and delivery rules are unchanged.

The existing sprites, jobs, movement, economy and atomic save path are reused. Five matching transparent atlases were made with the built-in image tool; the brief and cell layouts are in [animation asset prompts](docs/animation-asset-prompts.md). Wildlife and pose selection have small dedicated modules, with no parallel resource ledger. Startup loads five extra PNGs; saves grow by 2,180 bytes. No new runtime dependency, network access, background task or save frequency is introduced.

### Construction and idle-worker controls

Select an unfinished structure and choose **Resume building** to assign its nearest idle civilian. You can also select workers and right-click the site, or use **Build / repair** and click it. Progress and reserved materials are reused. If every worker is busy, the button explains how to reassign one. If paused, press Space or Play to let construction continue.

**Idle workers** and the header counter use actual availability, including builders whose previous work is finished. Workers with queued orders, cargo, ongoing work or training are excluded. Selecting idle workers centers the camera on them; if there are none, the current selection stays and a message explains why.

Left-drag empty ground to pan; the custom hand closes while dragging. Shift-drag selects groups, and dragging from a citizen still selects. Middle-drag and arrow keys remain available. The minimap also follows a held drag. The cursor is drawn with the existing UI palette; no additional assets, saved fields, filesystem operations or dependencies are required.

### Move, rotate, and demolish buildings

Press **Q** or click **Rotate** while placing a building to switch between its two isometric orientations. Rectangular footprints turn with the preview; **Shift** repeats placement in the chosen orientation. **R** still opens discoveries.

Select one of your structures to use **Move**, **Rotate Q**, or **Demolish** in the bottom tray. Move keeps the original structure in place until you click a valid destination; **Esc**, right-click, or **Cancel move** leaves it unchanged. Moves and rotations cost no materials and preserve health, construction, crops, research, and queues. Workers and trainees find the new location. Occupied tiles, resources, water, and invalid farm soil are rejected before anything changes.

Palisades and gates fit their one-tile footprints and meet along either grid axis. Orient adjacent segments along the same axis for a continuous line. Friendly citizens can pass through gates; enemy routes remain blocked. River crossings must stay clear.

**Demolish** requires a second confirmation. It returns no materials and loses reserved construction materials, research, and production queues attached to that structure. Housing is removed immediately. Use **Cancel building** instead to refund an unfinished site's reserved materials. Moving and demolition also work on unfinished sites.

Save version 4 appends 2,048 bytes for orientation, retaining the existing atomic save slot and frequency. Loading older country saves initializes their buildings to the original orientation. No assets, dependencies, or network operations were added.
