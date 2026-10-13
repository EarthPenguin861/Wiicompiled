#include <SDL3/SDL.h>

// Keep SDL's tvOS application-entrypoint shim enabled. This diagnostic deliberately
// avoids Aurora and Dawn so a visible result isolates SDL/UIKit startup and the
// SDL Metal renderer from the larger WebGPU renderer path.
#include <SDL3/SDL_main.h>

#include <cstdlib>

namespace {
void draw_status(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 9, 18, 42, 255);
    SDL_RenderClear(renderer);

    // High-contrast geometry remains obvious even if a debug-font glyph is missing.
    SDL_FRect header{18.0f, 18.0f, 604.0f, 54.0f};
    SDL_SetRenderDrawColor(renderer, 30, 190, 130, 255);
    SDL_RenderFillRect(renderer, &header);

    SDL_SetRenderDrawColor(renderer, 40, 180, 240, 255);
    SDL_FRect cyan{18.0f, 92.0f, 180.0f, 28.0f};
    SDL_RenderFillRect(renderer, &cyan);
    SDL_SetRenderDrawColor(renderer, 250, 180, 35, 255);
    SDL_FRect amber{210.0f, 92.0f, 180.0f, 28.0f};
    SDL_RenderFillRect(renderer, &amber);
    SDL_SetRenderDrawColor(renderer, 230, 80, 150, 255);
    SDL_FRect magenta{402.0f, 92.0f, 220.0f, 28.0f};
    SDL_RenderFillRect(renderer, &magenta);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, 30.0f, 34.0f, "WIICOMPILED TVOS DIAGNOSTIC");
    SDL_RenderDebugText(renderer, 24.0f, 150.0f, "SDL + UIKit entrypoint: OK");
    SDL_RenderDebugText(renderer, 24.0f, 174.0f, "SDL renderer presentation: OK");
    SDL_RenderDebugText(renderer, 24.0f, 198.0f, "Aurora and Dawn are NOT loaded");
    SDL_RenderDebugText(renderer, 24.0f, 222.0f, "If you see this, startup and SDL render work");
    SDL_RenderDebugText(renderer, 24.0f, 246.0f, "The other smoke app's blank gradient is downstream");
    SDL_RenderPresent(renderer);
}
} // namespace

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL-only tvOS diagnostic: SDL_Init failed: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Window* window = SDL_CreateWindow(
        "WiiCompiled SDL tvOS diagnostic", 1280, 720,
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        SDL_Log("SDL-only tvOS diagnostic: window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        SDL_Log("SDL-only tvOS diagnostic: renderer creation failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_SetRenderLogicalPresentation(renderer, 640, 360, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_Log("SDL-only tvOS diagnostic: initialization complete");
    bool running = true;
    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                running = false;
            }
        }

        draw_status(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
