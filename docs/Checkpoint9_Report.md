# Checkpoint 9 — Final audit remediation, GPL licensing, and hot-path refinement

Date: 2026-07-19

## Scope

This checkpoint prepares a public-release candidate without publishing it. It
finalizes the source license, updates release documentation, and makes one
minimal search-hot-path optimization. It does not retune search heuristics,
change NNUE scoring, or enable optional features.

## License

Source code and documentation are licensed under GPL-3.0-or-later, copyright
2026 Blucy1004. FIRST_NET v5 weight files remain separately licensed and are
excluded from redistribution unless accompanied by an explicit weight license.

`CONTRIBUTING.md` includes a non-binding request that authors of statistically
stronger forks share patches, source, benchmark conditions, or findings with
the upstream project. This request is not an additional GPL condition.

## Optimization

`Position::doMove` now accepts an internal `preserveAccumulator` policy. The
public/default path retains the existing byte-preserving child-state contract.
Search, move-generation legality probes, MovePicker probes, and SEE scratch
positions pass `false`, which avoids copying roughly 2 KiB of NNUE accumulator
storage into a child that is immediately invalidated.

Correctness basis:

- `previous` remains the authoritative parent state.
- `NNUE::Network::updateAccumulatorAfterMove` copies the parent accumulator
  before applying feature deltas.
- A skipped-copy child has `validMask = 0` and inherits only the generation.
- Undo rebinds the Position to the untouched parent state.
- Engine game-history calls continue using the default preserving path.

No Elo or NPS gain is claimed until reproduced on the target Windows machine.

## Release gate

The prior six-game, 50 ms-per-move classical-only smoke test ended 0-6 against
the frozen reference. Six games are not an Elo estimate, but the result remains
a playing-strength warning. Public source publication is technically possible;
calling the build strength-equivalent to v2.62 or promoting it as a stable
release is not supported by current evidence. A larger paired rerun should be
performed before removing the release-candidate label.
