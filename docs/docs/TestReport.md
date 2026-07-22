# Checkpoint 1 test report

Date: 2026-07-18

Decision: **ACCEPT** for Q0 and specification items 1 through 6 only.

This decision covers the frozen reference, dependency-leaf core, strong chess
types, hybrid board, explicit 16-bit moves, `Position` / `StateInfo`, FEN,
move generation, and exact state restoration.  It is not an NNUE inference,
search-equivalence, Elo, release-candidate, or public-redistribution decision.

## Toolchain and commands

Both configurations used MSVC 19.51.36248 x64 through the Visual Studio 18
developer environment, CMake's Ninja generator, C++17, `/W4`, and
`/permissive-`.  Release additionally used the CMake Release optimization and
`NDEBUG`; Debug retained all internal consistency assertions.

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-msvc --parallel
ctest --test-dir build-msvc --output-on-failure

cmake -S . -B build-msvc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-msvc-debug --parallel
.\build-msvc-debug\EunshinBishopQCoreTests.exe
```

## Results

| Gate | Release | Debug |
|---|---:|---:|
| Configure/build | PASS | PASS |
| CTest | 1/1 PASS, 2.17 s | direct test PASS |
| Test assertions | 518,978 PASS | 518,978 PASS |
| Internal debug consistency assertions | compiled out | PASS |
| Process result | exit 0 | exit 0 |
| Observed direct runtime | 2.2 s | 115.7 s |

No compiler warning was observed in either Q build.  The frozen single-file
reference's three pre-existing warnings are recorded separately in
`Q0_Reference.md`.

## Perft gates

| Position | d1 | d2 | d3 | d4 | d5 |
|---|---:|---:|---:|---:|---:|
| startpos | 20 | 400 | 8,902 | 197,281 | 4,865,609 |
| canonical Kiwipete | 48 | 2,039 | 97,862 | 4,085,603 | — |
| EP/check test | 14 | 191 | 2,812 | 43,238 | — |
| promotion/castling test | 6 | 264 | 9,467 | 422,333 | — |

Canonical Kiwipete:

```text
r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
```

## Covered invariants

- fixed raw encoding, lossless raw/UCI round-trip, explicit castle, en
  passant, and all four promotion types;
- every relevant rook and bishop magic-table occupancy subset against an
  independent slow-ray oracle;
- mailbox, color bitboards, type bitboards, occupied bitboard, side to move,
  checkers, position key, pawn key, and material key consistency;
- white and black en passant, both colors and both sides of castling, all 16
  quiet/capture and white/black promotion combinations;
- king-capture rejection, pins, pinned en passant, reserved move types,
  blocked and attacked-transit castling, and mutation-free illegal moves;
- transactional malformed-FEN rejection, null move, repetition, rule 50, and
  two independent `Position` instances;
- 2,048 deterministic sequences of up to 50 legal plies: 102,400 committed
  makes and 102,400 exact unmakes, including parent pointer and 2x256 NNUE
  accumulator sentinel restoration.

The accumulator check verifies ownership and bit-exact restoration only.  The
actual FIRST_NET v5 scratch/incremental inference cross-check belongs to the
evaluation/NNUE checkpoint.

## Artifact hashes

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Release core tests | 84,992 | `6279DA59722D78758BCCD51292FC1747BA4FD08A4115A022613E8BD7DE9DE025` |
| Debug core tests | 256,512 | `3D1849CF2418D6C471C8B11713688FB347AE8B1F96648437773129991D2035EC` |

## Deferred gates

GCC and Clang builds, complete NNUE loading and inference, residual equation,
clustered TT, search, UCI smoke tests, time control, A/B games, licensing, and
release packaging are intentionally deferred to their numbered checkpoints.
The Q policy remains `UseNNUE=true` by default with
`EvalFile=firstnet_v5_10b.snnue`; there is no evaluator or option object in
checkpoint 1 yet, so this is a frozen configuration requirement rather than a
claim that the partial core can run the network.
