#!/bin/bash

set -xe

CONFIG=${1:-release}
ARCH=arm64

xmake clean
# xmake m package --plat=macosx --arch="arm64,x86_64" -f "--mode=$CONFIG --yes"
xmake f -y -p macosx -a "arm64" -m $CONFIG
xmake
xmake f -y -p macosx -a "x86_64" -m $CONFIG
xmake

rm build/macosx/$ARCH/$CONFIG/libmods.a || true
lipo -create build/macosx/arm64/$CONFIG/macOSLauncher build/macosx/x86_64/$CONFIG/macOSLauncher -output build/macosx/$ARCH/$CONFIG/macOSLauncher.app/Contents/MacOS/macOSLauncher
lipo -create build/macosx/arm64/$CONFIG/stfc-community-patch-loader build/macosx/x86_64/$CONFIG/stfc-community-patch-loader  -output build/macosx/$ARCH/$CONFIG/macOSLauncher.app/Contents/stfc-community-patch-loader
lipo -create build/macosx/arm64/$CONFIG/libstfc-community-patch.dylib build/macosx/x86_64/$CONFIG/libstfc-community-patch.dylib -output build/macosx/$ARCH/$CONFIG/macOSLauncher.app/Contents/libstfc-community-patch.dylib
cp assets/launcher.icns build/macosx/$ARCH/$CONFIG/macOSLauncher.app/Contents/Resources
rm -r build/macosx/$ARCH/$CONFIG/STFC\ Community\ Patch.app || true
mv build/macosx/$ARCH/$CONFIG/macOSLauncher.app build/macosx/$ARCH/$CONFIG/STFC\ Community\ Patch.app

rm -rf build/macosx/$ARCH/$CONFIG/*.dSYM || true
codesign --force --verify --verbose --deep --sign "-" build/macosx/$ARCH/$CONFIG/STFC\ Community\ Patch.app

rm stfc-community-patch-installer.dmg || true
create-dmg \
  --volname "STFC Community Patch Installer" \
  --background "assets/mac_installer_background.png" \
  --window-pos 200 120 \
  --window-size 800 400 \
  --icon-size 100 \
  --icon "STFC Community Patch.app" 200 190 \
  --hide-extension "STFC Community Patch.app" \
  --app-drop-link 600 185 \
  "stfc-community-patch-installer.dmg" \
  "build/macosx/$ARCH/$CONFIG/"
