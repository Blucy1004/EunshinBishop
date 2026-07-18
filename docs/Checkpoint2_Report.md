# Checkpoint 2 test report

Date: 2026-07-18

Decision: **ACCEPT** for the checkpoint-2 structural increment only.

The accepted scope is the implementation and isolated verification of the
specification 7-12 foundation: `SearchStack`, worker-local state and histories,
the Engine/options ownership boundary, lazy staged move picking, and the
four-way clustered TT.  Full acceptance gates that require Evaluator,
PVS/qsearch, `Engine::go`, or TimeManager remain pending their numbered
implementation checkpoints.  This is not an Elo, NNUE-inference, fixed-depth
search-equivalence, release-candidate, or stable-release decision.

## Toolchain and commands

Both configurations used Visual Studio 2026 Developer Prompt 18.7.3, MSVC
19.51.36248.0 x64, Windows SDK 10.0.26100.0, CMake's `Visual Studio 18 2026`
generator, C++17, `/W4`, and `/permissive-`.

```powershell
cmake -S . -B build-vs -G "Visual Studio 18 2026" -A x64
cmake --build build-vs --config Release --parallel
ctest --test-dir build-vs -C Release --output-on-failure

cmake --build build-vs --config Debug --parallel
.\build-vs\Debug\EunshinBishopQCoreTests.exe
```

The Codex host process contained duplicate `Path`/`PATH` environment keys, so
the local automation removed the duplicate uppercase key before invoking
MSBuild.  This is specific to that host environment, not a project build step.
The Visual Studio generator's compiler tlog was inspected and contained the
changed headers.  It was selected because the installed Ninja/CMake pair
mis-decoded localized MSVC `/showIncludes` output and therefore did not provide
trustworthy incremental header dependencies.  All reported binaries came from
the fresh `build-vs` tree.

## Results

| Gate | Release | Debug |
|---|---:|---:|
| Configure/build | PASS | PASS |
| Compiler warnings | 0 observed | 0 observed |
| Test assertions | 519,108 PASS | 519,108 PASS |
| CTest | 1/1 PASS, 1.97 s | 1/1 PASS, 114.96 s |
| Internal debug assertions | compiled out | PASS |
| Process result | exit 0 | exit 0 |
| Observed test runtime | 3.4 s direct | 114.96 s through CTest |

The complete checkpoint-1 gates remain enabled, including startpos perft
through depth 5, canonical Kiwipete through depth 4, special-position perft,
and 102,400 deterministic make/unmake plies with exact state restoration.

## New covered invariants

- exact Q defaults: Hash 256 MiB, move overhead 30 ms, `UseNNUE=true`,
  `EvalFile=firstnet_v5_10b.snnue`, residual mode/scale 100/guard on,
  absolute blend 35, and experimental search options off;
- case-insensitive validated options, mutation-free rejection, idempotent
  revisioning, and legacy `NNUEBlend` mapping only to absolute blend;
- independent Worker histories/statistics/stop flags, persistent history across
  ordinary searches, new-game clearing, padded state/stack storage, and
  `VALUE_NONE` evaluation sentinels;
- explicit active-session rejection of overlapping search setup and concurrent
  Engine option, TT, position, or game mutation, with a cold-path control mutex
  closing the startup-check/resize TOCTOU window;
- rejection of search setup and even empty work before Engine initialization;
- transactional Engine FEN and move-list application;
- bounded main/capture history gravity, decay, and countermove round-trip;
- lazy MovePicker construction, deferred quiet generation after tactical
  stages, TT/killer/counter deduplication, stale-hint rejection, legal and
  complete main/evasion output, good/quiet/bad ordering, qsearch filtering, and
  discovered-check admission;
- 16-byte TT entries, four-entry 64-byte aligned clusters, power-of-two sizing,
  disabled mode, same-key update and move preservation, deep/exact protection,
  same-depth Exact/PV downgrade rejection, deterministic collision replacement,
  generation aging/wrap, hashfull, clear, and mate-distance normalization.

The reference-style approximate SEE remains ordering-only.  Strict pin/king/
en-passant/promotion SEE and all SEE pruning are still specification item 20;
`SEEPruning` therefore defaults to false and cannot consult this helper.

## Artifacts

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Release core tests | 136,192 | `1F0C1DC696E07ABB6405CAF35C1DA8B2DBD74DE9F84339DFD6822641BC7EC397` |
| Debug core tests | 414,720 | `8A48EA1C72EA1855FDFA594EC254DE8022AA2ABED5CB28A227F80BA0FB1F0FBB` |
| Bundled FIRST_NET v5 | 12,585,424 | `E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656` |

The network source copy and both post-build copies beside the Release and Debug
executables have the same hash.  The default CMake target also re-runs
`copy_if_different` on an otherwise up-to-date build so a network-only change
cannot leave a stale runtime copy.  The owner has authorized project inclusion;
the final public checkpoint must still record the outbound license granted for
the weight file.

## Deferred gates

- concrete Evaluator ownership/binding and safe network load/fallback;
- MG/EG classical evaluation, residual equation, guard, and accumulator
  scratch/incremental equality;
- templated PVS/qsearch, iterative deepening, fixed-depth bestmove/score/nodes/
  PV comparison, and the final `Engine::go` API;
- TimeManager, strict SEE, AEGIS/LIMBO policy integration, UCI, GCC/Clang,
  time controls, A/B matches, CI, packaging, and public release licensing.
