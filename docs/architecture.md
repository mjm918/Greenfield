# Code organization

One `GameState`, one command entry point, one economy owner and one serializer remain the core boundaries. The country expansion extends the refactored modules instead of replacing them with a general engine.

```text
main.c -> game_view.c -> view/* -> game.h (queries and commands)
       -> reference.c / scene.fs

game.c -> sim/* -> GameState (both factions and the shared valley)
save.c -> GameValidate -> explicit version-3 wire format (also reads country v2)
```

`greenfield_sim` has no raylib dependency. Simulation uses integer ticks and the RNG in `GameState`, never rendering time, input, filesystem access or platform randomness. `game.c` orchestrates initialization, dispatch and the fixed 100 ms tick. `save.c` alone owns persistence. The UI and rival AI both submit `GameCommandApply` commands.

## Authoritative owners

| Module | Responsibility |
| --- | --- |
| `game.c` | Initialization, command dispatch, tick scheduling, event log and deterministic RNG |
| `sim/definitions.c` | Resource/role names, building/discovery/age/unit definitions, recruitment and equipment costs |
| `sim/world.c` | Fixed valley, walkability, owner-aware gates, BFS routes, congestion recovery and movement |
| `sim/wildlife.c` | Bounded hunting-ground roaming, walking/running, obstacle checks and per-animal deterministic RNG |
| `sim/citizens.c` | Entity creation/reuse, population, deaths and actual civilian role/area/priority changes |
| `sim/orders.c` | Validated direct intent, order queues, interruption and rally instructions |
| `sim/jobs.c` | Select and execute work using orders or actual roles; route/retry, delivery and repair |
| `sim/economy.c` | Shared stocks, reservations, refunds, consumption, losses, production, cargo, delivery and transfers |
| `sim/buildings.c` | Placement, occupancy, construction, housing, upgrades, health limits and destruction |
| `sim/production.c` | Per-building queues; recruit, train, equip, cancel and demobilize |
| `sim/workshop.c` | Research prerequisites/progress, tools and smelting recipes |
| `sim/farming.c` | Reseeding, growth, harvest work and food cargo |
| `sim/survival.c` | Hunger, warmth, healing, spoilage, thaw and natural-resource renewal |
| `sim/progression.c` | Age projects, marked sites, village milestone and country victory/defeat |
| `sim/combat.c` | Both factions’ targeting, counters, range, cooldown, damage, retreat and plunder |
| `sim/rivals.c` | Diplomacy, bounded raid lifecycle and surrender cleanup |
| `sim/ai.c` | Deterministic rival command producer; economic and military priorities, no resource mutations |
| `sim/validation.c` | State ranges, cross-owner references, queue/occupancy consistency and conservation |
| `save.c` | Explicit canonical codec, hashes, atomic write and validated candidate load |

Public types and queries live in `game.h`. Feature handlers are declared in their owning private `sim/*.h`, not a shared catch-all header. Keep helpers static unless there is an actual cross-module consumer. New responsibilities should extend their closest owner. Roughly 200–400 lines is a review signal, not a reason to create speculative abstractions.

## State, handles and commands

`GameState` owns the 96 × 96 tiles and shared citizen/building arrays. Each entity has an owner and monotonically increasing generation. Each faction owns its resource accounting, discovery state, age, rally instruction, warnings, statistics and AI/raid timers. There are no per-building resource inventories.

Public commands targeting entities use `(slot, generation)` handles. Reusing a dead slot invalidates old orders, selection and groups. Internal active work uses an index whose lifecycle is owned by jobs; destruction resets dependent jobs before that slot can be reused. Population counts living citizens, including soldiers and trainees, with a separate 96-person limit for each owner.

Commands contain a tagged operation, faction, entity handle and typed order payload. Validation belongs at execution. UI placement previews share `GameCanPlaceFor`; age/truce requirements use the simulation queries. Workforce commands change selected citizens’ actual orders/roles. No second worker-allocation model is maintained.

Direct orders express intent. Jobs continue to own pathfinding and work. Workers finish physical delivery before resuming repeated gathering/farming. Queued targets are generation checked when consumed. Attack and plunder execution belong to shared combat; the former independent scripted-raider population is removed.

## Economy and queue lifecycle

All paired stock/ledger writes use the economy owner, except initial supply seeding and deserialization. The conservation invariant for each faction/resource is:

```text
initial + produced + imported
    = stored + carried + reserved + spent + exported + lost
```

Reservations have receipts stored on construction, research, advancement, production items or a crafting citizen. `EconomyReserve` transfers stock into reserved goods once. `EconomySettle` clears the receipt and either refunds, spends or loses it. Validation totals every receipt and compares it with the faction ledger. Imports must match the opponent’s exports.

Cancellation refunds unconsumed reservations. Destruction loses building/queue/project reservations. Dead trainees are removed from queues immediately. Crafting interruption refunds its unconsumed recipe; completed goods require delivery. Loot debits actual enemy stocks and enters the attacker’s carried goods; it cannot be spent before delivery.

Recruitment can pause for lost housing. Training converts one existing citizen after arrival at any valid edge/corner of the training building; it never creates an extra soldier entity. Parallel training grounds own independent queues. UI batch commands choose available queues, then normal simulation validation applies.

Blueprints reserve occupancy against overlapping construction but remain walkable until a builder starts. Building destruction clears occupancy and increments the map revision. Routes are recalculated on changes, seek free destinations and let idle allies step aside; hold-position orders are respected. Gates block the opposing faction.

## Fixed-step ordering

`GameAdvance` converts elapsed render time and 0/1/2/4× speed into 100 ms ticks. In each `GameTick`:

1. Increment time; apply survival and resolve deaths/lost cargo.
2. Run shared combat and resolve combat deaths.
3. Resolve raid lifecycle, diplomacy effects and surrendered buildings.
4. Grow crops; move wildlife; execute jobs by priority and citizen order.
5. Advance production queues.
6. Let enabled AI factions issue ordinary commands.
7. Evaluate milestones, country victory and defeat.

Iteration, BFS neighbor order, RNG calls and update order are replay-sensitive. Avoid wall-clock logic or unordered collections in simulation. Pausing and changing render rate do not alter the outcome for the same elapsed simulation time and command stream.

## Presentation

`game_view.c` owns the window/frame lifecycle and composition. Its one explicitly passed `ViewContext` owns transient selection, control groups, drag state, camera, command submenus, ledger state and feedback. These are UI state, not another simulation model. Save/load clears transient input and selections; simulation orders and production queues are persistent.

| Module | Responsibility |
| --- | --- |
| `view/assets.c` | Packaged atlases/fonts, alpha bounds, mipmaps and sprite drawing |
| `view/world.c` | Isometric transforms, shared drawing/picking geometry, depth order and previews |
| `view/animation.c` | State-driven directional poses, stable sprite anchors, animal interpolation and shared draw/pick bounds |
| `view/widgets.c` | Existing typography, stepped borders, buttons, bars, hit checks and the code-drawn hand cursor |
| `view/layout.h` | Shared viewport, adaptive command tray, minimap and overlay geometry for rendering and input |
| `view/hud.c` | Compact resources/rates, warnings, speeds, minimap, construction palette and world hit regions |
| `view/selection.c` | Floating contextual tray: units, buildings, occupied production queues and command submenus |
| `view/panels.c` | Dismissible objectives, batch workforce, technologies and diplomacy ledger |
| `view/screens.c` | Menu, field guide, defeat and continued-play overlays |
| `view/input.c` | Selection, groups, contextual/queued orders, camera and modal blocking |
| `view/actions.c` | Command adapter, distributed batch training, save feedback and autosave protection |

Input uses logical 1440 × 900 coordinates. Below the 48px header, the world renders across 1440 × 852 pixels to the bottom edge. Floating controls occupy only their actual rectangles: a 50px unselected toolbar, a 100px selection tray whose width follows building actions, a 142px construction palette, and a separate minimap. Production rows appear only for occupied queues. Rendering and picking share this geometry; point and drag selection reject occluded areas while gaps remain playable. Drag candidates are collected before updating the selection, so the tray cannot change its hit region halfway through a drag. The ledger blocks background actions; one pointer click can activate only one control, even when a submenu opens in that frame. B opens construction only for selected available civilians. Source Sans 3 Regular/Semibold use the existing smooth font-loading path. The composition boundary rasterizes at 2×. Terrain/sprites outside the viewport are skipped; ground shadows are batched ahead of depth-sorted sprites to avoid thousands of texture/primitive batch switches. Assets retain smooth filtering and readable fonts. `reference.c` and its original fidelity commands remain independent.

## Persistence and compatibility

Save version 3 keeps the existing `country.gfv2` slot. Country version-2 files load their unchanged payload and initialize facing/wildlife in memory; loading does not write the file. The next ordinary save writes version 3. Historical village version-1 files remain unsupported and untouched. All persistent fields have explicit little-endian serialization; no pointers or raw C struct dumps. The serializer also supplies the full-state hash. Validate, write temporary file, flush/sync, atomic rename. Loading validates a separate candidate and commits it only on success. Failed loading blocks autosave until a deliberate recovery action.

Add serialization and validation alongside every persistent state change. One added field must not create a parallel save format or derived authoritative store. The animation extension adds 2,180 bytes per save and five packaged PNG reads at startup. Existing manual/autosave frequency is unchanged. No network, database, background task or runtime dependency is added.

## Tests and intentional gameplay baselines

`tests/rts_tests.c` checks the new rules at owning boundaries, including conservation, reservations, generation reuse, direct orders, gate access, training arrival/cancellation/death, progression, metallurgy, repair, survival, combat, persistence failures and deterministic stepping. `test_view.c` drives the production input adapters with controlled platform input. `test_capacity.c` explicitly builds a maximum-population stress fixture.

`tests/campaign.c` is a test-only player, never linked into the game. Treaty and military campaigns issue ordinary validated commands without stock/population/health injection. They check the target timings, recover from food shortage, suffer theft, save/load midway and complete the appropriate ending. Both compare version-3 full-state checkpoints. Their faster execution measures simulated time; it is not a human playtest.

Historical version-1 save/hash fixtures are retained read-only. Old test source that depended on the retired single-faction/raider model is removed. It cannot prove intentional gameplay changes correct. See `tests/fixtures/README.md` for the explicit new baseline and `docs/country-validation.md` for evidence and remaining limits.

## Directional animation and wildlife

Citizen facing is persistent simulation state. Movement faces the next route step; work faces its actual resource or building; combat uses its chosen target. Resetting a job retains the last facing. Four isometric orientations use front/rear sheets and horizontal mirroring. The original front sheets remain in use for walking, chopping, carrying and spear work; specialist sheets provide hands, picks, hammers and hoes. Animation selection is derived from the current job, path and tick, with no second work state machine. Sprite scale and foot anchors remain stable across a clip.

Each hunting ground has one representative deer, roaming at most two tiles from that resource site. Deer alternate idle/walk and run away from nearby living citizens of either faction. They use existing walkability, additionally avoid occupied structures and mineral rocks, and can pass under forest canopies. Their RNG does not consume the economy/AI random stream. Depleted grounds hide their deer until resource renewal. Hunting still draws food/hides from the original tile through the existing work-and-delivery ledger; there is no animal inventory or separate hunting economy. Clicking a moving deer resolves to that ground. This is bounded wildlife movement, not an individual-prey chase/combat system.

`tests/test_wildlife.c` covers facing, obstacle recovery, faster flight, independent RNG, pause, persistence and country-v2 compatibility. View tests cover directional clip selection, interpolation, moving-animal picking and exported visual fixtures.

## Construction recovery and camera gestures

The idle counter, idle selection and Resume building action share `GameCitizenIsIdle` / `GameIdleCountFor` in the citizen owner. Assigned role counts remain separate for workforce/AI allocation: an idle builder still has the builder role. Available citizens are living civilians with no active job, route, queued orders, cargo or training. No second stored idle flag is introduced.

`ViewResumeConstruction` chooses the nearest idle player citizen and submits the existing `CMD_ORDER / ORDER_BUILD`. It never places a duplicate site or charges materials. `OrdersChoose` routes repair orders on unfinished buildings into `JOB_BUILD`, including queued orders and incomplete hut upgrades; existing job, building and economy owners handle labor and receipts.

The input owner distinguishes a click, selection drag and empty-ground pan using the existing six-pixel threshold. Shift-drag adds a group; a drag starting over a citizen also selects. Left and middle pans retain their screen/camera origins, operate in logical coordinates, and keep ownership across HUD boundaries until release. Minimap dragging stays in its existing HUD owner. `ViewCancelDrag` clears all transient gesture state for overlays, load, new game and focus loss. Nothing is serialized. The hand is drawn after composition, above all UI and letterboxing; the OS cursor is restored outside the focused game window and on exit.
