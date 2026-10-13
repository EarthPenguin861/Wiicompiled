cmake_minimum_required(VERSION 3.25)

project(WiiCompiledTvOSSDLSmoke LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    message(FATAL_ERROR "Configure with -DCMAKE_SYSTEM_NAME=tvOS")
endif()
if(NOT CMAKE_OSX_SYSROOT MATCHES "appletvos")
    message(FATAL_ERROR "Configure with -DCMAKE_OSX_SYSROOT=appletvos")
endif()
if(NOT CMAKE_OSX_ARCHITECTURES MATCHES "^arm64$")
    message(FATAL_ERROR "This diagnostic targets Apple TV devices (arm64), not simulator builds")
endif()

include(FetchContent)

# Compile just SDL3 and this diagnostic. Do not configure Dawn/Aurora here:
# this app is specifically meant to tell apart SDL/UIKit startup from the
# full WebGPU/Metal renderer, and should be deliverable without the long Dawn build.
set(SDL_SHARED OFF CACHE BOOL "Build SDL3 shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL3 static library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Build SDL3 tests" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "Install SDL3 targets" FORCE)
set(SDL_INSTALL_TESTS OFF CACHE BOOL "Install SDL3 tests" FORCE)
set(SDL_GPU OFF CACHE BOOL "Disable SDL GPU API for this renderer-only diagnostic" FORCE)
set(SDL_RENDER_GPU OFF CACHE BOOL "Disable the SDL GPU renderer for this diagnostic" FORCE)

set(_sdl_patches "${CMAKE_CURRENT_LIST_DIR}/../../../aurora-main/cmake/AuroraSDL3Patches.cmake")
FetchContent_Declare(SDL
    URL "https://github.com/libsdl-org/SDL/releases/download/release-3.4.4/SDL3-3.4.4.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PATCH_COMMAND "${CMAKE_COMMAND}" -DSDL_SOURCE_DIR=<SOURCE_DIR> -P "${_sdl_patches}"
)
FetchContent_MakeAvailable(SDL)

if(NOT TARGET SDL3::SDL3-static)
    message(FATAL_ERROR "SDL3 static target was not generated")
endif()

add_executable(WiiCompiledTvOSSDLSmoke MACOSX_BUNDLE
    "${CMAKE_CURRENT_LIST_DIR}/../tvos_sdl_smoke.cpp")
target_compile_features(WiiCompiledTvOSSDLSmoke PRIVATE cxx_std_20)
target_link_libraries(WiiCompiledTvOSSDLSmoke PRIVATE SDL3::SDL3-static)

set_target_properties(WiiCompiledTvOSSDLSmoke PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.earthpenguin.wiicompiled.sdl-smoke"
    MACOSX_BUNDLE_BUNDLE_NAME "WiiCompiled SDL tvOS Diagnostic"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/../../cmake/tvos/Info.plist.in"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.earthpenguin.wiicompiled.sdl-smoke"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "3"
    XCODE_ATTRIBUTE_TVOS_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}"
)
