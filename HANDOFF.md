# EunshinBishop Q -- handoff status

Last updated: 2026-07-19. This file is the single source of truth for "what
is actually done, what is next" across sessions; `docs/CheckpointN_Report.md`
files are the frozen, dated evidence each summary below points to. Update
this file, not just the checkpoint reports, whenever the state changes.

## TOP PRIORITY: release-blocking regression, root-caused but not fixed

`docs/Checkpoint6_Report.md`'s small-scale A/B test (v2.62 reference vs Q,
24 games) found Q losing 21-1-2. This was escalated into a root-cause
investigation (`docs/Checkpoint7_Report.md`) rather than accepted as an
expected tuning gap. That investigation found and reproduced **two
independent, concrete causes**, fixed **neither** (explicit instruction:
find and reproduce before changing code):

1. **Move-generation/make-unmake throughput regression**: `perft 6` from
   startpos produces the identical, correct node count on both engines
   (119,060,324 -- not a correctness bug) but Q does it at ~6.8M nodes/sec
   versus the reference's ~19.2M nodes/sec, a ~2.8x gap, present before
   evaluation is ever called. This is the dominant cause -- it persists
   with NNUE fully disabled (a 12-game classical-only isolation match still
   went 9-0-3 to the reference).
2. **NNUE residual-formula sign flip**: at canonical Kiwipete, classical
   (+103 cp) and NNUE raw (-131 cp) are numerically identical between the
   two engines, but Q's "classical + 100% residual" formula (per
   specification item 14) flips the final sign (-28 cp) relative to the
   reference's 35%-blend (+42 cp). `ResidualGuard` does not intervene
   because its knee (400 cp at this phase) is well above this 131 cp
   correction -- the guard is working as designed; the question this raises
   (unanswered) is whether FIRST_NET v5's raw output is actually calibrated
   as a small residual delta in the first place.

**Next session's first job is addressing these, not further release
scaffolding.** See `docs/Checkpoint7_Report.md`'s "Recommended next steps"
for where to start (profile `perft` to localize Finding 1's specific
function; check whether a smaller `ResidualScale` or `Absolute` mode
resolves Finding 2 across a larger FEN sample before concluding the
residual premise itself is wrong). Do not run a full 200-game
`docs/ABTesting.md` batch until both are addressed -- it will not produce a
different conclusion at the current gap size.

## Judgment (specification coverage)

The complete original Q Architecture specification (sections 1-31) has now
been supplied and cross-checked against the implementation. Sections 1-22
(core types through TimeManager/History) are implemented and independently
verified; in several places the existing code already matched the spec's
illustrative snippets closely (e.g. `ClassicalBreakdown`'s exact nine
components), confirming earlier sessions built to this same spec even before
it arrived in full. Section 23 (pure classical strengthening) is
deliberately not started -- it is a long-running, paired-game-gated program
by its own stated rules, not a code task. Sections 24-25 (porting-stage
narrative, Q0-Q5; test-plan narrative) describe work already covered by
checkpoints 1-4 and `docs/Testing.md`. Sections 26-28 (debug commands,
CMake/GCC/Clang build, A/B testing structure) had real gaps, implemented/
documented in checkpoint 5, and then actually *used* in checkpoints 6-7,
which is what surfaced the regression above. Section 31 (GitHub release
readiness) is scaffolded but explicitly incomplete, and now additionally
blocked on the regression above, not just on GCC/Clang/licensing.

## Completed stages

| Items | Scope | Report |
|---|---|---|
| 1-6 | frozen reference, core types, `Move`, `Position`/`StateInfo`, FEN, movegen | `docs/TestReport.md` |
| 7-12 | `SearchStack`, Worker histories/state, `Engine` ownership, `MovePicker`, clustered TT | `docs/Checkpoint2_Report.md` |
| 13-18 | classical + FIRST_NET v5 residual evaluator, PVS/qsearch/aspiration/AEGIS, `TimeManager`, `Engine::go`, async UCI | `docs/Checkpoint3_Report.md` |
| 19 | LIMBO bounded frontier verification (`src/search/limbo.*`) | `docs/Checkpoint4_Report.md` |
| 20 | independent strict SEE (`src/search/see.*`), wired into `SEEPruning` | `docs/Checkpoint4_Report.md` |
| 21 | `TimeManager` re-verified against the item-21 spec text; already satisfied, no change | `docs/Checkpoint4_Report.md` |
| 22 | `HistoryTables` re-verified against the item-22 spec text; already satisfied, no change | `docs/Checkpoint4_Report.md` |
| 26 | debug/diagnostic UCI commands (`perft`, `eval`, `evaldetail`, `nnuecheck`, `nnueverify`, `see`, `key`, `checkboard`) | `docs/Checkpoint5_Report.md` |
| 27 (partial) | direct-GCC build example documented; MSVC re-verified clean twice (checkpoints 5, 6); GCC/Clang still never actually run | `docs/Checkpoint5_Report.md`, `docs/Checkpoint6_Report.md` |
| 28 (partial) | A/B testing commands documented and **used**; first real run found the regression above | `docs/ABTesting.md`, `docs/ABResults.md`, `docs/Checkpoint6_Report.md`, `docs/Checkpoint7_Report.md` |

## Changed files (this migration's history, by commit)

```text
857c8ff  Import checkpoint-3 clean handoff source
dd57b6e  src/position/position.cpp, .gitignore -- GCC enum/non-enum fix
72c1ef6  src/search/{see,limbo}.{h,cpp}, search.cpp, engine.cpp, uci.cpp,
         CMakeLists.txt, tests/{core,integration}_tests.cpp -- items 19/20
387e8f1  docs/Checkpoint3_Report.md, reference/README.md,
         reference/checkpoint3/** -- checkpoint-3 closeout
52aaf9c  docs/Checkpoint4_Report.md, docs/ClassicalEvalBacklog.md,
         reference/checkpoint4/** -- checkpoint-4 report/archive
8839961  README.md, docs/Architecture.md, docs/Migration.md -- doc sync
c6f4c1c  src/core/version.h, src/main.cpp, src/uci/uci.cpp -- Version/--version/--help
13eff33  .github/workflows/*, LICENSE, CONTRIBUTING.md, SECURITY.md,
         CHANGELOG.md, docs/{Build,UCI,Testing}.md, README.md -- item-31 scaffold
8df57a2  docs/ReleaseNotes_v5.0.0-rc.1_draft.md -- item-31 draft deliverables
4dc285c  HANDOFF.md -- first handoff snapshot
fdfcbc6  src/engine/engine.{h,cpp}, src/uci/uci.cpp, docs/ABTesting.md,
         docs/Build.md, docs/UCI.md, docs/Checkpoint5_Report.md,
         tests/integration_tests.cpp -- section 26 debug commands,
         section 27/28 documentation
2a51190  .github/workflows/build.yml (no-network CI fix),
         docs/{ABResults,CIVerificationChecklist,Checkpoint6_Report}.md,
         reference/ab_tests/checkpoint6_architecture_regression/** --
         MSVC re-verification, CI audit+fix, and the A/B test that found
         the regression above (evidence preserved before investigating)
(uncommitted at time of writing -- commit before ending the session)
         docs/Checkpoint7_Report.md, docs/{ABResults,Checkpoint6_Report,
         Build}.md updates, reference/ab_tests/checkpoint7_root_cause/** --
         the root-cause investigation itself
```

## Test results (last full run, commit `fdfcbc6`; unchanged by checkpoints 6-7, which added no source changes)

Windows, MSVC 19.51.36248.0 x64, `Visual Studio 18 2026` generator:

| Gate | Release | Debug |
|---|---:|---:|
| Compiler warnings | 0 | 0 |
| Core test checks | 519,122 PASS | 519,122 PASS |
| Integration test checks | 70 PASS | 70 PASS |
| CTest | 2/2 PASS | 2/2 PASS |

Re-confirmed via a from-scratch clean rebuild in checkpoint 6 (same
counts, different binary hashes -- MSVC output is not byte-reproducible
across separate builds, see `docs/Checkpoint6_Report.md`).

NNUE-enabled path was exercised, not silently skipped: `testDebugCommands`
loads `networks/firstnet_v5_10b.snnue`
(SHA-256 `E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656`,
unchanged since checkpoint 2) and asserts on `nnuecheck` reporting
`"network loaded"` and `nnueverify` reporting exact incremental/scratch
agreement.

## Risks / things discovered worth knowing about

- **See "TOP PRIORITY" above for the regression -- this is now the most
  important entry in this section, not just another bullet.**
- The frozen `reference/bin/EunshinBishop_v2.62_reference.exe` looks for its
  `EvalFile` next to itself, not next to the caller's working directory.
  Without a colocated `.snnue` copy it silently falls back to classical and
  produces very different fixed-depth numbers. A copy now lives at
  `reference/bin/firstnet_v5_10b.snnue`; see `reference/README.md`.
- The reference console/UCI dispatcher already has `eval`, `d`, `perft`,
  `nnuecheck`, `nnueverify` commands (`reference/source/...cpp` around line
  4284+) -- section 26's debug commands were very likely modeled on these,
  which is why they were directly comparable for the Checkpoint 7
  investigation.
- Piping UCI commands into the engine from Windows PowerShell corrupts the
  very first byte of stdin with a stray BOM-like character on some
  invocation paths, regardless of encoding settings tried. This is a
  test-harness artifact (reproducible with a throwaway first line), not an
  engine defect.
- `LIMBO`'s "bounded re-search" design variant (re-verify a move's result
  after the fact, rather than extending inline before descending) is not
  implemented; only the inline-extension variant is.
  `SearchStatistics::limboBoundedResearches` stays at 0 by design, not a bug.
- **Found while building `evaldetail`**: `ClassicalEvaluator::evaluate`
  (`src/eval/classical.cpp`, pre-existing code, unmodified) adds a flat
  side-to-move tempo bonus directly in its `return` statement
  (`... + 10`), outside every `ClassicalBreakdown` category. Fixed in the
  *debug output only* (an explicit "Tempo/unmodeled" reconciling line); no
  evaluation value anywhere changed.
- `cutechess-cli` 1.5.1's `-debug` flag errors ("Warning: Empty value for
  option \"-debug\"") in this build/invocation; NNUE-load confirmation for
  A/B matches was done via a separate pre-batch interactive check instead
  (see `reference/ab_tests/checkpoint6_architecture_regression/MANIFEST.md`).
- `git worktree` builds under a deeply nested Temp scratch path can fail
  MSVC's `FileTracker` (`FTK1011`, path-length related) during CMake's
  compiler-detection `TryCompile` step specifically (not the main build).
  Use a short path (e.g. `C:\something`) for any future isolated worktree
  builds.

## Incomplete stages

- **23** (pure classical evaluation strengthening): not started by design;
  see `docs/ClassicalEvalBacklog.md`.
- **27** (build): GCC/Clang still never actually run on a physical machine
  (explicit user decision not to install locally; will run via
  `.github/workflows/build.yml` after a future push).
- **28** (A/B testing): the architecture-regression comparison has now been
  run (small-scale) and found the blocking regression above. The other
  three comparison types in `docs/ABTesting.md` (classical strengthening,
  residual effect, per-feature IIR/SEEPruning/AEGIS/LIMBO) have not been
  run at all.
- **31** (GitHub release readiness): blocked on the regression above in
  addition to the previously-known gaps (GCC/Clang, licensing, independent
  command re-verification, real release-asset checksums). No tag pushed,
  no GitHub Release created.
- No A/B or paired-game evidence exists for `SEEPruning=true` or
  `LIMBO=true`; their defaults must not change without it.

## Next starting point

1. **Fix or scope-plan Checkpoint 7's Findings 1 and 3 first.** Do not
   proceed to GCC/Clang builds, full-scale A/B batches, or further item-31
   work before this. Concretely:
   - Profile (not more A/B games) `perft` to localize the move-generation/
     make-unmake throughput gap to a specific function or data structure.
   - For the residual formula: check whether a smaller `ResidualScale` (or
     `NNUEOutputMode=Absolute`) keeps Q's evaluation sign-consistent with
     the reference's blend across a larger, systematic FEN sample -- not
     just the two positions checked in checkpoint 7 -- before concluding
     the residual premise itself (not just its scale) needs to change.
2. Only after both are independently addressed: re-run
   `docs/ABTesting.md`'s architecture-regression comparison at a larger
   scale to measure what improved.
3. GCC/Clang builds and full item-31 release work remain valuable but are
   no longer the highest-priority next step -- they were superseded by the
   regression above.
4. If continuing item 23 instead of the regression: pick exactly one group
   from `docs/ClassicalEvalBacklog.md`'s priority list, add it behind its
   own experiment flag, and do not touch a second group in the same change.
5. Do not push a `v*` tag or create a GitHub remote/release until the
   regression above is resolved and `docs/Build.md`'s RC-relevant checklist
   rows are checked with evidence linked from a checkpoint report.
