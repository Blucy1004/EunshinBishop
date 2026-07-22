# Checkpoint 6 test report

Date: 2026-07-19

Decision: **ACCEPT** for parts 1-2 of this checkpoint's scope only: (1)
MSVC clean Release/Debug re-verification, and (2) a static audit and
correctness fix of `.github/workflows/build.yml` (the actual GCC/Clang
execution happens after a future repository push -- this machine has
neither compiler installed by explicit user decision, see `HANDOFF.md`).

Part 3, the small-scale paired A/B test, is **not accepted as a closed
result**. It surfaced a 21-1-2 loss margin for Q that is treated as a
**release-blocking regression**, not a tuning gap to note and move past --
see `docs/Checkpoint7_Report.md` for the root-cause investigation this
triggered (classical-only isolation match, eval-component comparison, and a
pure move-generation speed comparison), which found two independent,
reproducible causes. Do not read this report's Part 3 in isolation; read it
together with Checkpoint 7.

This is explicitly **not** an Elo determination -- the game count is far
below the >=200/>=1000 threshold `docs/ABTesting.md` and `AGENTS.md`'s
experiment discipline require before trusting a result, and is reported as
such throughout. Specification item 23 was not touched. `SEEPruning` and
`LIMBO` remain at
their default `false` for every game.

## Part 1: MSVC re-verification

Clean rebuild (`Remove-Item -Recurse -Force build-vs` then a fresh
`cmake -S . -B build-vs -A x64`), same toolchain as every prior checkpoint:
MSVC 19.51.36248.0 x64 (VC Tools package `14.51.36231`), Windows SDK
10.0.26100.0, CMake's `Visual Studio 18 2026` generator, C++17, `/W4`,
`/permissive-`.

| Gate | Release | Debug |
|---|---:|---:|
| Compiler warnings | 0 | 0 |
| Core test checks | 519,122 PASS | 519,122 PASS |
| Integration test checks | 70 PASS | 70 PASS |
| CTest | 2/2 PASS | 2/2 PASS |

No source file changed since checkpoint 5 (commit `fdfcbc6`); this
re-verification exists to produce fresh, dated hashes and to confirm the
clean-rebuild path itself still works end to end, not to test new code.

| Artifact | SHA-256 |
|---|---|
| Release `EunshinBishopQCoreTests.exe` | `60F363C4FDA85CA46F5D24AFB91FBFC52576A8415B224EC5450625A392FEF9B6` |
| Release `EunshinBishop.exe` | `15BDAD4613C2423D4A4A97586EFEFE429B7F154D96D65282E54B869BD0B0F2FA` |
| Release `EunshinBishopQIntegrationTests.exe` | `2C545968D832FD8376DDFA36C801498491072A15162F96DC4B308D3D7A800B52` |
| Debug `EunshinBishopQCoreTests.exe` | `CADEDF17D04CCDAF1CAFC1897ECE046318E6558BAD2F68D45D25246E39D029FF` |
| Debug `EunshinBishop.exe` | `E5FB49D2B40BAB8C2AFBEAA25674C54E870C246B0DEEFA7C3DBBF997375A39CC` |
| Debug `EunshinBishopQIntegrationTests.exe` | `8121C5B121455654931FDE8C10E9207B823CDDD724A6AC926EBCA64ADA721078` |

These differ from checkpoint 5's binary hashes despite identical source:
MSVC's output is not byte-reproducible across separate builds (PDB GUIDs,
timestamps, and incremental-link metadata vary), which is expected and not
itself evidence of any behavioral change -- behavior is what the test suite
verifies, not binary identity across rebuilds.

## Part 2: GitHub Actions `build.yml` audit

Per explicit user decision, this machine does not install GCC/Clang
locally; the workflow will actually execute on GitHub after a future push.
This session audited the file statically instead.

**Finding**: the "No-network build sanity" steps (both OS jobs) were named
for, but did not actually test, the scenario specification item 31
requires -- "CI에서는 외부 NNUE 파일이 없어도 UseNNUE=false로 정상 빌드 및
테스트가 가능해야 한다." Because `networks/firstnet_v5_10b.snnue` is
committed directly in the repository, every prior step ran with the file
present on disk and merely toggled the `UseNNUE` UCI option at runtime.
`CMakeLists.txt`'s `if(EXISTS EUNSHIN_DEFAULT_NETWORK)` branch -- which
skips registering `q_integration_tests` and the network-copy custom
commands entirely -- was therefore never exercised by CI.

**Fix**: added a further step to each OS job (`Configure/build/test with
the network file genuinely absent`) that moves the network file aside,
reconfigures fresh into a separate `build-nonetwork` directory, builds,
runs CTest, explicitly asserts `q_integration_tests` is *not* registered
(`ctest -N`), confirms the engine still produces a `bestmove` with the file
genuinely absent, and restores the file (`finally`/`trap`) so later steps
or runs are unaffected.

This was verified locally to the extent possible without GCC/Clang: the
underlying mechanism it depends on (CMake's `if(EXISTS)` branch skip) is
compiler-independent, so it was spot-checked with the MSVC build instead of
trusted blind. Using an isolated `git worktree` (commit `7387dc2`) at a
short path (`C:\nnchk`, to avoid an unrelated Windows `MAX_PATH` issue seen
first at a deeply nested scratch path -- FileTracker log creation failure,
not a project bug) with `networks/firstnet_v5_10b.snnue` deleted entirely:

```text
cmake -S C:\nnchk -B C:\nnchk\build-nonetwork -A x64      -> configures clean, 0 warnings
cmake --build C:\nnchk\build-nonetwork --config Release   -> builds clean, 0 warnings
ctest --test-dir C:\nnchk\build-nonetwork -C Release -N   -> lists only "q_core_tests" (1 test)
ctest --test-dir C:\nnchk\build-nonetwork -C Release       -> 1/1 PASS
dir C:\nnchk\build-nonetwork\Release                        -> no .snnue file present, as expected
EunshinBishop.exe, "go depth 2" from startpos                -> bestmove e2e4 ponder e7e5 (classical fallback, no crash)
```

This confirms the fix's core logic is correct on at least one compiler;
`docs/CIVerificationChecklist.md` still calls for confirming the same on
GCC and Clang once the workflow actually runs.

`docs/CIVerificationChecklist.md` records what to check the first time this
workflow actually runs on GitHub -- exact compiler versions, warning
comparison across MSVC/GCC/Clang, and what to update once it goes green.
Nothing in `docs/Build.md`'s pre-release checklist was checked off by this
audit alone; those rows require an actual observed green run.

## Part 3: small-scale paired A/B (architecture regression)

Command (from `docs/ABTesting.md`, adapted to a small-scale batch):

```powershell
cutechess-cli.exe `
  -engine name="v2.62_reference" cmd="reference/bin/EunshinBishop_v2.62_reference.exe" proto=uci option.Hash=64 `
  -engine name="Q_checkpoint6" cmd="build-vs/Release/EunshinBishop.exe" proto=uci option.Hash=64 `
  -each tc=10+0.1 `
  -openings file=tests/openings_v1.epd format=epd order=sequential `
  -rounds 12 -games 2 -repeat `
  -concurrency 1 `
  -pgnout architecture_regression.pgn
```

`cutechess-cli` 1.5.1 (win64 build). `tc=10+0.1` is 10 seconds base time
plus 0.1 second increment per move (the fast-testing convention
`docs/ABTesting.md` documents, not 10 minutes). `-concurrency 1` keeps games
strictly sequential on the same hardware/CPU so neither engine is
disadvantaged by contention.

**Conditions held per the task**:

- Opening suite: `tests/openings_v1.epd`, 12 named positions (ruy_lopez,
  italian, sicilian_najdorf, sicilian_sveshnikov, french, caro_kann, qgd,
  slav, ...), `order=sequential`, no shuffling.
- Color alternation: `-repeat` replays each opening with colors swapped, so
  every position is played once with each engine as White.
- Same time control for both engines (`-each tc=10+0.1`), same hardware
  (single machine, `-concurrency 1`, no parallel contention).
- `Hash=64` set identically on both engines (the reference's own default;
  Q's own default is 256, overridden here to match).
- `SEEPruning=false`, `LIMBO=false`: both are Q's unchanged defaults; no
  option was set for either, and neither appears in the reference's option
  list at all.

**NNUE load confirmed for both engines, from the exact working directory
and relative paths `cutechess-cli` used, before the batch was run**:

- `v2.62_reference`: `info string NNUE ready:
  C:\Users\fly_t\Downloads\SniperBishop\EunshinBishop_Q\reference\bin\firstnet_v5_10b.snnue`
  -- whole-file SHA-256
  `E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656`.
- `Q_checkpoint6`: the `nnuecheck` debug command (checkpoint 5) reported
  `network loaded; NNUE loaded:
  C:\Users\fly_t\Downloads\SniperBishop\EunshinBishop_Q\networks\firstnet_v5_10b.snnue
  generation=1`, payload SHA-256
  `D8157A4943D889403C46A8F6C2D4A6D0EF9C12E86FC0AA0103812D6239F8F49D` (the
  network payload digest, distinct from but consistent with the whole-file
  digest above; both engines read byte-identical copies of the same file --
  see `reference/README.md`).
- Neither engine printed a "load failed"/"classical fallback" message in
  this check. Per the task's explicit condition, any game where either
  engine's NNUE silently fell back to classical would not have been counted
  as valid; no such fallback was observed.

**Result** (24 games):

```text
Score of v2.62_reference vs Q_checkpoint6: 21 - 1 - 2  [0.917] 24

v2.62_reference playing White: 11 - 0 - 1  (12 games)
v2.62_reference playing Black: 10 - 1 - 1  (12 games, i.e. Q as White: 1W 10L 1D)

Elo difference: 416.6 +/- nan (LOS 100.0%, DrawRatio 8.3%)
```

Outcome breakdown: 21 games ended in checkmate, 2 by the 50-move rule.
**Zero** time forfeits, crashes, or illegal moves for either engine across
all 24 games.

**Reading this result**: this is a small-scale batch, not an Elo
determination -- `docs/ABTesting.md` and `AGENTS.md`'s experiment
discipline both require >=200 games before treating a result as
preliminary evidence, extended to >=1000 for a promising candidate, and
cutechess-cli's own output reflects the small sample (the "Elo difference:
416.6" figure has an **undefined, `nan`, confidence interval**). That
caveat is about the *precision* of the number, not about whether the
result deserves attention. It does: both engines completed the batch with
zero crashes/forfeits/illegal moves (so this is not noise from a broken
harness), the loss was consistent across both colors (so it is not a
one-sided bug like a color-dependent time or book issue), and it is far
too lopsided for "not tuned yet" to be a sufficient explanation on its own.
**This was escalated to a root-cause investigation instead of being
accepted as an expected gap -- see `docs/Checkpoint7_Report.md`.** That
investigation found two independent, reproducible causes: a ~2.6-4x
search-throughput deficit rooted in move generation/make-unmake (present
with or without NNUE), and an NNUE evaluation-combination formula that can
reverse the sign of the evaluation relative to the reference's blend
(present only with NNUE enabled). Neither was fixed in that checkpoint
either, per instruction to find and reproduce before changing code.

Raw evidence: `reference/ab_tests/checkpoint6_architecture_regression/`
(PGN with per-move eval/time annotations, full cutechess-cli log,
`MANIFEST.md` with every hash and command used). Logged in
`docs/ABResults.md`.

## Deferred / out of scope for this checkpoint

- Item 23 (pure classical strengthening): untouched.
- `SEEPruning`/`LIMBO` defaults: untouched (`false`), no A/B evidence
  gathered for either flag in this checkpoint.
- Actual GCC/Clang execution: pending a future repository push; this
  checkpoint only fixed and statically audited the workflow that will run
  it, per explicit user decision not to install compilers locally.
- No GitHub tag, push, or release was made.
- The architecture-regression result above is a small-scale, single-batch
  observation, not a `docs/ABTesting.md`-grade decision (which requires
  >=200 games, extended to >=1000 for a promising candidate). It should not
  be used to justify any option-default change.
- **The regression itself is not resolved.** `docs/Checkpoint7_Report.md`
  found and reproduced two independent causes but fixed neither -- no
  source file was changed. Do not treat checkpoint 6 or 7 as clearing this
  blocker; the next session's first job is fixing (or scoping the fix for)
  Checkpoint 7's Findings 1 and 3, not further release scaffolding.
