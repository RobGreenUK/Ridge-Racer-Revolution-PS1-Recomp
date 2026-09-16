#!/bin/sh
set -eu
REVOLUTION_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec sh "$REVOLUTION_ROOT/scripts/run-macos.sh"
