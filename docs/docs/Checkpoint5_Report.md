# Checkpoint 5 test report

Date: 2026-07-19

Decision: **ACCEPT** for this checkpoint's scope only: specification
section 26 (debug/diagnostic UCI commands), section 28 (A/B testing command
reference), and section 27's direct-GCC build example. This is not a search,
evaluation, or pruning-behavior change of any kind, and not an Elo or
paired-game decision.

## Why this checkpoint exists

The user supplied the complete original Q Architecture specification
(sections 1-31) in full for the first time. Cross-checking it against the
already-implemented project (checkpoints 1-4, specification items 1-22)
found no gap in the *engine* itself -- the existing `SearchStack`,
`TimeManager`, `HistoryTables`, `ClassicalBreakdown`, `EvalResult`, and
`NNUE::AccumulatorCheck` types already matched section 3-22's design
closely (in several cases exactly, e.g. `ClassicalBreakdown`'s nine
components). The concrete gaps were all in the specified *tooling* that
exposes that already-correct internal state: section 26's debug commands
did not exist as UCI-reachable commands, section 28's A/B command lines were
never written down, and section 27's direct-compiler build example was
missing (only the CMake-driven one existed).

Per the handoff principles supplied with this request, items 1-22 and their
passing tests are treated as a frozen baseline; nothing in this checkpoint
touches evaluation, search conditions, or pruning values.

## Scope

| Item | Status |
|---|---|
| Section 26 debug commands | Implemented: `perft`, `eval`, `evaldetail`, `nnuecheck`, `nnueverify`, `see`, `key`, `checkboard` |
| Section 28 A/B testing | Documented: `docs/ABTesting.md` -- runnable `cutechess-cli` commands for all four comparison types, no results recorded (none have been run) |
| Section 27 GCC direct build | Documented: raw `g++` command line added to `docs/Build.md`, still unverified on a physical machine |

### Section 26: debug/diagnostic commands

New `Engine` methods (`src/engine/engine.h`/`.cpp`): `debugPerft`,
`debugEvaluate`, `debugClassicalBreakdown`, `debugVerifyAccumulator`. Each
follows the existing `controlMutex_` + `isSearching()` rejection discipline
`setOption`/`newGame` already use, rather than force-stopping an active
search to answer an unrelated diagnostic query. `perft()` and
`verifyAccumulator()` both pair every `doMove` with an `undoMove`
internally, so `position_` is unchanged once either returns.

New UCI commands (`src/uci/uci.cpp`), every line framed as `info string` so
a strict GUI parser can safely ignore them:

- `perft <depth>` (1-9): nodes, elapsed ms, nps.
- `eval`: one-line classical/NNUE/final summary.
- `evaldetail`: full `ClassicalBreakdown` printout (material, PSQT, pawns,
  mobility, king safety, threats, passed pawns, space, miscellaneous) plus
  the NNUE residual/absolute reconciliation, matching the specification's
  example format.
  - **Found and fixed during this checkpoint**: `ClassicalEvaluator::evaluate`
    (`src/eval/classical.cpp`, existing code, unchanged) adds a flat
    side-to-move tempo bonus directly in its `return` statement, outside
    every `ClassicalBreakdown` category. A first version of `evaldetail`
    printed the nine categories and "Classical STM" without accounting for
    it, so the categories silently did not sum to the total (e.g. startpos:
    all nine categories `+0 cp`, "Classical STM" `+10 cp`). Fixed by
    reconciling the difference into an explicit "Tempo/unmodeled" line
    computed generically (`Classical STM` minus the sum of displayed
    categories), so the printout stays honest even if the underlying
    constant changes later. This is a debug-output fix only; no evaluation
    value changed.
- `nnuecheck`: load status, path, payload SHA-256, generation.
- `nnueverify`: `NNUE::Network::verifyAccumulator` -- incremental vs.
  from-scratch accumulator agreement.
- `see <move>`: `See::see` / `seeGe(..., 0)` for a legal UCI move in the
  current position.
- `key`: position/pawn/material Zobrist keys in hex.
- `checkboard`: `Position::isConsistent()`, with the specific inconsistency
  reason if not.

Documented in `docs/UCI.md`'s new "Debug/diagnostic commands" section.

### Section 28: A/B testing

`docs/ABTesting.md` records the exact `cutechess-cli` command lines for all
four specified comparisons (architecture regression, classical
strengthening, residual effect, per-feature IIR/SEEPruning/AEGIS/LIMBO
isolation) under the specified shared conditions (`Threads=1`, matched
`Hash`, opening pairs, color reversal, `10+0.1`, 200-game initial batch,
>=1000 games before trusting a promising candidate, adjudication off,
crash/time-loss/illegal-move tracked separately). **No comparison has been
run; no result is recorded.** The classical-strengthening command is a
placeholder pending item 23 (`docs/ClassicalEvalBacklog.md`), since no
strengthened classical build exists yet to compare against.

### Section 27: direct GCC build

`docs/Build.md` gained a raw `g++ -std=c++17 ...` command line compiling
every `src/**/*.cpp` needed for `EunshinBishop` without CMake, distinct from
the existing CMake-driven Linux instructions. Same untested status as the
rest of the Linux path: no GCC or Clang has been run on a physical machine
for this project as of this checkpoint.

## Toolchain and commands

Same toolchain as prior checkpoints: Windows, MSVC 19.51.36248.0 x64,
Windows SDK 10.0.26100.0, CMake's `Visual Studio 18 2026` generator, C++17,
`/W4`, `/permissive-`.

```powershell
cmake --build build-vs --config Release --parallel
ctest --test-dir build-vs -C Release --output-on-failure

cmake --build build-vs --config Debug --parallel
ctest --test-dir build-vs -C Debug --output-on-failure
```

## Results

| Gate | Release | Debug |
|---|---:|---:|
| Compiler warnings | 0 observed (one `C4834` transiently introduced and fixed by dropping `[[nodiscard]]` from `debugClassicalBreakdown`, whose primary output is its out-parameter) | 0 observed |
| Core test checks | 519,122 PASS (unchanged from checkpoint 4: no core-test file was touched) | 519,122 PASS |
| Integration test checks | 70 PASS (+10 over checkpoint 4, all in the new `testDebugCommands`) | 70 PASS |
| CTest | 2/2 PASS | 2/2 PASS |

### NNUE-enabled evidence (not a silent fallback)

Per this checkpoint's explicit requirement to confirm the real network-load
path rather than accept a silent classical fallback: `testDebugCommands`
loads `networks/firstnet_v5_10b.snnue`
(SHA-256 `E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656`,
unchanged since checkpoint 2) and asserts on the `nnuecheck` output
containing `"network loaded"` (not a fallback message) and on `nnueverify`
reporting `"incremental matches scratch exactly"`. A manual UCI session
against the built `EunshinBishop.exe` confirmed the same:
`nnuecheck` reported `network loaded; NNUE loaded: ...firstnet_v5_10b.snnue
generation=1`, and `nnueverify` reported exact agreement with both the
white-relative and side-to-move-relative scratch recomputation.

### Regression check against checkpoint 4

No search, evaluation, or pruning code was touched in this checkpoint (only
new debug-command wiring, which is dead code on every path except the eight
new command names). Confirmed directly: a Kiwipete `go depth 6` run with
default options (`SEEPruning=false`, `LIMBO=false`) produced node counts
identical at every depth to every prior checkpoint's baseline
(70, 390, 6722, 10147, 48859, 122609 at depths 1-6), the same scores, the
same PVs, and the same best move `e2a6`. `perft 4` from the startpos debug
command reported the textbook value (197,281) exactly, and `perft 5`
reported 4,865,609, both matching `docs/TestReport.md`'s reference table.

## Artifacts

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Release core tests | see `reference/checkpoint5/SHA256SUMS` | `6F10E7A51A049F78917F5D3A1E93FE61DC7EBE9519010861A32257CC33CBE86F` |
| Release `EunshinBishop.exe` | see `reference/checkpoint5/SHA256SUMS` | `1B4BADB3D58BD685ABF7643D57BABD1CEB634997E57B1ED247050F477B84B834` |
| Release integration tests | see `reference/checkpoint5/SHA256SUMS` | `9FE5F46D90597067EA6BD2C3C2E90A7EAD2E1985E51BEA4778635D6E43082FDC` |
| Bundled FIRST_NET v5 | 12,585,424 | `E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656` (unchanged) |

Full listing including Debug binaries in `reference/checkpoint5/SHA256SUMS`.

## Deferred / out of scope for this checkpoint

- No cutechess-cli comparison from `docs/ABTesting.md` has actually been
  run; running one was explicitly out of scope per the handoff principle
  against enabling untested experiments without A/B evidence.
- Item 23 (pure classical strengthening) is still not started; the
  classical-strengthening A/B command in `docs/ABTesting.md` is a
  placeholder for when it is.
- GCC and Clang builds are still unverified on a physical machine.
- `SEEPruning=true` and `LIMBO=true` still have no A/B evidence and their
  defaults are unchanged.
- No GitHub tag, push, or release was made; item 31's scaffold is unchanged
  by this checkpoint beyond the two doc additions noted above.
