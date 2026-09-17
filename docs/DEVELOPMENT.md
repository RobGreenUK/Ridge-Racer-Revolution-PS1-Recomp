# Contributor and AI agent guide

Read the [README](../README.md), [technical overview](TECHNICAL_OVERVIEW.md),
[architecture](ARCHITECTURE.md) and relevant [RR comparison](RR_COMPARISON.md)
entry before changes. Knowledge needed to work on the project belongs in these
public guides; private reports and captures are not prerequisites.

## Source map

| Area | Entry points |
|---|---|
| Generation and runtime linkage | `game.toml`, `CMakeLists.txt`, `codegen_setup.c`, `scripts/build-macos.sh` |
| Local extraction | `tools/inspect_assets.py`, `prepare_native_assets.py`, `texture_codec.py`, `terrain_occlusion.py` |
| Game hooks | `src/scene/live.c`, `capture.c`, `input.c`, `submission_eval.cpp` |
| Snapshot layout and input | `src/scene/shared.h`, the bridge in `native.cpp` |
| Motion | `timeline.h`, `presentation_timeline.h`, `background.h` |
| Geometry and layers | `mesh.h`, `depth_renderer.h`, `hud_renderer.h`, `gp0_commands.h` |
| Timing evidence | `display_pacer.h`, `frame_metrics.h`, `gpu_timer.h`, camera/frame analyzers |
| Launch and persistence | `launcher/ServiceMenu.swift`, `settings.py`, `native_scene.py` |
| Upstream modifications | `patches/`, `scripts/apply-runtime-patches.sh` |

## Invariants

Preserve original simulation and physics timing, saved controls, memory cards and
replay behaviour. Never transplant RR guest addresses or state IDs based only on
similarity. Generated game C is disposable; maintained changes belong in source,
configuration or patches. Keep callbacks, snapshot exchange and rendering bounded
and avoid blocking diagnostic work in normal hot paths. P captures are explicit
exceptions and their stalls must not be included in ordinary pacing summaries.

Keep each game's settings, media, generated assets and launch processes separate.
Do not run scripted input against a user's live session. Diagnostic tools must
own their processes and isolate saves. The read-only evaluator must not write
authoritative game memory.

## Verification

Use the project's Mac environment:

```sh
. scripts/macos-env.sh
python3 -m unittest discover -s tests -v
ctest --test-dir build-recompiler -R '^revolution_reserved_guard$' --output-on-failure
python3 psxrecomp/recompiler/tests/test_overlay_guard_codegen.py --recompiler build-recompiler/psxrecomp-game
```

The suite includes C/C++ harnesses and real Mac OpenGL/CoreVideo checks; it is not
an all-platform headless runner. Build before emitter-specific checks. Missing
prerequisites or skipped graphics tests are not passes.

For an isolated original-runtime smoke run:

```sh
python3 tools/smoke_boot.py unique-run-name --headless --guest-frames --seconds 65
```

For an isolated native-renderer session (opens windows and applies scripted input):

```sh
python3 tools/test_native_scene.py unique-native-run --seconds 90
```

The native smoke tool hides the companion like a normal Enhanced launch. Use
`--manual-input` to disable scripted input and verify Galaga/boot controls through
the enhanced window. Timed captures can record either original 2D screens or an
enhanced race, so inspect the frame metrics and image to confirm the intended phase.

Use unique names and run only when they will not interrupt a live game. The tools
use a separate diagnostic port and temporary/private saves. Headless execution is
not display-pacing evidence. Original and enhanced screenshots alone cannot show
a temporal symptom; use the matching snapshot, VRAM, timestamps and live playtest.
Keep raw captures local because they contain game data.

Report exact executable/hash, mode, course, camera, resolution, aspect, display Hz,
render FPS, V-sync and relevant environment flags. Separate compilation, synthetic
regression, real-GL checks, smoke execution and full-course human playtests.
For tunnel-motion work compare source motion, interpolation holds, texture upload
bursts and submission timing; do not equate a flat frame graph with smooth scanout.

## Git and publication

Inspect root and submodule status before editing and preserve unrelated changes.
Use a focused branch from `main` (agent branches use `codex/`). Stage explicit
paths, review the diff, and run:

```sh
git diff --cached --check
python3 tools/audit_source.py
```

The audit checks the staged tree and reachable root history against a source
allowlist. Add public documentation paths explicitly after review; never permit
private folders wholesale. Do not publish disc files, generated code, captures,
settings, saves or build products, including via Actions artifacts or releases.

Open a PR with the concrete change, verification and limitations. Request an
independent review when available; do not mislabel self-review as independent.
Merge through GitHub after required checks, preferably squash-merging one focused
change. Do not rewrite published history or bypass branch rules.

When changing SDK code, preserve the upstream pin and add a reproducible patch
with applicable licence notices. Confirm patch application on a clean dependency
checkout; a modified local SDK is not sufficient for another person's build.

## Controller protocol compatibility

Keep `[controller] default_mode = "digital"` and `lock_mode = true` in
`game.toml`. The original pad decoder accepts digital ID `0x41` and neGcon ID
`0x23`, but rejects DualShock ID `0x73`. A modern host gamepad is not evidence
that the original game understands DualShock emulation. The runtime's digital
mode maps its buttons and left-stick directions to the supported wire format.

The SDK applies the game lock after saved settings, again on advanced-launcher
return, and disables analogue multitap overrides for a digital-locked title.
This preserves device assignment and mappings while correcting stale analogue
settings. Do not replace that with a one-time preference edit or change the guest
input decoder. The viewer's keyboard bridge and the companion's controller path
must both reach a supported pad type, including before the main game has loaded.

An isolated SIO-level regression with saved analogue mode confirmed that Start
arrived as `0073f7ff80808080` and was rejected before the lock; with the lock the
packet was `0041f7ff` and the original decoder accepted Start. These are protocol
observations, not copyrighted fixtures. Physical controller connection, custom
bindings and hotplug still require device-specific playtesting.
