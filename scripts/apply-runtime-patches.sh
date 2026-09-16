#!/bin/sh
# Keep the pinned upstream revision while recording the local runtime fixes.
set -eu
REVOLUTION_PATCH_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
for REVOLUTION_RUNTIME_PATCH in "$REVOLUTION_PATCH_ROOT"/patches/psxrecomp-*.patch; do
    if git -C "$REVOLUTION_PATCH_ROOT/psxrecomp" apply --reverse --check "$REVOLUTION_RUNTIME_PATCH" 2>/dev/null; then
        continue
    fi
    git -C "$REVOLUTION_PATCH_ROOT/psxrecomp" apply --check "$REVOLUTION_RUNTIME_PATCH"
    git -C "$REVOLUTION_PATCH_ROOT/psxrecomp" apply "$REVOLUTION_RUNTIME_PATCH"
done
