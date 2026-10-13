#!/usr/bin/env bash
# Package an already-built tvOS device .app as an IPA for sideloading.
#
# This intentionally does not sign the application. AtvLoadly can sign during
# installation with an Apple ID or with a user's own P12 + tvOS provisioning
# profile. Never commit signing keys, profiles, or game dumps to this repo.
#
# Usage:
#   bash scripts/package-tvos-ipa.sh path/to/WiiCompiled.app [dist/WiiCompiled-tvOS.ipa]

set -Eeuo pipefail

if [[ ${BASH_SOURCE[0]} != "$0" ]]; then
    echo "Run this script, do not source it." >&2
    return 2
fi

APP_PATH="${1:-}"
OUTPUT_IPA="${2:-dist/WiiCompiled-tvOS.ipa}"

if [[ -z "$APP_PATH" || ! -d "$APP_PATH" ]]; then
    echo "Usage: bash $0 path/to/WiiCompiled.app [output.ipa]" >&2
    exit 2
fi

if [[ "$APP_PATH" != /* ]]; then
    APP_PATH="$(cd "$(dirname "$APP_PATH")" && pwd)/$(basename "$APP_PATH")"
fi

if [[ "$OUTPUT_IPA" != /* ]]; then
    OUTPUT_IPA="$(pwd)/$OUTPUT_IPA"
fi

PLIST="$APP_PATH/Info.plist"
if [[ ! -f "$PLIST" ]]; then
    echo "Missing app bundle Info.plist: $PLIST" >&2
    exit 1
fi

BUNDLE_ID="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$PLIST")"
EXECUTABLE="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$PLIST")"
PLATFORM="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleSupportedPlatforms:0' "$PLIST")"
DEVICE_FAMILY="$(/usr/libexec/PlistBuddy -c 'Print :UIDeviceFamily:0' "$PLIST")"
EXECUTABLE_PATH="$APP_PATH/$EXECUTABLE"

if [[ "$PLATFORM" != "AppleTVOS" ]]; then
    echo "Expected CFBundleSupportedPlatforms[0]=AppleTVOS, got '$PLATFORM'." >&2
    exit 1
fi
if [[ "$DEVICE_FAMILY" != "3" ]]; then
    echo "Expected UIDeviceFamily to include tvOS family 3; first value was '$DEVICE_FAMILY'." >&2
    exit 1
fi
if [[ ! -f "$EXECUTABLE_PATH" ]]; then
    echo "App executable not found: $EXECUTABLE_PATH" >&2
    exit 1
fi

if ! command -v xcrun >/dev/null 2>&1 || ! command -v ditto >/dev/null 2>&1; then
    echo "Packaging requires macOS with Xcode command-line tools installed." >&2
    exit 1
fi

ARCHS="$(xcrun lipo -archs "$EXECUTABLE_PATH" 2>/dev/null || true)"
if [[ " $ARCHS " != *" arm64 "* ]]; then
    echo "Expected a tvOS device arm64 executable; detected architectures: '${ARCHS:-unknown}'." >&2
    exit 1
fi

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/wiicompiled-ipa.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT
mkdir -p "$TMP_DIR/Payload" "$(dirname "$OUTPUT_IPA")"

APP_NAME="$(basename "$APP_PATH")"
if [[ "$APP_NAME" != *.app ]]; then
    echo "Input must be an Apple app bundle ending in .app: $APP_PATH" >&2
    exit 1
fi

ditto "$APP_PATH" "$TMP_DIR/Payload/$APP_NAME"
rm -f "$OUTPUT_IPA"
(
    cd "$TMP_DIR"
    ditto -c -k --sequesterRsrc --keepParent Payload "$OUTPUT_IPA"
)

if [[ ! -s "$OUTPUT_IPA" ]]; then
    echo "IPA packaging failed: output is empty." >&2
    exit 1
fi

echo "Created: $OUTPUT_IPA"
echo "Bundle ID: $BUNDLE_ID"
echo "Platform: $PLATFORM"
echo "Architectures: $ARCHS"
echo "Signing: $(/usr/bin/codesign -dv "$APP_PATH" 2>&1 | sed -n 's/^Signature=//p' | head -n 1 || true)"
echo "The IPA is ready to upload to AtvLoadly; AtvLoadly must sign it for your Apple TV."
