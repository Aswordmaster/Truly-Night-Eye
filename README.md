# Truly Night Eye

An adjustable Night Eye addon for Skyrim with its own in-game configuration menu.

| Edition | Addon | Matching source |
| --- | --- | --- |
| Main 1.4 | [Download](downloads/Truly%20Night%20Eye%201.4.zip?raw=true) | [Source ZIP](downloads/Truly%20Night%20Eye%201.4%20Source.zip?raw=true) |
| Jiaye AIO 1.0.4 | [Download](downloads/Truly%20Night%20Eye%201.0.4%20-%20Jiaye%20AIO.zip?raw=true) | [Source ZIP](downloads/Truly%20Night%20Eye%201.0.4%20-%20Jiaye%20AIO%20Source.zip?raw=true) |

Enable one addon edition. The main edition supports the stock graphics cores listed below. The Jiaye edition requires the exact `CommunityShaders_AIO-2026-10-03T06-46Z.7z` build and its included README's postprocessing requirements. The source ZIPs preserve each edition's build snapshot, including its build-time documentation.

[Previous stable 1.0 download](downloads/Truly%20Night%20Eye%201.0.zip?raw=true), tested on Steam Skyrim 1.6.1170.

## Effects

- Visibility controls for exposure, shadow gamma, shadow lift and transition speed.
- Tint colour and strength, glow, vignette and grain.
- Adjustable eyeshine strength and colour for human and creature eyes.
- Anaglyph colour split with editable channel colours, separation and desaturation.
- Simulated infrared vision and Predator colours: red enemies, green allies and grey neutral actors by default.
- Editable custom VFX with an adjustable strength.

## Stable 1.0 requirements

- Windows 10/11 x64 and Skyrim Special Edition Steam **1.6.1170**. The paid Anniversary Upgrade is not required.
- [SKSE64 **2.2.6**](https://skse.silverlock.org/), matching that game version.
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444), containing `versionlib-1-6-1170-0.bin`.
- [Microsoft Visual C++ v14 Redistributable x64](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).
- **Community Shaders 1.8.4** or **Open Shaders 2.17.0**, with that core's requirements installed. Enable one core.

Effects 11 and SkyUI are not required.

## Main 1.4 compatibility

The main branch contains **Truly Night Eye 1.4** for Steam SE and AE. It adds guarded menu routing to the engine's redirected framebuffer UI target so the graphics core can include the overlay in its HDR composite. The corrected routing accepts a validated UI target when the separate framebuffer texture pointer is absent, as observed in the CS 1.9.1 diagnostic log. The actual swapchain backbuffer and scene resources are excluded from redirected target selection. The existing backbuffer fallback and Present order are retained. Other visual effects and shader assets are retained, and older settings files remain supported.

The startup log reports the addon version after SKSE initialization, and menu diagnostics record the selected target route. Live validation of the HDR menu fix and Skyrim 1.5.97 remains pending. The previous 1.0 binary remains available above; the current repository source tracks main 1.4.

The preview targets these exact Steam runtimes and stock graphics cores:

| Steam Skyrim runtime | Required Address Library file | Supported graphics cores |
| --- | --- | --- |
| 1.5.97 | `version-1-5-97-0.bin` | Community Shaders 1.8.4, Community Shaders 1.9.1, Open Shaders 2.17.0 |
| 1.6.1130 | `versionlib-1-6-1130-0.bin` | Community Shaders 1.8.4, Community Shaders 1.9.1, Open Shaders 2.17.0 |
| 1.6.1170 | `versionlib-1-6-1170-0.bin` | Community Shaders 1.8.4, Community Shaders 1.9.1, Open Shaders 2.17.0 |
| 1.7.104 | `versionlib-1-7-104-0.bin`, format 5 | Community Shaders 1.9.1, Open Shaders 2.17.0 |

Skyrim **1.5.97 requires SKSE64 2.0.20** and the SE Address Library file listed above. For the other runtimes, install the matching SKSE64 Steam build and Address Library file. Install the x64 Visual C++ Redistributable and the chosen core's own requirements, then enable one graphics core. On Skyrim 1.7.104, the core must declare format 5 Address Library support; Community Shaders 1.8.4 is disabled on that runtime. GOG, Skyrim VR, and other Steam runtimes are unsupported by this preview.

The addon and matching source ZIPs are linked above. Disable older addon versions when enabling an update. See [BUILDING.md](BUILDING.md) for building main 1.4. The Jiaye edition's complete editable source and build instructions are included in its separate source ZIP.

## Installation and controls

Install the download ZIP as a separate mod in your mod manager. Disable an older Night Eye Native integration or other Night Eye shader overrides, then launch Skyrim through SKSE.

Activate a Night Eye ability, such as the Khajiit racial power. Press **F10** to open the Truly Night Eye window. Adjust the effect in its tabs and click **Save** to retain your settings. The menu shortcut can be changed in the Menu tab.

To enable eyeshine, increase **Eyeshine Strength** above zero. Actor Vision, Anaglyph and Custom VFX are optional. Custom VFX can be edited in `Data/Shaders/NightEye/CustomVFX.hlsli`; click **Reload Night Eye Shader** after editing.

In 1.4, the menu automatically scales text and controls for the display resolution: 1080p retains the original size, 1440p uses about 1.33x and 4K uses 2x. **Menu Scale** in the Menu tab adjusts that size from 0.75x to 2x. **Remove Night Eye Blur** in Visibility disables blur and radial blur in the standard Night Eye intro, active and ending modifiers. Reactivate Night Eye after changing it. Other modifier settings and the separate Glow control are preserved; custom spells using different modifiers are outside this toggle. Both new settings are saved with the existing version 1 settings document, and older documents retain defaults for missing fields. Live verification of these controls remains pending.

Eyeshine supports identifiable eye meshes and included creature masks, including mammoths, giants and mudcrabs. Some custom creature meshes need an additional mask or eye identification. Version 1.4 does not provide automatic eyeshine for every creature mod.

The current addon also observes the standard Night Eye intro and active visual modifiers. This recognizes scripted abilities applying those presets, including Arcane Accessories' **Orum's Aquatic Escape**, alongside the existing magic-effect detector. It does not modify spell activation, scripts, costs, duration, invisibility or waterbreathing. The ending modifier alone does not activate the addon. Scripts using unrelated custom presets or shader-only changes still need separate compatibility support. Spell record/script inspection and native detector checks verify the compatibility path; in-game confirmation remains pending.

## Source and licence

Truly Night Eye is licensed under **GPL-3.0-or-later**, with the modding and linking permissions in [EXCEPTIONS.md](EXCEPTIONS.md). The full licence is in [LICENSE](LICENSE), and retained third-party notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

[Download the current main repository source](https://github.com/Aswordmaster/Truly-Night-Eye/archive/refs/heads/main.zip), including editable eye masks and dependency sources. For the exact source snapshots corresponding to the distributed DLLs, use the matching main 1.4 or Jiaye 1.0.4 source ZIP linked above. See [BUILDING.md](BUILDING.md) for build instructions and [dependency-sources](dependency-sources) for dependency source archives and their recorded versions.
