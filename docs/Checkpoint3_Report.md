# Checkpoint 3 test report

Date: 2026-07-19

Decision: **ACCEPT** for the checkpoint-3 scope only (specification items
13-18: classical evaluator, FIRST_NET v5 residual evaluator, NNUE
loader/inference, incremental accumulator, residual guard, iterative
deepening, aspiration windows, PVS/alpha-beta, qsearch, PV preservation,
AEGIS, `Engine::go`, `TimeManager`, and the UCI frontend with asynchronous
`stop` handling). This is not an Elo, fixed-depth search-equivalence, or
public-release decision, and it does not cover specification items 19-31.

## What changed since the handoff

The handoff (`EunshinBishop_Q_13-18_handoff_status.md`) reported items 13-18
implemented and independently verified on a clean Linux/CMake Release build,
with one open defect: a GCC `-Wenum-compare`-class warning ("enumeral and
non-enumeral type in conditional expression") in
`Position::castlingRights()`, from `return state_ ? state_->castlingRights :
NoCastling;` mixing a `std::uint8_t` branch with a `CastlingRight` enum
branch. That is fixed here by casting the enum branch to `std::uint8_t`,
matching the style already used everywhere else in that file. This is the
only source change in checkpoint-3 scope; commit `dd57b6e` is the fix,
`857c8ff` is the unmodified import.

## Toolchain and commands

Independently re-verified on a **second** toolchain/OS from the handoff's
Linux/CMake run, to cross-check the reported numbers rather than trust a
single build: Windows, MSVC 19.51.36248.0 x64, Windows SDK 10.0.26100.0,
CMake's `Visual Studio 18 2026` generator, C++17, `/W4`, `/permissive-`.

```powershell
cmake -S . -B build-vs -A x64
cmake --build build-vs --config Release --parallel
ctest --test-dir build-vs -C Release --output-on-failure

cmake --build build-vs --config Debug --parallel
ctest --test-dir build-vs -C Debug --output-on-failure
```

## Results

| Gate | Release | Debug |
|---|---:|---:|
| Configure/build | PASS | PASS |
| Compiler warnings | 0 observed | 0 observed |
| Core test assertions | 519,108 PASS | 519,108 PASS |
| Integration test assertions | 58 PASS | 58 PASS |
| CTest | 2/2 PASS, 3.24 s | 2/2 PASS, 128.64 s |
| UCI smoke (`uci`/`isready`/`position startpos`/`go depth 4`) | `bestmove e2e4 ponder e7e6` | not re-run |

The core and integration counts (519,108 / 58) match the handoff's
independently reported Linux numbers exactly, reproduced here on Windows.

## New covered invariants (beyond checkpoint 2)

- Classical MG/EG evaluator and FIRST_NET v5 residual-correction NNUE
  evaluator, selected by `NNUEOutputMode`, with `ResidualGuard` bounding how
  far the residual term may move the classical score;
- NNUE loader safe-failure: a missing or malformed `.snnue` file falls back
  to classical evaluation with an explicit `info string` reason rather than
  aborting the engine;
- incremental accumulator updates cross-checked against full scratch
  recomputation;
- iterative deepening with aspiration windows from depth 5, PVS with null
  move, LMR, IIR, futility pruning, and AEGIS-adjusted margins;
- `TimeManager` optimum/maximum budgeting, emergency mode, and
  iteration-stability tracking (best-move/score stability, root move
  changes, aspiration failures);
- `Engine::go` as the single synchronous search entry point, with the UCI
  frontend running it on a worker thread and handling `stop`/`quit`
  asynchronously without blocking command intake;
- `SEEPruning` and `LIMBO` remain specification items 20 and 19: this
  checkpoint keeps both options rejected/inert as designed. (Both are now
  implemented as of the follow-on checkpoint recorded in
  `Checkpoint4_Report.md`; this report intentionally describes only the
  state independently verified as checkpoint 3.)

## Fixed-depth comparison against the frozen v2.62 reference

Both engines run with `UseNNUE=true` and the bundled FIRST_NET v5 network
loaded (`E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656`),
`Threads=1`, default `Hash`, from `ucinewgame`, on the canonical Kiwipete FEN:

```text
r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
```

**v2.62 reference** (reproduces `docs/Q0_Reference.md` exactly):

| Depth | Best move | Score | Nodes | Time |
|---:|---|---:|---:|---:|
| 6 | `e2a6` | -15 cp | 101,323 | 171 ms |
| 8 | `e2a6` | -15 cp | 2,649,523 | 4,688 ms |
| 10 | `e2a6` | -19 cp | 7,122,135 | 13,815 ms |

**Q (checkpoint 3/4, all defaults, `SEEPruning=false`, `LIMBO=false`)**:

| Depth | Best move | Score | Nodes | Time |
|---:|---|---:|---:|---:|
| 6 | `e2a6` | -101 cp | 122,609 | 766 ms |
| 8 | `d5e6` | -90 cp | 2,350,137 | ~17.0 s |
| 10 | `d5e6` | -93 cp | 37,157,259 | ~249 s |

**Reading this table**: depth 6 agrees on the best move; depths 8 and 10
diverge (`e2a6` vs `d5e6`) and the scores differ by 70-75 cp. This is
**expected, not a regression**, for two compounding reasons that
`docs/Q0_Reference.md` already flags: (1) Q's search algorithm (PVS +
aspiration windows + a four-way clustered TT + LMR/null-move/IIR/AEGIS) is
structurally different from the reference's, so the two engines explore
different trees at the same nominal depth; and (2) Q evaluates with the
residual-correction NNUE equation described in `docs/Q0_Reference.md`
("Q will keep NNUE enabled by default but will later replace the
absolute-style blend with the explicitly requested residual-correction
equation"), not the reference's absolute blend, so even a shared position
can score differently by design. Neither engine's move is illegal or
nonsensical, and per-node throughput is markedly lower for Q (NNUE
inference is not yet a hot-path optimization target in this checkpoint;
Q reaches ~150k nps at depth 10 versus the reference's ~515k nps). This
table is evidence about the current gap, not a claim of Q-equivalence or of
either engine's relative strength -- that requires the paired-game
regression testing item 31 describes, which has not been run.

## Artifacts

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Checkpoint-3 source archive | see `reference/checkpoint3/SHA256SUMS` | `B4F80B594273FE210BDDA01E3BF60615A96D09B968C0EDCF7F80F35E75C05AAC` |
| Release core tests | 208,384 | `8A790625FD74AA735C8CF2E22DDF2C532B253F39C9F1E5795B6A95A429453102` |
| Debug core tests | 1,396,224 | `929FB6BEFD7EF6AEB06F45453950181D4A08473A48F49F4202F25436239D9A14` |
| Release `EunshinBishop.exe` | 212,992 | `AABB6050FD5F32224A3970B329F900CA90942CCCBE656305445E666CE8D4B39D` |
| Debug `EunshinBishop.exe` | 1,337,344 | `2B12600D11B03414524626C284EA713E40850CC31FCE5203FF49BD078BA88C55` |
| Release integration tests | 254,464 | `A07819323BF067C910E8698137099C0F5C52CB2E90CE446CE89FC7D1BCF19AA8` |
| Debug integration tests | 1,426,432 | `EDC81D0A658FF31EFC3817CF6CADA59E6DD42D8F3DB7E4FFC5473AE38A4C8A2D` |
| Bundled FIRST_NET v5 | 12,585,424 | `E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656` (unchanged from checkpoint 2) |

Full listing in `reference/checkpoint3/SHA256SUMS`.

## Deferred gates

Strict SEE (item 20) and LIMBO (item 19) are implemented in the follow-on
checkpoint documented in `Checkpoint4_Report.md`, not in this one. Still
outstanding after that checkpoint: History-structure item 22 was already
satisfied by the checkpoint-2 `HistoryTables` design (confirmed, no change
needed); pure classical evaluation strengthening (item 23) is tracked as a
backlog in `ClassicalEvalBacklog.md` and is explicitly not started; GCC and
Clang builds have still not been run on this machine (only MSVC is verified
here); no A/B or paired-game regression testing has been performed; and the
GitHub public-release readiness work (item 31) is scaffolded separately and
explicitly marked incomplete where the underlying gates above are not met.
