# Security policy

EunshinBishop is a local UCI chess engine: a command-line/stdin-stdout
process meant to be driven by a trusted chess GUI or script on the same
machine, and a `.snnue` network file loaded from local disk. It does not open
network sockets, accept remote connections, or execute untrusted code by
design.

## Supported versions

Only the latest tagged release and the `main` branch receive fixes. There is
no long-term-support branch while this project is pre-1.0.

## Realistic attack surface

- **UCI input parsing** (`src/uci/uci.cpp`): malformed or adversarial UCI
  commands/FEN strings from stdin should be rejected safely (an `info
  string` error or a no-op), never a crash or undefined behavior. If you can
  make a malformed `position`/`go`/`setoption` command crash the process or
  corrupt state, that is a bug worth reporting.
- **`.snnue` network loading** (`src/eval/nnue/network.cpp`): a malformed or
  truncated network file must fail safely into classical evaluation with a
  reported reason, never crash or read out of bounds. This is an explicit
  correctness gate (see `docs/Checkpoint3_Report.md`, `docs/Testing.md`).
- **PGN/analysis input** (frozen reference only, `reference/source`): not
  part of Q; do not report issues in the frozen, never-edited reference
  source here.

## Reporting a vulnerability

Open a GitHub issue describing the input that triggers the problem (exact
UCI command sequence, or the `.snnue` file/its hash if the file itself
cannot be shared) and the observed crash or misbehavior. There is no
dedicated security contact yet; treat all reports as public, since this
project does not currently handle secrets, credentials, or remote user data.

Do not report general playing-strength complaints here -- see
`docs/ClassicalEvalBacklog.md` and the issue tracker for that instead.
