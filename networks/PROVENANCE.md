# FIRST_NET v5 provenance record

Recorded on 2026-07-18 from the project owner's statement in the development
task.

## Ownership and training statement

The project owner states that `firstnet_v5_10b.snnue` is their own trained
network and that its training source was the public Lichess database they
identify as CC0-licensed.  The owner explicitly authorized placing this
network inside the EunshinBishop Phase 5 Q project.

This file records that statement; it does not invent an upstream URL, database
snapshot date, extraction command, teacher configuration, training seed, or
weight license that was not supplied.  Those reproducibility details should be
added when available.

## Artifact identity

```text
filename  firstnet_v5_10b.snnue
bytes     12585424
sha256    E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656
container SBNNUE2 v2, Architecture A/256
role      teacher-minus-classical residual correction
```

## Licensing separation

- The owner's statement identifies the source database as CC0.
- The trained binary is identified as the owner's original network.
- The owner authorized bundling the binary in this project.
- A public release must still name the license the owner grants specifically
  for the trained weight file; the dataset's CC0 status does not automatically
  choose that outbound weight license.

Until the final release-preparation checkpoint records that outbound license,
do not describe the network binary itself as CC0 merely by inference.
