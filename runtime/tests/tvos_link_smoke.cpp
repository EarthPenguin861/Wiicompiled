#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <aurora/aurora.h>
#include <aurora/event.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

// Asset-free Apple TV launch and Metal/WebGPU smoke app.
//
// This target exists so CI can verify that the vendored SDL/Dawn/Aurora stack
// can link into a real tvOS app bundle without putting the translated Mario Kart
// product or user-owned game data onto CI. It can also be installed temporarily
// to confirm that tvOS enters through SDL and creates an Aurora Metal surface.
int main(int, char**) {
    const char* home = std::getenv("HOME");
    const char* temp = std::getenv("TMPDIR");
    std::filesystem::path root;
    if (home != nullptr && *home != '\0') {
        root = std::filesystem::path(home) / "Library" / "Application Support" / "WiiCompiledTvOSSmoke";
    } else if (temp != nullptr && *temp != '\0') {
        root = std::filesystem::path(temp) / "WiiCompiledTvOSSmoke";
    } else {
        std::fprintf(stderr, "[tvOS-smoke] No sandboxed HOME or TMPDIR is available.\n");
        return EXIT_FAILURE;
    }

    const std::filesystem::path cache = root / "Cache";
    std::error_code filesystemError;
    std::filesystem::create_directories(cache, filesystemError);
    if (filesystemError) {
        std::fprintf(stderr, "[tvOS-smoke] Cannot create app-container cache: %s\n",
                     filesystemError.message().c_str());
        return EXIT_FAILURE;
    }

    const std::string userPath = root.string();
    const std::string cachePath = cache.string();
    AuroraConfig config{};
    config.appName = "WiiCompiled tvOS Metal smoke";
    config.userPath = userPath.c_str();
    config.cachePath = cachePath.c_str();
    config.desiredBackend = BACKEND_METAL;
    config.startFullscreen = true;
    config.allowJoystickBackgroundEvents = true;
    config.pauseOnFocusLost = false;
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.logLevel = LOG_INFO;

    const AuroraInfo info = aurora_initialize(0, nullptr, &config);
    if (info.initializationStatus != AURORA_INITIALIZATION_SUCCESS) {
        std::fprintf(stderr, "[tvOS-smoke] Aurora/Metal initialization failed: %s\n",
                     info.initializationError != nullptr ? info.initializationError : "unknown error");
        return EXIT_FAILURE;
    }

    std::fprintf(stderr, "[tvOS-smoke] Aurora initialized; backend=%d, surface=%ux%u.\n",
                 static_cast<int>(info.backend), info.windowSize.native_fb_width,
                 info.windowSize.native_fb_height);

    // Present a few blank frames so a device run exercises surface acquisition and
    // presentation, then quit automatically. The Menu/Escape event exits sooner.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    bool running = true;
    while (running && std::chrono::steady_clock::now() < deadline) {
        const AuroraEvent* events = aurora_update();
        for (const AuroraEvent* event = events;
             event != nullptr && event->type != AURORA_NONE; ++event) {
            if (event->type == AURORA_EXIT ||
                (event->type == AURORA_SDL_EVENT &&
                 event->sdl.type == SDL_EVENT_KEY_DOWN &&
                 event->sdl.key.key == SDLK_ESCAPE)) {
                running = false;
                break;
            }
        }
        if (running && aurora_begin_frame()) {
            aurora_end_frame();
        }
        SDL_Delay(16);
    }

    aurora_shutdown();
    return EXIT_SUCCESS;
}
