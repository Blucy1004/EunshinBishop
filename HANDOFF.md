# EunshinBishop Q -- handoff status

Last updated: 2026-07-19. This file is the single source of truth for "what
is actually done, what is next" across sessions; `docs/CheckpointN_Report.md`
files are the frozen, dated evidence each summary below points to. Update
this file, not just the checkpoint reports, whenever the state changes.

## Judgment

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
checkpoints 1-4 and `docs/Testing.md`; no new gap found there. Sections
26-28 (debug commands, debug output format, CMake/GCC/Clang build,
A/B testing structure) had real gaps and are now implemented/documented
(checkpoint 5). Section 29 (deliverables) and 30 (success criteria) are
satisfied by what already exists. Section 31 (GitHub release readiness) is
scaffolded but explicitly incomplete.

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
| 27 (partial) | direct-GCC build example documented (still unverified on a physical machine) | `docs/Checkpoint5_Report.md`, `docs/Build.md` |
| 28 | A/B testing (`cutechess-cli`) command reference for all four comparison types -- documented, **none run** | `docs/Checkpoint5_Report.md`, `docs/ABTesting.md` |

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
```

## Test results (last full run, commit `fdfcbc6`)

Windows, MSVC 19.51.36248.0 x64, `Visual Studio 18 2026` generator:

| Gate | Release | Debug |
|---|---:|---:|
| Compiler warnings | 0 | 0 |
| Core test checks | 519,122 PASS | 519,122 PASS |
| Integration test checks | 70 PASS (+10 over checkpoint 4, all in `testDebugCommands`) | 70 PASS |
| CTest | 2/2 PASS | 2/2 PASS |

NNUE-enabled path was exercised, not silently skipped: `testDebugCommands`
loads `networks/firstnet_v5_10b.snnue`
(SHA-256 `E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656`,
unchanged since checkpoint 2) and asserts on `nnuecheck` reporting
`"network loaded"` and `nnueverify` reporting exact incremental/scratch
agreement -- not a silent classical fallback. A manual UCI session against
the built `EunshinBishop.exe` confirmed the same interactively.

Regression check: with `SEEPruning=false`/`LIMBO=false` (both still
default, and no search/eval code touched in this checkpoint), Kiwipete
`go depth 6` node counts are byte-for-byte identical to every prior
checkpoint's baseline (70, 390, 6722, 10147, 48859, 122609). `perft 4` from
startpos via the new debug command reports the textbook 197,281 exactly.

## Risks / things discovered worth knowing about

- The frozen `reference/bin/EunshinBishop_v2.62_reference.exe` looks for its
  `EvalFile` next to itself, not next to the caller's working directory.
  Without a colocated `.snnue` copy it silently falls back to classical and
  produces very different fixed-depth numbers. A copy now lives at
  `reference/bin/firstnet_v5_10b.snnue`; see `reference/README.md`.
- Q's own NNUE inference throughput is low (~150k nps at Kiwipete depth 10
  vs. the reference's ~515k nps); depth-10 fixed-depth runs took ~4 minutes.
  Not a correctness bug, but worth knowing before scripting deeper
  comparisons or paired games.
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
  (`... + 10`), outside every `ClassicalBreakdown` category. This means the
  nine displayed breakdown categories never summed to the evaluator's actual
  return value on their own. Fixed in the *debug output only* by computing
  and displaying the residual as an explicit "Tempo/unmodeled" line; no
  evaluation value anywhere changed. Worth knowing if item 23 work later
  restructures `classical.cpp`.

## Incomplete stages

- **23** (pure classical evaluation strengthening): not started by design;
  see `docs/ClassicalEvalBacklog.md` for the priority list and the
  one-group-per-commit-with-paired-games discipline it must follow.
- **27** (build): CMake path and a direct-`g++` command line are both
  documented; neither GCC nor Clang has actually been run on a physical
  machine for this project. `.github/workflows/build.yml` targets both but
  has never executed (no CI has run).
- **28** (A/B testing): all four comparison command sets are documented in
  `docs/ABTesting.md`; **none has been run**. No `docs/ABResults.md` exists
  because there is no result to record yet.
- **31** (GitHub release readiness): scaffolded (`.github/workflows/`,
  `LICENSE`, `CONTRIBUTING.md`, `SECURITY.md`, `CHANGELOG.md`,
  `docs/{Build,UCI,Testing,ABTesting}.md`,
  `docs/ReleaseNotes_v5.0.0-rc.1_draft.md`, `Version` namespace /
  `--version` / `--help`), but explicitly **not** complete or release-ready.
  Unchecked items in `docs/Build.md`'s checklist: GCC build, Clang build,
  residual-equation independent verification, time-loss root-cause
  investigation, Cute Chess regression, independent command re-verification,
  license confirmation (both source and network weights), and real
  release-asset checksums. No tag has been pushed and no GitHub Release has
  been created; the workflows are local files only.
- No A/B or paired-game evidence exists for `SEEPruning=true` or
  `LIMBO=true`; their defaults must not change without it.

## Next starting point

Highest-value next steps, in the order they unblock the most:

1. An actual GCC and/or Clang build (this machine has neither installed).
   This unblocks the `27`/`31` checklist rows and is a prerequisite for even
   the RC tag, not just stable.
2. A real `cutechess-cli` paired-game run using `docs/ABTesting.md`'s
   commands -- starting with the architecture-regression comparison (Q vs.
   the frozen v2.62 reference), since that has no missing prerequisite
   (unlike the classical-strengthening comparison, which needs item 23
   first). Record the result in a new `docs/ABResults.md`, not by editing
   `docs/ABTesting.md`.
3. If continuing item 23 instead: pick exactly one group from
   `docs/ClassicalEvalBacklog.md`'s priority list, add it behind its own
   experiment flag, and do not touch a second group in the same change.
4. Do not push a `v*` tag or create a GitHub remote/release until
   `docs/Build.md`'s RC-relevant checklist rows are checked with evidence
   linked from a checkpoint report.
