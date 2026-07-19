# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Every entry below is verified against a test run recorded in a
`docs/CheckpointN_Report.md`; nothing here describes an unimplemented
feature.

## [Unreleased]

Not yet released. See `docs/Build.md` for the pre-release checklist and what
is still missing before any `v5.0.0` tag.

### Added

- GPL-3.0-or-later licensing and a non-binding upstream-sharing request for stronger forks.

- Q Architecture: a modular from-scratch reimplementation of the frozen
  single-file `EunshinBishop`/`SniperBishop` v2.62 reference, split into
  `core`, `position`, `engine`, `eval`, `search`, and `uci` modules with an
  explicit hot-path/cold-path ownership boundary (`docs/Architecture.md`).
- Explicit two-byte `Move` encoding (six from bits, six to bits, four
  explicit type bits); castling, en passant, and promotion kind are never
  inferred from geometry during execution.
- Caller-owned `StateInfo` make/unmake chain, replacing the reference's
  global undo stack and repetition array.
- `Engine`-owned four-way clustered `TranspositionTable` and a lazy staged
  `MovePicker` (TT, good captures, killers, countermove, quiets, bad
  captures), replacing the reference's direct-mapped TT and full-list sort.
- Worker-owned `HistoryTables` (main/capture/countermove, gravity-bounded
  updates) and node-local `SearchStack`.
- Classical MG/EG evaluator and a FIRST_NET v5 **residual-correction** NNUE
  evaluator (`NNUEOutputMode=Residual`, `ResidualGuard` bounded), with a
  safe classical fallback when `.snnue` loading fails.
- Iterative deepening with aspiration windows, PVS/alpha-beta, null move,
  LMR, IIR, futility pruning, and an AEGIS instability-response policy.
- `TimeManager` with optimum/maximum budgeting, emergency mode, and
  best-move/score-stability-aware iteration extension.
- `Engine::go` as the single synchronous search entry point, plus a UCI
  frontend that runs it on a worker thread and handles `stop`/`quit`
  asynchronously.
- A standalone `EunshinBishop.exe` UCI binary, and `--version`/`--help` CLI
  flags backed by a single `Version` namespace (`src/core/version.h`) shared
  by the UCI `id name` line, the console banner, and `--version`.
- An independent, pin/king-legality/en-passant/promotion-correct strict SEE
  module (`src/search/see.*`), consulted only from the optional
  `SEEPruning` qsearch path (default `false`).
- `LIMBO`, a bounded frontier verification policy (`src/search/limbo.*`),
  gated by cooldown, no-consecutive-extension, AEGIS-non-overlap, and
  remaining time/iteration budget checks (default `false`).

### Changed

- Added an internal search/probe `doMove` path that invalidates child NNUE cache metadata without copying the ~2 KiB accumulator; public/game-state calls retain the existing byte-preserving contract.

- Replaced the reference's contextual-castle/EP integer move encoding with
  the explicit typed `Move` above.
- Replaced the reference's piece-type-only board array with a `Piece`
  mailbox plus color/type bitboards, updated through single helpers to keep
  them synchronized.

### Fixed

- Fixed a GCC "enumeral and non-enumeral type in conditional expression"
  warning in `Position::castlingRights()`.

### Known Issues

- Q's search and the frozen v2.62 reference diverge past a shallow fixed
  depth by design (different search algorithm, different NNUE blending
  equation); see `docs/Checkpoint3_Report.md`. No search-equivalence or
  relative-strength claim is made.
- No Elo or paired-game evidence exists for either engine, or for
  `SEEPruning=true`/`LIMBO=true`.
- Pure classical evaluation strengthening (pawn structure, king safety,
  etc.) has not started; see `docs/ClassicalEvalBacklog.md`.
- Only MSVC has been built and tested; GCC and Clang builds have not been
  run on any machine yet.
- Repository source and documentation are licensed under GPL-3.0-or-later. FIRST_NET v5
  network weights remain excluded pending a separate weight-file license.
