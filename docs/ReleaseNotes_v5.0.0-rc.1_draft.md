# `v5.0.0-rc.1` release deliverables (draft)

This is a draft prepared per specification item 31. **It does not declare
the project ready to publish.** Every unchecked row in `docs/Build.md`'s
pre-release checklist blocks the actual tag; this document exists so the
mechanical release steps are ready to run the moment those rows are checked,
not to suggest they already are.

## 1. Recommended Git tag

```text
v5.0.0-rc.1
```

Only after every row in `docs/Build.md`'s checklist that applies to an RC is
checked, at minimum: build/test/perft/UCI-smoke/network-fallback. Do not tag
before then -- `.github/workflows/release.yml` triggers packaging and a
(draft) GitHub Release the moment a `v*` tag is pushed.

## 2. GitHub Release title

```text
EunshinBishop 5.0.0-rc.1 -- Q Architecture
```

## 3. Release notes draft

```markdown
## EunshinBishop 5.0.0-rc.1 -- Q Architecture

First public release candidate of the Q Architecture migration: a
from-scratch, modular reimplementation of the frozen single-file
EunshinBishop/SniperBishop v2.62 reference engine.

This is a release **candidate**, not a strength claim. Q's search and the
frozen v2.62 reference diverge past a shallow fixed depth by design
(different search algorithm, different NNUE blending equation); no Elo or
paired-game evidence exists for either engine. See CHANGELOG.md and
docs/Checkpoint2_Report.md through docs/Checkpoint4_Report.md for exactly
what was implemented and tested, with SHA-256 hashes for every reported
binary.

### Known issues
- No Elo / paired-game / search-equivalence evidence, in either direction.
- GCC and Clang builds are CI-scaffolded but not yet verified on a physical
  machine.
- Pure classical evaluation strengthening has not started.
- SEEPruning and LIMBO default to off; no strength evidence exists for
  turning them on.
- Outbound source and network-weight licenses are undecided (see LICENSE).

See CHANGELOG.md for the full list of what changed.
```

## 4. Built asset list (produced by `release.yml`)

```text
EunshinBishop-v5.0.0-rc.1-windows-x64.zip
  EunshinBishop.exe
  README.txt
  LICENSE
  networks/README.txt
EunshinBishop-v5.0.0-rc.1-linux-x64.tar.gz
  EunshinBishop
  README.txt
  LICENSE
  networks/README.txt
SHA256SUMS.txt
```

The `.snnue` network is **not** included in either package until its
redistribution license is confirmed (see item 8 below); packages ship
`networks/README.txt` with placement instructions instead.

## 5. SHA-256 checksum command

Already automated in `release.yml`'s `publish` job
(`sha256sum * > SHA256SUMS.txt` over the downloaded build artifacts). To
reproduce locally after downloading the release assets:

```bash
sha256sum EunshinBishop-v5.0.0-rc.1-windows-x64.zip \
          EunshinBishop-v5.0.0-rc.1-linux-x64.tar.gz > SHA256SUMS.txt
```

## 6. Pre-upload checklist

Use `docs/Build.md`'s checklist verbatim; do not push the tag until the RC
rows are checked and their evidence is linked from a checkpoint report.

## 7. Files that must not be published yet

- `LICENSE` in its current placeholder form must be replaced with an actual
  license before the repository is made public, not just before the tag.
- `reference/source/SniperBishop_v2.62_Phase5_FIRSTNET_v5.cpp` (the frozen
  reference) and `reference/bin/EunshinBishop_v2.62_reference.exe`: per
  `reference/README.md`, "Public redistribution rights for the source and
  network have not been audited. Do not publish this directory until the
  provenance and license review required by specification item 31 is
  complete." This blocks publishing the whole `reference/` tree, not just
  the network.
- `networks/firstnet_v5_10b.snnue`: see item 8.
- Any personal/workstation-specific paths accidentally captured in a
  committed log or manifest (none currently known, but re-check
  `reference/bin/*.build.json` and similar before a public push).

## 8. Network redistribution warning

**Do not publish or package `firstnet_v5_10b.snnue` until its outbound
license is explicitly confirmed by the project owner.**
`networks/PROVENANCE.md` records that the training data is stated to be
CC0, and that the owner authorized bundling the file inside this *private*
project tree -- neither statement decides the *trained weight file's* own
outbound redistribution license for a *public* release. `LICENSE` restates
this as a release blocker.

## 9. RC -> stable promotion conditions

Promote `v5.0.0-rc.1` to stable `v5.0.0` only after:

1. Every row in `docs/Build.md`'s pre-release checklist is checked, not just
   the RC subset.
2. A completed Cute Chess paired-game regression (>=200 games, both colors,
   fixed opening suite, adjudication off, `10+0.1`) between Q and the frozen
   v2.62 reference, recorded with the same rigor as `Checkpoint2_Report.md`
   through `Checkpoint4_Report.md` (command, hashes, options, W/L/D,
   confidence interval).
3. No severe regression or crash found in that regression; if one is found,
   fix the RC and re-run rather than shipping stable.
4. Source and network licenses finalized and `LICENSE` replaced with real
   text.
5. At least one full GCC and one full Clang build verified green in CI, not
   just configured.

Do not declare a release ready before its code and tests are actually
finished; this document is deliberately dated and will read as stale the
moment any of the above changes -- update it, don't trust it blindly.
