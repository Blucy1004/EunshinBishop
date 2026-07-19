# Testing

## Test binaries

| Binary | Source | What it covers |
|---|---|---|
| `EunshinBishopQCoreTests` | `tests/core_tests.cpp` | types/moves, attack tables, perft, special moves, illegal-move/FEN rejection, null move/repetition/50-move, 102,400 randomized make/unmake plies, independent-position isolation, `EngineOptions`/`SearchConfig`, `TranspositionTable`, `MovePicker`, `See`, `Engine` ownership boundary |
| `EunshinBishopQIntegrationTests` | `tests/integration_tests.cpp` | classical/NNUE evaluator selection and fallback, accumulator scratch-vs-incremental agreement, `Engine::go` fixed-depth search, `TimeManager`, AEGIS, `SEEPruning`/`LIMBO` end-to-end, UCI protocol parsing |

Run both through CTest, or directly:

```powershell
.\build-vs\Release\EunshinBishopQCoreTests.exe
.\build-vs\Release\EunshinBishopQIntegrationTests.exe .\networks\firstnet_v5_10b.snnue
```

The integration test binary takes the network path as an argument so it can
run with or without a real `.snnue` file present.

## Correctness gates every change must pass

```text
startpos perft(1..5) = 20, 400, 8902, 197281, 4865609
Kiwipete perft(1..3) = 48, 2039, 97862
```

plus make/unmake board and Zobrist restoration, en passant, castling and
attacked-transit-square legality, all four promotion kinds, repetition, the
50-move draw, king-capture rejection, illegal TT/PV fallback, and null-move
restoration -- all exercised inside `core_tests.cpp`, not left to manual
verification.

For NNUE: malformed-file rejection, safe load failure, repeated load/unload,
scratch inference, exact incremental-vs-scratch agreement, and king-bucket
transitions -- exercised inside `integration_tests.cpp`.

If any of these fail, stop; do not run time-control or paired-game testing
on top of a broken correctness gate.

## UCI smoke test

```text
uci
isready
position startpos
go depth 4
quit
```

expects `uciok`, then `readyok`, then a `bestmove` line reflecting a legal,
sane move. This is exercised by both `.github/workflows/build.yml` (see
below) and manually before every checkpoint report.

## What is not yet tested

- No time-control games (Cute Chess or otherwise) have been run; see the
  pre-release checklist in `docs/Build.md`.
- No GCC or Clang build has been exercised on a physical machine, only the
  commands CI is expected to run.
- `SEEPruning=true` and `LIMBO=true` are exercised for correctness (still a
  legal PV, no crash) but not for strength; see `docs/Checkpoint4_Report.md`.

## CI

`.github/workflows/build.yml` runs, on every pull request and `main` push:
Windows MSVC / Ubuntu GCC / Ubuntu Clang matrix builds, both test binaries,
a perft smoke check, and a UCI smoke check, all with `UseNNUE=false` so the
matrix does not depend on the 12 MB network binary being present. See
`docs/Build.md` for the exact commands and current pass/fail status per
platform.
