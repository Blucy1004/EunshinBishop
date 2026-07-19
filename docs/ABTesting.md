# A/B testing (specification section 28)

This document records the actual `cutechess-cli` command lines for every
comparison the specification asks for. **No results from any of these runs
are recorded in this project yet** -- see `docs/Checkpoint4_Report.md` and
`HANDOFF.md` for what evidence currently exists (none, for any of the
comparisons below). Do not treat the presence of a runnable command as
evidence the comparison has been run.

## Shared conditions (apply to every comparison below)

```text
Threads=1
same Hash on both sides
opening pairs (each opening played from both colors)
10+0.1
initial batch: 200 games
promising candidates: extend to >=1000 games before trusting the result
adjudication disabled
crash / time-loss / illegal-move counted separately from W/L/D
```

`cutechess-cli` is available locally in this workspace's parent Downloads
folder (`cutechess-1.5.1-win64.zip`) but is not bundled inside this project
tree; point `-engine cmd=` at wherever it is extracted. Replace
`<opening-suite>.pgn` with an actual opening book/suite file -- none is
bundled in this repository yet.

## 1. Architecture regression: old single-file vs Q baseline

```text
old single-file reference
vs
Q Architecture baseline
```

```powershell
cutechess-cli.exe `
  -engine name="v2.62 reference" cmd="reference\bin\EunshinBishop_v2.62_reference.exe" proto=uci option.Hash=64 `
  -engine name="Q baseline" cmd="build-vs\Release\EunshinBishop.exe" proto=uci option.Hash=64 `
  -each tc=10+0.1 -rounds 200 -games 2 -repeat `
  -openings file=<opening-suite>.pgn format=pgn order=sequential `
  -resign movecount=0 -draw movenumber=0 `
  -pgnout results\architecture_regression.pgn `
  -concurrency 1
```

Both engines' `Hash` option must match; the reference does not expose all of
Q's options (no `SEEPruning`/`LIMBO`/`AEGIS`), so this comparison is
necessarily "reference defaults" vs "Q defaults," not a feature-isolated
test -- see `docs/Checkpoint3_Report.md` for why the two are expected to
diverge (different search algorithm, different NNUE equation), which this
regression is meant to quantify in games rather than fixed-depth output.

## 2. Classical strengthening: Q baseline classical vs Q improved classical

Only meaningful once at least one group from `docs/ClassicalEvalBacklog.md`
has been implemented behind its own build/option flag. Placeholder command
(update the option name once such a flag exists):

```powershell
cutechess-cli.exe `
  -engine name="Q classical baseline" cmd="build-vs\Release\EunshinBishop.exe" proto=uci option.Hash=64 option.UseNNUE=false `
  -engine name="Q classical improved" cmd="build-vs\Release\EunshinBishop.exe" proto=uci option.Hash=64 option.UseNNUE=false option.<NewFlag>=true `
  -each tc=10+0.1 -rounds 200 -games 2 -repeat `
  -openings file=<opening-suite>.pgn format=pgn order=sequential `
  -resign movecount=0 -draw movenumber=0 `
  -pgnout results\classical_strengthening.pgn `
  -concurrency 1
```

## 3. Residual effect: Q classical vs Q classical + v5 residual

```powershell
cutechess-cli.exe `
  -engine name="Q classical only" cmd="build-vs\Release\EunshinBishop.exe" proto=uci option.Hash=64 option.UseNNUE=false `
  -engine name="Q classical+residual" cmd="build-vs\Release\EunshinBishop.exe" proto=uci option.Hash=64 option.UseNNUE=true option.NNUEOutputMode=Residual option.EvalFile=firstnet_v5_10b.snnue `
  -each tc=10+0.1 -rounds 200 -games 2 -repeat `
  -openings file=<opening-suite>.pgn format=pgn order=sequential `
  -resign movecount=0 -draw movenumber=0 `
  -pgnout results\residual_effect.pgn `
  -concurrency 1
```

## 4. Per-feature isolation: IIR / SEEPruning / AEGIS / LIMBO

One feature at a time, each candidate against the same Q baseline with that
one option flipped:

```powershell
$features = @("IIR", "SEEPruning", "AEGIS", "LIMBO")
foreach ($feature in $features) {
  cutechess-cli.exe `
    -engine name="Q baseline" cmd="build-vs\Release\EunshinBishop.exe" proto=uci option.Hash=64 `
    -engine name="Q $feature=true" cmd="build-vs\Release\EunshinBishop.exe" proto=uci option.Hash=64 "option.$feature=true" `
    -each tc=10+0.1 -rounds 200 -games 2 -repeat `
    -openings file=<opening-suite>.pgn format=pgn order=sequential `
    -resign movecount=0 -draw movenumber=0 `
    -pgnout "results\feature_$feature.pgn" `
    -concurrency 1
}
```

`SEEPruning=true` and `LIMBO=true` are exactly the two flags
`docs/Checkpoint4_Report.md` says have no A/B evidence yet; this is the
command that would produce it. Do not flip either default to `true` from a
single 200-game batch -- extend to >=1000 games first, per the shared
conditions above, and record the result the same way
`docs/Checkpoint2_Report.md` through `docs/Checkpoint4_Report.md` already do
(command, hashes, options, W/L/D, confidence interval, color split, time
forfeits/crashes/illegal moves).

## Recording a result

When any of the above is actually run, add a dated entry to a new
`docs/ABResults.md` (not this file -- this file is the command reference,
that one would be the evidence log) with the exact fields `AGENTS.md`'s
experiment discipline requires: command, hashes, active options, opening
suite/hash, seed, W/L/D, confidence interval, color split, depth/nodes/move
time if fixed, time forfeits, crashes, and illegal moves. No such file
exists yet because no comparison has been run.
