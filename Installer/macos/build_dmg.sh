#!/bin/bash
# NF De-Esser - macOS installer builder (by NF Audio Tools / Nenno Fernando).
#
# Builds VST3 + AU + AAX (universal arm64 + x86_64) on THIS Mac, signs the AAX with PACE wraptool,
# wraps everything into a standard macOS installer (.pkg: welcome, read-me, format options, conclusion,
# all in English) and puts it inside a DMG. Nothing is uploaded anywhere.
#
# Usage (from anywhere):   bash Installer/macos/build_dmg.sh
# Result:                  ~/Desktop/NF De-Esser <version>.dmg   (open it, double-click "Install NF De-Esser")
#
# No Apple Developer account is needed: the VST3/AU are ad-hoc signed and the .pkg is unsigned
# (on another Mac: right-click the .pkg > Open the first time, or System Settings > Privacy & Security > Open Anyway).
#
# Optional environment variables:
#   OUT_DIR=/some/folder      where the .dmg is written (default: ~/Desktop)
#   WRAP_ACCOUNT=name         PACE account for signing (default: nenofernando). wraptool asks for the password
#                             (or set WRAP_PASSWORD for a one-off run; it is never stored).
#   AAX_SDK_PATH=/path        AAX SDK folder (default: auto-detected inside ~/Documents, e.g. ~/Documents/AAX_SDK)
#   WRAPTOOL=/path/wraptool   PACE wraptool (default: found on PATH or under /Applications/PACEAntiPiracy)
#   WRAP_GUID=...             wrap GUID (default: "NF De-Esser - Signing Only")
#   EXTRA_WRAP_ARGS="..."     extra `wraptool sign` options your PACE setup needs
#   SIGN_ID="..."             Apple code-sign identity for VST3/AU (default "-" = ad-hoc, no Apple Developer)
#   WRAP_SIGNID="..."         codesign identity handed to wraptool --signid for the AAX (default: SIGN_ID, else the local
#                             certificate "NF Audio Tools AAX Local Signing" if found in the keychain, else "-" ad-hoc). wraptool REQUIRES a --signid because the wrap has "Digitally sign binary".
#                             If your wraptool rejects "-", create a free local certificate (Keychain Access >
#                             Certificate Assistant > Create a Certificate > type "Code Signing") and pass its name here.
#   SKIP_AAX=1                installer WITHOUT AAX (VST3 + AU only)
#   ARCHS="arm64"             build only this architecture (default: arm64;x86_64 universal)
#   WRITE_RESOURCES_ONLY=dir  only write the installer texts/XML to "dir" and exit (for checking)
#
# The version comes from CMakeLists.txt (project(NFDeEsser VERSION X.Y.Z ...)): a version bump needs no edit here.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CMAKE_FILE="$REPO_ROOT/CMakeLists.txt"

VERSION="$(grep -m1 -oE 'project\(NFDeEsser VERSION [0-9]+\.[0-9]+\.[0-9]+' "$CMAKE_FILE" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
[ -n "$VERSION" ] || { echo "Could not read the version from $CMAKE_FILE" >&2; exit 1; }

PRODUCT="NF De-Esser"
ID_BASE="com.nfaudiotools.nfdeesser"
BUILD_DIR="$REPO_ROOT/build-deesser"
WORK="$REPO_ROOT/build-deesser-installer"
OUT_DIR="${OUT_DIR:-$HOME/Desktop}"
SIGN_ID="${SIGN_ID:--}"
WRAP_ACCOUNT="${WRAP_ACCOUNT:-nenofernando}"
# Identity for `wraptool --signid`: explicit WRAP_SIGNID, else SIGN_ID if set, else the owner's local certificate
# "NF Audio Tools AAX Local Signing" when it is in the keychain, else ad-hoc ("-").
LOCAL_SIGNID="NF Audio Tools AAX Local Signing"
if [ -z "${WRAP_SIGNID:-}" ]; then
  if [ "$SIGN_ID" != "-" ]; then WRAP_SIGNID="$SIGN_ID"
  elif security find-identity -v -p codesigning 2>/dev/null | grep -q "$LOCAL_SIGNID"; then WRAP_SIGNID="$LOCAL_SIGNID"
  else WRAP_SIGNID="-"; fi
fi
WRAP_GUID="${WRAP_GUID:-A3705280-BD40-11F1-B096-005056920FF7}"   # "NF De-Esser - Signing Only" (product NFDEESSER001)
WITH_AAX=1; [ "${SKIP_AAX:-0}" = "1" ] && WITH_AAX=0
PKG_NAME="Install $PRODUCT $VERSION.pkg"
VOLNAME="$PRODUCT $VERSION"
DMG_PATH="$OUT_DIR/$PRODUCT $VERSION.dmg"
YEAR="$(date +%Y)"

# ---------------------------------------------------------------------------------------------
# Installer texts (English) and distribution definition
# ---------------------------------------------------------------------------------------------
write_resources() {
  local dir="$1"
  mkdir -p "$dir/resources"
  local style='<style>body{font-family:-apple-system,Helvetica,Arial,sans-serif;font-size:13px;line-height:1.5}h1{font-size:20px;margin:0 0 4px}h2{font-size:14px;margin:16px 0 4px}.by{color:#555;margin:0 0 14px}td{padding:2px 14px 2px 0;vertical-align:top}</style>'

  cat > "$dir/resources/Welcome.html" <<EOF
<html><head><meta charset="utf-8">$style</head><body>
<h1>$PRODUCT $VERSION</h1>
<p class="by">NF Audio Tools by Nenno Fernando</p>
<p>Welcome to the $PRODUCT installer.</p>
<p>$PRODUCT is a de-esser with Frequency, Threshold and Range controls, a Target / Full mode, a High-pass / Band-pass side-chain, a Listen monitor, level and
reduction meters, and 13 factory presets.</p>
<p>This installer can install the following formats. On the <b>Customize</b> step you can choose which ones to install.</p>
<ul><li>VST3</li><li>Audio Unit (AU)</li><li>AAX (Pro Tools)</li></ul>
<p>Click <b>Continue</b> to proceed.</p>
</body></html>
EOF

  cat > "$dir/resources/ReadMe.html" <<EOF
<html><head><meta charset="utf-8">$style</head><body>
<h1>$PRODUCT - Read Me</h1>
<p class="by">NF Audio Tools by Nenno Fernando &middot; Version $VERSION</p>
<h2>Formats and install locations</h2>
<table>
<tr><td><b>VST3</b></td><td>/Library/Audio/Plug-Ins/VST3/$PRODUCT.vst3</td></tr>
<tr><td><b>Audio Unit</b></td><td>/Library/Audio/Plug-Ins/Components/$PRODUCT.component</td></tr>
<tr><td><b>AAX</b></td><td>/Library/Application Support/Avid/Audio/Plug-Ins/$PRODUCT.aaxplugin</td></tr>
</table>
<h2>System requirements</h2>
<p>macOS 10.15 or later. Native for Apple Silicon and Intel (universal). A host that supports VST3, AU or AAX (Pro Tools).</p>
<h2>After installing</h2>
<p>Restart your DAW and rescan plug-ins if it does not show up. $PRODUCT is listed under <b>Dynamics</b>.
In Logic Pro use the Audio Unit version.</p>
<p>Presets are saved in Documents/NF Audio Tools/$PRODUCT/Presets.</p>
<h2>About</h2>
<p>$PRODUCT &copy; $YEAR NF Audio Tools by Nenno Fernando. All rights reserved.</p>
</body></html>
EOF

  cat > "$dir/resources/Conclusion.html" <<EOF
<html><head><meta charset="utf-8">$style</head><body>
<h1>Installation complete</h1>
<p class="by">NF Audio Tools by Nenno Fernando</p>
<p>$PRODUCT $VERSION was installed successfully.</p>
<p>Restart your DAW and rescan your plug-ins. You will find $PRODUCT under <b>Dynamics</b>.</p>
<p>Thank you for choosing NF Audio Tools.</p>
</body></html>
EOF

  local aax_choice="" aax_line="" aax_ref=""
  if [ "$WITH_AAX" = "1" ]; then
    aax_line='<line choice="aax"/>'
    aax_choice="<choice id=\"aax\" visible=\"true\" start_selected=\"true\" title=\"AAX (Pro Tools)\" description=\"Installs $PRODUCT.aaxplugin (PACE signed) in /Library/Application Support/Avid/Audio/Plug-Ins.\"><pkg-ref id=\"$ID_BASE.aax\"/></choice>"
    aax_ref="<pkg-ref id=\"$ID_BASE.aax\" version=\"$VERSION\" onConclusion=\"none\">NFDeEsser-AAX.pkg</pkg-ref>"
  fi

  cat > "$dir/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>$PRODUCT $VERSION</title>
  <organization>com.nfaudiotools</organization>
  <welcome file="Welcome.html" mime-type="text/html"/>
  <readme file="ReadMe.html" mime-type="text/html"/>
  <conclusion file="Conclusion.html" mime-type="text/html"/>
  <background file="logo.png" mime-type="image/png" alignment="bottomleft" scaling="none"/>
  <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_anywhere="false" enable_localSystem="true"/>
  <choices-outline>
    <line choice="vst3"/>
    <line choice="au"/>
    $aax_line
  </choices-outline>
  <choice id="vst3" visible="true" start_selected="true" title="VST3" description="Installs $PRODUCT.vst3 in /Library/Audio/Plug-Ins/VST3.">
    <pkg-ref id="$ID_BASE.vst3"/>
  </choice>
  <choice id="au" visible="true" start_selected="true" title="Audio Unit (AU)" description="Installs $PRODUCT.component in /Library/Audio/Plug-Ins/Components (Logic Pro, GarageBand).">
    <pkg-ref id="$ID_BASE.au"/>
  </choice>
  $aax_choice
  <pkg-ref id="$ID_BASE.vst3" version="$VERSION" onConclusion="none">NFDeEsser-VST3.pkg</pkg-ref>
  <pkg-ref id="$ID_BASE.au" version="$VERSION" onConclusion="none">NFDeEsser-AU.pkg</pkg-ref>
  $aax_ref
</installer-gui-script>
EOF
}

if [ -n "${WRITE_RESOURCES_ONLY:-}" ]; then
  write_resources "$WRITE_RESOURCES_ONLY"
  echo "Installer texts written to $WRITE_RESOURCES_ONLY"
  exit 0
fi

command -v cmake >/dev/null || { echo "cmake not found (brew install cmake)" >&2; exit 1; }
xcode-select -p >/dev/null 2>&1 || { echo "Xcode command line tools not found (xcode-select --install)" >&2; exit 1; }

# ---- AAX prerequisites: fail early, before the long build --------------------------------------
if [ "$WITH_AAX" = "1" ]; then
  if [ -z "${AAX_SDK_PATH:-}" ]; then
    for c in "$HOME/Documents/AAX_SDK" "$HOME/Documents/aax-sdk" "$HOME/Documents/AAXSDK"; do
      [ -d "$c/Interfaces" ] && AAX_SDK_PATH="$c" && break
    done
  fi
  if [ -z "${AAX_SDK_PATH:-}" ]; then
    # Any folder in ~/Documents (up to 2 levels deep) named like *AAX*SDK* that holds "Interfaces".
    while IFS= read -r c; do
      [ -d "$c/Interfaces" ] && AAX_SDK_PATH="$c" && break
    done < <(find "$HOME/Documents" -maxdepth 2 -type d \( -iname "*aax*sdk*" -o -iname "*aax_sdk*" \) 2>/dev/null)
  fi
  [ -n "${AAX_SDK_PATH:-}" ] && [ -d "$AAX_SDK_PATH" ] || { echo "AAX SDK not found in ~/Documents (set AAX_SDK_PATH=/path, or SKIP_AAX=1 for an installer without AAX)" >&2; exit 1; }
  echo "AAX SDK: $AAX_SDK_PATH"
  if [ -z "${WRAPTOOL:-}" ]; then
    for c in "$(command -v wraptool || true)" \
             "/Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool" \
             "$(find /Applications/PACEAntiPiracy -maxdepth 6 -name wraptool -type f 2>/dev/null | head -n 1)"; do
      [ -n "$c" ] && [ -x "$c" ] && WRAPTOOL="$c" && break
    done
  fi
  [ -n "${WRAPTOOL:-}" ] && [ -x "$WRAPTOOL" ] || { echo "PACE wraptool not found (set WRAPTOOL=/path/to/wraptool, or SKIP_AAX=1)" >&2; exit 1; }
fi

# ---- Build ---------------------------------------------------------------------------------------
echo "==> Building $PRODUCT $VERSION"
CMAKE_ARGS=(-S "$REPO_ROOT" -B "$BUILD_DIR" -G Xcode)
[ -n "${ARCHS:-}" ] && CMAKE_ARGS+=("-DCMAKE_OSX_ARCHITECTURES=$ARCHS")
TARGETS=(NFDeEsser_VST3 NFDeEsser_AU)
if [ "$WITH_AAX" = "1" ]; then
  CMAKE_ARGS+=(-DNFDeEsser_ENABLE_AAX=ON "-DNFDeEsser_AAX_SDK_PATH=$AAX_SDK_PATH")
  TARGETS+=(NFDeEsser_AAX)
else
  CMAKE_ARGS+=(-DNFDeEsser_ENABLE_AAX=OFF)
fi
cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --config Release --target "${TARGETS[@]}"

VST3="$(find "$BUILD_DIR" -name "$PRODUCT.vst3" -type d | head -n 1)"
AU="$(find "$BUILD_DIR" -name "$PRODUCT.component" -type d | head -n 1)"
[ -d "$VST3" ] && [ -d "$AU" ] || { echo "Built plug-ins not found under $BUILD_DIR" >&2; exit 1; }

# ---- Stage payloads ------------------------------------------------------------------------------
echo "==> Staging"
rm -rf "$WORK"
mkdir -p "$WORK/root/vst3" "$WORK/root/au" "$WORK/root/aax" "$WORK/pkgs" "$WORK/dmg"
cp -R "$VST3" "$WORK/root/vst3/"
cp -R "$AU" "$WORK/root/au/"
echo "==> Signing VST3 + AU (identity: $SIGN_ID; \"-\" = ad-hoc, no Apple Developer needed)"
codesign --force --deep --sign "$SIGN_ID" "$WORK/root/vst3/$PRODUCT.vst3"
codesign --force --deep --sign "$SIGN_ID" "$WORK/root/au/$PRODUCT.component"

if [ "$WITH_AAX" = "1" ]; then
  AAX="$(find "$BUILD_DIR" -name "$PRODUCT.aaxplugin" -type d | head -n 1)"
  [ -d "$AAX" ] || { echo "Built AAX not found under $BUILD_DIR" >&2; exit 1; }
  echo "==> Signing AAX with PACE wraptool (account: $WRAP_ACCOUNT, wrap $WRAP_GUID)"
  WRAP_ARGS=(sign --verbose --account "$WRAP_ACCOUNT" --wcguid "$WRAP_GUID" --signid "$WRAP_SIGNID" --in "$AAX" --out "$WORK/root/aax/$PRODUCT.aaxplugin")
  [ -n "${WRAP_PASSWORD:-}" ] && WRAP_ARGS+=(--password "$WRAP_PASSWORD")
  # shellcheck disable=SC2086
  if ! "$WRAPTOOL" "${WRAP_ARGS[@]}" ${EXTRA_WRAP_ARGS:-}; then
    echo >&2
    echo "wraptool failed. Used: --signid \"$WRAP_SIGNID\" (wraptool needs a codesign identity for this wrap)." >&2
    echo "Available codesign identities on this Mac:" >&2
    security find-identity -v -p codesigning >&2 || true
    echo "Retry with e.g.:  WRAP_SIGNID=\"<identity name>\" bash <this script>   (or check: \"$WRAPTOOL\" sign --help)" >&2
    exit 1
  fi
  [ -d "$WORK/root/aax/$PRODUCT.aaxplugin" ] || { echo "wraptool did not produce the signed AAX" >&2; exit 1; }
  # Do NOT run codesign on the .aaxplugin afterwards: it would break the PACE signature.
  "$WRAPTOOL" verify --in "$WORK/root/aax/$PRODUCT.aaxplugin" --verbose || echo "(wraptool verify reported a problem, check the output above)"
fi

# ---- Component packages --------------------------------------------------------------------------
make_component() {   # name, identifier suffix, install location
  local root="$WORK/root/$1" plist="$WORK/$1.plist"
  pkgbuild --analyze --root "$root" "$plist" >/dev/null
  # Install exactly where we say (never "relocate" to an older copy found elsewhere on the disk).
  /usr/libexec/PlistBuddy -c "Set :0:BundleIsRelocatable false" "$plist" >/dev/null 2>&1 || true
  pkgbuild --root "$root" --component-plist "$plist" --identifier "$ID_BASE.$1" --version "$VERSION" \
           --install-location "$2" "$WORK/pkgs/NFDeEsser-$(echo "$1" | tr '[:lower:]' '[:upper:]').pkg" >/dev/null
}
echo "==> Building installer packages"
make_component vst3 "/Library/Audio/Plug-Ins/VST3"
make_component au   "/Library/Audio/Plug-Ins/Components"
[ "$WITH_AAX" = "1" ] && make_component aax "/Library/Application Support/Avid/Audio/Plug-Ins"

write_resources "$WORK"
cp "$REPO_ROOT/Assets/PNG_READY_1200x400/10_logo_nf_audio_tools.png" "$WORK/resources/logo.png"
# No Apple Developer ID: the installer package is intentionally unsigned.
productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/resources" \
             --package-path "$WORK/pkgs" "$WORK/dmg/$PKG_NAME" >/dev/null

# ---- DMG (opens showing only the installer) ------------------------------------------------------
echo "==> Creating DMG"
mkdir -p "$OUT_DIR"
RW="$WORK/rw.dmg"
hdiutil create -volname "$VOLNAME" -srcfolder "$WORK/dmg" -fs HFS+ -format UDRW -ov "$RW" >/dev/null
MOUNT="$(hdiutil attach -readwrite -noverify -noautoopen "$RW" | grep -o '/Volumes/.*' | head -n 1)"
if [ -n "$MOUNT" ]; then
  # Clean Finder window: just the installer, big icon. Needs Finder automation permission the first time;
  # if it is refused the DMG is still fine (plain window).
  osascript >/dev/null 2>&1 <<OSA || echo "(Finder window layout skipped: allow Terminal to control Finder to enable it)"
tell application "Finder"
  tell disk "$VOLNAME"
    open
    set current view of container window to icon view
    set toolbar visible of container window to false
    set statusbar visible of container window to false
    set the bounds of container window to {200, 140, 720, 400}
    set viewOptions to the icon view options of container window
    set arrangement of viewOptions to not arranged
    set icon size of viewOptions to 128
    set position of item "$PKG_NAME" of container window to {260, 120}
    update without registering applications
    delay 2
    close
  end tell
end tell
OSA
  bless --folder "$MOUNT" --openfolder "$MOUNT" >/dev/null 2>&1 || true   # open the window when the DMG is mounted
  sync
  hdiutil detach "$MOUNT" >/dev/null || hdiutil detach "$MOUNT" -force >/dev/null
fi
rm -f "$DMG_PATH"
hdiutil convert "$RW" -format UDZO -imagekey zlib-level=9 -o "$DMG_PATH" >/dev/null

echo
echo "Done: $DMG_PATH"
[ "$WITH_AAX" = "1" ] && echo "Installer includes: VST3, AU and the PACE-signed AAX (each optional on the Customize step)." \
                      || echo "Installer includes: VST3 and AU (AAX skipped)."
