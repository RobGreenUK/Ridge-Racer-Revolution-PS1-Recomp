#!/bin/sh
set -eu
REVOLUTION_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$REVOLUTION_ROOT/scripts/macos-env.sh"
c++ -std=c++17 -O2 -Wall -Wextra "$REVOLUTION_ROOT/src/scene/preview.cpp" $(pkg-config --cflags --libs sdl3) -framework OpenGL -o "$REVOLUTION_ROOT/build-macos/RevolutionScenePreview"
