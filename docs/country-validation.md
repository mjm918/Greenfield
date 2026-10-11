# Country expansion validation

Local macOS validation, 2026-10-11. Implementation is based on refactor commit
`a7b4253`. These results cover the country expansion, compact HUD and the facing/wildlife fix based on `6a7988c`. The current codec is version 3; the country gameplay rules remain unchanged.

## Automated acceptance

Release CTest: **10/10 passed** after the facing/wildlife update (22.56 seconds). The simulation
executable checks 351 assertions; wildlife/facing checks 47; input/pose tests check 99, and the hidden-window HUD
run checks 133 (including those same 99). Counts include repeated assertions and
are not a count of independent gameplay scenarios.

After the facing/wildlife update, AddressSanitizer and UndefinedBehaviorSanitizer passed **4/4 focused suites**: simulation, wildlife, input/pose and hidden-window HUD (5.41 seconds).

Before the HUD update, AddressSanitizer and UndefinedBehaviorSanitizer: **5/5 passed** in the
RelWithDebInfo build, covering simulation, both complete campaigns, population
capacity and the production UI input adapter. The final run completed in 85.96
seconds without reported sanitizer failures.

- Two faction ledgers conserve initial/produced/imported goods against stored,
  carried, reserved, spent, exported and lost goods. Reservation receipts are
  checked against the authoritative ledger; AI contains no stock-awarding path.
- Construction charges once; blueprints stay walkable until work starts. Completed
  buildings occupy tiles; destruction opens routes. Queues refund on cancellation
  and lose materials on destruction/death. Recruitment and training have distinct
  entity semantics; dead slots invalidate old handles.
- Direct orders, queued movement, owner checks, stale targets, full order queues,
  attack-move continuation, blocked routes, friendly gate access and hostile gate
  rejection are covered. Training at a footprint corner has a regression test.
- Hunger/cold warnings, damage, recovery and total-population defeat are covered.
  Farms remain locked until progression, harvests require delivery, smelting
  consumes reserved inputs, advancement cannot double-charge, and repairs honor
  stronger-building health limits.
- Combat tests cover the counter triangle, physical loot limits, raid retreat,
  truce restrictions/expiry and treaty prerequisites. Campaigns exercise guard
  response, real enemy theft, offensive combat and center surrender.
- Saving while construction, recruitment, training, research, orders and raid
  warnings are active preserves the canonical hash and deterministic continuation.
  Corrupt/version-1 loads and interrupted/failed temporary writes preserve usable
  state and the previous slot. The player save is not used by tests.
- Identical command streams match at 30/60 render frames per second and 1×/2×/4×
  for equivalent elapsed simulation time. Both rival AI and actual work are active.
  Pausing leaves simulation unchanged.
- Both country campaigns enforce 13 full-state checkpoints each. A version-3
  save/load at minute 75 preserves their continued result.

## Campaigns

| Scenario | Village | Country | Ending | Recovery evidence |
| --- | --- | --- | --- | --- |
| Economic/treaty | 53:10 | 124:25 | Lasting treaty, 50 housed | Four shortage recoveries; health fell to 421; one organized raid; goods stolen before tribute |
| Military | 53:10 | 124:25 | Rival surrender, 50 housed | Four shortage recoveries; health fell to 8; two organized raids; goods stolen before truce expired |

The test-only player uses ordinary commands and the fixed-step API at speed 1.
It deliberately diverts food workers for the first ten minutes, restores food
production, allows the first theft, then either negotiates lasting peace or trains
and equips an attacking force. No resource, population, health, research or clock
shortcuts are used. Elapsed values above are **simulated campaign time**. Faster
headless execution is not a completed wall-clock or human playthrough.

Ending hashes:

- Treaty: `714bd3183f24a208`
- Military: `e2c8fb99fed80876`

CTest writes replay logs and ending saves under `build/evidence/`. The current
full save payload including its header is 978,232 bytes.

## Presentation and performance

The existing reference-fidelity checks pass. Art tests inspect 132 atlas cells,
alpha transparency, a 2880 × 1800 capture, world detail and smooth text edges.
Winter, Bronze settlement, selected-group controls, all seven discoveries,
building preview and full-valley screenshots were inspected. Native macOS checks of the earlier layout
confirmed individual selection, the field guide, pause, fullscreen and overview.
Group selection/queued input have controlled-adapter coverage; a native drag check
was interrupted by changing foreground UI and is not claimed as completed.

The capacity fixture contains **96 living citizens per faction** with settlements,
work assignments and shared production. Six hundred simulation ticks passed state
validation at about **1.0–1.5 ms/tick** in Release on this Mac. It is an explicit
stress fixture, not campaign evidence.

Before widening the HUD viewport, after batching shadows and culling off-screen
terrain/sprites, uncapped rendering of that fixture measured:

| View | Mean render time | Maximum observed |
| --- | --- | --- |
| Settlement | 11.62 ms | 38.44 ms |
| Whole valley | 21.83 ms | 33.10 ms |

These are local samples after ten warm-up frames, not a guarantee of sustained
60 FPS on every Mac. Overview remains around 46 FPS by the measured mean and
can benefit from further rendering optimization. Normal play retains its 60 FPS
frame target. Simulation outcomes are independent of these frame rates.

## Contextual HUD update

The full-width world viewport now extends from the 48px header to the bottom
edge (1440 × 852 logical pixels). The previous 168px full-width bottom band is
replaced by floating controls: a 100px selection tray, a 50px unselected toolbar,
a construction palette that expands to 142px, and a separate minimap. Normal
selected controls plus the minimap occupy **8.9% of the canvas**, down from
18.7% for the old band (about 52% less bottom HUD area). With nothing selected,
they occupy 5.6%. Queues take space only when occupied; hover hints replace the
permanent instruction column. Text sizes are unchanged. Objectives, workforce, discoveries and
diplomacy open in a dismissible ledger. Source Sans 3 Regular/Semibold replace the
two gameplay fonts through the existing loader; assets remain smooth.

Focused production-input tests cover newly exposed world edges, HUD/ledger click
blocking, worker-only construction, group selection, queued orders, rally input,
and Escape navigation. Compact-layout checks cover commands in the exposed
bottom gap, drag selection excluding citizens behind the tray, the expanded
construction hit area, and an empty queue returning its area to the world.
The additional hidden-window test draws the real HUD and
activates its buttons: construction, training submenu, workforce, recruitment,
refund of the final queue item, minimap navigation, soldier orders and pause
while a ledger is open.
A consumed-click guard prevents a newly opened submenu from accepting the same
click twice. Actual packaged-font measurements check the longest age labels.
The final input and rendered-HUD suites also pass **2/2 under AddressSanitizer
and UndefinedBehaviorSanitizer**.

macOS GPU captures of the idle settlement, workers, soldiers, center, construction,
objectives, workforce, discoveries and diplomacy were inspected. After compacting
the tray, fresh captures of the idle settlement, workers, soldiers, center and
construction were inspected. This is capture
and controlled-input evidence, not a new full human playthrough. No live player
session or player save was modified during this UI work.

`view/selection.c` owns contextual controls; `view/layout.h` supplies shared
geometry. Existing command, selection, economy and save owners remain authoritative.
No new persistent state, queue, serializer or runtime dependency was added. The two
local font loads replace two previous loads (857,100 font bytes instead of 109,604,
about 730 KiB more at startup); runtime network and save activity are unchanged.
Historical Atkinson font files remain in the repository.

## Remaining acceptance limits

Human testing across varied economic and military strategies, a full wall-clock
1× playthrough, and broader macOS hardware profiling remain open. Automated
success establishes reachability and regression protection; it does not establish
that every strategy is well balanced. Long sandbox endurance beyond the capacity
and campaign runs has not been exhaustively tested.

## Ownership and I/O audit

Reused the original simulation state, jobs, movement, gathering, farming,
construction, survival, workshop, renderer, fonts, three atlases and atomic codec.
New private modules own orders, production, progression, shared combat and rival
command decisions. The scripted-raider path and incompatible v1 test runner are
retired; historical save fixtures remain read-only for provenance and rejection
checks. No parallel resource pool, worker-allocation budget or serializer exists.

Runtime adds one local PNG asset read and a larger payload to the existing
manual/two-minute autosave path. No network calls, accounts, services, telemetry,
background workers or runtime dependencies were added.

## Facing and moving wildlife verification

- The original rendering bug reset horizontal facing when citizens stopped and had no rear-facing sprite option. Movement, work and combat now write facing at their existing execution boundaries. Work targets choose hands/pick/hammer/hoe/chop/spear clips; stopping retains the heading.
- Six initial deer all moved in fixed-tick tests. Tests cover walk/run speed, turning away from a person mid-step, walkable terrain, blocked-route recovery, bounded roaming, depletion/renewal visibility and pause. Animal RNG is independent of the economy/AI stream.
- Version-3 save/load retains facing, positions, goals, gait, interpolation steps and RNG; continued hashes match. Invalid wildlife state is rejected before writing and leaves the last save usable. A constructed country-v2 payload loads and can subsequently save in v3. A separately retained real v2 campaign save loaded through the application capture path.
- Both complete scripted campaigns retained their timing, shortage and raid outcomes. Their entire original v2 ending payloads match the saved pre-change payloads byte-for-byte. Only the appended facing/wildlife data changes the full-state checkpoints. These remain accelerated scripted runs, not new wall-clock human playthroughs.
- Inspected `build/evidence/work-facing.png`, `animal-walking.png`, `animal-running.png` and `v2-save-compatible.png`. The first three are explicit rendering fixtures; workers surround a bush and a deer reacts to a nearby citizen. Tests exercise the real renderer and click-to-hunting-ground adapter. The reference-fidelity test remains passing.
- Reused the original front citizen sprites, scenery, job targets, movement, economy and serializer. Added five matching atlases using the built-in image tool, one bounded wildlife owner and one shared pose/draw/pick module. Removed static hunting-ground animal drawing; there is no duplicate animal render path, resource inventory or harvesting rule.
- Runtime change: five additional PNG reads at startup, 2,180 more bytes per ordinary save, and one bounded wildlife pass per existing 100 ms tick. For the current six deer this scans at most 1,152 citizens per tick in a full 192-person world. No extra filesystem writes, save cadence, network calls, dependency or background thread.
- The currently running player session was not restarted. Native interaction with that session and a new human playthrough are not claimed.

## Construction recovery, idle workers and camera controls

- Interrupted construction now has a contextual Resume building action. It assigns an idle civilian through the existing build command. The worker's Build / repair action also resumes unfinished sites through the authoritative order executor. Regression tests stop, save/load and resume construction for both factions, verify retained progress and reservations, and complete the projects without charging twice.
- The header counter, both idle-worker buttons and automatic resume assignment share the citizen owner's availability query. A builder with no remaining work is eligible regardless of its retained role; queued orders, active work, routes, cargo, training, soldiers and dead citizens are excluded. No-idle feedback preserves the current selection.
- Production input tests cover left-drag empty-ground panning, Shift-drag selection, retained selection and orders, the six-pixel threshold, returning below that threshold after starting a pan, HUD crossing, middle-button capture and overlay cancellation. Rendered HUD tests click both Idle buttons, Resume building and Build / repair, then verify actual construction progress. A held minimap drag updates the camera without another click.
- The release build passed all **10/10 CTest tests**, run as six headless tests and four graphics tests. The rendered input/HUD executable reports **164 checks, zero failures**. Simulation, headless input and rendered HUD tests also passed **3/3 under AddressSanitizer and UndefinedBehaviorSanitizer**. The original reference capture still has zero differing pixels out of 1,441,170.
- Inspected `build/evidence/controls-resume.png` and `controls-hand-grab.png` after the Mac was unlocked. They show the compact Resume/Cancel tray and the open/closed hand states. These are macOS GPU captures with controlled input, not interaction with the existing player session or a new human campaign playthrough.
- Reused the command, jobs, construction, economy, camera, selection and UI palette owners. New surfaces are derived idle queries, a Resume UI action, transient gesture fields and a code-drawn cursor. Searches confirmed no second idle flag, material-charge path, construction operation or persistent camera state. This controls change adds no asset reads, save fields, filesystem writes, network calls, dependency or background task.

### Retina cursor correction

The hand overlay used `BeginMode2D` after window composition, replacing raylib's
Retina display scale and drawing the hand away from its pointer hotspot. The
existing cursor renderer now takes the viewport offset and scale explicitly,
preserving the window transform. It still uses the same hand, closed-grip pose,
focus/visibility lifecycle and letterboxing behavior; there is no second cursor
renderer, new asset, saved field or runtime I/O.

The rendered HUD test now opens with the same high-DPI flag as the game and reads
the actual window framebuffer before swapping buffers. It checks the palm at the
pointer location for both poses, at full and reduced scale with an offset.
`build/evidence/cursor-window.png` records the Retina-window check, in addition to
the existing canvas captures. All 173 input/HUD checks pass, including under
AddressSanitizer and UndefinedBehaviorSanitizer. The five focused input, HUD,
game-capture, artwork and reference tests pass. The game was restarted and its
saved village restored; live mouse-hover verification remains unconfirmed because
native coordinate actions returned `noWindowsAvailable` even though keyboard
actions and window screenshots worked.

### Building availability and selection feedback

The existing 142px construction tray now labels each building Ready, Missing or
Locked. Hovering opens a requirements card; clicking an unavailable building pins
that card without arming placement or reserving resources. The card names the
required age/research and lists each resource's cost, stored amount and exact
shortfall. Its height follows the number of resources. Drawing and map hit tests
share its bounds, so inspecting it cannot select or move something underneath.

`GameBuildingUnlockedFor` extracts the existing gates from the building owner and
is called by both placement validation and the palette. Costs and available stock
still come from `BUILDINGS` and the faction economy. There are no UI copies of the
unlock rules, extra charging paths, or changed balance requirements. The only new
state is a temporary inspected-building selection; it is not saved. Existing
sprites, Source Sans 3, widgets and selection handles are reused. Selected buttons
have a contrasting border, buildings have outlined footprints, and citizens have
rings and overhead markers drawn after sprites with a minimum visible screen size.
The former thin selection drawing was removed rather than kept as a second path.
No assets, dependencies, runtime I/O, network calls, storage writes or background
work were added.

The six focused release tests pass: simulation, headless view input, rendered HUD,
game capture, artwork and reference fidelity. The simulation reports 441 checks;
the rendered input/HUD run reports 189 checks, all passing. Simulation, input and
rendered HUD also pass under AddressSanitizer and UndefinedBehaviorSanitizer.
Tests cover both factions' age/research/thaw gates, unchanged state on rejection,
locked and unaffordable clicks, exact-stock readiness, requirements-card hit
blocking, and visible selection strokes at close and overview zoom. Inspected
macOS GPU fixtures: `build/evidence/build-locked.png`, `build-missing.png`,
`build-ready.png`, `selection-citizens.png`, `selection-overview.png` and
`selection-building.png`. These are controlled fixtures, not a live player-session
check or a new campaign playthrough. The already-running player process must be
reopened to use the rebuilt executable.

### Gather targeting and acknowledgement

Gather now retains a selected button border and a specific left-click targeting
prompt. Hovering a resource outlines its ground footprint. Clicking the command
again, right-clicking or pressing Escape cancels targeting; an invalid target
leaves targeting active with the existing simulation rejection message. A
successful order reports the accepted worker count and whether it was queued,
including a Space-to-resume reminder while paused. Shift keeps Gather active for
further queued targets. Depleted resources are rejected by `OrdersApply` before
clearing or appending orders, rather than accepting an order that immediately ends.

`ViewResourceAt` shares the existing resource sprite geometry in `view/world.c`
and composes moving-animal picking. Both explicit Gather and contextual right-click
use it, fixing clicks on canopies that project onto unrelated ground tiles.
`ViewIssueSelected` now returns its already-computed acceptance count so input can
retain a rejected target mode. Existing callers still use the same command path.
The existing jobs and economy owners continue to perform work and delivery; there
is no parallel gathering operation, selection state, saved field, new asset,
filesystem operation, network call or background work. Picking scans the fixed
tile array on a contextual click or while showing a Gather hover target; it adds
no persistent cache or simulation work.

Eight release checks passed across simulation, view input, rendered HUD, treaty
and military campaigns, reference fidelity, game capture and artwork. The three
simulation/input/HUD sanitizer tests passed too (453 simulation checks and 209
rendered view checks). The new HUD test follows the real button-to-canopy click,
then executes worker jobs until wood is delivered. It also covers invalid-target
retry, depleted targets, metallurgy rejection, right-click picking, paused
acknowledgement, Shift queueing and cancellation. Inspected macOS GPU captures
`build/evidence/gather-target.png` and `gather-confirmed.png`. These remain controlled
tests; the running player session has not been restarted or changed.
