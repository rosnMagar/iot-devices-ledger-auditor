# IOT-56: activity cursor lands one past the chain, so `ledger_reachable` goes false

**Sprint:** sprint-04
**Story points:** 2
**Status:** Review
**Depends on:** IOT-35

## Story
As an operator, I want `ledger_reachable` to mean what it says, so the dashboard does not warn about a healthy ledger.

## What happened
`LedgerActivity` set `_next_index = chain_length`, which is **one past the last
valid block index**. Once the cache had caught up, every refresh asked
storage-core for a range starting past the end:

```
GET /blocks?from=4    # chain has 4 blocks, indices 0..3
{"error":"invalid range: from=4 exceeds to=3"}   HTTP 400
```

`raise_for_status()` turned that into an exception, which the handler treated as
"storage-core unreachable" — so `ledger_reachable` went **false whenever no new
event had arrived since the last request**, which is most of the time on a quiet
fleet. `last_seen` and `status` stayed correct (the cache is kept on failure),
so the only symptom was a dashboard permanently warning that the ledger was
unreachable when it was fine.

Reproduced against a live storage-core:
```
refresh 1: reachable=True
refresh 2: reachable=False    # nothing new arrived
refresh 3: reachable=False
```

## Why the tests missed it
`ledger_transport` in `tests/test_activity.py` returned a canned 200 for every
call, including once its pages were exhausted. **It could not produce
storage-core's 400 at all.** Every end-to-end run posted new events immediately
before checking, which kept the cursor behind the head and hid it.

A mock that cannot produce the failure under test is worse than no mock — it
reports confidence it has not earned.

## Acceptance criteria
- [x] Repeated refreshes with no new blocks keep `ledger_reachable` true
- [x] New blocks are still picked up once the cache has caught up
- [x] A cursor past the end of the chain recovers instead of failing forever
- [x] The test double models storage-core's real range behaviour, including the 400
- [x] `ruff check` and `pytest` green

## The fix
**Request `from = max(0, _next_index - 1)`.** The cursor trails the head by one
block, so the range is always valid. Re-reading the last block each poll is
harmless — `_consume` keeps the maximum timestamp per actor, so it is idempotent.

**A 400 rewinds the cursor to 0.** That is what a range past the end means: the
ledger was reset or replaced and our cursor is stale. Without the rewind, every
future poll would repeat the same bad request forever. The next refresh rescans
from the start and recovers on its own.

Considered and rejected: changing storage-core so `from == chain_length` returns
an empty list instead of 400. Arguably the friendlier contract for a polling
client, but it changes a published API for the convenience of one caller, and
`from` beyond the end genuinely is a client error. Fixing the caller is the
smaller, more honest change.

## Testing
`ruff check` clean, **51 passed** (3 new).

- **`chain_transport`** — a new test double that models storage-core properly:
  serves `blocks[from:]` with the real `chain_length`, and returns 400 when
  `from` exceeds the last index. This is the part that stops the bug recurring.
- four consecutive refreshes with no new blocks all stay reachable, and the
  cursor never asks for an index past the end
- a block appended after the cache caught up is still picked up
- a cursor past the end rewinds: first refresh 400s, the next rescans from 0 and
  recovers (`calls == ["5", "0"]`)
- the two existing cursor assertions were updated for the trailing cursor
  (`["0", "1"]`, `["0", "49"]`)

Verified against a live storage-core: three consecutive refreshes with no new
events, all `reachable=True`.

## Note
Found by a question about whether restarting storage-core would restore
`ledger_reachable`. It would have — but the far more common trigger was simply
not posting an event.
