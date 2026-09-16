# Ridge Racer Revolution PS1 Recomp

An unofficial **Ridge Racer Revolution (PlayStation, USA / SLUS-00214)**
recompilation and enhanced-renderer preview for **Apple Silicon macOS**.
Build locally with your own copy of the game. This is a separate project from
[Ridge Racer PS1 Recomp](https://github.com/RobGreenUK/Ridge-Racer-PS1-Recomp),
with its own game addresses, assets, settings, saves and Git history.

PSXRecomp translates the original boot and racing executables into native code.
The original game remains responsible for physics, timing, race logic, audio and
replay. The optional enhanced renderer adds higher resolutions, selectable
rendering FPS, 4:3/16:9 output, perspective-correct textures, interpolated camera
and model poses, expanded scenery and opponent visibility, and a rear-view mirror.
Higher rendering FPS does not change the physics tick rate.

**Development was carried out using OpenAI Codex**, directed and playtested by
Rob Green. Supporting projects remain the work of their own contributors.

## Status

The original Mac renderer has been playtested. Enhanced rendering remains a
preview: lighting/material parity, tunnel-motion perception, mirror/HUD coverage
and scene transitions need further gameplay validation. Read the
[comparison with Ridge Racer](docs/RR_COMPARISON.md) for shared fixes already
present, changes applied here and game-specific work still requiring verification.

**Windows is not currently supported by this project.** The Swift launcher and
POSIX shared-memory bridge require a Windows port; Ridge Racer's Windows workflow
cannot simply be copied. See [Windows porting scope](docs/BUILD_WINDOWS.md).

## Game content

**No copyrighted Ridge Racer Revolution game files are included.** Disc BIN/CUE
files, the original executable, textures, models, music, extracted assets,
generated game code and playable builds remain local and ignored. Supply your own
USA disc dump. Other regions/revisions are unsupported. No Sony BIOS is supplied;
the framework uses separately licensed OpenBIOS.

Project code and licensed dependencies retain their own copyrights; this is not
a claim that source code is copyright-free. The game and its trademarks belong
to their respective owners. This project is not affiliated with or endorsed by them.

## Build and play on Mac

Requirements: Apple Silicon Mac, Apple's command-line tools, Homebrew and a
compatible SwiftUI/macOS SDK. The current build is tested on macOS 26; earlier
macOS releases are not verified. Follow [the complete build guide](docs/BUILD_MACOS.md).

```sh
brew install cmake ninja python@3.13 pkg-config sdl3 freetype
git clone --recurse-submodules https://github.com/RobGreenUK/Ridge-Racer-Revolution-PS1-Recomp.git
cd Ridge-Racer-Revolution-PS1-Recomp
```

Place your CUE and its **20 referenced BIN tracks** in `disc-images/`:

```text
disc-images/
  Ridge Racer Revolution (USA).cue
  Ridge Racer Revolution (USA) (Track 01).bin
  ...
  Ridge Racer Revolution (USA) (Track 20).bin
```

Then build and launch:

```sh
sh scripts/build-macos.sh
sh scripts/run-macos.sh
```

Select **Original** or **Enhanced — native rendering** in the service menu’s
**Motion** tab. Enhanced mode shows Galaga, menus and racing in one game window.
See [controls and settings](docs/PLAY.md). Initial builds need Internet access for
dependencies. Game code and assets are derived from your disc, not downloaded.

## Technical documentation

Start with the [technical overview](docs/TECHNICAL_OVERVIEW.md). The documentation
explains the boot/racing-executable pipeline, scene transport, interpolation,
renderer design and testing for both human contributors and AI agents. It also
records why some Ridge Racer fixes transfer and others need Revolution-specific
investigation. Public documentation shares implementation knowledge without
publishing private development conversations, issue logs or diagnostic captures.

## Source and local files

Source, launchers, tools, tests, patches and public guides are tracked. Disc images,
`disc/`, `generated/`, `build-*/`, `dist/`, `diagnostics/`, `saves/` and `local/`
are ignored. Never upload game-derived files or playable folders to GitHub,
Releases, Actions artifacts or Git LFS. Preserve saves and preferences when rebuilding.

## Acknowledgements

Thank you to the authors, maintainers and contributors of:

- [PSXRecomp](https://github.com/mstan/psxrecomp), for the recompilation framework,
  generation tools and runtime.
- [recomp-ui](https://github.com/mstan/recomp-ui), for runtime interface components.
- [PCSX-Redux / OpenBIOS](https://github.com/grumpycoders/pcsx-redux) and
  [uC-sdk](https://github.com/grumpycoders/uC-sdk), for the replacement BIOS work.
- [SDL](https://github.com/libsdl-org/SDL), and fmt, toml11, ELFIO, rabbitizer,
  libchdr and their supporting libraries.
- Python, Swift, Clang, CMake, Ninja, Homebrew and the wider toolchain community.

## Licence

Original contributions use [MIT](LICENSE), including warranty and liability
disclaimers. **PSXRecomp's noncommercial licence remains applicable** to the
framework and relevant derived work. This is not an unrestricted MIT-only project.
See [third-party notices](THIRD_PARTY_NOTICES.md). No rights to game content are granted.
