#!/bin/sh
set -eu
REVOLUTION_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$REVOLUTION_ROOT/scripts/macos-env.sh"
exec "$REVOLUTION_ROOT/build-macos/Ridge Racer Revolution.app/Contents/MacOS/RevolutionLauncher"
