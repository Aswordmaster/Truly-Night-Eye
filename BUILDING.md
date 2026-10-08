# Building Truly Night Eye

Build on Windows x64 with Visual Studio 2026, the Desktop development with C++ workload, a Windows SDK, CMake 4.2 or newer, Git, and Python 3.10 or newer. Building the plugin does not require a Skyrim installation.

Clone and bootstrap [vcpkg](https://github.com/microsoft/vcpkg), then set `VCPKG_ROOT` to its checkout directory. Run these commands from this repository in PowerShell:

```powershell
cmake --preset Release
cmake --build --preset Release
python tools/BuildPackage.py
```

The preset installs the dependencies from `vcpkg.json`. CommonLibSSE-NG is pinned to the public CommonLibVR 4.26.1 commit. Its upstream CMake resolver downloads and verifies the matching Release bundle; if that bundle is unavailable, it builds the pinned dependency from source. The CommonLib SE, AE, and VR layout options match the dependency bundle. The plugin itself accepts only the game runtime and graphics cores listed in the README.

The DLL is written to `build/Release/Release/NightEye.dll`. Packaging creates `dist/Truly Night Eye 1.0.zip` with the addon assets, GPL license, and third-party notices. Build dependencies, settings, logs, and shader caches are excluded. The package tool accepts `--build-dir` and `--output-dir` when using a different build or destination directory.

Dependencies use static libraries and the dynamic Release CRT (`/MD`). Only Release is supported by this preset. An existing dependency cache may be supplied through the standard CMake `CMAKE_PREFIX_PATH`, `FETCHCONTENT_SOURCE_DIR_COMMONLIBSSE`, and `COMMONLIB_PREBUILT_DIR` variables; the defaults download public dependencies and do not require a local shader-core source tree.

To compile the optional shader compiler harness:

```powershell
cmake --build build/Release --config Release --target CompilerHarness
```
