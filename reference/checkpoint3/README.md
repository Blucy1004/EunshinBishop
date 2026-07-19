# Checkpoint 3 accepted baseline

Frozen on 2026-07-19 after independent re-verification of the checkpoint-3
handoff on a clean Windows MSVC toolchain, plus one closeout fix.

This directory is immutable migration evidence. Do not rebuild into it or
replace its files. `EunshinBishop_Q_Checkpoint3_source.zip` contains the exact
accepted `CMakeLists.txt`, project `README.md`, `src/`, `tests/`, `docs/`, and
`reference/README.md` as of commit `dd57b6e` (the checkpoint-2 handoff import
plus the `Position::castlingRights()` GCC enum/non-enum conditional fix). The
network weights are not duplicated in the archive; their accepted project copy
is identified below by hash, unchanged from checkpoint 2.

Accepted verification (Windows, MSVC 19.51.36248.0 x64, `Visual Studio 18
2026` generator):

```text
Release  519108 core checks PASS, 58 integration checks PASS, CTest 2/2
Debug    519108 core checks PASS, 58 integration checks PASS, CTest 2/2
0 compiler warnings in both configurations
UCI smoke (uci/isready/position startpos/go depth 4): bestmove e2e4 ponder e7e6
```

These counts match the independently reported Linux/CMake verification in
`../../docs/EunshinBishop_Q_13-18_handoff_status.md`-equivalent handoff notes
exactly (519,108 / 58 / 2-of-2), reproduced here on a second, independent
toolchain and OS.

Fixed-depth Kiwipete comparison against the frozen `reference/source` v2.62
artifact (both with NNUE loaded) is recorded in `docs/Checkpoint3_Report.md`;
Q is not yet claiming search-equivalence with the reference, only that both
searches are sane, legal, and terminate correctly at matching depths.

Artifact identities are recorded in `SHA256SUMS`. Unlike the checkpoint-2
archive, `docs/Checkpoint3_Report.md` was written after this snapshot was
frozen and therefore lives only in the current project tree
(`../../docs/Checkpoint3_Report.md`), not inside
`EunshinBishop_Q_Checkpoint3_source.zip`.
