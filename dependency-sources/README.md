# Dependency source for Truly Night Eye 1.0

This folder provides the source of the non-system libraries used by the 1.0
plugin, together with the matching vcpkg build recipes and patches.

`Manifest.json` records each upstream location, installed package version,
original source archive SHA-512, bundled archive SHA-256, and consumed vcpkg
port tree. Archives were verified against the installed packages' source
records before copying or repacking them.

The bundle includes Detours, DirectXMath, DirectXTK, fmt, ImGui, nlohmann-json,
spdlog, Xbyak, rapidcsv, CommonLibSSE-NG, OpenVR, and MinHook's hde64 decoder. Each `*-vcpkg-port.zip`
contains that library's original port files, including its build instructions
and patches. The baseline is
`dddca6fa87f177e0678e2545c4b4636a44aa05bd`.

The current build pins CommonLibSSE-NG 11.0.0, commit
`94faaed0c60eddd8347767f2d4d29a97c93bde8c`. Its source snapshot includes the
build scripts, GPL text, modding permissions, and original third-party notices.
The matching OpenVR submodule is commit `60eb187801956ad277f1cae6680e3a410ee0873b`.
Its separate snapshot includes library sources, headers, root build files, and
license; extract it into CommonLibSSE-NG's `extern/openvr` directory when building
from these snapshots. MinHook 1.3.4, commit `c3fcafdc10146beb5919319d0683e44e3c30d537`,
provides the hde64 decoder used by CommonLibSSE-NG's trampoline checks.

The earlier CommonLibSSE-NG 4.26.1 and OpenVR `ebdea152` snapshots are retained
for the initially distributed 1.0 DLL. The manifest records both dependency
generations without removing their source or notices.

The installed DirectXMath package is labelled `2026-03-12`, while its consumed
source revision is `apr2025`. The pinned port explicitly selects that revision;
the bundle preserves its three original patches.

The ImGui snapshot removes two unrelated prebuilt GLFW example libraries and
six optional font files. Its library source, embedded default fonts, examples'
source, build files, and license documentation are retained. The manifest lists
the omitted files and the original archive's hash. OpenVR sample applications,
their assets, and prebuilt distribution binaries are also omitted. No compiled
libraries, Skyrim meshes, creature textures, or local configuration are included.

Original library and embedded-font notices are preserved in the repository's
`LICENSES` directory and within these upstream source archives. ImGui's three
stb headers offer MIT or Public Domain terms; their complete original dual
license notice is also preserved as `LICENSES/stb.txt`.

The addon source, shaders, build and package scripts are in the repository root.
Editable eyeshine mask source is in `assets/eyeshine`. General purpose build
tools and Windows system libraries are obtained separately as described in
`BUILDING.md`.
