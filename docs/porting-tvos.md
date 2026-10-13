# tvOS / Apple TV porting notes

This document tracks the initial investigation for running WiiCompiled on Apple TV, with Apple TV HD as the first target and Apple TV 4K as a follow-up target.

## Current status

**Not yet a working tvOS port.** The existing project targets desktop Windows, Linux, and macOS. A tvOS build needs platform work beyond changing the CMake target or forcing a fullscreen window.

The current runtime CMake logic treats any Apple arm64 target as macOS. That is incorrect for tvOS: it selects macOS deployment settings and desktop assumptions, and can make a cross-configuration look more supported than it is. Until the required dependencies and lifecycle changes are implemented, configuration should fail explicitly rather than silently take the macOS path.

## Findings

- The translated game/runtime is C++17/20 and uses Aurora for Wii/GameCube APIs, SDL3 for window/input, and Dawn/WebGPU for graphics.
- The desktop macOS renderer uses Dawn's Metal backend. Dawn's supported-platform matrix does not currently establish tvOS as a supported target, so Metal-on-tvOS must be verified or replaced/adapted before claiming renderer support.
- `runtime/src/platform/host_platform.cpp` already has an Apple executable-path implementation and stores application data under `~/Library/Application Support`. tvOS sandboxing and app-container paths need an explicit design; desktop filesystem assumptions must not be carried over untested.
- A usable Apple TV app needs an app bundle, signing/provisioning, tvOS lifecycle handling, focus-based remote/controller UI, and a way to provide the user's own game assets within the app sandbox. The existing desktop setup/launcher flow is not a tvOS app installer.
- The project currently expects a clean, user-provided PAL `RMCP01` game image. The port must continue to exclude Nintendo game data and must not add copyrighted assets to the repository or app package.

## Suggested implementation order

1. **Build-system safety (this change):** detect tvOS/iOS explicitly and stop before the generic Apple/macOS branch can be selected.
2. **Dependency spike:** verify a supported SDL3 tvOS build and whether the pinned Dawn version can compile and run its Metal/WebGPU path on tvOS. Record exact upstream patches/options instead of assuming desktop macOS support transfers.
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
