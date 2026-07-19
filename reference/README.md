# Frozen reference

`source/SniperBishop_v2.62_Phase5_FIRSTNET_v5.cpp` is an exact, unedited copy
of the user-supplied source.  Its SHA-256 is
`F2C81769951B936D82D5A5B4F4A8BD56AAC28AE258F983578371101F16328A9E`.

`bin/EunshinBishop_v2.62_reference.exe` is the local MSVC x64 reference build;
its adjacent JSON manifest records compiler, flags, source, and hashes.  These
reference artifacts must never be overwritten by Q builds.

Public redistribution rights for the source and network have not been audited.
Do not publish this directory until the provenance and license review required
by specification item 31 is complete.

`bin/firstnet_v5_10b.snnue` is a byte-identical copy of
`../networks/firstnet_v5_10b.snnue`
(`E30958DE815A8E6104B9EF3F734FDBF817B1D698BC3B74815B48596030DED656`), placed
here because the reference executable looks for its `EvalFile` default next
to itself, not next to the caller's working directory. Without it, the
reference silently falls back to classical evaluation and produces different,
non-representative fixed-depth results -- this bit checkpoint-3 verification
once already; see `docs/Checkpoint3_Report.md` for the corrected NNUE-enabled
comparison.
