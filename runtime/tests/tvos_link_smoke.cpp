#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <imgui.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

// Asset-free Apple TV launch and Metal/WebGPU smoke app.
//
// It verifies the platform app entry point, SDL window, Dawn/Metal renderer,
// frame presentation, and sandbox paths without compiling or bundling any
// translated Mario Kart code or user-owned game data.
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
    config.appName = "WiiCompiled tvOS graphics smoke";
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
        // Copy the diagnostic immediately: SDL owns this string and another SDL
        // call could invalidate it.
        const std::string message =
            info.initializationError != nullptr ? info.initializationError : "unknown error";
        std::fprintf(stderr, "[tvOS-smoke] Aurora/Metal initialization failed: %s\n", message.c_str());
        return EXIT_FAILURE;
    }

    std::fprintf(stderr, "[tvOS-smoke] Aurora initialized; backend=%d, surface=%ux%u.\n",
                 static_cast<int>(info.backend), info.windowSize.native_fb_width,
                 info.windowSize.native_fb_height);

    // Keep a visible result on screen long enough for a person to inspect it.
    // Menu/Escape exits immediately; otherwise it closes itself after 30 seconds.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
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
        if (!running || !aurora_begin_frame()) {
            SDL_Delay(16);
            continue;
        }

        ImGui::SetNextWindowPos(ImVec2(64.0f, 64.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(960.0f, 420.0f), ImGuiCond_Always);
        ImGui::Begin("WiiCompiled tvOS graphics smoke test", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoCollapse);
        ImGui::TextUnformatted("Apple TV app entry point: OK");
        ImGui::TextUnformatted("SDL3 fullscreen window: OK");
        ImGui::TextUnformatted("Aurora + Dawn Metal initialization: OK");
        ImGui::Text("Surface: %u x %u",
                    info.windowSize.native_fb_width, info.windowSize.native_fb_height);
        ImGui::Separator();
        ImGui::TextWrapped("This is an asset-free port test, not Mario Kart Wii.");
        ImGui::TextWrapped("If this screen appears, the app launched and the Metal/WebGPU renderer initialized.");
        ImGui::TextWrapped("Press Menu / Escape to close, or wait 30 seconds.");
        ImGui::End();
        aurora_end_frame();
        SDL_Delay(16);
    }

    aurora_shutdown();
    return EXIT_SUCCESS;
}
