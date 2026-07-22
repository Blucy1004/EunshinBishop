# Checkpoint 4 test report

Date: 2026-07-19

Decision: **ACCEPT** for the checkpoint-4 scope only (specification item 20,
strict SEE; specification item 19, LIMBO; re-confirmation that items 21
`TimeManager` and 22 `HistoryTables` already satisfy their specification
text as designed in checkpoint 3). This is not an Elo, strength, or
paired-game regression decision: `SEEPruning` and `LIMBO` remain off by
default, and no A/B evidence exists yet to justify changing that.

## Scope

| Item | Status |
|---|---|
| 19 LIMBO | Implemented: `src/search/limbo.h`, `.cpp` |
| 20 SEE | Implemented: `src/search/see.h`, `.cpp` |
| 21 TimeManager | Re-verified against the item-21 spec text; already satisfied by the checkpoint-3 `TimeManager`, no change needed |
| 22 History structure | Re-verified against the item-22 spec text; already satisfied by the checkpoint-2 `HistoryTables`, no change needed |

### Item 20: SEE

`See::see(position, move)` and `See::seeGe(position, move, threshold)` are a
second, independent SEE implementation from `MovePicker`'s frozen-reference
approximate ordering SEE (`move_picker.cpp`'s file-local `orderingSee`,
unchanged). Rather than a second hand-rolled bitboard swap-list, `See`
replays the capture sequence with real `Position::doMove` on a scratch
position obtained from `snapshotForSearch`, so:

- **pinned attackers** are excluded because `doMove` itself rejects any move
  that would leave the mover's king in check, and the candidate-selection
  loop simply tries the next-cheapest attacker when that happens;
- **king-capture legality** falls out of the same `doMove` check -- a king
  recapture that would move into an attacked square is never accepted;
- **en passant occupancy** is handled by `doMove`'s existing en passant
  logic for the initiating move (a recapture square is always occupied by a
  real piece afterward, so en passant cannot recur mid-exchange);
- **promotion/underpromotion**: the four promotion-type moves a promoting
  pawn generates collapse to one candidate (queen, the value-maximizing
  choice), and the resulting gain includes the promotion bonus at whichever
  ply it occurs, not only the initiating move;
- **discovered attacks / x-ray recaptures** fall out of generating captures
  against the scratch position's real, current board after each step,
  rather than a bitboard occupancy mask that has to be kept in sync by hand;
- **recapture sequencing** reuses the exact backward min-max fold already
  proven correct in `orderingSee`, unchanged.

`SEEPruning` is wired into `quiescence()`: when enabled, a strictly losing,
non-promoting capture (`!See::seeGe(position, move, 0)`) is skipped before
`doMove` is ever called for it. Move ordering is untouched; `See` is only
consulted from this one pruning site.

Unit tests (`tests/core_tests.cpp::testSee`) cover: an undefended-pawn
capture (+100), a queen capturing a pawn defended only by a pawn (-800), an
equal pawn trade (0), an x-ray double-rook exchange where the rear rook is
blocked until the front one moves (+500), a knight pinned to its king that
cannot recapture even though it geometrically "defends" the square (+100,
not the -800 a pin-unaware SEE would compute), and an undefended promoting
capture (+1,120, including the promotion bonus).

### Item 19: LIMBO

`Limbo::shouldVerify(position, ss, context)` and
`Limbo::verificationDepth(normalDepth)` implement the specification's
principles as a single bounded inline extension point in
`SearchContext::alphaBeta`, gated behind `useLIMBO` (default `false`):

| Principle (from the spec) | How it is enforced |
|---|---|
| frontier-only | `context.depth == 1` is required (the last real ply before the child would drop into quiescence); quiescence itself has no LIMBO call site |
| max +1 ply | `verificationDepth` returns `normalDepth + 1`; nothing calls it more than once per candidate |
| few candidates only | `moveIndex` (the move's 1-based rank from `MovePicker`, already TT/killer/history staged) must be 1 or 2, at the root or any other frontier node |
| no consecutive extension | `SearchStack::limboChain`, set when LIMBO fires and checked before firing again |
| cooldown | `SearchStack::limboCooldown`, set to 6 plies on firing and decremented once per ply along the current line (threaded parent-to-child, since the stack slot is otherwise reset every visit) |
| never in qsearch | the `context.depth == 1` gate structurally excludes quiescence; there is no `Limbo` call inside `quiescence()` |
| remaining time / iteration budget | refuses to fire with zero completed iterations, and refuses once elapsed time reaches 70% of the current maximum time budget |
| root best/second candidate verification | the same `moveIndex <= 2` gate applies at the root, where those are literally the best and second-best candidates so far |
| no overlap with AEGIS | refuses to fire when `AegisAssessment::unstable()` is already true at this node |
| never risks a 10+0.1 time loss | `TimeManager`'s hard deadline is untouched; LIMBO can only add search work inside the existing soft/hard budget checks already enforced every node, never bypass them |

This is the inline-extension realization of "Bounded deepening Override";
`SearchStatistics` reserves separate `limboExtensions` and
`limboBoundedResearches` counters, and only the former is populated by this
checkpoint. A second design -- searching a candidate at normal depth first
and only then re-searching it deeper if the result looks unstable -- is not
implemented; `limboBoundedResearches` stays at 0. This is a scoped,
documented limitation, not an oversight.

**No strength claim is made for either flag.** Both default to `false`, and
turning either on is untested by paired games; the spec explicitly forbids
defaulting `SEEPruning` to `true` without A/B evidence, and the same
discipline is applied to `LIMBO` here even though it is not stated for it
verbatim.

### Items 21/22 re-verification

`TimeManager` (`search/time_manager.h`) already exposes `initialize`,
`shouldStop`, `optimumTimeMs()`, `maximumTimeMs()`, and covers
wtime/btime/winc/binc/movestogo/movetime/depth/nodes/infinite, emergency
mode, best-move/score stability, root move changes, aspiration failures, and
AEGIS/LIMBO root-risk flags (`IterationInfo::aegisRootUnstable`,
`limboRootRisk`) exactly as item 21 specifies; `shouldStartNextIteration`
plays the role of item 21's naming, with an equivalent contract. No source
change was needed.

`HistoryTables` (`search/history.h`) is a `Worker`-owned class (not the raw
struct the spec's illustrative snippet shows) with `mainScore`/`updateMain`
sized `[COLOR_NB][SQUARE_NB][SQUARE_NB]`, `captureScore`/`updateCapture`
sized by moved piece type, target square, and captured piece type, and
`counterMove`/`setCounterMove`; a private `gravity()` bounds saturation, and
`clear()`/`decay()` cover the `ucinewgame` and mid-game policies item 22
requires. No source change was needed.

## Toolchain and commands

Same toolchain as checkpoint 3: Windows, MSVC 19.51.36248.0 x64, Windows SDK
10.0.26100.0, CMake's `Visual Studio 18 2026` generator, C++17, `/W4`,
`/permissive-`.

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
| Core test assertions | 519,122 PASS (+14 SEE checks over checkpoint 3) | 519,122 PASS |
| Integration test assertions | 60 PASS (+2 over checkpoint 3) | 60 PASS |
| CTest | 2/2 PASS | 2/2 PASS |

### Regression check against checkpoint 3

With `SEEPruning=false` and `LIMBO=false` (both defaults, unchanged), a
Kiwipete `go depth 6` run produced node counts identical at every depth to
the checkpoint-3 run recorded in `Checkpoint3_Report.md`
(70, 390, 6,722, 10,147, 48,859, 122,609 nodes at depths 1-6; same scores,
same PVs, same best move `e2a6`). The new code paths are inert when their
flags are off, as designed.

### Functional check with both flags enabled

`tests/integration_tests.cpp` sets `SEEPruning=true` and `LIMBO=true`,
runs a short search, and confirms the result is still a legal PV
(`legalPv`). This is a correctness smoke test, not a strength claim.

## Artifacts

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Checkpoint-4 source archive | 130,802 | `BEA6F34B74EF48C68249441505CE70D18C827197DA8CD40C0308FA94E83956D2` |
| Release core tests | 218,112 | `5CB64E039E8A4F7E1A3F42157F9183601E91CDF23074DF52095B7074818B1FCE` |
| Debug core tests | 1,392,640 | `933AB4038C9B786B0F30A754D36B66484204ADB2F4870FBCF59229FE4B65D1F6` |
| Release `EunshinBishop.exe` | 218,624 | `5E2746F9EE2267FCD3676BA50933F72B0D4D61C5ED4715B79BAF15C062416711` |
| Debug `EunshinBishop.exe` | 1,333,248 | `6F749275C8D860DA3AC87A5C61DFFE04A4E60FD946769BA406047D4F0E2CA2FD` |
| Release integration tests | 260,608 | `2211AB4937F6D9AB7BD5DA1601F0DEF814821A054222EA1E101E80753A4F8D4D` |
| Debug integration tests | 1,422,848 | `29594BE1C94CDE2C2EF21C0AD5E2C6A479BB99D144928859DA25461D4FD021ED` |
| Bundled FIRST_NET v5 | 12,585,424 | `E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656` (unchanged) |

Full listing in `reference/checkpoint4/SHA256SUMS`.

## Deferred gates

- Specification item 23 (pure classical evaluation strengthening) is
  explicitly not started; see `ClassicalEvalBacklog.md`.
- Specification items 24-30 have no recorded text in this project tree as of
  this checkpoint and are not attempted here.
- GCC and Clang builds have still not been run on this machine; only MSVC is
  verified.
- No A/B or paired-game testing exists for `SEEPruning=true` or
  `LIMBO=true`; neither flag's default should change without it.
- `Limbo`'s "bounded re-search" design variant (as opposed to the inline
  extension implemented here) remains unimplemented;
  `SearchStatistics::limboBoundedResearches` stays at 0 by design.
