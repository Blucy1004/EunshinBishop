# UCI reference

EunshinBishop speaks standard UCI on stdin/stdout. This page documents the
options and `go` limits this checkpoint actually implements; it is generated
from `src/engine/options.cpp` and `src/search/time_manager.h`, not aspirational.

## Identity

```text
id name EunshinBishop 5.0.0-rc.1 Q
id author Blucy1004
```

The version string is centralized in `src/core/version.h` (`Version::idString()`)
and is identical across the UCI `id name` line, the `--version` CLI flag, and
the console `--help` banner.

## Options

| Name | Type | Default | Notes |
|---|---|---:|---|
| `Hash` | spin | 256 | MiB, 1-4096 |
| `MoveOverhead` | spin | 30 | ms, 0-5000 |
| `UseNNUE` | check | true | `false` uses the classical evaluator only, with effectively zero NNUE runtime cost |
| `EvalFile` | string | `firstnet_v5_10b.snnue` | resolved relative to the executable; a missing/unreadable file falls back to classical with an `info string` reason, never aborts |
| `NNUEOutputMode` | combo | `Residual` | `Residual` or `Absolute`; Q's default differs intentionally from the frozen reference's absolute blend, see `docs/Q0_Reference.md` |
| `ResidualScale` | spin | 100 | 0-200 |
| `ResidualGuard` | check | true | bounds how far the residual term may move the classical score |
| `AbsoluteBlend` | spin | 35 | 0-100; only consulted in `Absolute` mode |
| `IIR` | check | false | internal iterative reduction |
| `AEGIS` | check | false | instability-response search-safety policy; can only tighten an existing margin, never extend a line |
| `SEEPruning` | check | false | strict SEE-based qsearch pruning (specification item 20); **no A/B evidence exists for `true` yet -- do not change this default without paired-game data** |
| `LIMBO` | check | false | bounded frontier verification extension (specification item 19); same caveat as `SEEPruning` |

Names are matched ASCII case-insensitively. An invalid value leaves every
option unchanged and returns an error rather than silently coercing input.

## `go` limits

| Limit | Behavior |
|---|---|
| `wtime` / `btime` | remaining clock time for White/Black, ms |
| `winc` / `binc` | increment per move, ms |
| `movestogo` | moves remaining to the next time control; defaults to a bounded estimate (never so low it can consume the whole clock on one move) if omitted under a clock |
| `movetime` | fixed time for this move, ms; an explicit `0` is a valid immediate hard limit |
| `depth` | fixed search depth |
| `nodes` | fixed node budget |
| `infinite` | disable clock-derived limits; an explicit `depth`/`nodes` limit still applies |

`stop` and `quit` are handled asynchronously by the UCI frontend while a
search runs on its own thread; they never block command intake, and a
search never exceeds `TimeManager`'s hard deadline regardless of which other
options are enabled.

## Debug/diagnostic commands (specification section 26)

Not standard UCI; every response line is still framed as `info string` so a
strict GUI parser can safely ignore it. None of these auto-stop an active
search -- each rejects cleanly with an `info string ... failed: ...` line
instead of racing or force-stopping it.

| Command | Effect |
|---|---|
| `perft <depth>` | runs perft (depth 1-9) from the current position, reports nodes/time/nps |
| `eval` | one-line classical/NNUE/final score summary, side-to-move relative |
| `evaldetail` | full classical breakdown (material/PSQT/pawns/mobility/king safety/threats/passed pawns/space/miscellaneous, plus any unmodeled remainder) and the NNUE residual/absolute reconciliation |
| `nnuecheck` | network load status, path, payload SHA-256, generation |
| `nnueverify` | `NNUE::Network::verifyAccumulator`: incremental vs. from-scratch accumulator agreement |
| `see <move>` | `See::see` value and `seeGe(..., 0)` for a UCI move in the current position |
| `key` | position/pawn/material Zobrist keys, hex |
| `checkboard` | `Position::isConsistent()`; prints the specific inconsistency if not |

## Commands not implemented

`ponderhit`, `register`, and MultiPV are not implemented in this checkpoint.
An unrecognized command produces `info string unknown command: <text>` and is
otherwise ignored, rather than terminating the process.
