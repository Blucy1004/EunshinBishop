# Optional local network

The repository intentionally does **not** distribute `firstnet_v5_10b.snnue`
until the project owner chooses an outbound license for the trained weights.

To use NNUE locally, place an authorized copy beside the executable or at:

```text
networks/firstnet_v5_10b.snnue
```

Then either keep the default `EvalFile=firstnet_v5_10b.snnue` when the file is
beside the executable, or set `EvalFile` to its explicit path. Without the
file, the engine reports the reason and falls back to classical evaluation.

Known private artifact identity (for verification only):

```text
bytes   12585424
format  SBNNUE2 v2, Architecture A/256
sha256  E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656
```

See `PROVENANCE.md` for the ownership and training statement. The hash above
is not a redistribution grant.
