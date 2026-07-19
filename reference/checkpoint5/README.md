# Checkpoint 5 accepted baseline

Frozen on 2026-07-19 immediately after specification section 26's
debug/diagnostic commands were implemented and sections 27/28 documented on
top of checkpoint 4.

This directory is immutable migration evidence. Do not rebuild into it or
replace its files. `EunshinBishop_Q_Checkpoint5_source.zip` contains the
exact accepted `CMakeLists.txt`, project `README.md`, `src/`, `tests/`,
`docs/`, and `reference/README.md` as of commit `fdfcbc6`. The network
weights are not duplicated in the archive; their accepted project copy is
identified below by hash, unchanged since checkpoint 2.

Accepted verification (Windows, MSVC 19.51.36248.0 x64, `Visual Studio 18
2026` generator):

```text
Release  519122 core checks PASS, 70 integration checks PASS, CTest 2/2
Debug    519122 core checks PASS, 70 integration checks PASS, CTest 2/2
0 compiler warnings in both configurations
perft 4 from startpos via the new UCI debug command: 197281 nodes (exact)
nnuecheck / nnueverify: real network load confirmed, incremental/scratch
accumulators agree exactly -- not a silent classical fallback
Kiwipete depth-6 node counts identical to every prior checkpoint baseline
```

See `../../docs/Checkpoint5_Report.md` for the full report, including the
tempo-bonus reconciliation fix found while building `evaldetail`, and what
remains explicitly out of scope (no cutechess-cli comparison has been run,
GCC/Clang still unverified, item 23 not started).

Artifact identities are recorded in `SHA256SUMS`.
