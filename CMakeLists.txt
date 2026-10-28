cmake_minimum_required(VERSION 3.25)
project(NightEye VERSION 1.4.0 LANGUAGES CXX)

if(NOT PROJECT_VERSION_TWEAK)
    set(PROJECT_VERSION_TWEAK 0)
endif()
set(NIGHT_EYE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.${PROJECT_VERSION_TWEAK}")

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Night Eye requires a Windows x64 Visual Studio build.")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "Supported build configuration" FORCE)
else()
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Supported build configuration" FORCE)
endif()

find_package(spdlog CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_library(DETOURS_LIBRARY NAMES detours REQUIRED)
find_path(DETOURS_INCLUDE_DIR NAMES detours/detours.h REQUIRED)

include(FetchContent)
set(BUILD_TESTS OFF CACHE BOOL "Build CommonLibSSE tests" FORCE)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "Enable the CommonLibSSE SE layout" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "Enable the CommonLibSSE AE layout" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "Enable the CommonLibSSE VR layout" FORCE)
set(SKSE_SUPPORT_XBYAK ON CACHE BOOL "Enable CommonLibSSE trampoline support" FORCE)
set(COMMONLIB_PREBUILT OFF CACHE BOOL "Build CommonLibSSE against the pinned dependencies" FORCE)
set(COMMONLIB_PREBUILT_MULTICONFIG OFF CACHE BOOL "Build CommonLibSSE against the pinned dependencies" FORCE)
FetchContent_Declare(CommonLibSSE
    GIT_REPOSITORY https://github.com/alandtse/CommonLibSSE-NG.git
    GIT_TAG 94faaed0c60eddd8347767f2d4d29a97c93bde8c)
FetchContent_MakeAvailable(CommonLibSSE)

file(GLOB NIGHT_EYE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_library(NightEye SHARED ${NIGHT_EYE_SOURCES})
target_compile_features(NightEye PRIVATE cxx_std_23)
target_include_directories(NightEye PRIVATE include src "${DETOURS_INCLUDE_DIR}")
target_precompile_headers(NightEye PRIVATE include/PCH.h)
target_compile_definitions(NightEye PRIVATE
    WINVER=0x0601 _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
    NIGHT_EYE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    NIGHT_EYE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    NIGHT_EYE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    NIGHT_EYE_VERSION_TWEAK=${PROJECT_VERSION_TWEAK})
target_compile_options(NightEye PRIVATE /utf-8 /Zc:preprocessor /permissive- /W4 /WX /wd4200 /wd4201)
target_link_libraries(NightEye PRIVATE CommonLibSSE nlohmann_json::nlohmann_json imgui::imgui D3D11 DXGI dxguid d3dcompiler Comctl32 "${DETOURS_LIBRARY}")
set_target_properties(NightEye PROPERTIES OUTPUT_NAME NightEye)

add_executable(CompilerHarness EXCLUDE_FROM_ALL tools/CompilerHarness.cpp)
target_compile_features(CompilerHarness PRIVATE cxx_std_23)
target_precompile_headers(CompilerHarness PRIVATE include/PCH.h)
foreach(property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS LINK_LIBRARIES)
    get_target_property(value NightEye ${property})
    set_property(TARGET CompilerHarness PROPERTY ${property} "${value}")
endforeach()

file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/NightEyeVersion.rc" CONTENT
"#include <windows.h>
VS_VERSION_INFO VERSIONINFO
FILEVERSION ${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},${PROJECT_VERSION_TWEAK}
PRODUCTVERSION ${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},${PROJECT_VERSION_TWEAK}
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_DLL
BEGIN
    BLOCK \"StringFileInfo\"
    BEGIN
        BLOCK \"040904b0\"
        BEGIN
            VALUE \"FileDescription\", \"Truly Night Eye standalone plugin\\0\"
            VALUE \"FileVersion\", \"${NIGHT_EYE_VERSION}\\0\"
            VALUE \"ProductName\", \"Truly Night Eye\\0\"
            VALUE \"ProductVersion\", \"${NIGHT_EYE_VERSION}\\0\"
        END
    END
    BLOCK \"VarFileInfo\"
    BEGIN
        VALUE \"Translation\", 0x0409, 1200
    END
END
")
target_sources(NightEye PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/NightEyeVersion.rc")
