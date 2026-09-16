# Playing and local settings

Run `sh scripts/run-macos.sh`, open `Ridge Racer Revolution.command`, or open
`build-macos/Ridge Racer Revolution.app`. The service menu uses the same tabbed layout as Ridge Racer:

- **Motion:** choose Original or Enhanced rendering, then set FPS, VSync and input latency.
  In Enhanced mode, `0` follows the display refresh rate; presets are on a separate row.
- **Display:** choose resolution and display mode; Enhanced mode also offers aspect
  ratio, custom render width and draw distance.
- **Image:** choose texture filtering and the renderer’s texture/antialiasing options.
- **Controls:** configure rewind, open advanced controller settings or view diagnostics.

**Save** and **Launch game** remain visible at the bottom. The window can be resized;
longer settings pages scroll independently. Closing and reopening the service menu
after rebuilding loads the updated layout.

The original runtime handles startup/minigame and menus. The native window appears
when the racing executable publishes its first snapshot. Keeping the original
window available at boot is intentional: hiding it before a boot framebuffer/input
bridge exists would leave that part of the game inaccessible.

The native renderer offers 4:3/16:9 output, matching resolutions, Display FPS or
30–360 FPS, nearest/bilinear filtering and perspective/affine texture projection.
These are presentation settings, not changes to original physics timing.

Use the service menu's Controls tab → Controls & advanced settings for runtime controller setup.
In the native window use Enter for Start, arrows for direction, X/Space to
accelerate, Z to brake and Esc to close. G toggles the frame-time graph; P or F8
captures diagnostics and can briefly stall presentation. Close one session before
opening another. Closing either owned process ends the enhanced session.

Preferences are in `build-macos/settings.toml`; mod state is under
`build-macos/mods/`. Memory cards are in `saves/`. These belong to Revolution and
must not be replaced with Ridge Racer's settings or saves. Diagnostics are under
`diagnostics/`. Preserve these local files when rebuilding, and do not upload
captures or game-derived data to the source repository.
