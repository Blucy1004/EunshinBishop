# EunshinBishop Phase 5 Q

This directory is the isolated Q Architecture migration of the supplied
single-file EunshinBishop engine.  The reference source is preserved byte for
byte under `reference/source`; it is never edited during the migration.

## Current checkpoint

Checkpoint 2 adds the structural portion of specification items 7 through 12:
node-local `SearchStack`, worker-owned histories/state/stats, the `Engine`
ownership boundary, validated immutable option snapshots, a genuinely lazy
staged `MovePicker`, and a four-way clustered transposition table.  The earlier
strong types, explicit 16-bit move, hybrid board, and caller-owned
`Position` / `StateInfo` implementation remain under the same tests.

Evaluator ownership, `Engine::go`, PVS/qsearch integration, fixed-depth search
parity, and `TimeManager` cannot be completed honestly before specification
items 13 through 17 and 21.  The present Worker and Engine are therefore an
executable ownership foundation, not a dummy chess search.  There is not yet a
standalone `EunshinBishop.exe` in this checkpoint.

Q uses the supplied FIRST_NET v5 as a residual-correction network.  The
implemented option defaults are `UseNNUE=true`, residual mode, and
`EvalFile=firstnet_v5_10b.snnue`; that policy is isolated from the workspace's v2.8
experimental lineage.  The exact network is bundled under `networks/` after
the user confirmed that it is their own trained artifact, produced from the
public Lichess database they identify as CC0.  The provenance record separates
that training-data statement from the still-to-be-declared weight license.

## Build this checkpoint

Open an **x64 Developer PowerShell for Visual Studio**, then run the tested
multi-configuration MSBuild path:

```powershell
cmake -S . -B build-vs -A x64
cmake --build build-vs --config Release --parallel
ctest --test-dir build-vs -C Release --output-on-failure
```

Run the core verification executable directly from the generated directory:

```powershell
.\build-vs\Release\EunshinBishopQCoreTests.exe
```

The bundled network is copied beside each built test executable and checked by
hash.  No inference is performed until the evaluator checkpoint.

The authoritative architecture and migration notes are in
`docs/Architecture.md`, `docs/Migration.md`, `docs/Q0_Reference.md`, and
`docs/TestReport.md`, and `docs/Checkpoint2_Report.md`.

## Status and claims

This is a development checkpoint, not `v5.0.0-rc.1`.  No Elo, fixed-depth
search-equivalence, NNUE-inference, or public-release claim is made.  The
reference engine's observed results are evidence about the reference artifact
only, not the partially migrated Q core.
