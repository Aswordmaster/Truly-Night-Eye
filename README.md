# Truly Night Eye

An adjustable Night Eye addon for Skyrim with its own in-game configuration menu.

[Download Truly Night Eye 1.0](downloads/Truly%20Night%20Eye%201.0.zip?raw=true)

## Effects

- Visibility controls for exposure, shadow gamma, shadow lift and transition speed.
- Tint colour and strength, glow, vignette and grain.
- Adjustable eyeshine strength and colour for human and creature eyes.
- Anaglyph colour split with editable channel colours, separation and desaturation.
- Simulated infrared vision and Predator colours: red enemies, green allies and grey neutral actors by default.
- Editable custom VFX with an adjustable strength.

## Requirements

- Windows 10/11 x64 and Skyrim Special Edition Steam **1.6.1170**. The paid Anniversary Upgrade is not required.
- [SKSE64 **2.2.6**](https://skse.silverlock.org/), matching that game version.
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444), containing `versionlib-1-6-1170-0.bin`.
- [Microsoft Visual C++ v14 Redistributable x64](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).
- **Community Shaders 1.8.4** or **Open Shaders 2.17.0**, with that core's requirements installed. Enable one core.

Effects 11 and SkyUI are not required.

## Installation and controls

Install the download ZIP as a separate mod in your mod manager. Disable an older Night Eye Native integration or other Night Eye shader overrides, then launch Skyrim through SKSE.

Activate a Night Eye ability, such as the Khajiit racial power. Press **F10** to open the Truly Night Eye window. Adjust the effect in its tabs and click **Save** to retain your settings. The menu shortcut can be changed in the Menu tab.

To enable eyeshine, increase **Eyeshine Strength** above zero. Actor Vision, Anaglyph and Custom VFX are optional. Custom VFX can be edited in `Data/Shaders/NightEye/CustomVFX.hlsli`; click **Reload Night Eye Shader** after editing.

Eyeshine supports identifiable eye meshes and included creature masks, including mammoths, giants and mudcrabs. Some custom creature meshes need an additional mask or eye identification. Version 1.0 does not provide automatic eyeshine for every creature mod.

## Source and licence

Truly Night Eye is licensed under **GPL-3.0-or-later**, with the modding and linking permissions in [EXCEPTIONS.md](EXCEPTIONS.md). The full licence is in [LICENSE](LICENSE), and retained third-party notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

[Download the source](https://github.com/Aswordmaster/Truly-Night-Eye/archive/refs/heads/main.zip), including editable eye masks and the dependency sources corresponding to the 1.0 DLL. See [BUILDING.md](BUILDING.md) for build instructions and [dependency-sources](dependency-sources) for dependency source archives and their recorded versions.
