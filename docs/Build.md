# Build

## Supported platforms

| Platform | Compiler | Status |
|---|---|---|
| Windows x64 | MSVC 19.51.36248.0 (`Visual Studio 18 2026` generator) | Verified every checkpoint; see `docs/CheckpointN_Report.md` |
| Ubuntu x64 | GCC | Not yet run on any machine in this project's history |
| Ubuntu x64 | Clang | Not yet run on any machine in this project's history |

CMake requires `>= 3.20`. C++17, no compiler extensions
(`CMAKE_CXX_EXTENSIONS OFF`).

## Windows (MSVC)

Open an **x64 Developer PowerShell for Visual Studio** (or run from any
PowerShell with `cmake`/`cl` on `PATH`), then:

```powershell
cmake -S . -B build-vs -A x64
cmake --build build-vs --config Release --parallel
ctest --test-dir build-vs -C Release --output-on-failure
```

For a debug build, repeat with `--config Debug`. Both configurations must
build with zero compiler warnings under `/W4 /permissive-`.

## Linux (GCC / Clang) -- untested, expected commands

These are the commands `.github/workflows/build.yml` runs in CI; they have
not yet been run and verified on a physical Linux machine by this project.
Do not claim a Linux build works until a `docs/CheckpointN_Report.md` (or a
green CI run) says so.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Swap `CXX=g++` / `CXX=clang++` before the `cmake -S` step to select the
compiler explicitly.

### Direct GCC build (no CMake)

CMake is the supported path; this is provided per specification section 27
as a direct-compiler example, e.g. for a minimal container without CMake
installed. It has the same untested status as the CMake-driven Linux build
above.

```bash
g++ -std=c++17 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
    -Isrc \
    src/core/attacks.cpp src/core/bitboard.cpp src/core/random.cpp \
    src/core/zobrist.cpp \
    src/position/fen.cpp src/position/movegen.cpp src/position/position.cpp \
    src/eval/classical.cpp src/eval/evaluator.cpp src/eval/nnue/network.cpp \
    src/search/history.cpp src/search/aegis.cpp src/search/limbo.cpp \
    src/search/move_picker.cpp src/search/search.cpp src/search/see.cpp \
    src/search/time_manager.cpp src/search/tt.cpp src/search/worker.cpp \
    src/engine/engine.cpp src/engine/options.cpp \
    src/uci/uci.cpp src/main.cpp \
    -lpthread \
    -o EunshinBishop
```

Copy `networks/firstnet_v5_10b.snnue` next to the resulting `EunshinBishop`
binary (or leave it out and run with `UseNNUE=false`).

## Building without the NNUE network

The engine must build and start with no `.snnue` file present at all: set
`UseNNUE=false`, or simply do not place a network file next to the
executable. A missing or unreadable network must never abort startup --
`Engine`/`Evaluator` fall back to classical evaluation and report the reason
via `info string`. This is exercised by CI on every PR (see
`.github/workflows/build.yml`) precisely because the network is a large
binary that may not always be checked out.

## Placing the NNUE network

1. Download or build `firstnet_v5_10b.snnue` (see `networks/README.md` and
   `networks/PROVENANCE.md` for provenance and the still-undecided
   redistribution license).
2. Place it next to the `EunshinBishop` executable (the default `EvalFile`
   option is the bare filename, resolved relative to the process, not the
   caller's working directory).
3. Confirm with `uci` / `isready`: a successful load reports nothing
   abnormal; a failed load reports `info string ... using classical
   fallback`.

## Registering with a UCI GUI

Point the GUI's "add engine" dialog at the built `EunshinBishop.exe` (or the
Linux binary once verified). No special working directory is required
beyond having the network file (if any) adjacent to the executable. See
`docs/UCI.md` for the supported option list.

## Pre-release checklist (specification item 31)

All of the following must be true before a public `v5.0.0` stable tag. The
`v5.0.0-rc.1` tag must also satisfy every row marked as an RC blocker.

```text
[x] Windows MSVC Release build succeeds
[x] GCC 14.2 Release build succeeds (independently verified during final audit)
[x] Clang 17 Release build succeeds (independently verified during final audit)
[x] startpos perft 1-5 passes
[x] special-move perft passes
[x] random make/unmake verification passes
[x] UCI smoke test passes
[x] network-not-installed fallback works correctly
[x] residual target contract verified from the training/conversion kit
[x] NNUE scratch/incremental accuracy match
[x] illegal moves: 0 observed in all recorded test runs
[x] crashes: 0 observed in all recorded test runs
[x] Checkpoint 8 StateInfo optimization preserves Debug/Release correctness
[x] Clang configured build is warning-clean
[x] public source tree excludes unlicensed .snnue weights
[x] release workflow has explicit contents: write permission for publish job
[x] optimized Q paired A/B regression rerun completed (0-6; gate failed)
[x] source-code outbound license chosen: GPL-3.0-or-later
[ ] network outbound license chosen before distributing weights
[ ] every README command independently re-verified
[ ] SHA-256 checksums generated for the actual release assets
[ ] remaining ~1.3x perft throughput gap resolved or explicitly accepted
```

Checkpoint 7's 21-1-2 result predates the Checkpoint 8 StateInfo optimization
and used a reference engine that does not implement the residual output mode.
It remains evidence of a release regression, but its two original suspected
causes have been reclassified: the performance defect was partially fixed,
and the residual formula was confirmed to match the training kit. A fresh paired A/B batch was completed in Checkpoint 9 and Q lost 0-6.
The execution requirement is closed, but the playing-strength regression remains
an RC blocker until a concrete cause is fixed and a subsequent paired rerun passes.

The public repository does not include the private network binary. Local NNUE
integration tests remain available when an authorized file is placed at
`networks/firstnet_v5_10b.snnue`; CI validates the supported no-network build
and classical fallback path.
