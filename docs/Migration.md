# Migration map and risk analysis

## Current single-file structure

The supplied 4,555-line reference combines attack initialization, Zobrist,
hybrid position state, contextual integer moves, a global undo/repetition
stack, classical evaluation, NNUE load/inference/accumulators, TT, search,
policies, time state, UCI, CLI, and PGN analysis in one translation unit.

## Major global state

| Reference state | Q owner |
|---|---|
| attack/magic tables | initialized `Attacks` module, read-only after startup |
| Zobrist arrays | initialized `Zobrist` module, read-only after startup |
| `undoStack`, `undoSp` | caller-owned `StateInfo` chain |
| `repKeys`, `repIdx` | `StateInfo::previous` chain |
| position NNUE arrays | `NNUE::AccumulatorState` inside each `StateInfo` |
| `nodes`, stop flag, policy counters | `SearchWorker::statistics` and atomics |
| killers and histories | `SearchStack::killers`, worker `HistoryTables` |
| root/PV node state | worker-owned `SearchStack[]` (algorithm deferred) |
| AEGIS/LIMBO stacks and counters | bounded stack fields and worker statistics |
| TT vector and mask | Engine-owned four-way `TranspositionTable` |
| NNUE path and configuration | `EngineOptions`; network weights/evaluation remain future `Evaluator` |

## Structural mapping

```text
old int move with contextual castle/EP
    -> Move(from, to, explicit MoveType)

old Position::board piece-type plus color occupancy lookup
    -> Piece mailbox plus color/type bitboards

old global makeMove/unmakeMove + Undo[]
    -> Position::doMove(move, supplied StateInfo)
       Position::undoMove(move)

old repKeys[]
    -> StateInfo position-key chain

old mutable globals read by AEGIS/LIMBO
    -> SearchStack/StateInfo predecessor data and Worker statistics

old direct-mapped global TT
    -> Engine-owned power-of-two array of four-way cache-line clusters

old global ordering arrays and full-list sort
    -> Worker HistoryTables + fixed-capacity lazy staged MovePicker

old UCI/global option reads during search
    -> EngineOptions validated on the cold path
       -> immutable SearchConfig copied at session start
```

## Highest-risk migration points

1. Changing the mailbox from piece type only to full colored `Piece` can
   desynchronize mailbox and bitboards unless every mutation uses one helper.
2. Promotion raw values change when special move kinds occupy the four type
   bits.  TT, UCI, SAN, killers, and history must consume `Move`, never legacy
   promotion integers.
3. Illegal pseudo-moves must roll back board, side, all three keys, castling,
   en-passant, rule-50 state, checkers, and accumulator state without touching
   global storage.
4. Castling transit attacks and en-passant occupancy exposure require explicit
   legality handling; geometry alone is insufficient.
5. `StateInfo` contains accumulator storage while Position is below Evaluator.
   The POD contract therefore lives in a dependency-leaf header.
6. The reference NNUE loader is non-transactional and the current NNUE result
   is blended as if absolute.  Those issues are recorded but deliberately not
   mixed into checkpoint 1.
7. Clustered TT keys are intentionally compressed to 16 bits inside an indexed
   cluster.  Correctness must never depend on a TT hit; stale moves are still
   checked by Position before use.
8. A returned Worker reference would make TT resizing unsafe if options could
   race session startup.  Engine serializes all frontend mutation and session
   start/finish with a cold-path mutex, then rejects option, position, and game
   changes until the explicit search session is finished.  Search nodes and
   `stop()` never acquire that mutex.
9. Numbered items 7-12 refer to Evaluator, TimeManager, and fixed-depth search
   whose implementations are numbered later.  The current checkpoint builds
   their ownership boundary without adding a placeholder evaluator or fake
   `go()` result.

## Implementation order

1. Freeze and hash the supplied reference.
2. Add strong types, bitboard helpers, attacks, and Zobrist.
3. Add explicit two-byte Move and text round-trip.
4. Add the synchronized hybrid Position representation.
5. Add caller-owned StateInfo make/unmake, null move, and repetition.
6. Add FEN/move generation and correctness tests needed to verify 1-5.
7. Add `SearchStack`, worker-local histories/statistics/states, and explicit
   session lifetime.
8. Add validated `EngineOptions`, immutable `SearchConfig`, and the Engine
   composition boundary.
9. Add lazy staged MovePicker and clustered TT with isolated unit gates.
10. Add classical/NNUE Evaluator, then integrate PVS/qsearch and TimeManager;
    only then run depth 6/8/10 behavioral comparisons.
