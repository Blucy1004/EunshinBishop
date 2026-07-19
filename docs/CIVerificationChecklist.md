# CI verification checklist (post-push)

`.github/workflows/build.yml` has never actually executed on GitHub as of
this writing -- this machine has no local GCC/Clang and no network access to
run the workflow directly (that decision is recorded in `HANDOFF.md`). This
checklist is what to check the first time the workflow runs after a push,
so that a green checkmark is verified evidence, not assumed.

## Before pushing

- [ ] Confirm `networks/firstnet_v5_10b.snnue` is still committed (the
      no-network jobs below deliberately move it aside and restore it
      inside the same job; they do not remove it from git).
- [ ] Confirm the workflow file itself passes YAML validation (a syntax
      error fails silently as "no jobs ran" in some GitHub UI states --
      check the Actions tab shows 4 jobs queued: `windows-msvc` and 2x
      `linux` (gcc, clang), not 0).

## windows-msvc job

- [ ] `Configure` and `Build` succeed with **zero** compiler warnings.
      Compare the warning count to `docs/Checkpoint6_Report.md`'s MSVC
      column (0) -- any warning here is a CI-vs-local discrepancy worth
      investigating, not ignoring.
- [ ] `Run tests`: CTest reports 2/2 passed.
- [ ] `Perft smoke test`: prints a line starting with `PASS:`.
- [ ] `UCI smoke test`: both `uciok` and a `bestmove ` line appear.
- [ ] `No-network build sanity`: a `bestmove ` line appears with
      `UseNNUE=false` (network file still present, just disabled).
- [ ] `Configure/build/test with the network file genuinely absent`:
      - [ ] CTest passes with the network absent.
      - [ ] The job explicitly fails (by design, if this ever regresses) if
            `q_integration_tests` is listed by `ctest -N` -- confirm it was
            **not** listed, i.e. this step's own internal check passed.
      - [ ] A `bestmove ` line appears from `build-nonetwork\Release\EunshinBishop.exe`
            with no network file on disk at all (not just `UseNNUE=false`).
      - [ ] `networks/firstnet_v5_10b.snnue` is restored at the end of the
            step (the `finally`/`trap` ran) -- a later job step or a
            subsequent run should not find it missing.

## linux (gcc) and linux (clang) jobs

- [ ] Record the **exact** compiler version each job reports (add a
      `gcc --version` / `clang --version` step if the existing log does not
      already show it clearly enough to quote precisely -- do not write
      down an assumed version).
- [ ] Same checklist as `windows-msvc` above, run twice (once per
      compiler): configure/build warnings, CTest 2/2, perft smoke, UCI
      smoke, no-network sanity, and the genuinely-absent-network job.
- [ ] Compare GCC's and Clang's `-Wall -Wextra -Wpedantic -Wshadow
      -Wconversion` warning output against MSVC's `/W4 /permissive-` output
      from the same commit. A warning that only one toolchain emits is
      still worth a look even if it doesn't fail the build (this project's
      standing rule is zero warnings on every configured toolchain, not
      just MSVC).

## After a green run

- [ ] Update `docs/Checkpoint6_Report.md` (or open a new checkpoint report)
      with the actual GCC and Clang version strings, the actual CTest
      output, and a link to the successful run.
- [ ] Update `docs/Build.md`'s pre-release checklist: check off "GCC
      Release build succeeds" and "Clang Release build succeeds" only once
      this has actually been observed green, and cite the run.
- [ ] Update `HANDOFF.md`'s "Incomplete stages" section to remove the
      GCC/Clang line, or narrow it if only one of the two went green.

## If it goes red

Do not immediately "fix" by loosening a warning flag or deleting a step to
make it pass. Read the actual compiler diagnostic first -- GCC and Clang are
often stricter than MSVC about implicit conversions, signed/unsigned
comparisons, and enum handling; the `Position::castlingRights()` fix in
`docs/Checkpoint3_Report.md` is exactly this kind of finding, caught only
because it was independently checked rather than assumed to match MSVC's
warning set.
