# Playing and local settings

Run `sh scripts/run-macos.sh`, open `Ridge Racer Revolution.command`, or open
`build-macos/Ridge Racer Revolution.app`. Choose Original or Native preview.

The original runtime handles startup/minigame and menus. The native window appears
when the racing executable publishes its first snapshot. Keeping the original
window available at boot is intentional: hiding it before a boot framebuffer/input
bridge exists would leave that part of the game inaccessible.

The native renderer offers 4:3/16:9 output, matching resolutions, Display FPS or
30–360 FPS, nearest/bilinear filtering and perspective/affine texture projection.
These are presentation settings, not changes to original physics timing.

Use the service menu's Controls & advanced settings for runtime controller setup.
In the native window use Enter for Start, arrows for direction, X/Space to
accelerate, Z to brake and Esc to close. G toggles the frame-time graph; P or F8
captures diagnostics and can briefly stall presentation. Close one session before
opening another. Closing either owned process ends the enhanced session.

Preferences are in `build-macos/settings.toml`; mod state is under
`build-macos/mods/`. Memory cards are in `saves/`. These belong to Revolution and
must not be replaced with Ridge Racer's settings or saves. Diagnostics are under
`diagnostics/`. Preserve these local files when rebuilding, and do not upload
captures or game-derived data to the source repository.
