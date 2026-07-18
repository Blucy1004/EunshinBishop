# Q Architecture

## Scope through checkpoint 2

The Q migration starts with a leaf-first core.  Checkpoint 1 implements the
dependencies needed for correct chess state transitions:

```text
core types / bitboards / attacks / Zobrist
                    |
                    v
        explicit 16-bit Move
                    |
                    v
       Position / StateInfo / FEN / move generation
```

Checkpoint 2 adds ownership and ordering layers without inventing an evaluator
or search result:

```text
Engine
  |-- EngineOptions
  |-- Position + game StateInfo chain
  |-- TranspositionTable (four entries per cluster)
  `-- SearchWorker
        |-- TT reference
        |-- SearchConfig snapshot
        |-- HistoryTables / statistics / stop state
        `-- SearchStack[] / StateInfo[]

future search() --constructs--> MovePicker(Position, HistoryTables, hints)
```

Core and Position still may not include or refer to Search, Engine, UCI,
filesystem paths, or option strings.  Search contains only numeric/boolean
configuration; `EvalFile` remains owned above it by `EngineOptions`.

The full intended direction remains:

```text
UCI / Console
      |
      v
Engine
      |
      v
SearchWorker ---------- Evaluator
      |                     |
      v                     v
Position / Move generation / NNUE state
      |
      v
Bitboards / Attacks / Types
```

`Q Architecture` names the engine's modular core generation.  It does not
change the supplied `.snnue` v2 Architecture-A/256 tensor contract.

## Data and ownership

- `Move` is a two-byte value: six from bits, six to bits, and four explicit
  type bits.  Castling, en passant, and each promotion kind are encoded in the
  move and are never inferred from geometry during execution.
- `Position` owns the mailbox and color/type bitboards.  It points at, but does
  not allocate, the current `StateInfo`.
- The caller owns every `StateInfo`.  `doMove` links a supplied state to its
  predecessor; `undoMove` restores the predecessor.  No global undo or
  repetition array exists.
- NNUE accumulator storage is a leaf POD contract so `Position` does not depend
  on an evaluator implementation.  A state transition copies or restores it
  exactly; incremental delta logic is added in its dedicated checkpoint.
- Attack and Zobrist tables are initialized once and read-only thereafter.
- `Engine` owns the current game position, its stable reserved game-state
  chain, options, TT, and the single Worker.  Move-list application is
  transactional.
- `SearchWorker` references the Engine-owned TT and owns all per-worker mutable
  histories, statistics, state slots, node stacks, stop flag, and immutable
  per-search `SearchConfig` copy.  A cold-path Engine control mutex serializes
  mutation against session start/finish, while an active-session flag rejects
  option, TT, or game-state mutation while a search caller is using it.  The
  atomic stop request remains lock-free.
- `SearchStack` carries node-local moves, killers, eval/TT-value sentinels, ply,
  move count, flags, and bounded AEGIS/LIMBO state.  Unknown evaluations use
  `VALUE_NONE`, never a legitimate zero score.
- `TranspositionTable` owns a power-of-two vector of 64-byte aligned clusters.
  Each cluster contains four 16-byte entries.  It normalizes mate distance by
  ply and protects deeper/exact same-key information from shallow writes.
- `HistoryTables` owns fixed main, capture, and countermove arrays.  Gravity
  updates are bounded; normal searches preserve history and `newGame` clears
  it.

## Move picking

`MovePicker` uses the fixed sequence TT, good captures, killers, countermove,
quiets, and bad captures.  It generates captures only when the tactical stage
is reached and does not generate the larger quiet set after an earlier cutoff.
Each stage uses partial best selection, never a full sort or heap allocation.
Stale TT/killer/counter hints are rejected and every returned move is legal and
unique.

The frozen reference's approximate check bonus remains unchanged for ordering
scores.  Qsearch move admission uses an exact post-move check test so discovered
checks are not silently omitted; this correctness distinction is covered by a
dedicated test and will be included in later fixed-depth comparison reports.

## Hot-path rules

Move generation, make/unmake, MovePicker, histories, TT access, and future
search use fixed-capacity arrays and concrete value types.  They contain no
per-node allocation, virtual dispatch, `std::function`, `shared_ptr`, string
parsing, mutex, or exceptions.  TT resizing, Engine option parsing, text
conversion, the Engine control mutex, and diagnostic consistency reporting
remain cold paths.

## Behavioral preservation

The original evaluation and integrated search are not yet migrated.  The
reference artifact is frozen first, then typed position behavior is checked by
perft, special-move tests, random make/unmake restoration, Zobrist restoration,
and board/bitboard consistency.  Checkpoint 2 additionally verifies the new
ownership modules in isolation.  Search equivalence belongs to the later
evaluator/search integration checkpoint.
