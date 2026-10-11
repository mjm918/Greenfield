# Historical version-1 baseline (retained, not executed)

These fixtures were captured from commit
`b1fbbc9e1f4cef9322f4aa05ad67ef3e35918ea1` using its `GameInit`, `GameTick`,
`GameSave`, `GameHash` and the existing `tests/campaign.c` command script.
The original source was retained and compiled separately from the refactored modules.

Starting from `GameInit`, apply `CampaignOrders(g)` immediately before each
`GameTick(g)`. Record the hash every 600 ticks from 0 through 30600, plus ticks
5000 and 8600. Continue this script after victory. Saves and hashes are captured
after reaching a checkpoint, before issuing the next tick's orders.

- `campaign-5000.gfv1`: winter village with active insulation research and jobs.
- `campaign-8600.gfv1`: thawed village with an active raid, storage research and
  deliveries in progress.
- `campaign-checkpoints.inc`: 54 canonical full-state hashes, including the two
  save checkpoints and continued play after the 48-minute victory.

SHA-256:

```text
59d83735bd148c6f12df9cc3d56693d5d26e90f25717d2c1eba0863eb1cfcaad  campaign-5000.gfv1
09dd26ac4e6ad7abfc5e52dc17023671f904c14154fa28ba11c816699f268b81  campaign-8600.gfv1
34783bdd72e42e9d9ecefe4941d45f5b85c09fa3c71b3407ec68f52f74ed4d12  campaign-checkpoints.inc
```

The former `test_baseline.c` loaded these saves and replayed their original checkpoints.
That runner is retired with the single-faction rules; it cannot compile against the
country state and does not establish version-2 correctness. The fixtures are read-only inputs. They do
not replace or access the player's save. Preserve them during structural changes;
an intentional gameplay change needs an explicit explanation of affected baselines.


# Country version-2 baseline

`rts-checkpoints.inc` was recorded on 2026-10-11 from the intentional country
expansion based on `a7b4253`, using the new `GameInit`, `GameAdvance`, canonical
serializer and `tests/campaign.c`. The source is not yet committed. Routine rival construction/research/recovery events do not masquerade as player
notifications, and the first-harvest event is emitted only once. These intentional
event-log changes are part of the new full-state baseline. Each row is
`{tick, treaty_hash, military_hash}`. It covers initialization and ten-minute
checkpoints through minute 120 for each campaign (26 full-state expectations).
Campaigns also validate the ending and round-trip a save at minute 75.

The scripts use identical economic commands until the military scenario builds
its army in Bronze. They issue ordinary game commands and never inject stock,
population, health, discoveries or objective completion. The deliberate early
food shortage and the first enemy theft exercise recovery. Country completion
must remain within 120–180 simulated minutes, with the village at 45–60 minutes.

Both normal CTest campaigns enforce these hashes. For a deliberate rule change,
review its semantic assertions first, then record candidate hashes explicitly:

```sh
cd build
./game_campaign treaty --record-baseline > evidence/treaty-baseline.log
./game_campaign military --record-baseline > evidence/military-baseline.log
```

Recording writes candidates to stdout, never changes this fixture automatically.
Explain the changed rules and inspect both campaigns before replacing expectations.
UI rendering changes need not change simulation hashes. There is intentionally no
v1 migration or comparison against the old refactor hashes.
