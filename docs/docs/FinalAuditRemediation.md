# Final audit remediation

This patch closes the non-owner-decision findings from the final release audit.

## Changes

- Removed the two tracked copies of `firstnet_v5_10b.snnue` from the public
  source tree and ignored future local copies until an outbound weight license
  is chosen.
- Added explicit `contents: write` permission to the release publish job.
- Reworked build CI so its checked-out state genuinely has no bundled network;
  it verifies CMake's no-network branch, core tests, UCI startup, and classical
  fallback without pretending to run network-dependent integration tests.
- Updated `docs/Build.md` and `docs/CIVerificationChecklist.md` to match
  Checkpoint 8 and the final independent audit.
- Fixed all observed GCC/Clang warnings in `src/eval/nnue/network.cpp`.
- Added `.gitattributes` to prevent CRLF/LF-only dirty working trees.

## Independent verification in this patch workspace

- GCC Release build: success, 0 warnings.
- GCC CTest: 1/1 passed with no network file present.
- Clang Release build: success, 0 warnings.
- Clang CTest: 1/1 passed with no network file present.
- `q_integration_tests` is not registered without the network.
- UCI starts, reports classical fallback, and returns a legal `bestmove`.

## Remaining release blockers

1. Closed: repository source code and documentation are licensed under GPL-3.0-or-later.
2. Closed as an execution task, failed as a release gate: the optimized Q
   completed a fresh paired A/B smoke test and scored 0-6 against the frozen
   reference. See `docs/Checkpoint9_Report.md`.
3. The private network must not be distributed until its outbound weight
   license is chosen.
4. The unresolved playing-strength regression remains the active RC blocker.
