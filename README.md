# EunshinBishop Phase 5 Q

This directory is the isolated Q Architecture migration of the supplied
single-file EunshinBishop engine.  The reference source is preserved byte for
byte under `reference/source`; it is never edited during the migration.

## Current checkpoint

Checkpoint 2 (specification items 7-12: node-local `SearchStack`,
worker-owned histories/state/stats, the `Engine` ownership boundary,
validated immutable option snapshots, a genuinely lazy staged `MovePicker`,
and a four-way clustered transposition table) and checkpoint 3
(specification items 13-18: classical and FIRST_NET v5 residual-correction
`Evaluator`, incremental accumulator, PVS/alpha-beta with aspiration windows,
qsearch, AEGIS, `TimeManager`, `Engine::go`, and the asynchronous UCI
frontend) are both implemented and independently re-verified; see
`docs/Checkpoint2_Report.md` and `docs/Checkpoint3_Report.md`.

Checkpoint 4 adds specification item 20 (an independent, pin/king-legality/
en-passant/promotion-correct strict SEE module, `src/search/see.*`, wired
into `SEEPruning`) and specification item 19 (`LIMBO`, a bounded frontier
verification policy, `src/search/limbo.*`). Both options default to `false`
and change nothing in the default search path; see `docs/Checkpoint4_Report.md`.

A standalone `EunshinBishop.exe` now exists and is the target UCI engine
binary. Q uses the supplied FIRST_NET v5 as a residual-correction network.
The implemented option defaults are `UseNNUE=true`, residual mode, and
`EvalFile=firstnet_v5_10b.snnue`; that policy is isolated from the workspace's
v2.8 experimental lineage. The exact network is bundled under `networks/`
after the user confirmed that it is their own trained artifact, produced from
the public Lichess database they identify as CC0. The provenance record
separates that training-data statement from the still-to-be-declared weight
license.

Not yet done, in order: specification item 23 (pure classical evaluation
strengthening -- tracked, not started, see `docs/ClassicalEvalBacklog.md`),
any specification items 24-30 (no recorded text in this tree yet), GCC/Clang
builds, and any A/B or paired-game evidence for `SEEPruning=true` or
`LIMBO=true`. Specification item 31 (GitHub public-release readiness) is
scaffolded under `.github/`, `docs/Build.md`, `docs/UCI.md`,
`docs/Testing.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, and `SECURITY.md`, and
is explicitly **not** a release-ready claim -- see the checklist in
`docs/Build.md` for what is still missing.

## Build this checkpoint

Open an **x64 Developer PowerShell for Visual Studio**, then run the tested
multi-configuration MSBuild path:

```powershell
cmake -S . -B build-vs -A x64
cmake --build build-vs --config Release --parallel
ctest --test-dir build-vs -C Release --output-on-failure
```

Run the verification executables directly from the generated directory:

```powershell
.\build-vs\Release\EunshinBishopQCoreTests.exe
.\build-vs\Release\EunshinBishopQIntegrationTests.exe .\networks\firstnet_v5_10b.snnue
.\build-vs\Release\EunshinBishop.exe
```

The bundled network is copied beside each built executable (including
`EunshinBishop.exe` itself) and checked by hash; a missing or unreadable
network falls back to classical evaluation with an explicit `info string`
reason instead of failing to start.

The authoritative architecture and migration notes are in
`docs/Architecture.md`, `docs/Migration.md`, `docs/Q0_Reference.md`,
`docs/TestReport.md`, `docs/Checkpoint2_Report.md`,
`docs/Checkpoint3_Report.md`, `docs/Checkpoint4_Report.md`, and
`docs/ClassicalEvalBacklog.md`. Build, UCI, and testing instructions aimed at
a third-party clone (rather than this migration's own working notes) are in
`docs/Build.md`, `docs/UCI.md`, and `docs/Testing.md`.

## Status and claims

This is a development checkpoint, not `v5.0.0-rc.1`. No Elo, fixed-depth
search-equivalence, NNUE-inference, or public-release claim is made. The
reference engine's observed results are evidence about the reference artifact
only, not about Q's relative strength. `docs/Checkpoint3_Report.md` records a
fixed-depth comparison between Q and the reference at Kiwipete depths 6/8/10:
the two searches diverge past depth 6 by design (a different search algorithm
and a different, residual-correction NNUE equation), which is expected
evidence about the current gap, not a strength or equivalence claim.
