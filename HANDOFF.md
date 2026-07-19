# EunshinBishop Q -- handoff status

Last updated: 2026-07-19. This file is the single source of truth for "what
is actually done, what is next" across sessions; `docs/CheckpointN_Report.md`
files are the frozen, dated evidence each summary below points to. Update
this file, not just the checkpoint reports, whenever the state changes.

## Judgment

Specification items 1-22 are implemented and independently verified on
Windows MSVC. Item 23 is deliberately not started (it is a long-running,
paired-game-gated program by its own stated rules, not a code task). Item 31
(GitHub release readiness) is scaffolded but explicitly incomplete. Items
24-30 have no recorded original text in this project as of this update --
do not guess their scope; get the actual spec text before touching them.

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
```

## Test results (last full run, commit `8df57a2`)

Windows, MSVC 19.51.36248.0 x64, `Visual Studio 18 2026` generator, clean
`build-vs` reconfigure from scratch:

| Gate | Release | Debug |
|---|---:|---:|
| Compiler warnings | 0 | 0 |
| Core test checks | 519,122 PASS | 519,122 PASS |
| Integration test checks | 60 PASS | 60 PASS |
| CTest | 2/2 PASS | 2/2 PASS |

NNUE-enabled path was exercised, not silently skipped: `EunshinBishop.exe`
and the integration test binary both load
`networks/firstnet_v5_10b.snnue`
(SHA-256 `E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656`,
unchanged since checkpoint 2) successfully, confirmed via the UCI smoke test
(`uci`/`isready`/`position startpos`/`go depth 4` -> `bestmove e2e4 ponder
e7e6`, no `info string ... fallback` line) and via
`docs/Checkpoint3_Report.md`'s Kiwipete depth 6/8/10 comparison. Deliberate
negative-path tests (missing/malformed network) are separate integration
test cases that assert the classical-fallback message explicitly; they are
not the default path for a healthy network.

Regression check: with `SEEPruning=false`/`LIMBO=false` (both still
default), Kiwipete `go depth 6` node counts are byte-for-byte identical to
the pre-checkpoint-4 baseline at every depth (70, 390, 6722, 10147, 48859,
122609). No default-path behavior changed.

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
  invocation paths, regardless of encoding settings tried. This is a test
  -harness artifact (reproducible with a throwaway first line), not an
  engine defect -- confirmed by checking `isPseudoLegal`/`isLegal` on the
  actual mutated position, not the caller's stale copy.
- `LIMBO`'s "bounded re-search" design variant (re-verify a move's result
  after the fact, rather than extending inline before descending) is not
  implemented; only the inline-extension variant is.
  `SearchStatistics::limboBoundedResearches` stays at 0 by design, not a bug.

## Incomplete stages

- **23** (pure classical evaluation strengthening): not started by design;
  see `docs/ClassicalEvalBacklog.md` for the priority list and the
  one-group-per-commit-with-paired-games discipline it must follow.
- **24-30**: no original spec text has been supplied to this project as of
  this update. Do not implement anything under these numbers from
  inference alone -- get the actual text first (principle 2: don't guess
  requirements not in the original text).
- **31** (GitHub release readiness): scaffolded (`​.github/workflows/`,
  `LICENSE`, `CONTRIBUTING.md`, `SECURITY.md`, `CHANGELOG.md`,
  `docs/{Build,UCI,Testing}.md`, `docs/ReleaseNotes_v5.0.0-rc.1_draft.md`,
  `Version` namespace / `--version` / `--help`), but explicitly **not**
  complete or release-ready. Unchecked items in `docs/Build.md`'s
  checklist: GCC build, Clang build, residual-equation independent
  verification, time-loss root-cause investigation, Cute Chess regression,
  independent command re-verification, license confirmation (both source
  and network weights), and real release-asset checksums. No tag has been
  pushed and no GitHub Release has been created; the workflows are local
  files only.
- No A/B or paired-game evidence exists for `SEEPruning=true` or
  `LIMBO=true`; their defaults must not change without it.

## Next starting point

1. If continuing the numbered specification: get the actual items 24-30
   text (the placeholder that arrived in this project's chat history was
   empty -- `[여기에 네가 Codex에서 가져온 21~30 원문 붙여넣기]` -- and was
   never filled in with real content).
2. If continuing item 23 instead: pick exactly one group from
   `docs/ClassicalEvalBacklog.md`'s priority list, add it behind its own
   experiment flag, and do not touch a second group in the same change.
3. If continuing item 31 instead: the highest-value next step is an actual
   GCC and/or Clang build (this machine has neither installed) or a real
   Cute Chess paired-game regression -- both are listed unchecked in
   `docs/Build.md` and are prerequisites for even the RC tag, not just
   stable.
4. Do not push a `v*` tag or create a GitHub remote/release until
   `docs/Build.md`'s RC-relevant checklist rows are checked with evidence
   linked from a checkpoint report, per principle 8.
