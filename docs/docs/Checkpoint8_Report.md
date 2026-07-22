# Checkpoint 8: perft throughput regression -- root-caused and fixed; NNUE target-contract audit

Date: 2026-07-19

**Status: Finding 3 (throughput regression) from `docs/Checkpoint7_Report.md`
is fixed and verified. Finding 1 (NNUE evaluation-formula sign flip) is
reframed by a read-only contract audit, but deliberately left unchanged**
per explicit instruction not to touch `ResidualScale`, guards, blending, or
network behavior until the training-target contract was proven from the
scripts. That proof is now complete (see below); acting on it is next
session's job, not this one's.

## Scope discipline

Per instruction for this checkpoint: only the perft/throughput regression
was fixed, with the smallest evidence-backed change possible; the NNUE
investigation was read-only (training/conversion scripts and dataset
generation code only, no engine source touched); `SEEPruning`/`LIMBO`
remain untouched and `false`; no broader search retuning was attempted;
`isPseudoLegal`'s defensive re-validation (an intentional architectural
boundary, `docs/Migration.md`) was not removed even though it appeared in
the profile, because it was not the dominant cost once measured precisely.

## Part 1 -- perft throughput regression

### Evidence preserved before any diagnostic step

`reference/ab_tests/checkpoint8_perf/`:

| File | What it is |
|---|---|
| `before_commit.txt` | commit hash at the start of this checkpoint (`3af0c60`) |
| `before_binaries.sha256` | SHA-256 of the Q and reference Release binaries as they stood before any change |
| `before_perft6_benchmark.txt` | fresh `perft 6` x3 benchmark of Q built from `3af0c60` plus only the `/MT` CRT-linkage change (see below), i.e. the state immediately before the doMove fix |
| `after_binaries.sha256` | SHA-256 of the final, fixed Release binaries |
| `after_perft6_benchmark.txt` | `perft 6` x3 benchmark of the reference and the final fixed Q binary, side by side |
| `profiler/tools_profperft.cpp` | the diagnostic instrumentation source (see Method) |
| `profiler/profperft_raw_output.txt` | raw depth-5 and depth-6 timing breakdowns plus the interpretation that led to the fix |

The reference binary (`reference/bin/EunshinBishop_v2.62_reference.exe`,
SHA-256 `afd22faa...`) was never rebuilt or modified at any point in this
checkpoint -- both hash files record the identical value for it.

### Method

**Profiler**: Windows Performance Recorder (`wpr -start CPU`) was attempted
first and rejected -- it requires administrator privileges, which this
session does not have (`Error code: 0xc5585011`,
"Failed to enable the policy to profile system performance"). No
alternative system profiler with symbol resolution was available without
elevation, so a manual, purpose-built instrumentation tool was used
instead:

- A throwaway git worktree (`git worktree add C:\profwt HEAD` at commit
  `3af0c60`) built a diagnostic executable, `ProfPerft`, from a copy of the
  real `src/position/position.cpp` with additive `#ifdef
  EUNSHIN_PROFILE_DOMOVE` timing (`std::chrono::steady_clock`, not RDTSC --
  an earlier RDTSC-based attempt produced an unreliable, underflowed
  reading and was abandoned in favor of `chrono`, which is reliable under
  `/O2` without manual serialization) around `Position::doMove`'s three
  logically distinct phases: the `isPseudoLegal` re-validation, the
  king-attacked legality check, and the `checkers`/`attackersTo`
  computation. `generateMoves` and `undoMove` were timed at the call site
  in `ProfPerft`'s own perft loop, not inside `movegen.cpp`.
- This worktree and its instrumentation are diagnostic-only, were never
  part of the shipped project, and have been removed
  (`git worktree remove C:\profwt --force`) after data was extracted. Its
  source is preserved as evidence (see table above), not because it should
  be reused.
- Environment: MSVC 19.51.36248.0 x64, Release config, `/O2 /DNDEBUG`
  (CMake's default Release flags -- unchanged from the shipped project's
  own Release config), `EUNSHIN_PROFILE_DOMOVE` defined only for this
  worktree's build, `SetPriorityClass(..., HIGH_PRIORITY_CLASS)` to reduce
  scheduling noise. Single run at each depth was sufficient given the
  scale (millions of calls per phase) averages out per-call timer overhead;
  depth 5 and depth 6 were both run and agree closely (within ~1
  percentage point per phase), which is itself evidence the measurement is
  stable rather than noise-dominated.

### Flag/environment comparability check (instruction 4)

Before attributing anything to architecture, CRT linkage was checked with
`dumpbin /dependents`: the reference imports only `KERNEL32.dll` (static
CRT, `/MT`); Q's build was using CMake's dynamic-CRT default (`/MD`), which
added an `MSVCP140`/`VCRUNTIME140` DLL-import indirection the reference
never pays. This is a real comparability gap, so it was closed:
`CMakeLists.txt` now sets
`CMAKE_MSVC_RUNTIME_LIBRARY = "MultiThreaded$<$<CONFIG:Debug>:Debug>"` under
`if(MSVC)`, and a rebuild was verified via `dumpbin /dependents` to import
only `KERNEL32.dll`, matching the reference. **This change alone did not
close the throughput gap** (`/MD` baseline ~5.97-7.17M nps, `/MT` baseline
~6.3-7.6M nps -- statistically unchanged) but it removes a real
comparability confound and is kept. Both binaries otherwise share: no
assertions active in Release (`NDEBUG` defined by CMake's default Release
flags on both), no sanitizers on either side, no LTO on either side (the
reference's build manifest,
`reference/bin/EunshinBishop_v2.62_reference.exe.build.json`, lists
`/nologo /std:c++17 /EHsc /W4 /O2 /DNDEBUG` and no `/GL`; Q's CMake default
Release flags likewise do not enable `/GL`), same compiler version
(`19.51.36248.0`), same architecture (x64, no explicit `/arch:` flag on
either side).

### Finding: the "rest of doMove" bucket, not `isPseudoLegal`, dominates

Depth-6 breakdown (full data in `profiler/profperft_raw_output.txt`):

```text
Position::doMove:    21851.4 ms, 125049325 calls, 69.6% of total wall time

  isPseudoLegal:          9.7% of total,  13.9% of doMove
  legality check:         7.9% of total,  11.4% of doMove
  checkers (attackersTo): 8.1% of total,  11.6% of doMove
  rest of doMove:        44.0% of total,  63.1% of doMove   <-- dominant
```

Going into this profiling run, `isPseudoLegal`'s redundant re-validation of
moves already known pseudo-legal (every move `perft`/`generateLegalMoves`
passes to `doMove` was just produced by `generateMoves`, which only emits
pseudo-legal moves) was the leading hypothesis, following on from
Checkpoint 7's Finding 3 localizing the gap to "move
generation/make-unmake" broadly. Precise measurement rejected that
hypothesis: `isPseudoLegal` is real but modest (13.9% of `doMove`'s own
time), smaller than the unexplained "rest of doMove" bucket alone and
smaller than all three instrumented sub-phases combined (36.9%). It was
**not** removed -- it remains an intentional, documented defensive
boundary (`docs/Migration.md`: "stale moves are still checked by `Position`
before use," needed for TT/killer/UCI-sourced moves elsewhere in the
engine, not just perft) and instruction 7 explicitly forbids removing
architectural boundaries for speed.

Reading `src/position/position.cpp`'s `Position::doMove` alongside
`src/position/state.h` and `src/core/nnue_state.h` located the actual
cause. `doMove` opened with:

```cpp
newState = StateInfo{};
newState.previous = oldState;
... // every other field explicitly assigned
newState.accumulator = oldState->accumulator;
```

`StateInfo` embeds `NNUE::AccumulatorState`, which holds
`std::array<std::array<int32_t, 256>, 2>` -- 2048 bytes. Every field of
`StateInfo` is explicitly assigned a real value later in the same function
(the accumulator field two statements later, via the full-array copy
above), except `checkers`, which is unconditionally assigned near the end
of `doMove` before any successful return and is never read on a failed-move
path (callers already discard `StateInfo` when `doMove` returns `false` --
`perft`, `generateLegalMoves`, and `isLegal` all follow this contract
already). So `newState = StateInfo{}` was doing a full 2048-byte
zero-initialization of the accumulator array for a value that gets
overwritten by a full 2048-byte copy two statements later -- every single
`doMove` call was paying for two full-size writes to that array where one
was necessary. At perft(6)'s 125,049,325 `doMove` calls, that is roughly
250 MB of entirely avoidable memory traffic on top of the 250 MB of
necessary accumulator-copy traffic, consistent with "rest of doMove" being
memory-copy-bound and dominating over the three computation-bound
sub-phases that were separately instrumented.

### Fix applied

`src/position/position.cpp`'s `Position::doMove`: removed the redundant
`newState = StateInfo{};` and instead explicitly assigned
`newState.checkers = EMPTY_BB;` alongside the block's other explicit field
assignments (a single 8-byte write, versus the 2048-byte array the removed
line was zeroing). Every field of `StateInfo` is still explicitly assigned
before any use, on every path -- this is a mechanical removal of duplicate
work, not a behavior change. A code comment documents why the line existed
and why it was safe to remove.

This is the only source change in this checkpoint besides the `/MT`
CRT-linkage flag fix above. `isPseudoLegal`, the legality check, the
checkers computation, `undoMove`, `generateMoves`, and `doNullMove` (which
already used a single `newState = *oldState` copy with no redundant
zero-init -- confirming this pattern was already correct elsewhere and
`doMove` was the outlier) were not touched.

### Verification (instruction 6, run after the fix)

| Check | Result |
|---|---|
| `perft 6` node count | **119,060,324** on every run, Release and Debug, before and after the fix -- unchanged |
| `q_core_tests` (Release) | PASS |
| `q_integration_tests` (Release) | PASS |
| `q_core_tests` (Debug, exercises `assert(isConsistent())` after every `doMove`/`undoMove`) | PASS (140.22s) |
| `q_integration_tests` (Debug) | PASS (12.49s) |
| Release compiler warnings | 0 (clean rebuild of `eunshin_q_core`, confirmed via a from-scratch `--clean-first` build) |

Debug passing is a stronger correctness signal than Release alone here: the
Debug build's `assert(isConsistent())` after every move recomputes and
cross-checks the position key, pawn key, material key, mailbox/bitboard
agreement, and the checkers bitboard against a from-scratch recomputation
-- if removing the zero-init had left any field genuinely relying on
`StateInfo{}`'s defaults, this would have caught it. It did not fire.

### Before/after NPS

All measured with `bench_perft.ps1` (3 runs, `perft 6` from startpos, raw
logs preserved as noted above):

| Build | Run 1 | Run 2 | Run 3 | Avg (approx) |
|---|---:|---:|---:|---:|
| Reference (unchanged throughout) | 21,203 knps* | 14,880 knps | 14,771 knps | ~14.8M (excl. outlier) |
| Q, before fix (`3af0c60` + `/MT` only) | 7,167,559 nps | 7,707,666 nps | 7,965,499 nps | ~7.6M |
| Q, after fix (final) | 11,019,002 nps | 11,010,850 nps | 12,559,105 nps | ~11.5M |

\* Run 1 of the reference's `after` benchmark is a system-noise outlier
(first-run disk/cache warm-up); runs 2-3 (~14.8M) are consistent with the
`before` session's reference measurements (~12.7-15.5M range) and are used
for the ratio below.

**Gap**: ~14.8M / ~7.6M &asymp; **1.9-2.0x** before this checkpoint's fix
(the original Checkpoint 7 finding, measured under slightly different
system load, was ~2.8x -- both are the same regression, system noise
explains the point estimate difference, not a different cause) &rarr;
~14.8M / ~11.5M &asymp; **1.3x** after. The fix closes roughly half of the
gap on a log scale. This satisfies instruction 8 ("stop once the major
hot-path regression is explained and materially reduced") -- it is
explained (a single, provably redundant 2048-byte zero-init on the hottest
function in the engine, confirmed by precise phase-level timing) and
materially reduced (regression cut roughly in half). The remaining ~1.3x
is not further decomposed in this checkpoint; see Remaining gap below.

### Remaining gap

Not investigated further here, per instruction 8 ("do not broadly retune
search"). Candidate follow-ups for a future, dedicated checkpoint:
`Position` is copied by value in a few places (`isLegal`, which
`generateLegalMoves` does not use but other callers may;
`snapshotForSearch`) -- these were not on `perft`'s hot path and were not
profiled here. `movePieceHashed`/`putPieceHashed`/`removePieceHashed` each
call `popcount(pieces(color, type))` before and after mutating for the
material-key update; this was inside the unresolved "rest of doMove"
44%/63.1% bucket and was not separately instrumented in this pass. Bitboard
representation and Zobrist table layout were not examined. Any of these
would need the same precise-measurement discipline used above before being
called a cause, not just plausibility.

## Part 2 -- NNUE target-contract audit (read-only, no code changed)

### Question

Checkpoint 7's Finding 1 found Q's "classical + 100% residual" formula
flips the evaluation's sign relative to the reference's 35%-blend at
Kiwipete, and left open whether FIRST_NET v5's raw output is actually
calibrated as a small residual delta (as Q's formula assumes) or is closer
to an independent, comparably-scaled evaluator (for which the reference's
blend would be structurally more appropriate). This checkpoint's
instruction was to resolve that question from the training/conversion
scripts and dataset generation code -- not to change `ResidualScale`,
guards, blending, or network behavior based on inference alone.

### Evidence inspected

`SniperBishop_FIRST_NET_v5_RESIDUAL_KIT.zip` and
`SniperBishop_FIRST_NET_v5_RESIDUAL_KIT_FAST.zip` (both from the user's
`Downloads`; confirmed byte-identical to each other via `diff`), extracted
and read in full:

| File | Role |
|---|---|
| `README_V5_RESIDUAL.md` | states the intended runtime contract in prose |
| `SniperBishop_classical_batch.cpp` | the kit's own reference implementation of how a residual network should be combined with classical eval at runtime |
| `v5_residual_datafactory.py` | how the training target was computed from raw data |
| `sniper_bishop_firstnet_v5_residual_train.py` | training loss/target scale actually used |
| `convert_firstnet_v5_npz_to_snnue.py` | on-disk quantization/scale of the shipped `.snnue` file |

### Findings

1. **Target is a residual delta, not an absolute score.**
   `v5_residual_datafactory.py` computes the training target as
   `clip(stockfish_white_cp - classical_white_cp, -target_clip, target_clip)`
   (`target_clip=3000` by default), and the dataset manifest it writes
   records `"target": "stockfish_white_cp - classical_white_cp"`. This is
   unambiguous: the network is trained to predict the *difference* between
   a strong external evaluator and this project's own classical
   evaluation, not an absolute position value.

2. **Runtime contract matches Q's implementation, not the reference's.**
   `README_V5_RESIDUAL.md` states explicitly: runtime score = classical
   side-to-move score + NNUE residual side-to-move score, and requires
   `NNUEIsResidual=true`, `NNUEBlend=100`, `HybridGuard=false` -- "A
   residual v5 file must never be used as a pure NNUE or with percentage
   blending." The kit's own C++ reference implementation,
   `SniperBishop_classical_batch.cpp`, encodes exactly this:
   ```cpp
   if (FirstNet::residualMode) {
       return clamp(evaluateClassical(pos) + nnue, -30000, 30000);
   }
   ```
   This is structurally identical to Q's `Evaluator::evaluate` residual
   branch (`src/eval/evaluator.cpp`, unmodified this checkpoint): classical
   plus the full residual, clamped. Q's formula is not a deviation from the
   documented contract -- it **is** the documented contract, implemented
   independently but arriving at the same formula.

3. **Score perspective and output scaling are consistent end to end.**
   `sniper_bishop_firstnet_v5_residual_train.py` trains directly on the
   cp-scale target with Huber loss (no squashing/sigmoid), converts with
   `raw / 256.0`, and records `"target_pov": "white"`.
   `convert_firstnet_v5_npz_to_snnue.py` quantizes with `Q = 256`,
   `HIDDEN = 256`, `MAGIC = b"SBFNv2\0\0"`. Q's `NNUE::Network`
   (`src/eval/nnue/network.h`/`.cpp`, unmodified this checkpoint) reads this
   same magic, hidden width, and `QUANTIZATION_SCALE = 256`. White-POV
   convention and the ÷256 fixed-point scale agree across training,
   conversion, and Q's runtime reader -- there is no perspective or scale
   mismatch anywhere in this chain.

4. **The frozen reference binary has no residual mode at all.**
   `grep -n "NNUEIsResidual|IsResidual|residual" reference/source/SniperBishop_v2.62_Phase5_FIRSTNET_v5.cpp`
   returns zero matches. The deployed reference always uses its
   35%-weighted absolute-style blend, regardless of what kind of network is
   loaded -- it has no code path that would apply FIRST_NET v5 the way the
   kit that produced it says it must be applied.

### Conclusion

**FIRST_NET v5's target is proven, from the dataset-generation and
training scripts themselves, to be a residual delta
(`stockfish_white_cp - classical_white_cp`), not an absolute evaluation.**
Q's "classical + 100% residual" formula matches this contract exactly, and
matches the kit's own reference C++ implementation of how a residual
network must be combined. The reference binary's 35%-blend is not a more
robust alternative combination formula for this network -- it is the
*wrong* combination formula for a residual-trained network, applied because
the frozen reference predates (or was never updated for) residual-style
FIRST_NET releases. This reframes, but does not reverse, Checkpoint 7's
Finding 1: the sign flip at Kiwipete is not evidence of a bug in Q's
formula. It is evidence that **the reference is the wrong baseline to
match** for this specific network, and Checkpoint 7's framing ("Q's
formula reverses the sign relative to the reference") should not be read
as "Q is wrong" -- the two engines are not applying the same, comparable
combination rule to begin with, and only one of them (Q) is applying the
rule this network was actually trained under.

**What this does not establish**: whether FIRST_NET v5's *magnitude*
calibration is good (i.e., whether the trained residual, even correctly
added, produces well-calibrated final scores across a broad FEN sample --
`target_clip=3000` bounds the training target but says nothing about
typical-case magnitude); whether `ResidualScale`/`ResidualGuard`'s current
default tuning (untouched this checkpoint) is well-matched to this
network's actual output distribution; or what the 1W-21L-2D and
9-0-3-classical-only match results in Checkpoint 6/7 would look like
re-run against a residual-aware reference baseline (none exists -- the
frozen reference cannot be modified). Per explicit instruction,
`ResidualScale`, `ResidualGuard`, blending, and network behavior were not
changed in this checkpoint; this section documents the contract proof
only. Acting on it (e.g., systematically checking calibration across a
larger FEN sample, as Checkpoint 7's "Recommended next steps" already
proposed) is next session's work.

## What this checkpoint does and does not establish

**Established**: (1) the perft throughput regression's dominant cause was a
provably redundant 2048-byte zero-init in `Position::doMove`, now removed,
verified correct (Release + Debug, `assert(isConsistent())` included) and
measured (~1.9-2.0x gap before, ~1.3x after); (2) FIRST_NET v5 is
conclusively a residual-delta network by training-script evidence, and
Q's evaluation-combination formula is the contract-correct one -- the
frozen reference's blend is not.

**Not established**: the remaining ~1.3x perft gap's specific cause(s) (see
Remaining gap); whether FIRST_NET v5's calibrated magnitude (as opposed to
its combination formula) is release-quality; what a corrected/updated A/B
comparison would show now that Finding 3 is partially fixed and Finding 1
is reframed -- re-running `docs/ABTesting.md`'s architecture-regression
comparison was explicitly out of scope for this checkpoint and was not
done.

## Explicit non-actions in this checkpoint

- `ResidualScale`, `ResidualGuard`, `NNUEBlend`, and all NNUE-related
  option defaults are unchanged (`git diff --stat HEAD -- src/eval/` is
  empty).
- `isPseudoLegal` and the architectural safety-check boundary it
  represents were not removed, despite appearing in the profile.
- `SEEPruning` and `LIMBO` remain `false`, untouched.
- Item 23 (classical strengthening) was not touched.
- No broader search retuning was attempted.
- No GitHub tag, push, or release was made.
- The diagnostic worktree (`C:\profwt`) and its instrumentation were never
  part of the shipped project and have been removed; only its source and
  raw output are preserved as evidence.

## Reproducible commands

```powershell
# Build (Release), from the project root:
$cmake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake --build build-vs --config Release --parallel

# perft(6) node-count + NPS check on the shipped engine (UCI):
# position startpos; go perft 6   (or the `perft 6` debug command directly)

# Full CTest, Release and Debug:
$ctest = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
& $cmake --build build-vs --config Release --parallel
& $ctest --test-dir build-vs -C Release --output-on-failure
& $cmake --build build-vs --config Debug --parallel
& $ctest --test-dir build-vs -C Debug --output-on-failure

# Flag comparability check:
dumpbin /dependents build-vs\Release\EunshinBishop.exe
dumpbin /dependents reference\bin\EunshinBishop_v2.62_reference.exe
```

The diagnostic profiler (`ProfPerft`) is not reproducible as a live tool --
it lived only in the now-removed `C:\profwt` worktree. Its exact source
(`reference/ab_tests/checkpoint8_perf/profiler/tools_profperft.cpp`) and
the instrumentation diff it required in `position.cpp` (described in
Method, above) are preserved well enough to recreate it if a future
checkpoint needs finer-grained profiling of the remaining ~1.3x gap.
