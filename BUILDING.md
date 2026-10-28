# Building Truly Night Eye 1.4

Version 1.4 includes guarded HDR menu routing, resolution-aware UI scaling and optional removal of native Night Eye blur. The HDR selector accepts a valid UI RTV without a scene texture pointer and excludes the actual swapchain backbuffer. Native blur control clones only the standard Night Eye blur-radius and radial-strength interpolators and swaps them on the game thread. Shader assets, GPU buffers and supported host/runtime gates are retained. New settings are optional additions to the version 1 settings document. In-game validation remains pending.

Build on Windows x64 with Visual Studio 2026, the Desktop development with C++ workload, a Windows SDK, CMake 4.2 or newer, Git, and Python 3.10 or newer. Building the plugin does not require a Skyrim installation.
The detector additionally reads `RE::TES::activeImageSpaceModifiers` in its existing game-thread query, matching cached standard Night Eye intro/main records. It does not invoke or replace spell scripts or image-space activation functions. Actor highlighting and postprocessing use the same combined activation state.

Clone and bootstrap [vcpkg](https://github.com/microsoft/vcpkg), then set `VCPKG_ROOT` to its checkout directory. Run these commands from this repository in PowerShell:

```powershell
cmake --preset Release
cmake --build --preset Release
python tools/BuildPackage.py
```

The preset installs the dependencies from `vcpkg.json`. CommonLibSSE-NG is pinned to the public 11.0.0 commit, which supports the format 5 Address Library used by Skyrim 1.7.104 and continues to read the older formats used by 1.5.97 and 1.6.1170. It builds from source against the pinned dependencies to keep its fmt and spdlog ABI consistent with the plugin. The initial build takes longer while compiling this library. The CommonLib SE, AE, and VR layout options are all enabled. The plugin itself accepts only the game runtime and graphics cores listed in the README. Skyrim 1.5.97 requires SKSE64 2.0.20 and `version-1-5-97-0.bin` from the SE Address Library for in-game testing; SE in-game validation remains pending.

The DLL is written to `build/Release/Release/NightEye.dll`. Packaging creates `dist/Truly Night Eye 1.4.zip` with the addon assets, GPL license, and third-party notices. Supply `Truly Night Eye 1.4 Source.zip` alongside the addon package; the matching source ZIP preserves its exact build snapshot. Both are available in `downloads/`, alongside the Jiaye 1.0.4 addon and its separate corresponding source. Build dependencies, settings, logs, and shader caches are excluded from the addon ZIP. The package tool accepts `--build-dir` and `--output-dir` when using a different build or destination directory.

Dependencies use static libraries and the dynamic Release CRT (`/MD`). Only Release is supported by this preset. An existing dependency cache may be supplied through the standard CMake `CMAKE_PREFIX_PATH` and `FETCHCONTENT_SOURCE_DIR_COMMONLIBSSE` variables; the defaults download public dependency source and do not require a local shader-core source tree.

The supplied binary is built offline with MSVC 19.51.36252.0, `/MD`, `/W4` and `/WX`, using an isolated corresponding-source copy. Its cache explicitly enables `COMMONLIB_PREBUILT` and `COMMONLIB_PREBUILT_MULTICONFIG` and reuses the verified CommonLibSSE-NG 11 library with SHA256 `d331d574fbdf141e8e3995546085ef012c7f2f9cdbda190f0484372e39592667`. Its pinned headers, toolset, runtime layouts, fmt/spdlog dependency ABI and OpenVR library match the prior verified build. No dependency rebuild is part of this menu update.

The repository defaults force a pinned CommonLib source build. The accompanying source ZIP keeps both prebuilt options `OFF` by default and permits explicit cache selection for reproducing the offline build. Set `$taskDependencyPrefix`, `$taskCommonLibSource` and `$taskCommonLibBundle` to your matching verified directories, then run these commands from that source ZIP:

```powershell
cmake -S . -B build/Offline -G "Visual Studio 18 2026" -A x64 -DCOMMONLIB_PREBUILT=ON -DCOMMONLIB_PREBUILT_MULTICONFIG=ON "-DCMAKE_PREFIX_PATH=$taskDependencyPrefix" "-DFETCHCONTENT_SOURCE_DIR_COMMONLIBSSE=$taskCommonLibSource" "-DCOMMONLIB_PREBUILT_DIR=$taskCommonLibBundle" -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build/Offline --config Release --target NightEye --parallel 8
python tools/BuildPackage.py --build-dir build/Offline
```

The portable source-build commands above remain the default. The bundled dependency archives and manifests provide the corresponding dependency sources; the offline cache and a graphics-host installation are not required by that default.

To compile the optional shader compiler harness:

```powershell
cmake --build build/Release --config Release --target CompilerHarness
```
