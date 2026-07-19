# EunshinBishop

`EunshinBishop 5.0.0-rc.1 Q` -- a from-scratch, modular UCI chess engine.
This is the "Q Architecture" migration of a supplied single-file
`SniperBishop`/`EunshinBishop` v2.62 reference engine; the reference source
is preserved byte for byte under `reference/source` and is never edited.

## 1. About

Q reimplements the frozen reference as separate `core` (bitboards, attacks,
Zobrist, typed `Move`), `position` (mailbox + bitboard `Position`,
`StateInfo` chain, move generation), `engine` (ownership boundary, validated
options), `eval` (classical + NNUE evaluators), `search` (PVS, TT, move
ordering, time management), and `uci` modules, with an explicit rule that
`core`/`position` never depend on `search`, `engine`, or `uci`. See
`docs/Architecture.md` for the full module map and `docs/Migration.md` for
what maps to what from the original single file.

## 2. Features

- Explicit two-byte typed `Move` encoding; castling, en passant, and
  promotion kind are never inferred from geometry.
- Caller-owned `StateInfo` make/unmake chain, four-way clustered
  transposition table, lazy staged `MovePicker`, worker-owned history
  tables with gravity-bounded updates.
- Classical MG/EG evaluator and a FIRST_NET v5 residual-correction NNUE
  evaluator, with a safe classical fallback if the network is missing or
  unreadable.
- Iterative deepening with aspiration windows, PVS/alpha-beta, null move,
  LMR, IIR, futility pruning, and an AEGIS instability-response policy.
- An independent, pin/king-legality/en-passant/promotion-correct strict SEE
  module (optional qsearch pruning, off by default) and LIMBO, a bounded
  frontier verification policy (optional, off by default).
- Asynchronous UCI frontend (`stop`/`quit` never block on a running search)
  with a `TimeManager` that never exceeds its hard deadline.

See `CHANGELOG.md` for the versioned list this is generated from.

## 3. Current development status

This is a pre-release development checkpoint, **not** a `v5.0.0` release.
Checkpoints 1-2 (core types, ownership boundary) and checkpoint 3 (evaluator,
search, `Engine::go`, UCI) are implemented and independently re-verified;
checkpoint 4 adds SEE and LIMBO. See `docs/Checkpoint2_Report.md` through
`docs/Checkpoint4_Report.md` for what was actually tested, with SHA-256
hashes for every reported binary.

**No Elo, fixed-depth search-equivalence, or public-release claim is made.**
`docs/Checkpoint3_Report.md` records a fixed-depth comparison between Q and
the frozen reference at Kiwipete depths 6/8/10: the two searches agree at
depth 6 and diverge past it by design (a different search algorithm and a
different, residual-correction NNUE equation), which is evidence about the
current gap, not a strength or equivalence claim. No paired-game or
time-control testing has been run for either engine, or for `SEEPruning=true`
/ `LIMBO=true`. See `docs/Build.md` for the full pre-release checklist and
exactly what is still missing before any `v5.0.0` tag.

## 4. Supported platforms

| Platform | Status |
|---|---|
| Windows x64 (MSVC) | Built and tested every checkpoint |
| Linux x64 (GCC / Clang) | CI-scaffolded (`.github/workflows/build.yml`); not yet run on a physical machine |

## 5. Quick start

```powershell
cmake -S . -B build-vs -A x64
cmake --build build-vs --config Release --parallel
.\build-vs\Release\EunshinBishop.exe --version
echo "uci`nisready`nposition startpos`ngo depth 6`nquit" | .\build-vs\Release\EunshinBishop.exe
```

`EunshinBishop --help` prints the CLI flags; with no arguments the engine
speaks UCI on stdin/stdout, meant to be driven by a GUI or script.

## 6. Build

See `docs/Build.md` for full platform-by-platform instructions, the
building-without-a-network path, and the pre-release checklist. Short
version for Windows/MSVC:

```powershell
cmake -S . -B build-vs -A x64
cmake --build build-vs --config Release --parallel
ctest --test-dir build-vs -C Release --output-on-failure
```

## 7. Registering with a GUI

Point the GUI's "add engine" dialog at the built `EunshinBishop.exe`
(`build-vs\Release\EunshinBishop.exe` for the command above). No special
working directory is required beyond having the `.snnue` network file (if
any) placed next to the executable -- see the next section.

## 8. Placing the NNUE network

Place `firstnet_v5_10b.snnue` next to `EunshinBishop.exe`, or point
`EvalFile` at another path via `setoption`. Without it, the engine falls back
to classical evaluation and reports why via `info string`; it does not fail
to start. See `networks/README.md` and `networks/PROVENANCE.md` for the
network's provenance, and `LICENSE` for its still-undecided redistribution
license.

## 9. Key UCI options

| Option | Default | Meaning |
|---|---:|---|
| `Hash` | 256 | transposition table size, MiB |
| `UseNNUE` | true | `false` uses the classical evaluator only |
| `EvalFile` | `firstnet_v5_10b.snnue` | network path, resolved next to the executable |
| `SEEPruning` | false | strict-SEE qsearch pruning; no strength evidence yet |
| `LIMBO` | false | bounded frontier verification extension; no strength evidence yet |

Full option and `go`-limit reference: `docs/UCI.md`.

## 10. Testing

```powershell
ctest --test-dir build-vs -C Release --output-on-failure
```

runs both `EunshinBishopQCoreTests` (perft, make/unmake, TT, MovePicker, SEE,
...) and `EunshinBishopQIntegrationTests` (evaluator, search, UCI). See
`docs/Testing.md` for what each covers and what is intentionally not yet
tested (time-control games, GCC/Clang builds).

Example of the kind of claim this project records instead of an unverified
Elo number:

```text
Kiwipete, depth 6, Threads 1, Hash 256 MiB, UseNNUE=true:
Q: bestmove e2a6, score -101 cp, 122609 nodes
v2.62 reference: bestmove e2a6, score -15 cp, 101323 nodes
(docs/Checkpoint3_Report.md)
```

## 11. Known limitations

- No Elo, paired-game, or search-equivalence claim for Q, in either
  direction relative to the frozen reference.
- Pure classical evaluation strengthening has not started
  (`docs/ClassicalEvalBacklog.md`).
- `SEEPruning=true` and `LIMBO=true` are correctness-tested (legal PV, no
  crash) but not strength-tested; their defaults should not change without
  paired-game evidence.
- Only MSVC has actually been built and tested; GCC/Clang builds are
  CI-scaffolded but unverified.
- Lazy SMP / multi-threaded search is not implemented (`Threads` is not a
  UCI option).
- `ponderhit`, `register`, and MultiPV are not implemented.
- The outbound source and network-weight licenses are undecided; see
  `LICENSE`.

## 12. License

**Undecided.** See `LICENSE` -- it is a placeholder, not a grant, and states
what the project owner still needs to decide before a public release.

## 13. Credits

- `Blucy1004` -- project owner and author of the frozen `SniperBishop`/
  `EunshinBishop` v2.62 reference this migration is built from
  (`reference/source`, `reference/README.md`).
- FIRST_NET v5 network training data: the project owner identifies its
  source as the public Lichess database, CC0-licensed
  (`networks/PROVENANCE.md`); the trained weight file's own outbound license
  is a separate, still-undecided question.
- Search and evaluation techniques reference commonly known chess
  programming ideas (staged move ordering, PVS, aspiration windows, SEE
  swap-list evaluation, NNUE-style residual correction); see
  `CONTRIBUTING.md` for the distinction this project draws between using a
  known technique and copying another project's specific implementation.

---

Internal migration notes (architecture, checkpoint reports, comparisons
against the frozen reference) live in `docs/`; the ones a third-party clone
needs are `docs/Build.md`, `docs/UCI.md`, and `docs/Testing.md`.
