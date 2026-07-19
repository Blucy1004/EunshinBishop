# Classical evaluation strengthening backlog (specification item 23)

Status: **not started**. This document records the item-23 scope and its
verification discipline so future work stays scoped and honest; it is not an
implementation and makes no strength claim.

## Goal (from the specification)

After the architecture port and the residual-correction NNUE fix are
complete, strengthen the classical evaluator so that `UseNNUE=false` is an
independently strong engine, not a fallback that merely fails safely. The
target "순수 클래식 2800" (pure-classical 2800) is a goal, not something to
declare achieved from reading the code.

## Priority order (from the specification, unchanged)

1. pawn structure
2. passed pawns
3. protected passers
4. connected passers
5. candidate passers
6. backward pawns
7. pawn islands
8. bishop pair
9. bad bishop
10. knight outpost
11. rook files (open/half-open)
12. rook on the seventh
13. rook behind a passer
14. safe mobility
15. king shelter
16. pawn storm
17. king attack scaling
18. threats
19. hanging pieces
20. minor attacks on major pieces
21. endgame king activity
22. opposite-colored bishop scaling
23. low-material draw scaling

## Discipline this backlog must follow

- One group at a time. Do not land several of the groups above in one
  change.
- Each group needs its own commit or `EngineOptions`/compile-time
  experiment flag so it can be turned off and A/B tested independently, per
  `AGENTS.md`'s existing experiment discipline (`[[SniperBishop AGENTS.md]]`
  for the sibling v2.8 project line, whose rules this backlog reuses):
  fixed-depth bestmove/score/nodes/PV must stay unchanged with the flag off,
  and only paired games (>=100, preferably >=200, at a stated time control)
  are evidence of strength -- compilation is not.
- Record command, hashes, active options, opening suite, seed, W/L/D,
  confidence interval, color split, and time forfeits/crashes/illegal moves
  for every batch, exactly as `Checkpoint2_Report.md`/`Checkpoint3_Report.md`
  already do for structural gates.
- Do not claim an Elo number without the opponent, exact settings, time
  control, sample size, and confidence interval attached.

## Why this is deferred rather than attempted here

This checkpoint's other work (items 19-22) is bounded, code-only, and
verifiable by unit tests and a fixed-depth regression check in the same
session it was written. Item 23 is a long-running, game-tested feature
program by its own stated discipline -- each group needs independent paired
games, not just compilation, before it can be called done. Bulk-implementing
all 23 groups in one pass would violate the "one group per commit,
independently verifiable" rule this very document exists to state, so no
group has been started.
