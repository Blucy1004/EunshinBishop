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

All of the following must be true before a `v5.0.0` **stable** tag; the
first `v5.0.0-rc.1` tag additionally requires at least the build/test/perft/
UCI-smoke/fallback rows.

```text
[x] Windows MSVC Release build succeeds
[ ] GCC Release build succeeds        (never attempted on this machine)
[ ] Clang Release build succeeds      (never attempted on this machine)
[x] startpos perft 1-5 passes
[x] special-move perft passes
[x] random make/unmake verification passes
[x] UCI smoke test passes
[x] network-not-installed fallback works correctly
[ ] residual equation verified against an independent reference computation
[x] NNUE scratch/incremental accuracy match (checkpoint 3 integration tests)
[x] illegal moves: 0 observed in all recorded test runs
[x] crashes: 0 observed in all recorded test runs
[ ] time-loss root cause investigation (no time-control games run yet)
[ ] Cute Chess regression testing completed
[ ] every command in README.md independently re-verified by someone other than its author
[ ] network and training-data licenses confirmed (see LICENSE, networks/PROVENANCE.md)
[ ] SHA-256 checksums generated for the actual release assets (not just checkpoint archives)
```

Checked rows are backed by a specific `docs/CheckpointN_Report.md`; do not
check a row here without updating that trail. Unchecked rows are exactly
what block a stable release right now -- see `docs/Checkpoint4_Report.md`'s
"Deferred gates" for the same list from the implementation side.
