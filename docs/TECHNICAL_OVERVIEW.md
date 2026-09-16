# Technical documentation

These guides describe the maintained implementation for human contributors and
AI coding agents, without requiring private conversations, issue logs or captures.

1. [Architecture](ARCHITECTURE.md): boot and racing executable generation, local
   asset extraction, process ownership, snapshots and interpolation.
2. [Comparison with Ridge Racer](RR_COMPARISON.md): shared improvements, applied
   fixes and the boundaries that prevent a blind cross-game port.
3. [Development guide](DEVELOPMENT.md): source map, verification and contribution
   workflow.

For users: [Mac build](BUILD_MACOS.md), [Windows scope](BUILD_WINDOWS.md) and
[playing](PLAY.md). See [licensing](../THIRD_PARTY_NOTICES.md) before reusing code.

The intended experience preserves the original simulation while independently
rendering sampled game geometry. Higher resolution and rendering frequency do
not change the original engine's timing. The enhanced renderer is still a preview;
compilation and synthetic tests do not establish complete visual/gameplay parity.

The target is USA / SLUS-00214. Course files, model IDs, addresses and submission
callers differ from Ridge Racer USA. Runtime hooks must be established from this
executable and checked against original output. Matching a function name or opcode
pattern is a research lead, not verification of its semantics.

Codex was used for implementation and documentation under human direction and
playtesting. Keep explanations linked to maintained source and actual checks.
Update these guides when protocol, generation, ownership or rendering contracts
change. Never publish the locally generated game assets or code as documentation.

## Initial source-release verification

A clean Mac checkout fetched the pinned dependencies from their upstream GitHub
repositories and generated both executable paths and all four course banks using
only the owner's disc as local input. All 18 project tests passed, including real
OpenGL readback checks. The Revolution reserved-guard CTest and overlay guard
code-generation checks passed. A separate 75-second isolated native smoke run
reached a race and produced original/enhanced captures. These checks do not replace
a complete course/replay playtest or establish a Windows port.
