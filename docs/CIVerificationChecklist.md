# CI verification checklist (post-push)

The public repository intentionally contains no `.snnue` binary. CI therefore
verifies the supported no-network configuration and classical fallback path.
Network-dependent integration tests are local-only until the trained weight
file has an explicit outbound license or a secure CI injection mechanism.

## Expected jobs

- `windows-msvc`
- `linux (gcc)`
- `linux (clang)`

## Required checks

- [ ] Configure and build succeed.
- [ ] Compiler version is visible in the Linux logs.
- [ ] Configured warning count is zero.
- [ ] CTest passes and lists no `q_integration_tests` without a network.
- [ ] Core/perft smoke prints `PASS:`.
- [ ] UCI smoke prints `uciok` and `bestmove`.
- [ ] Missing network produces a classical-fallback diagnostic rather than a crash.

## Release workflow

- [ ] Windows and Linux packages contain the executable, README, LICENSE, and
      network placement instructions only.
- [ ] No `.snnue` file is present in any release archive.
- [ ] `publish` has `contents: write` and creates a draft release.
- [ ] `SHA256SUMS.txt` matches the uploaded archives.
