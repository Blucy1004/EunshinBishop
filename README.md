# EunshinBishop

`EunshinBishop 5.0.0-rc.1 Q` -- a from-scratch, modular UCI chess engine.
This is the "Q Architecture" migration of a supplied single-file
`SniperBishop`/`EunshinBishop` v2.62 reference engine; the reference source
is preserved byte for byte under `reference/source` and is never edited.

은신비숍은 한국산 UCI 체스 엔진이며, 이 버전은 기존 SniperBishop 싱글파일 구조를 리뉴얼한 Q 아키텍처 버전입니다.

**은신비숍을 추후 CCRL 등 해외 포럼에 공개할 계획이라 README를 영어로 썼습니다만 제 영어 실력이 그렇게 좋지 않기에 AI의 도움을 받았습니다**

## 1. About

Q reimplements the frozen reference as separate `core` (bitboards, attacks,
Zobrist, typed `Move`), `position` (mailbox + bitboard `Position`,
`StateInfo` chain, move generation), `engine` (ownership boundary, validated
options), `eval` (classical + NNUE evaluators), `search` (PVS, TT, move
ordering, time management), and `uci` modules, with an explicit rule that
`core`/`position` never depend on `search`, `engine`, or `uci`. See
`docs/Architecture.md` for the full module map and `docs/Migration.md` for
what maps to what from the original single file.

## 2. Features

- Explicit two-byte typed `Move` encoding; castling, en passant, and
  promotion kind are never inferred from geometry.
- Caller-owned `StateInfo` make/unmake chain, four-way clustered
  transposition table, lazy staged `MovePicker`, worker-owned history
  tables with gravity-bounded updates.
- Classical MG/EG evaluator and a FIRST_NET v5 residual-correction NNUE
  evaluator, with a safe classical fallback if the network is missing or
  unreadable.
- Iterative deepening with aspiration windows, PVS/alpha-beta, null move,
  LMR, IIR, futility pruning, and an AEGIS instability-response policy.
- An independent, pin/king-legality/en-passant/promotion-correct strict SEE
  module (optional qsearch pruning, off by default) and LIMBO, a bounded
  frontier verification policy (optional, off by default).
- Asynchronous UCI frontend (`stop`/`quit` never block on a running search)
  with a `TimeManager` that never exceeds its hard deadline.

See `CHANGELOG.md` for the versioned list this is generated from.

## 3. Current development status

This is a pre-release development checkpoint, **not** a `v5.0.0` release.
Checkpoints 1-2 (core types, ownership boundary) and checkpoint 3 (evaluator,
search, `Engine::go`, UCI) are implemented and independently re-verified;
checkpoint 4 adds SEE and LIMBO. See `docs/Checkpoint2_Report.md` through
`docs/Checkpoint4_Report.md` for what was actually tested, with SHA-256
hashes for every reported binary.

**No Elo, fixed-depth search-equivalence, or public-release claim is made.**
`docs/Checkpoint3_Report.md` records a fixed-depth comparison between Q and
the frozen reference at Kiwipete depths 6/8/10: the two searches agree at
depth 6 and diverge past it by design (a different search algorithm and a
different, residual-correction NNUE equation), which is evidence about the
current gap, not a strength or equivalence claim. A six-game classical-only paired smoke test was run after Checkpoint 8; Q
scored 0-6 against the frozen reference under a 50 ms-per-move fast control.
This is not enough for Elo, but it fails the release gate. `SEEPruning=true`
and `LIMBO=true` still lack strength testing. See `docs/Build.md` for the full pre-release checklist and
exactly what is still missing before any `v5.0.0` tag.

## 4. Supported platforms

| Platform | Status |
|---|---|
| Windows x64 (MSVC) | Built and tested every checkpoint |
| Linux x64 (GCC / Clang) | GCC 14.2 and Clang 17 Release builds verified; no-network CTest passes |

## 5. Quick start

```powershell
cmake -S . -B build-vs -A x64
cmake --build build-vs --config Release --parallel
.\build-vs\Release\EunshinBishop.exe --version
echo "uci`nisready`nposition startpos`ngo depth 6`nquit" | .\build-vs\Release\EunshinBishop.exe
```

`EunshinBishop --help` prints the CLI flags; with no arguments the engine
speaks UCI on stdin/stdout, meant to be driven by a GUI or script.

## 6. Build

See `docs/Build.md` for full platform-by-platform instructions, the
building-without-a-network path, and the pre-release checklist. Short
version for Windows/MSVC:

```powershell
cmake -S . -B build-vs -A x64
cmake --build build-vs --config Release --parallel
ctest --test-dir build-vs -C Release --output-on-failure
```

## 7. Registering with a GUI

Point the GUI's "add engine" dialog at the built `EunshinBishop.exe`
(`build-vs\Release\EunshinBishop.exe` for the command above). No special
working directory is required beyond having the `.snnue` network file (if
any) placed next to the executable -- see the next section.

## 8. Placing the NNUE network

Place `firstnet_v5_10b.snnue` next to `EunshinBishop.exe`, or point
`EvalFile` at another path via `setoption`. Without it, the engine falls back
to classical evaluation and reports why via `info string`; it does not fail
to start. See `networks/README.md` and `networks/PROVENANCE.md` for the
network's provenance, and `LICENSE` for its separate, currently unpublished redistribution license.

## 9. Key UCI options

| Option | Default | Meaning |
|---|---:|---|
| `Hash` | 256 | transposition table size, MiB |
| `UseNNUE` | true | `false` uses the classical evaluator only |
| `EvalFile` | `firstnet_v5_10b.snnue` | network path, resolved next to the executable |
| `SEEPruning` | false | strict-SEE qsearch pruning; no strength evidence yet |
| `LIMBO` | false | bounded frontier verification extension; no strength evidence yet |

Full option and `go`-limit reference: `docs/UCI.md`.

## 10. Testing

```powershell
ctest --test-dir build-vs -C Release --output-on-failure
```

runs both `EunshinBishopQCoreTests` (perft, make/unmake, TT, MovePicker, SEE,
...) and `EunshinBishopQIntegrationTests` (evaluator, search, UCI). See
`docs/Testing.md` for what each covers and what is intentionally not yet
tested (longer time-control strength batches and optional-feature strength).

Example of the kind of claim this project records instead of an unverified
Elo number:

```text
Kiwipete, depth 6, Threads 1, Hash 256 MiB, UseNNUE=true:
Q: bestmove e2a6, score -101 cp, 122609 nodes
v2.62 reference: bestmove e2a6, score -15 cp, 101323 nodes
(docs/Checkpoint3_Report.md)
```

## 11. Known limitations

- No Elo or search-equivalence claim for Q. The only post-optimization paired
  smoke test is 0-6 against the frozen reference and blocks the RC release.
- Pure classical evaluation strengthening has not started
  (`docs/ClassicalEvalBacklog.md`).
- `SEEPruning=true` and `LIMBO=true` are correctness-tested (legal PV, no
  crash) but not strength-tested; their defaults should not change without
  paired-game evidence.
- GCC 14.2 and Clang 17 no-network Release builds are verified, but networked
  Linux integration still depends on a separately supplied licensed weight file.
- Lazy SMP / multi-threaded search is not implemented (`Threads` is not a
  UCI option).
- `ponderhit`, `register`, and MultiPV are not implemented.
- The source code and documentation are GPL-3.0-or-later. FIRST_NET v5 network
  weights remain separately licensed and are not distributed without an
  explicit weight-file license.

## 12. License

EunshinBishop source code and documentation are licensed under the GNU
General Public License v3.0 or later (`GPL-3.0-or-later`). Modified versions
may be used and distributed under the GPL, including commercially, but
distributed derivative binaries must be accompanied by corresponding source
under the same GPL terms. FIRST_NET v5 weight files are licensed separately
and are not included unless an explicit weight-file license accompanies them.

## 13. Credits

- `Blucy1004` -- project owner and author of the frozen `SniperBishop`/
  `EunshinBishop` v2.62 reference this migration is built from
  (`reference/source`, `reference/README.md`).
- FIRST_NET v5 network training data: the project owner identifies its
  source as the public Lichess database, CC0-licensed
  (`networks/PROVENANCE.md`); the trained weight file requires a separate outbound license and is not
  included in the public source tree.
- Search and evaluation techniques reference commonly known chess
  programming ideas (staged move ordering, PVS, aspiration windows, SEE
  swap-list evaluation, NNUE-style residual correction); see
  `CONTRIBUTING.md` for the distinction this project draws between using a
  known technique and copying another project's specific implementation.

---

Internal migration notes (architecture, checkpoint reports, comparisons
against the frozen reference) live in `docs/`; the ones a third-party clone
needs are `docs/Build.md`, `docs/UCI.md`, and `docs/Testing.md`.
