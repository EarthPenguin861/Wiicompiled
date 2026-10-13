# tvOS / Apple TV porting notes

This document tracks the initial investigation for running WiiCompiled on Apple TV, with Apple TV HD as the first target and Apple TV 4K as a follow-up target.

## Current status

**Still not a working tvOS port.** The branch now has an experimental device-SDK path, a tvOS app-bundle template, a Dawn source-build path, SDL lifecycle and Siri Remote navigation changes, and compile-only CI coverage. The platform test suite passes, and the native runtime sources plus SDL/Aurora/Dawn dependency graph have compiled against the Apple TV device SDK. The generated Mario Kart product has not been linked, signed, installed, or launched; actual rendering remains unverified.

The target is deliberately device-only arm64 for now. SDL3 documents tvOS builds, but upstream Dawn's current support matrix explicitly does not support tvOS. The CMake provider therefore avoids trying to download a nonexistent tvOS prebuilt and attempts an exploratory source build instead. This is dependency-spike work, not evidence that Dawn compiles or renders correctly on Apple TV.

## Findings

- The translated game/runtime is C++17/20 and uses Aurora for Wii/GameCube APIs, SDL3 for window/input, and Dawn/WebGPU for graphics.
- The deployment floor is tvOS 14.0: the SDK marks `std::filesystem::path` unavailable before tvOS 13, and Dawn's current `WorkerThread.cpp` calls `std::condition_variable::wait` / `notify_all`, which the tvOS SDK marks unavailable before tvOS 14.
- SDL3 documents tvOS as a CMake target. The port can vendor the pinned SDL source rather than reuse a desktop binary.
- The desktop macOS renderer uses Dawn's Metal backend. Upstream Dawn currently lists tvOS as unsupported, so the vendor build may need source-level changes before a Metal/WebGPU renderer can run.
- `runtime/src/platform/host_platform.cpp` already has an Apple executable-path implementation and stores application data under `~/Library/Application Support`. tvOS sandboxing and app-container paths need an explicit design; desktop filesystem assumptions must not be carried over untested.
- macOS's CoreAudio-based external-media activity monitor is excluded on tvOS; volume controls remain, but detecting other apps' audio is a desktop-only feature.
- The branch now has an initial Siri Remote navigation path: tvOS configures SDL to expose directional/select/back events, enables ImGui keyboard navigation, and uses Menu/Escape to open/close the existing settings bar. This still needs validation on real Apple TV hardware and is not a replacement for a dedicated focus-polished TV interface.
- A usable Apple TV app needs an app bundle, signing/provisioning, tvOS lifecycle handling, focus-based remote/controller UI, and a way to provide the user's own game assets within the app sandbox. The existing desktop setup/launcher flow is not a tvOS app installer.
- The project currently expects a clean, user-provided PAL `RMCP01` game image. The port must continue to exclude Nintendo game data and must not add copyrighted assets to the repository or app package.

## Suggested implementation order

1. **Build-system foundation (in progress):** select a tvOS device SDK, use a conservative arm64 CPU baseline, bundle the executable as an Apple TV app, and never route tvOS to desktop Dawn artifacts.
2. **Native compile + app-bundle contract:** the platform suite and native SDL/Aurora/Dawn/runtime source compile pass in CI. A separate smoke target now checks the tvOS app-bundle layout and plist metadata without game data.
3. **Renderer/device milestone:** upstream Dawn lists tvOS as unsupported. Its source and runtime compile, but Metal/WebGPU device initialization, rendering, input, and resume behavior must still be validated on Apple TV hardware; compile success alone is not renderer support.
4. **Runtime platform layer:** finish validating tvOS app-container paths and suspend/resume behavior; remove any remaining desktop-only assumptions.
5. **TV interaction:** polish the initial focus/remote navigation, gamepad routing, and on-screen settings flow. Current remote handling is a starting point, not a final TV UX.
6. **Packaging:** build the generated game product as an Apple TV app with correct bundle metadata, entitlements, signing, and a documented install/deploy workflow.
7. **Device validation:** test first on Apple TV HD, then profile and validate Apple TV 4K separately. Do not claim either device works until the app launches, renders, accepts input, persists settings, and resumes correctly.

## Hardware target notes

Apple TV HD is the first compatibility target, not a performance guarantee. Its older A8-class hardware makes renderer compatibility and frame-time profiling especially important. Apple TV 4K models vary by generation, so performance results should name the exact model and tvOS version.

## What is needed to validate the port

- A Mac or remote macOS build host with Xcode and the tvOS SDK. Windows alone cannot perform Apple's normal tvOS build/sign/package workflow.
- The exact Apple TV HD / Apple TV 4K model and tvOS versions used for device tests.
- A legally dumped PAL `RMCP01` game image, kept local and out of source control.
