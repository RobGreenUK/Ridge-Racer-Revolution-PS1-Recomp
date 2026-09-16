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

Enhanced mode uses one visible game window from startup: Galaga and other 2D
screens are copied from the original runtime, then racing switches to enhanced
geometry. Keyboard input in that window also controls the boot/minigame.
The original runtime stays hidden while providing simulation, GPU work and audio.
For diagnostics only, `REVOLUTION_VISIBLE_COMPANION=1 sh scripts/run-macos.sh`
restores its window. Original mode retains its normal visible runtime window.

The native renderer offers 4:3/16:9 output, matching resolutions, Display FPS or
30–360 FPS, nearest/bilinear filtering and perspective/affine texture projection.
These are presentation settings, not changes to original physics timing.

Use the service menu's Controls tab → Controls & advanced settings for runtime controller setup.
In the native window use Enter for Start, arrows for direction, X/Space to
accelerate, Z to brake and Esc to close. G toggles the frame-time graph; P or F8
captures diagnostics and can briefly stall presentation. Close one session before
opening another. Closing the enhanced window ends both owned processes.
Rewind via F8 requires the original runtime window: use Original mode or the
visible-companion diagnostic option. In the enhanced window F8 captures diagnostics.

Preferences are in `build-macos/settings.toml`; mod state is under
`build-macos/mods/`. Memory cards are in `saves/`. These belong to Revolution and
must not be replaced with Ridge Racer's settings or saves. Diagnostics are under
`diagnostics/`. Preserve these local files when rebuilding, and do not upload
captures or game-derived data to the source repository.

Custom render resolution is entered as **Horizontal × Vertical**, followed by
**Apply**. Both dimensions are editable and must match the selected 4:3 or 16:9
aspect ratio. Invalid dimensions leave the applied resolution unchanged.
