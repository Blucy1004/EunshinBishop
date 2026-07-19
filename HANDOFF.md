# EunshinBishop Q -- handoff status

Last updated: 2026-07-19. This file is the single source of truth for "what
is actually done, what is next" across sessions; `docs/CheckpointN_Report.md`
files are the frozen, dated evidence each summary below points to. Update
this file, not just the checkpoint reports, whenever the state changes.

## TOP PRIORITY: one Checkpoint-7 finding fixed, one reframed -- neither fully closed

`docs/Checkpoint7_Report.md` found two independent, concrete causes behind
the 21-1-2 regression from `docs/Checkpoint6_Report.md`. `docs/Checkpoint8_Report.md`
addressed both, in the scope explicitly requested (perf fix + read-only
NNUE audit, not a full re-match):

1. **Move-generation/make-unmake throughput regression -- fixed, not fully
   closed.** `Position::doMove` opened with a redundant
   `newState = StateInfo{}` that zero-initialized a 2048-byte NNUE
   accumulator array immediately before overwriting it with a real copy two
   statements later -- every field of `StateInfo` was explicitly assigned
   afterward regardless, making the zero-init pure waste. Removing it
   (`src/position/position.cpp`) cut the gap from ~1.9-2.0x to ~1.3x
   (perft 6 nps: reference ~14.8M, Q ~7.6M before -> ~11.5M after; node
   count unchanged at 119,060,324, Release+Debug CTest and
   `assert(isConsistent())` all pass). The remaining ~1.3x is not
   decomposed yet -- see `docs/Checkpoint8_Report.md`'s "Remaining gap".
2. **NNUE residual-formula "sign flip" -- reframed, not a bug in Q.** A
   read-only audit of the FIRST_NET v5 training/conversion scripts
   (`v5_residual_datafactory.py`, `sniper_bishop_firstnet_v5_residual_train.py`,
   `convert_firstnet_v5_npz_to_snnue.py`, `README_V5_RESIDUAL.md`,
   `SniperBishop_classical_batch.cpp`) proves the network's training target
   is `stockfish_white_cp - classical_white_cp` -- a residual delta, not an
   absolute score -- and that Q's "classical + 100% residual" formula is
   exactly the documented, kit-mandated combination rule. The frozen
   reference's 35%-blend has **no residual-mode code path at all**
   (confirmed by grep on the reference source) -- it is applying the wrong
   combination rule to this network, not a more-robust one. Checkpoint 7's
   framing ("Q reverses the sign relative to the reference") should be read
   as "the reference is the wrong baseline for this network," not "Q is
   wrong." `ResidualScale`/`ResidualGuard`/blending were **not changed** --
   this checkpoint proved the contract, it did not act on it.

**Next session's first job**: decide whether/how to re-run a
Checkpoint-6/7-style A/B match now that Finding 1 is partially fixed and
Finding 2 is reframed (no code changed there yet, so a re-match would not
show improvement from Finding 2 specifically -- only from Finding 1's
throughput fix). See `docs/Checkpoint8_Report.md`'s "Remaining gap" and its
NNUE section's "What this does not establish" for the concrete open
threads (decompose the remaining ~1.3x perft gap; check FIRST_NET v5's
*magnitude* calibration, not just its combination formula, across a larger
FEN sample). Do not run a full 200-game `docs/ABTesting.md` batch before at
least the perft gap is further decomposed or explicitly accepted as
residual system noise -- 1.3x is much smaller than 2.8x but is not yet
proven to be zero.

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
3af0c60  docs/Checkpoint7_Report.md, docs/{ABResults,Checkpoint6_Report,
         Build}.md updates, reference/ab_tests/checkpoint7_root_cause/** --
         the root-cause investigation itself (no source changes)
(uncommitted at time of writing -- commit before ending the session)
         CMakeLists.txt (/MT CRT-linkage flag-comparability fix),
         src/position/position.cpp (removed the redundant StateInfo
         zero-init in Position::doMove), docs/Checkpoint8_Report.md,
         HANDOFF.md, reference/ab_tests/checkpoint8_perf/** -- the perft
         throughput fix and the read-only NNUE contract audit
```

## Test results (last full run, this checkpoint's commit; Release+Debug re-verified after the `doMove` fix)

Windows, MSVC 19.51.36248.0 x64, `Visual Studio 18 2026` generator:

| Gate | Release | Debug |
|---|---:|---:|
| Compiler warnings | 0 | 0 |
| CTest (`q_core_tests`, `q_integration_tests`) | 2/2 PASS | 2/2 PASS |

`tests/` was not modified this checkpoint, so per-check counts are
unchanged from the 519,122 core / 70 integration checks recorded in
checkpoint 6. Re-confirmed via a from-scratch clean rebuild (same counts,
different binary hashes each time -- MSVC output is not byte-reproducible
across separate builds even from identical source, see
`docs/Checkpoint6_Report.md` and `docs/Checkpoint8_Report.md`). Debug is a
meaningful re-check this checkpoint specifically: it exercises
`assert(isConsistent())` after every `doMove`/`undoMove`, which would have
caught any state-corruption introduced by removing the redundant
`StateInfo` zero-init in `Position::doMove`.

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
- **`wpr -start CPU` requires administrator privileges** and fails with
  `0xc5585011` without them; not available in this environment. Checkpoint
  8 used manual `std::chrono`-based instrumentation in a throwaway git
  worktree instead (see `docs/Checkpoint8_Report.md`'s Method section) --
  reuse that approach, not WPR, unless the session has elevation.
- The reference's build manifest
  (`reference/bin/EunshinBishop_v2.62_reference.exe.build.json`) is the
  authoritative source for its exact compiler flags
  (`/nologo /std:c++17 /EHsc /W4 /O2 /DNDEBUG`) -- check it before assuming
  a flag mismatch when comparing performance or output against Q.
- MSVC's default Release CRT linkage is dynamic (`/MD`); the frozen
  reference uses static (`/MT`, confirmed via `dumpbin /dependents` --
  imports only `KERNEL32.dll`). `CMakeLists.txt` now forces `/MT` to match
  (`CMAKE_MSVC_RUNTIME_LIBRARY`) for flag comparability, though this alone
  did not close checkpoint 8's throughput gap.

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

1. **Decide next steps on the remaining Checkpoint 8 threads before
   GCC/Clang builds, full-scale A/B batches, or further item-31 work.**
   Concretely:
   - The perft gap is down to ~1.3x (from ~1.9-2.0x) but not further
     decomposed. `docs/Checkpoint8_Report.md`'s "Remaining gap" section
     names concrete candidates (`popcount`-based material-key updates
     inside `putPieceHashed`/`removePieceHashed`, `Position` value-copies
     in `isLegal`/`snapshotForSearch`) that were inside the still-dominant
     "rest of doMove" bucket but were not separately instrumented -- reuse
     the same `std::chrono` worktree-instrumentation method (not WPR) to
     go further, if the gap is judged worth closing further before a
     re-match.
   - The NNUE contract is now proven (residual delta, Q's formula is
     correct per the training scripts) but its *calibration* is not --
     check FIRST_NET v5's actual output magnitude/distribution across a
     larger, systematic FEN sample (not just Kiwipete/startpos) before
     concluding the current `ResidualScale`/`ResidualGuard` defaults are
     well-tuned for it.
2. Only after at least a decision is made on both: re-run
   `docs/ABTesting.md`'s architecture-regression comparison to measure what
   improved. Note the reference is not a valid baseline for judging Finding
   2/NNUE correctness specifically (it has no residual-mode code path at
   all, per `docs/Checkpoint8_Report.md`) -- a re-match will show whether Q
   is more *competitive*, not whether Q's eval formula is "right"; that
   question is already answered by the contract audit.
3. GCC/Clang builds and full item-31 release work remain valuable but are
   still not the highest-priority next step.
4. If continuing item 23 instead: pick exactly one group
   from `docs/ClassicalEvalBacklog.md`'s priority list, add it behind its
   own experiment flag, and do not touch a second group in the same change.
5. Do not push a `v*` tag or create a GitHub remote/release until the
   regression above is resolved and `docs/Build.md`'s RC-relevant checklist
   rows are checked with evidence linked from a checkpoint report.
