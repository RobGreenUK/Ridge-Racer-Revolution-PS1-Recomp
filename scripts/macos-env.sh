#!/bin/sh
# Source from the Apple Silicon build/run scripts.
if [ -z "${DEVELOPER_DIR:-}" ] && [ -d /Library/Developer/CommandLineTools ]; then
    export DEVELOPER_DIR=/Library/Developer/CommandLineTools
fi
export PATH="/opt/homebrew/opt/python@3.13/libexec/bin:/opt/homebrew/bin:$PATH"
