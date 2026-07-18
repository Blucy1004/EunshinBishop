# Q0 reference freeze

Captured on 2026-07-18 before Q source changes.

## Artifacts

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| supplied source | 198,671 | `F2C81769951B936D82D5A5B4F4A8BD56AAC28AE258F983578371101F16328A9E` |
| local reference executable | 538,624 | `AFD22FAA10F93F28913E655BA5380BF8A637F37A51AB46F32B04BE1F7183BB3A` |
| supplied FIRST_NET v5 | 12,585,424 | `E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656` |

The `.snnue` header and payload are canonical SBNNUE2 v2 Architecture A/256;
the recorded header payload digest matches the actual payload.  This validates
container integrity, not redistribution permission or playing strength.

After this freeze was recorded, the project owner stated that the weights are
their own network trained from the public Lichess database they identify as
CC0 and authorized bundling the file in Q.  The exact copied artifact and the
licensing distinction are recorded in `../networks/PROVENANCE.md`.

## Build

MSVC 19.51.36248 x64 Release was invoked through the workspace build wrapper
with `/std:c++17 /EHsc /W4 /O2 /DNDEBUG` and an embedded source hash.  The build
completed with the reference's two C4244 warnings and one C4505 warning.  The
machine-readable manifest is adjacent to the reference executable.

## UCI identity and defaults

```text
id name Sniper Bishop 2.62 Phase 5 FIRST_NET v5
id author Blucy1004
Hash=64
UseNNUE=true
EvalFile=firstnet_v5_10b.snnue
NNUEBlend=35
HybridGuard=true
SEEPruning=false
IIR=false
AEGIS=false
LIMBO=false
```

Q will keep NNUE enabled by default but will later replace the absolute-style
blend with the explicitly requested residual-correction equation.

## Correctness observations

| Position | Depth | Nodes |
|---|---:|---:|
| startpos | 1 | 20 |
| startpos | 2 | 400 |
| startpos | 3 | 8,902 |
| startpos | 4 | 197,281 |
| startpos | 5 | 4,865,609 |
| Kiwipete | 1 | 48 |
| Kiwipete | 2 | 2,039 |
| Kiwipete | 3 | 97,862 |

Canonical Kiwipete FEN:

```text
r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
```

Static Kiwipete observation:

```text
selected = +42 cp STM
Classical = +103 cp STM
FIRST_NET incremental = -131 cp STM / White POV
FIRST_NET scratch = -131 cp White POV
incremental-scratch difference = 0
```

## Fixed-depth Kiwipete snapshots

Each depth was run after `ucinewgame` with the same default options.

| Depth | Best move | Score | Nodes | PV prefix |
|---:|---|---:|---:|---|
| 6 | `e2a6` | -15 cp | 101,323 | `e2a6 b4c3 d2c3 e6d5 e5g4 h3g2 f3g2` |
| 8 | `e2a6` | -15 cp | 2,649,523 | `e2a6 b4c3 d2c3 e6d5 e5g4 h3g2 f3g2 d5e4` |
| 10 | `e2a6` | -19 cp | 7,122,135 | `e2a6 b4c3 d2c3 e6d5 e5g4 h3g2 f3g2 d5e4 a6b7` |

These snapshots freeze the supplied reference.  They are not yet Q-equivalence
results and are not Elo evidence.
