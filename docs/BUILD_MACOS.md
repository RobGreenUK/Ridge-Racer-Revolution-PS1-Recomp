# Build on macOS

Requires Apple Silicon, Apple's command-line tools and a SwiftUI-capable SDK.
The current development host is macOS 26; older releases have not been verified.
Install the tools with `xcode-select --install` and Homebrew:

```sh
brew install cmake ninja python@3.13 pkg-config sdl3 freetype
git clone --recurse-submodules https://github.com/RobGreenUK/Ridge-Racer-Revolution-PS1-Recomp.git
cd Ridge-Racer-Revolution-PS1-Recomp
```

For an existing clone run `git submodule update --init --recursive`. Keep the
pinned dependency revisions. The build applies the tracked SDK patches itself;
no private dependency commit or Ridge Racer checkout is required.

Place `Ridge Racer Revolution (USA).cue` and all 20 referenced BIN tracks in
`disc-images/`. Keep the original track names. The configured USA data-track
fingerprint is checked during generation. Then:

```sh
sh scripts/build-macos.sh
sh scripts/run-macos.sh
```

The full script builds generation tools, generates the boot executable, extracts
and compiles the racing overlay, regenerates OpenBIOS code, builds the runtime,
extracts four course asset banks, and builds the renderer and Swift service menu.
The code-generation hash must match before overlay compilation; do not bypass it.

Outputs are `build-macos/RidgeRacerRevolution`, `build-macos/RevolutionNative`,
`build-macos/native-scene/` and `build-macos/Ridge Racer Revolution.app`.
`Ridge Racer Revolution.command` opens that app. Keep the checkout and dependencies
available; this is a local source build, not a standalone distributable package.
No playable build is published to GitHub.

For incremental work after a full build:

```sh
. scripts/macos-env.sh
sh scripts/build-native-scene.sh
sh scripts/build-launcher-macos.sh
cmake --build build-macos --target psx-runtime -j 8
```

Run only the relevant commands. Changes to code generation, hooks, overlays or
CMake require the full workflow. `REVOLUTION_JOBS=4 sh scripts/build-macos.sh`
reduces parallel runtime compilation if memory is limited.

Source `scripts/macos-env.sh` before standalone Python/toolchain commands. If the
Apple tools themselves need setup or licence acceptance, complete Apple's setup
in Terminal. Use Python 3.11 or newer for `tomllib` (the Mac scripts select 3.13).

Do not delete `saves/` or `build-macos/settings.toml` while clearing build caches.
Malformed/missing-disc errors should be resolved using your supported disc, not
by downloading generated code or bypassing validation. See [development notes](DEVELOPMENT.md).
