# Checkpoint 7: root-cause investigation of the checkpoint-6 regression

Date: 2026-07-19

**Status: release-blocking. Not accepted, not resolved. No code was
changed in this checkpoint** -- this report documents diagnosis only, per
explicit instruction to find and reproduce the cause(s) before touching
any implementation. `docs/Build.md`'s pre-release checklist should treat
this as an additional, more specific blocker than the general "Cute Chess
regression testing" row it already carries.

## Why this checkpoint exists

`docs/Checkpoint6_Report.md` recorded a 24-game small-scale A/B match:
v2.62 reference 21, Q 1, draws 2. That report initially framed the result
as "consistent with Q not being strength-tuned yet, not a bug" -- an
acceptable-gap framing. That framing was rejected: a result this lopsided,
this consistent across both colors, deserves a root-cause investigation
before anything resembling release preparation continues. This checkpoint
is that investigation.

## Evidence preserved before any diagnostic step

Per instruction, nothing about the original checkpoint-6 result was
touched or re-run before preserving it:

- `reference/ab_tests/checkpoint6_architecture_regression/` -- the
  original 24-game PGN, full cutechess-cli log, opening suite, and a
  MANIFEST recording the exact command, both engine binaries' SHA-256, the
  network's SHA-256 (whole-file and payload), options, and conditions.
- The two engine binaries referenced there were not rebuilt or modified
  before or during this investigation.

New evidence gathered during this investigation lives in
`reference/ab_tests/checkpoint7_root_cause/`:

| File | What it is |
|---|---|
| `classical_only_isolation.pgn`, `classical_only_isolation_out.log` | 12-game paired match, both engines `UseNNUE=false`, same 6-opening subset, same time control |
| `openings_v1_first6.epd` | the exact 6-opening subset used above |
| `eval_comparison_startpos_kiwipete.txt` | side-by-side `eval`/`evaldetail` output, both engines, identical FENs |
| `perft6_speed_comparison.txt` | `perft 6` timing, both engines, identical position |
| `search_speed_comparison_kiwipete.txt` | fixed-`movetime` depth/nps comparison, both engines, NNUE on and off |

## Method

1. **Classical-only isolation match**: both engines `UseNNUE=false`,
   otherwise identical conditions to the checkpoint-6 batch (same `Hash`,
   same time control, same color alternation, a 6-opening subset of the
   same suite for faster turnaround). This isolates whether the gap exists
   independent of NNUE at all.
2. **Eval component comparison on identical FENs**: called both engines'
   own `eval`/`evaldetail`-equivalent commands (`eval` on the reference,
   `eval`+`evaldetail` on Q) on the same two positions (startpos, canonical
   Kiwipete) and compared every reported number side by side.
3. **Pure move-generation speed comparison**: `perft 6` from startpos on
   both engines, isolating throughput from evaluation and NNUE entirely.
4. **Fixed-`movetime` search comparison**: `go movetime 3000` at Kiwipete,
   both engines, both with and without NNUE, comparing depth reached and
   nodes/sec.

No source file was edited between or during any of these steps.

## Findings

### Finding 1 -- the residual-additive NNUE formula produces materially different, sometimes sign-flipped, evaluations compared to the reference's blend

At canonical Kiwipete, with `UseNNUE=true` (default) on both engines:

```text
                  Classical   NNUE raw    Combination formula        Final
v2.62 reference   +103 cp     -131 cp     35%-blend                  +42 cp
Q                 +103 cp     -131 cp     classical + 100% residual  -28 cp
```

Classical and NNUE-raw are **identical** between the two engines at this
position (and at startpos, where both also matched exactly: classical
+10 cp, NNUE raw +39 cp on both sides). This rules out a classical-eval
bug and an NNUE-inference bug -- the network loads correctly and infers
identically on both engines. The divergence is entirely in how the two
numbers are *combined*: the reference's blend (35% NNUE / 65% classical)
keeps the same sign as classical alone; Q's "classical + full residual"
formula (the behavior specification item 14 explicitly requested, to
replace the reference's absolute-style blend) adds the entire -131 cp on
top of +103 cp and **reverses the sign** -- Q evaluates this exact position
as favoring the opponent, the reference does not.

`ResidualGuard` did not intervene (`Residual guard factor: 100%`) because
its knee threshold (400 cp at this phase) sits well above this 131 cp
correction -- this is the guard behaving exactly as designed
(`src/eval/evaluator.cpp`'s `applyResidualGuard`: "Preserve ordinary
tactical corrections exactly"). The guard is not the bug. What this finding
actually raises -- **not answered, not investigated further, per
instruction to stop before changing code** -- is whether FIRST_NET v5's raw
output is really calibrated as `teacher score - classical score` (a small
delta, as specification item 14 assumes), or is closer to an independent,
comparably-scaled evaluator, for which the reference's blend is inherently
more robust than a 100%-weight addition regardless of any guard tuning.

### Finding 2 -- Q's search throughput is 2.6-4x lower than the reference's, with and without NNUE

Fixed `movetime 3000` at Kiwipete (full data in
`search_speed_comparison_kiwipete.txt`):

| Mode | Reference nps | Q nps | Ratio | Reference depth reached | Q depth reached |
|---|---:|---:|---:|---:|---:|
| NNUE on (default) | ~580-610k | ~220k | ~2.7x | 7 | 6 |
| NNUE off (classical) | ~1.28-1.30M | ~320-335k | ~3.9x | 10 | 7 |

Q reaches meaningfully shallower depths than the reference in the same
wall-clock budget, in both modes. At the fast `10+0.1` time control the
checkpoint-6 and this checkpoint's isolation match both used, this gap is
proportionally severe.

### Finding 3 -- the throughput gap is rooted in move generation / make-unmake, not evaluation

`perft 6` from startpos, both engines (full data in
`perft6_speed_comparison.txt`):

```text
v2.62 reference: 119,060,324 nodes in 6,194 ms  = 19,221,000 nps
Q:               119,060,324 nodes in 17,471 ms =  6,814,740 nps
```

Both engines produce the **exact same, textbook-correct** node count --
this is not a move-generation correctness bug. But `perft` touches only
move generation and make/unmake, none of evaluation, NNUE, search
heuristics, or the transposition table -- and Q is still ~2.8x slower here.
This means the search-level gap in Finding 2 cannot be attributed to NNUE
inference cost or search-algorithm overhead alone; a substantial part of
it is already present in the most basic hot path
(`Position`/`movegen.cpp`'s generation and `doMove`/`undoMove`), before
evaluation is ever called.

### Finding 4 -- both effects are real and independent, confirmed by the classical-only isolation match

12-game classical-only match (`UseNNUE=false` both sides): reference
9 - Q 0 - draws 3 (score 0.875), versus the original NNUE-enabled result
of reference 21 - Q 1 - draws 2 (score 0.917). Removing NNUE from the
equation makes the gap **smaller but not close** -- consistent with two
separate, additive causes: the throughput/depth deficit (Finding 2/3, which
persists with NNUE off) and the evaluation-formula issue (Finding 1, which
only applies with NNUE on, and which the numbers suggest makes the result
modestly worse still). Q also drew more often classical-only (3/12 = 25%)
than in the NNUE-enabled batch (2/24 = 8.3%), consistent with an evaluator
that is at least more *stable* (if not stronger) without the residual
formula's large swings.

## What this investigation does and does not establish

**Established, reproducible**: (1) a search-throughput gap rooted in the
core move-generation/make-unmake hot path, present with or without NNUE;
(2) an evaluation-combination-formula issue that can reverse the sign of
the evaluation relative to the reference's blend, present only with NNUE
enabled; (3) both are real and independent, not one explaining the other
away.

**Not established**: the specific line(s) of code responsible for the
throughput gap (Finding 3 only localizes it to move generation/make-unmake,
not a specific data structure or function); whether FIRST_NET v5's raw
output is actually miscalibrated for residual use or whether a smaller
`ResidualScale`/different guard tuning would resolve Finding 1 without
architecture changes; the relative Elo cost of each finding in isolation
(would require dedicated, larger-sample A/B batches per
`docs/ABTesting.md`, not run here). None of this was investigated further
because the instruction was to find and reproduce the cause(s), not to fix
them in this pass.

## Explicit non-actions in this checkpoint

- No source file was edited.
- No option default was changed.
- `SEEPruning` and `LIMBO` remain `false`, untouched, and unrelated to
  this investigation (both isolation matches used Q's unmodified defaults
  for everything except `UseNNUE`).
- Item 23 (classical strengthening) was not touched.
- No GitHub tag, push, or release was made.

## Recommended next steps (not executed here)

1. Profile `perft` specifically (not full search) to localize Finding 3 to
   a specific function -- move generation, the mailbox/bitboard
   synchronization in `doMove`/`undoMove`, or dispatch/inlining overhead
   around them. A profiler run, not more A/B games, is the right next tool.
2. For Finding 1: compute what `ResidualScale` (or a return to
   `NNUEOutputMode=Absolute`) would need to be for Q's Kiwipete evaluation
   to stay sign-consistent with the reference's blend, and check whether
   that also holds across a larger, systematic FEN sample -- not just the
   two positions checked here -- before concluding the residual assumption
   itself (not just its scale) is wrong.
3. Only after both are independently addressed, re-run
   `docs/ABTesting.md`'s architecture-regression comparison at the full
   200-game scale to measure what remains.
