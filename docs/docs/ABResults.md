# A/B test results log

Dated entries only. Each entry follows the fields `AGENTS.md`'s experiment
discipline and prior checkpoint reports already use: command, hashes,
active options, opening suite/hash, seed, W/L/D, confidence interval, color
split, time forfeits, crashes, illegal moves. See `docs/ABTesting.md` for
the reusable command templates these entries are run from.

## 2026-07-19 -- Checkpoint 6: architecture regression (small-scale)

**Status: release-blocking regression, root-caused but not fixed.** See
`docs/Checkpoint7_Report.md`.

- Command, hashes, options, openings: `reference/ab_tests/checkpoint6_architecture_regression/MANIFEST.md`.
- Raw evidence: `architecture_regression.pgn`, `architecture_regression_out.log` in the same directory.
- Result: v2.62_reference 21 - Q_checkpoint6 1 - draws 2 (24 games), Q as
  White 1W/10L/1D, Q as Black 0W/11L/1D. Zero time forfeits, crashes, or
  illegal moves.
- This is a 24-game batch, far below the >=200/>=1000 game threshold this
  project requires before treating a result as calibrated Elo evidence; see
  `docs/Checkpoint6_Report.md` for the full framing. The lopsidedness and
  consistency across both colors is what escalated this from "small sample,
  keep watching" to "investigate now" rather than something to wait on more
  games to clarify.

## 2026-07-19 -- Checkpoint 7: classical-only isolation match

**Status: diagnostic, part of the root-cause investigation above.**

- Command, hashes, options: `reference/ab_tests/checkpoint7_root_cause/` (no
  separate MANIFEST -- same engines/hashes as checkpoint 6, `UseNNUE=false`
  added on both sides, 6-opening subset of the same suite).
- Raw evidence: `classical_only_isolation.pgn`, `classical_only_isolation_out.log`.
- Result: v2.62_ref_classical 9 - Q_classical 0 - draws 3 (12 games).
  Zero time forfeits, crashes, or illegal moves.
- Purpose: isolate whether the checkpoint-6 gap exists independent of NNUE.
  It does (score 0.875 vs 0.917 with NNUE on) -- smaller but still lopsided,
  pointing to a second, NNUE-independent cause. See `docs/Checkpoint7_Report.md`
  Finding 3 (move-generation throughput) for what that cause turned out to
  be.
