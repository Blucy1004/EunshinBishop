# Checkpoint 4 accepted baseline

Frozen on 2026-07-19 immediately after specification items 19 (LIMBO) and 20
(SEE) were implemented on top of checkpoint 3.

This directory is immutable migration evidence. Do not rebuild into it or
replace its files. `EunshinBishop_Q_Checkpoint4_source.zip` contains the exact
accepted `CMakeLists.txt`, project `README.md`, `src/`, `tests/`, `docs/`, and
`reference/README.md` as of commit `387e8f1`. The network weights are not
duplicated in the archive; their accepted project copy is identified below by
hash, unchanged from checkpoints 2 and 3.

Accepted verification (Windows, MSVC 19.51.36248.0 x64, `Visual Studio 18
2026` generator):

```text
Release  519122 core checks PASS, 60 integration checks PASS, CTest 2/2
Debug    519122 core checks PASS, 60 integration checks PASS, CTest 2/2
0 compiler warnings in both configurations
Kiwipete depth-6 fixed-depth output byte-for-byte identical to checkpoint 3
with SEEPruning=false and LIMBO=false (both still the default): confirms
zero regression to the checkpoint-3 default search path.
```

See `../../docs/Checkpoint4_Report.md` for the full report, including what
items 19-22 cover and what remains explicitly out of scope (items 23-30,
GCC/Clang builds, and any A/B or paired-game evidence for SEEPruning=true or
LIMBO=true).

Artifact identities are recorded in `SHA256SUMS`.
