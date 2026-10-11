# Country expansion validation

Local macOS validation, 2026-10-11. Implementation is based on refactor commit
`a7b4253`. These results cover the country expansion and compact HUD and belong to the
version-2 rules, not the historical version-1 hashes.

## Automated acceptance

Release CTest: **9/9 passed** after the contextual HUD update. The simulation
executable checks 351 assertions; input tests check 71, and the hidden-window HUD
run checks 97 (including those same 71). Counts include repeated assertions and
are not a count of independent gameplay scenarios.

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
- Both country campaigns enforce 13 full-state checkpoints each. A version-2
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

- Treaty: `08117f8245a283bc`
- Military: `f245c7a9fb1b63ce`

CTest writes replay logs and ending saves under `build/evidence/`. The current
full save payload including its header is 976,052 bytes.

## Presentation and performance

The existing reference-fidelity checks pass. Art tests inspect 52 atlas cells,
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
