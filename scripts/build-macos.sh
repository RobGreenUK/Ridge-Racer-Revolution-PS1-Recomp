#!/bin/sh
set -eu
REVOLUTION_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$REVOLUTION_ROOT/scripts/macos-env.sh"
cd "$REVOLUTION_ROOT"
mkdir -p diagnostics
sh scripts/apply-runtime-patches.sh
bash psxrecomp/tools/ci/build_emitters.sh
python3 psxrecomp/psxrecomp_cli.py generate --config game.toml --project-root "$REVOLUTION_ROOT" --disc "$REVOLUTION_ROOT/disc-images/Ridge Racer Revolution (USA).cue"
python3 tools/inspect_assets.py > diagnostics/assets.json
python3 psxrecomp/tools/aot_overlay_spike/extract_generic.py --game-toml game.toml --recompiler build-recompiler/psxrecomp-game --out diagnostics/overlay-captures.json --tmp diagnostics/overlay-discovery
PSXRECOMP_BIOS_BUILD="$REVOLUTION_ROOT/build-recompiler" bash psxrecomp/tools/regen_bios.sh --config bios/OpenBIOS.toml
# Configure the hash target before compiling overlays; the compiler checks this
# stamp against its baked-in hash. Never bypass that compatibility check.
cmake -S . -B build-macos -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPSX_ENABLE_VULKAN=OFF -DPSX_GAME_VERSION=0.1.0
cmake --build build-macos --target psxrecomp_codegen_hash
python3 psxrecomp/tools/compile_overlays.py --captures diagnostics/overlay-captures.json --game-toml game.toml --recompiler build-recompiler/psxrecomp-game --runtime-include psxrecomp/runtime/include --out-dir generated --static --cps --target-os macos --force
cmake -S . -B build-macos -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPSX_ENABLE_VULKAN=OFF -DPSX_GAME_VERSION=0.1.0
cmake --build build-macos --target psx-runtime -j "${REVOLUTION_JOBS:-8}"
python3 tools/prepare_native_assets.py
sh scripts/build-native-scene.sh
sh scripts/build-launcher-macos.sh
