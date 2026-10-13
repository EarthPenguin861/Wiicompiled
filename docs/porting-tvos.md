# tvOS / Apple TV porting notes

This document tracks the initial investigation for running WiiCompiled on Apple TV, with Apple TV HD as the first target and Apple TV 4K as a follow-up target.

## Current status

**Experimental port; no device-tested release yet.** GitHub Actions now successfully configures the pinned SDL/Dawn dependency graph and compiles the native runtime sources against the tvOS device SDK. The separate tvOS platform contract-test suite also passes. The translated game-product targets have not yet been built and linked into a final app, and no IPA has been installed or run on an Apple TV.

The target is deliberately device-only arm64 for now. SDL3 documents tvOS builds, but upstream Dawn's current support matrix explicitly does not support tvOS. The CMake provider therefore avoids trying to download a nonexistent tvOS prebuilt and attempts an exploratory source build instead. This is dependency-spike work, not evidence that Dawn compiles or renders correctly on Apple TV.

## Findings

- The translated game/runtime is C++17/20 and uses Aurora for Wii/GameCube APIs, SDL3 for window/input, and Dawn/WebGPU for graphics.
- The deployment floor is tvOS 14.0: the SDK marks `std::filesystem::path` unavailable before tvOS 13, and Dawn's current `WorkerThread.cpp` calls `std::condition_variable::wait` / `notify_all`, which the tvOS SDK marks unavailable before tvOS 14.
- SDL3 documents tvOS as a CMake target. The port can vendor the pinned SDL source rather than reuse a desktop binary.
- The desktop macOS renderer uses Dawn's Metal backend. The pinned Dawn source now compiles as part of the tvOS native-runtime audit, despite upstream not listing tvOS as officially supported. That verifies compilation only; WebGPU device creation, Metal presentation and gameplay still require on-device testing.
- `runtime/src/platform/host_platform.cpp` now resolves tvOS user data under the app-container `HOME/Library/Application Support` directory. It never falls back to a user-account home outside the sandbox; `TMPDIR` is only a last-resort sandboxed fallback. Real-device persistence still needs verification.
- macOS's CoreAudio-based external-media activity monitor is excluded on tvOS; volume controls remain, but detecting other apps' audio is a desktop-only feature.
- The branch now has an initial Siri Remote navigation path: tvOS configures SDL to expose directional/select/back events, enables ImGui keyboard navigation, and uses Menu/Escape to open/close the existing settings bar. This still needs validation on real Apple TV hardware and is not a replacement for a dedicated focus-polished TV interface.
- The branch now has an IPA packager and a local-only `MKW_TVOS_DVD_DATA_DIR` option. A tvOS product build copies the user's extracted `Assets/DATA` into `WiiCompiled.app/Assets/DATA`; the runtime auto-discovers it if `dvd_root` is unset. This keeps the game dump out of the repository and GitHub CI, but makes the resulting IPA larger.
- The project currently expects a clean, user-provided PAL `RMCP01` game image. The port must continue to exclude Nintendo game data and must not add copyrighted assets to the repository or app package.

## Suggested implementation order

1. **Build-system + dependency compile:** passed for the platform tests and native runtime dependency audit on the tvOS device SDK. The main translated product is still unbuilt.
2. **Local game-data workflow:** implemented with `MKW_TVOS_DVD_DATA_DIR` and the bundled-data lookup. The runtime-data copy has not yet been exercised by a full game-product build.
3. **Runtime platform layer:** tvOS sandbox paths and app background/foreground events are implemented; persistence and suspend/resume remain to be tested on hardware.
4. **TV interaction:** initial Siri Remote support uses the existing ImGui settings bar and still needs real-device focus/navigation testing.
5. **Packaging:** an IPA packaging helper and a passing synthetic IPA smoke test exist. Real code signing and installation remain untested.
6. **Device validation:** build/link the translated base-game target, create and sign a private IPA, then test on Apple TV HD. Profile Apple TV 4K only after the HD build launches reliably.

## Hardware target notes

Apple TV HD is the first compatibility target, not a performance guarantee. Its older A8-class hardware makes renderer compatibility and frame-time profiling especially important. Apple TV 4K models vary by generation, so performance results should name the exact model and tvOS version.

## What is needed to validate the port

- A Mac or remote macOS build host with Xcode and the tvOS SDK. Windows alone cannot perform Apple's normal tvOS build/sign/package workflow.
- The exact Apple TV HD / Apple TV 4K model and tvOS versions used for device tests.
- A legally dumped PAL `RMCP01` game image, kept local and out of source control.

## Building a private Apple TV IPA

This is a local build path. It does not upload a game dump to GitHub and does not put Nintendo assets into source control.

### Recommended one-command build

Use a Mac with Xcode (including the tvOS SDK), CMake 3.25+, Ninja, .NET 8 and Python 3.12. Dawn's pinned generator currently fails on Python 3.14, so explicitly select Python 3.12.

With your clean PAL `RMCP01` image and `nodtool` available, the helper extracts the disc, runs the project translator, configures the tvOS device SDK, builds WiiCompiled, bundles your local extracted DATA, and creates an unsigned IPA:

```bash
bash scripts/build-tvos-local.sh \
  --game "/path/to/RMCP01.iso" \
  --nodtool "./nodtool"
```

If you have already extracted `Assets/main.dol`, `Assets/StaticR.rel`, and `Assets/DATA`, run `bash scripts/build-tvos-local.sh` without `--game` or `--nodtool`. If you already have the translator output and `generated/build_shards/shards.cmake`, use `--skip-translation`.

The output defaults to `dist/WiiCompiled-tvOS.ipa`. The IPA is **unsigned**: upload it to AtvLoadly on the Raspberry Pi and use AtvLoadly's signing/install flow. The helper never uploads the disc image or extracted assets to GitHub.

### Manual build steps

The following steps describe the underlying stages and are useful for debugging individual steps.

1. Follow `docs/building-macos.md` through disc extraction and the translator steps. Use a clean, legally dumped PAL `RMCP01` image. Stop before its macOS configure/build step. The generated translator output is required for the real game target.

2. Follow `docs/building-macos.md` through disc extraction and the translator steps. Use a clean PAL `RMCP01` image that you dumped yourself. Stop before its macOS configure/build step. The generated translator output is required for the real game target.

3. From the repository root, configure a separate device build. Point `MKW_TVOS_DVD_DATA_DIR` at the extracted `Assets/DATA` directory (the directory containing both `files/` and `sys/fst.bin`):

```bash
cmake -S runtime -B build-tvos -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DPython3_EXECUTABLE="$(python3.12 -c 'import sys; print(sys.executable)')" \
  -DCMAKE_SYSTEM_NAME=tvOS \
  -DCMAKE_OSX_SYSROOT=appletvos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DAURORA_SDL3_PROVIDER=vendor \
  -DMKW_TVOS_DVD_DATA_DIR="$(pwd)/Assets/DATA"
cmake --build build-tvos --target WiiCompiled --parallel 3
```

4. If the product build completes, package the app bundle:

```bash
bash scripts/package-tvos-ipa.sh build-tvos/WiiCompiled.app dist/WiiCompiled-tvOS.ipa
```

5. Upload that private IPA to your AtvLoadly instance on the Raspberry Pi and let AtvLoadly sign/install it with your own allowed signing method. The packaging script only zips the app; it does not sign, create provisioning profiles, or claim the IPA is installable before AtvLoadly signs it.

The IPA includes the user-provided extracted DATA and may be large. Keep it private. CI separately builds an asset-free tvOS Metal smoke app to exercise the real app-bundle linker and packaging path; it never builds the private game-product bundle or handles your disc data.
