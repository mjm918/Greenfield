# Pre-refactor simulation baseline

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

`test_baseline.c` loads each original save and compares subsequent checkpoints,
as well as replaying from a new game. The fixtures are read-only inputs. They do
not replace or access the player's save. Preserve them during structural changes;
an intentional gameplay change needs an explicit explanation of affected baselines.
