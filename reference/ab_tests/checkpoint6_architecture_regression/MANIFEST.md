# Checkpoint 6 architecture-regression A/B evidence

Run 2026-07-19. Small-scale paired test, `docs/ABTesting.md`'s
architecture-regression command, using `cutechess-cli` 1.5.1 (win64 build)
on a single machine (`-concurrency 1`, fully sequential).

## Command

```powershell
cutechess-cli.exe `
  -engine name="v2.62_reference" cmd="reference/bin/EunshinBishop_v2.62_reference.exe" proto=uci option.Hash=64 `
  -engine name="Q_checkpoint6" cmd="build-vs/Release/EunshinBishop.exe" proto=uci option.Hash=64 `
  -each tc=10+0.1 `
  -openings file=openings_v1.epd format=epd order=sequential `
  -rounds 12 -games 2 -repeat `
  -concurrency 1 `
  -pgnout architecture_regression.pgn
```

`tc=10+0.1` = 10 seconds base time + 0.1 second increment per move.

## Engines

| Engine | Binary | SHA-256 |
|---|---|---|
| `v2.62_reference` | `reference/bin/EunshinBishop_v2.62_reference.exe` | `AFD22FAA10F93F28913E655BA5380BF8A637F37A51AB46F32B04BE1F7183BB3A` (matches `docs/Q0_Reference.md` exactly -- the frozen reference, unmodified) |
| `Q_checkpoint6` | `build-vs/Release/EunshinBishop.exe` | `15BDAD4613C2423D4A4A97586EFEFE429B7F154D96D65282E54B869BD0B0F2FA` |

Both loaded `firstnet_v5_10b.snnue` successfully before this batch was run
(checked interactively from the exact working directory/relative paths
`cutechess-cli` uses):

- `v2.62_reference`: `info string NNUE ready: ...\reference\bin\firstnet_v5_10b.snnue`,
  whole-file SHA-256 `E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656`.
- `Q_checkpoint6`: `nnuecheck` reported `network loaded`, payload SHA-256
  `D8157A4943D889403C46A8F6C2D4A6D0EF9C12E86FC0AA0103812D6239F8F49D`.

Neither engine printed a load-failure/fallback message in that pre-batch
check. `cutechess-cli`'s own log
(`architecture_regression_out.log`) does not capture each engine's UCI
`info string` output, so it cannot independently confirm NNUE state
game-by-game; the stronger evidence is that both engine processes were
observed via `Get-Process` to run as the same PID, with monotonically
increasing CPU time, for the entire 24-game batch (`cutechess-cli` does not
restart an engine between games of one match) -- so the successful load
confirmed immediately before the batch started covers the whole batch, not
just that one check.

## Options

`Hash=64` set explicitly on both engines (matching the reference's own
default; overriding Q's own default of 256). No other option was set on
either engine -- both ran with every other option, including Q's
`SEEPruning=false` and `LIMBO=false`, at their unmodified defaults.

## Conditions

- Opening suite: `openings_v1.epd` (12 named positions, `order=sequential`,
  copied unmodified from the sibling `SniperBishop` project's
  `tests/openings_v1.epd`).
- Color alternation: `-repeat` replayed every opening once per color.
- Same time control both sides, same single-threaded/single-machine
  execution (`-concurrency 1`), no parallel contention.

## Result

```text
Score of v2.62_reference vs Q_checkpoint6: 21 - 1 - 2  [0.917] 24

v2.62_reference playing White: 11 - 0 - 1  (12 games)
v2.62_reference playing Black: 10 - 1 - 1  (12 games, i.e. Q_checkpoint6 as White: 1W 10L 1D)
White vs Black overall: 12 - 10 - 2  [0.542]

Elo difference: 416.6 +/- nan (LOS 100.0%, DrawRatio 8.3%)
```

Outcome types (from cutechess-cli's own tally): 21 games ended in
checkmate, 2 in the 50-move rule, 0 by adjudication, 0 by time forfeit, 0 by
crash, 0 by illegal move.

**This is not an Elo determination.** 24 games is two orders of magnitude
below `docs/ABTesting.md`'s stated 200-game initial batch (extended to
>=1000 for a promising candidate); the reported "Elo difference: 416.6"
carries an undefined (`nan`) confidence interval precisely because the
score is this lopsided at this sample size, which is a symptom of too few
games, not a precise measurement. The only things this result actually
supports: (1) both engines played the full batch without any crash, time
forfeit, or illegal move; (2) the current Q build lost the large majority
of games against the frozen reference under a fast time control, consistent
with `docs/Checkpoint3_Report.md`'s Kiwipete finding that Q's search and
evaluation diverge from the reference's and have not been strength-tuned;
(3) Q is not always losing -- it won one game and drew two, so the gap is
not absolute.

## Files in this directory

- `architecture_regression.pgn` -- all 24 games, with per-move eval/time
  annotations (SHA-256 `EC21BEBF72BF230FC409604AF0C8BE4EAC58B1B3F25AF8D3B60EE50231FFD5B6`).
- `architecture_regression_out.log` -- full `cutechess-cli` stdout, including
  the per-game and final summary lines quoted above
  (SHA-256 `DAB202B7C95A12D3EA23939996C7F2B774CF606BF8FFB4EB8800FE6E40F928AE`).
- `openings_v1.epd` -- the exact opening suite used
  (SHA-256 `518B5894705D83770770FA30857B9C52A1C452FAFCDEFD6E1C895085154AC7B6`).
