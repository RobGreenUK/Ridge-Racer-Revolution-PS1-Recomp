#!/bin/sh
set -eu
REVOLUTION_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$REVOLUTION_ROOT/scripts/macos-env.sh"
REVOLUTION_APP="$REVOLUTION_ROOT/build-macos/Ridge Racer Revolution.app"
mkdir -p "$REVOLUTION_APP/Contents/MacOS"
swiftc -parse-as-library -O -framework SwiftUI -framework AppKit "$REVOLUTION_ROOT/launcher/ServiceMenu.swift" -o "$REVOLUTION_APP/Contents/MacOS/RevolutionLauncher"
cat > "$REVOLUTION_APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>RevolutionLauncher</string>
<key>CFBundleIdentifier</key><string>local.ridgeracerrevolution.service-menu</string>
<key>CFBundleName</key><string>Ridge Racer Revolution</string>
<key>CFBundleVersion</key><string>1</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
PLIST
codesign --force --sign - "$REVOLUTION_APP"
echo "Built $REVOLUTION_APP"
