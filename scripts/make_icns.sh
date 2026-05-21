#!/usr/bin/env bash
# Generate assets/icon/ShadowSSH.icns from appicon-*.png (macOS only).
# Re-run after scripts/make_icon.py to refresh the bundle icon.
set -euo pipefail
ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
ICON_DIR="$ROOT/assets/icon"
cd "$ICON_DIR"

command -v iconutil >/dev/null 2>&1 || { echo "iconutil requires macOS" >&2; exit 1; }
command -v sips     >/dev/null 2>&1 || { echo "sips requires macOS"     >&2; exit 1; }

rm -rf ShadowSSH.iconset
mkdir -p ShadowSSH.iconset

sips -z 16   16   appicon-16.png  --out ShadowSSH.iconset/icon_16x16.png       >/dev/null
sips -z 32   32   appicon-32.png  --out ShadowSSH.iconset/icon_16x16@2x.png    >/dev/null
sips -z 32   32   appicon-32.png  --out ShadowSSH.iconset/icon_32x32.png       >/dev/null
sips -z 64   64   appicon-64.png  --out ShadowSSH.iconset/icon_32x32@2x.png    >/dev/null
sips -z 128  128  appicon-128.png --out ShadowSSH.iconset/icon_128x128.png     >/dev/null
sips -z 256  256  appicon-256.png --out ShadowSSH.iconset/icon_128x128@2x.png  >/dev/null
sips -z 256  256  appicon-256.png --out ShadowSSH.iconset/icon_256x256.png     >/dev/null
sips -z 512  512  appicon-512.png --out ShadowSSH.iconset/icon_256x256@2x.png  >/dev/null
sips -z 512  512  appicon-512.png --out ShadowSSH.iconset/icon_512x512.png     >/dev/null
cp appicon.png ShadowSSH.iconset/icon_512x512@2x.png

iconutil -c icns ShadowSSH.iconset -o ShadowSSH.icns
rm -rf ShadowSSH.iconset
echo "Wrote $ICON_DIR/ShadowSSH.icns"
