# Bundled network

`firstnet_v5_10b.snnue` is the Q engine's default network.  Q defaults to
`UseNNUE=true`, and this network is interpreted as a residual correction
(`teacher - classical`).  Packaging will place it beside the executable; an
explicit `EvalFile` path may override that location.

Artifact facts:

```text
bytes   12585424
format  SBNNUE2 v2, Architecture A/256
sha256  E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656
```

The bundled copy and the originally supplied file have the same SHA-256:

```text
E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656
```

See `PROVENANCE.md` for the owner's provenance statement and the distinction
between dataset licensing, authorship of the trained weights, permission to
bundle this file, and the final weight-license declaration.
