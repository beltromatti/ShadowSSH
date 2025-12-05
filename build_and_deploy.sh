#!/bin/bash

# Navigate to project root
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"

APP_VERSION="1.0"

# Clean previous build
echo "Cleaning previous build..."
rm -rf build
rm -rf ShadowSSH.app

# Build
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j

# Package
echo "Packaging ShadowSSH..."

# Check if .app bundle was created inside build
if [ -d "ShadowSSH.app" ]; then
    cp -r ShadowSSH.app ../
    echo "Done! ShadowSSH.app is ready in $DIR/ShadowSSH.app"
elif [ -f "ShadowSSH" ]; then
    # If only binary exists (sometimes happens if bundle settings fail), create minimal structure manually
    echo "Bundle not found, creating minimal app structure..."
    cd ..
    mkdir -p ShadowSSH.app/Contents/MacOS
    cp build/ShadowSSH ShadowSSH.app/Contents/MacOS/
    chmod +x ShadowSSH.app/Contents/MacOS/ShadowSSH
    
    # Basic Info.plist
    echo '<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>ShadowSSH</string>
    <key>CFBundleIdentifier</key>
    <string>com.shadowssh.client</string>
    <key>CFBundleName</key>
    <string>ShadowSSH</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>' > ShadowSSH.app/Contents/Info.plist
    
    echo "Done! ShadowSSH.app (Manual Bundle) is ready in $DIR/ShadowSSH.app"
else
    echo "Error: Build failed to produce executable."
    exit 1
fi

cd "$DIR"

# Ensure icon and Info.plist are set
APP_DIR="$DIR/ShadowSSH.app"
RES_DIR="$APP_DIR/Contents/Resources"
mkdir -p "$RES_DIR"

# Copy icon if present
if [ -f "$DIR/assets/icon/ShadowSSH.icns" ]; then
    cp "$DIR/assets/icon/ShadowSSH.icns" "$RES_DIR/"
fi

cat > "$APP_DIR/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>ShadowSSH</string>
    <key>CFBundleIdentifier</key>
    <string>com.shadowssh.client</string>
    <key>CFBundleName</key>
    <string>ShadowSSH</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${APP_VERSION}</string>
    <key>CFBundleVersion</key>
    <string>${APP_VERSION}</string>
    <key>CFBundleIconFile</key>
    <string>ShadowSSH</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

echo "Done! ShadowSSH.app is ready in $DIR/ShadowSSH.app"
