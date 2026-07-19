# Contributing

This project is a from-scratch chess engine migration (the "Q Architecture").
Its correctness and provenance discipline exist because the eventual goal is
a public GitHub release; please keep both when contributing.

## Before you start

Read, in order:

1. `README.md` -- current status and build instructions.
2. `docs/Architecture.md` -- module ownership and the hot-path/cold-path
   split; core and `Position` may never depend on Search, Engine, UCI, or
   filesystem paths.
3. `docs/Migration.md` -- what maps to what from the frozen reference, and
   the highest-risk migration points.
4. The most recent `docs/CheckpointN_Report.md` -- what is actually verified
   right now, versus what is still a documented gap.

## Source and idea provenance

Distinguish, explicitly, in any PR description:

- Code you wrote from scratch for this project.
- A well-known chess-programming *algorithm* or *idea* you implemented
  independently (e.g., "staged move ordering," "aspiration windows," "SEE
  swap-list evaluation") -- referencing the general technique is fine.
- Any code actually copied or adapted from another engine's source. This
  must be disclosed in the PR, including the original project, its license,
  and any required attribution. Do not silently port code from another
  engine's repository.

Do not confuse "I used a commonly known algorithm" with "I copied a specific
project's implementation" -- the first is normal engineering, the second has
license consequences that must be disclosed before the code is merged.

## Correctness gates

Every change must still pass:

```powershell
cmake -S . -B build-vs -A x64
cmake --build build-vs --config Release --parallel
ctest --test-dir build-vs -C Release --output-on-failure
```

See `docs/Testing.md` for what each test target actually covers, and for the
perft/UCI smoke tests a PR is expected to run manually if it touches move
generation, search, or the UCI frontend.

## Experiment discipline for search/eval changes

Mirrors the sibling `SniperBishop` v2.8 line's rules (`AGENTS.md`) and the
item-23 classical-evaluation backlog (`docs/ClassicalEvalBacklog.md`):

1. One change has one feature. Do not bundle unrelated search/eval changes
   in one PR.
2. Every new heuristic needs a UCI or compile-time off switch, and OFF must
   reproduce the prior fixed-depth bestmove/score/nodes/PV exactly.
3. Compilation and passing unit tests are not evidence of playing-strength
   change. A strength claim needs paired games at a stated time control,
   with W/L/D, a confidence interval, and the exact options/hashes recorded
   -- see `docs/Checkpoint4_Report.md` for the format this project already
   uses.
4. Do not flip a pruning/search flag's *default* based on unit tests alone
   (this is explicit in the SEE/LIMBO specification: `SEEPruning` must not
   default to `true` without A/B evidence, and the same bar applies to any
   new flag).

## Style

- No exceptions, `std::function`, `shared_ptr`, heap allocation, or mutex
  acquisition on the hot path (move generation, make/unmake, `MovePicker`,
  history/TT access, search). See `docs/Architecture.md`'s "Hot-path rules."
- `/W4 /permissive-` (MSVC) or `-Wall -Wextra -Wpedantic -Wshadow
  -Wconversion` (GCC/Clang) must stay warning-free; do not silence a warning
  with a cast that changes behavior.
