#!/usr/bin/env bash
# Local Apple TV build: translate a user-owned RMCP01 dump, compile the tvOS
# device app, and package an unsigned IPA for AtvLoadly.
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GAME=""
NODTOOL=""
DVD_DATA_DIR="$ROOT/Assets/DATA"
BUILD_DIR="$ROOT/build-tvos"
OUTPUT_IPA="$ROOT/dist/WiiCompiled-tvOS.ipa"
PARALLEL=3
SKIP_TRANSLATION=0

fail() {
    printf 'build-tvos-local.sh: error: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage: bash scripts/build-tvos-local.sh [options]

Options:
  --game IMAGE          Your clean PAL RMCP01 dump; extract it before translating
  --nodtool PATH        nodtool executable (required together with --game)
  --dvd-data-dir DIR    Extracted Assets/DATA directory to bundle (default: Assets/DATA)
  --build-dir DIR       tvOS CMake build directory (default: build-tvos)
  --ipa PATH            Output IPA (default: dist/WiiCompiled-tvOS.ipa)
  --parallel N          Translator/Ninja concurrency (default: 3)
  --skip-translation    Reuse generated translation files and build shards already in this checkout
  -h, --help            Show this help

Requirements:
  macOS + Xcode tvOS SDK, CMake 3.25+, Ninja, .NET 8, and Python 3.12.
  Provide a legally dumped PAL RMCP01 image. Game data stays local and is copied
  into the app bundle; it is never uploaded by this script.
EOF
}

root_path() {
    case "$1" in
        /*) printf '%s\n' "$1" ;;
        *) printf '%s/%s\n' "$ROOT" "$1" ;;
    esac
}

while (($#)); do
    case "$1" in
        --game) GAME="${2:-}"; shift 2 ;;
        --nodtool) NODTOOL="${2:-}"; shift 2 ;;
        --dvd-data-dir) DVD_DATA_DIR="$(root_path "${2:-}")"; shift 2 ;;
        --build-dir) BUILD_DIR="$(root_path "${2:-}")"; shift 2 ;;
        --ipa) OUTPUT_IPA="$(root_path "${2:-}")"; shift 2 ;;
        --parallel) PARALLEL="${2:-}"; shift 2 ;;
        --skip-translation) SKIP_TRANSLATION=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) fail "unknown option: $1" ;;
    esac
done

[[ $(uname -s) == Darwin ]] || fail "this build must run on macOS with Xcode; Windows and the Raspberry Pi cannot produce a signed tvOS app"
[[ -z "$GAME" && -z "$NODTOOL" || -n "$GAME" && -n "$NODTOOL" ]] || fail "--game and --nodtool must be supplied together"
[[ "$PARALLEL" =~ ^[1-9][0-9]*$ ]] || fail "--parallel must be a positive integer"

for tool in cmake ninja xcrun python3.12; do
    command -v "$tool" >/dev/null 2>&1 || fail "required tool not found: $tool"
done
command -v clang >/dev/null 2>&1 || fail "Apple Clang was not found"
command -v clang++ >/dev/null 2>&1 || fail "Apple Clang++ was not found"
xcrun --sdk appletvos --show-sdk-path >/dev/null 2>&1 || fail "the tvOS SDK is unavailable; install/select Xcode and its tvOS platform"

if (( SKIP_TRANSLATION )); then
    [[ -z "$GAME" && -z "$NODTOOL" ]] ||
        fail "--game/--nodtool cannot be combined with --skip-translation"
    [[ -f "$ROOT/generated/build_shards/shards.cmake" ]] ||
        fail "missing generated/build_shards/shards.cmake; omit --skip-translation to run the translator"
elif [[ -n "$GAME" ]]; then
    # Extract once, then validate DATA before starting the potentially lengthy
    # translation pass. The translator's macOS helper will reuse these files.
    printf '\n== Extract and verify the clean PAL RMCP01 image ==\n'
    bash "$ROOT/Launcher/macos/extract-disc.command" \
        --game "$GAME" --assets-dir "$ROOT/Assets" --nodtool "$NODTOOL"
fi

DVD_DATA_DIR="$(cd "$DVD_DATA_DIR" 2>/dev/null && pwd)" || fail "DVD DATA directory not found: $DVD_DATA_DIR"
[[ -d "$DVD_DATA_DIR/files" ]] || fail "DATA directory must contain files/: $DVD_DATA_DIR"
[[ -f "$DVD_DATA_DIR/sys/fst.bin" ]] || fail "DATA directory must contain sys/fst.bin: $DVD_DATA_DIR"

if (( ! SKIP_TRANSLATION )); then
    printf '\n== Translate the local base game; skip the macOS native build ==\n'
    bash "$ROOT/Launcher/local-build-macos.command" \
        --workspace "$ROOT" --profile base --translate-only --parallel "$PARALLEL"
fi

PYTHON_EXECUTABLE="$(python3.12 -c 'import sys; print(sys.executable)')"
mkdir -p "$BUILD_DIR" "$(dirname "$OUTPUT_IPA")"

printf '\n== Configure the Apple TV device target ==\n'
cmake -S "$ROOT/runtime" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DPython3_EXECUTABLE="$PYTHON_EXECUTABLE" \
    -DCMAKE_SYSTEM_NAME=tvOS \
    -DCMAKE_OSX_SYSROOT=appletvos \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
    -DAURORA_SDL3_PROVIDER=vendor \
    -DAURORA_DAWN_PROVIDER=vendor \
    -DMKW_TRANSLATED_COMPILE_JOBS="$PARALLEL" \
    -DMKW_TVOS_DVD_DATA_DIR="$DVD_DATA_DIR"

printf '\n== Compile WiiCompiled for Apple TV ==\n'
cmake --build "$BUILD_DIR" --parallel "$PARALLEL" --target WiiCompiled

APP="$BUILD_DIR/WiiCompiled.app"
[[ -d "$APP" ]] || fail "expected app bundle was not produced: $APP"
bash "$ROOT/scripts/package-tvos-ipa.sh" "$APP" "$OUTPUT_IPA"

printf '\nBuild finished. The IPA is unsigned; upload it to AtvLoadly on your Raspberry Pi and let AtvLoadly sign/install it.\n'
printf 'IPA: %s\n' "$OUTPUT_IPA"
