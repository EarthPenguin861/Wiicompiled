# tvOS / Apple TV porting notes

This document tracks the initial investigation for running WiiCompiled on Apple TV, with Apple TV HD as the first target and Apple TV 4K as a follow-up target.

## Current status

**Still not a working tvOS port.** The branch now has an experimental device-SDK platform path, an Apple TV app-bundle template, a Dawn provider guard, and a compile-only CI check for platform utilities. The full game runtime has not yet been built or launched on tvOS.

The target is deliberately device-only arm64 for now. SDL3 documents tvOS builds, but upstream Dawn's current support matrix explicitly does not support tvOS. The CMake provider therefore avoids trying to download a nonexistent tvOS prebuilt and attempts an exploratory source build instead. This is dependency-spike work, not evidence that Dawn compiles or renders correctly on Apple TV.

## Findings

- The translated game/runtime is C++17/20 and uses Aurora for Wii/GameCube APIs, SDL3 for window/input, and Dawn/WebGPU for graphics.
- The deployment floor is tvOS 14.0: the SDK marks `std::filesystem::path` unavailable before tvOS 13, and Dawn's current `WorkerThread.cpp` calls `std::condition_variable::wait` / `notify_all`, which the tvOS SDK marks unavailable before tvOS 14.
- SDL3 documents tvOS as a CMake target. The port can vendor the pinned SDL source rather than reuse a desktop binary.
- The desktop macOS renderer uses Dawn's Metal backend. Upstream Dawn currently lists tvOS as unsupported, so the vendor build may need source-level changes before a Metal/WebGPU renderer can run.
- `runtime/src/platform/host_platform.cpp` now resolves tvOS user data under the app-container `HOME/Library/Application Support` directory. It never falls back to a user-account home outside the sandbox; `TMPDIR` is only a last-resort sandboxed fallback. Real-device persistence still needs verification.
- macOS's CoreAudio-based external-media activity monitor is excluded on tvOS; volume controls remain, but detecting other apps' audio is a desktop-only feature.
- The branch now has an initial Siri Remote navigation path: tvOS configures SDL to expose directional/select/back events, enables ImGui keyboard navigation, and uses Menu/Escape to open/close the existing settings bar. This still needs validation on real Apple TV hardware and is not a replacement for a dedicated focus-polished TV interface.
- A usable Apple TV app needs an app bundle, signing/provisioning, tvOS lifecycle handling, focus-based remote/controller UI, and a way to provide the user's own game assets within the app sandbox. The existing desktop setup/launcher flow is not a tvOS app installer.
- The project currently expects a clean, user-provided PAL `RMCP01` game image. The port must continue to exclude Nintendo game data and must not add copyrighted assets to the repository or app package.

## Suggested implementation order

1. **Build-system foundation (in progress):** select a tvOS device SDK, use a conservative arm64 CPU baseline, bundle the executable as an Apple TV app, and never route tvOS to desktop Dawn artifacts.
2. **Dependency spike:** the new compile-only CI check covers the platform utility layer. Next, get the full Aurora/SDL/Dawn dependency graph through a tvOS SDK build, then patch Dawn where the source build fails. A successful compile is not enough; Metal rendering must be exercised on-device.
3. **Runtime platform layer:** use tvOS-safe app-container paths, handle suspend/resume and foreground/background transitions, and remove desktop-only assumptions (keyboard shortcuts, window sizing, filesystem writes outside the container).
4. **TV interaction:** provide focus navigation and controller-first settings; the F10 desktop settings bar is not an adequate Apple TV interface.
5. **Packaging:** build a tvOS app target with correct bundle metadata, entitlements, signing, and a documented install/deploy workflow.
6. **Device validation:** test first on Apple TV HD, then profile and validate Apple TV 4K separately. Do not claim either device works until the app launches, renders, accepts input, persists settings, and resumes correctly.

## Hardware target notes

Apple TV HD is the first compatibility target, not a performance guarantee. Its older A8-class hardware makes renderer compatibility and frame-time profiling especially important. Apple TV 4K models vary by generation, so performance results should name the exact model and tvOS version.

## What is needed to validate the port

- A Mac or remote macOS build host with Xcode and the tvOS SDK. Windows alone cannot perform Apple's normal tvOS build/sign/package workflow.
- The exact Apple TV HD / Apple TV 4K model and tvOS versions used for device tests.
- A legally dumped PAL `RMCP01` game image, kept local and out of source control.
