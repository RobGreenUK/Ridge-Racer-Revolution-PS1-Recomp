# Windows porting scope

Revolution currently builds on Apple Silicon macOS only. There is no supported
Windows build command, and no Windows build or gameplay validation is claimed.

The sibling Ridge Racer project supplies useful patterns, but this project has
a different renderer and transport. A Windows port needs:

- A Windows mapping/locking/input bridge in place of POSIX `mmap`, `flock`,
  `unistd` and related lifecycle operations in the current renderer/runtime.
- Windows OpenGL extension loading and presentation integration.
- A native Windows launcher with equivalent process ownership and save isolation,
  replacing the SwiftUI interface and POSIX-only Python locking.
- Native Windows generation of both boot and racing executable code, with verified
  overlay codegen hashes and Revolution's reserved-instruction guard patch.
- Local extraction of all four course banks, dependency/runtime packaging and
  tests from a clean checkout with the user's disc.

Reusing Ridge Racer's Windows binary, game hooks or scene files is not valid.
Read [the comparison](RR_COMPARISON.md) and preserve original engine timing.
