# A/B test results log

Dated entries only. Each entry follows the fields `AGENTS.md`'s experiment
discipline and prior checkpoint reports already use: command, hashes,
active options, opening suite/hash, seed, W/L/D, confidence interval, color
split, time forfeits, crashes, illegal moves. See `docs/ABTesting.md` for
the reusable command templates these entries are run from.

## 2026-07-19 -- Checkpoint 6: architecture regression (small-scale)

**Status: release-blocking regression under investigation.** Not a
tuning gap -- see `docs/Checkpoint7_Report.md` (root-cause investigation)
once filed.

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
