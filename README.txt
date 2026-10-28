Truly Night Eye 1.4

An adjustable Night Eye effect with its own in-game configuration menu.
Mod version: 1.4. Compatibility preview; live validation of the HDR menu fix
and Skyrim 1.5.97 remains pending.

This version uses the engine's redirected framebuffer UI target when valid,
allowing the graphics core to composite the menu into HDR output. It retains
the original backbuffer fallback and Present order. Other visual effects and
shader assets are retained, and older settings files remain supported. The HDR
routing correction accepts a valid UI
target even when the separate framebuffer texture pointer is absent.

EFFECTS
- Visibility: exposure, shadow gamma, shadow lift and transition speed.
- Appearance: tint colour and strength, glow, vignette and grain.
- Optional removal of the standard Night Eye blur and radial blur.
- Resolution-aware menu sizing with an adjustable Menu Scale multiplier.
- Eyeshine: adjustable strength and colour for human and creature eyes.
- Anaglyph: adjustable colour split, channel colours, separation and desaturation.
- Actor Vision: simulated infrared colours or Predator relationship colours.
  Predator defaults to red enemies, green allies and grey neutral actors.
- Custom VFX: editable shader effects with an adjustable strength.

REQUIREMENTS
- Windows 10/11, 64-bit.
- Skyrim Special Edition, Steam version 1.5.97, 1.6.1130, 1.6.1170 or 1.7.104.
  The paid Anniversary Upgrade is not required.
- SKSE64 2.0.20 for Skyrim 1.5.97; the matching Steam SKSE64 build for the
  other listed game versions.
- Address Library for SKSE Plugins containing the matching runtime file:
  version-1-5-97-0.bin (SE), versionlib-1-6-1130-0.bin,
  versionlib-1-6-1170-0.bin or
  versionlib-1-7-104-0.bin (format 5).
- Latest Microsoft Visual C++ v14 Redistributable, x64.
- Exactly one enabled stock graphics core: Community Shaders 1.8.4,
  Community Shaders 1.9.1 or Open Shaders 2.17.0, with its own requirements.
  Skyrim 1.7.104 requires the core to declare format 5 Address Library support:
  use Community Shaders 1.9.1 or Open Shaders 2.17.0 on that runtime.
  Community Shaders 1.8.4 is disabled on Skyrim 1.7.104.
- GOG, Skyrim VR and other Steam runtimes are unsupported by this preview.

Dependency pages:
SKSE: https://skse.silverlock.org/ (select the matching Steam build)
Address Library: https://www.nexusmods.com/skyrimspecialedition/mods/32444
Visual C++: https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist

Effects 11 and SkyUI are not required.

INSTALLATION
Install the ZIP as a separate mod in your mod manager and enable it alongside
ONE supported graphics core. Disable other Truly Night Eye versions, the older
Night Eye Native integration and other Night Eye shader overrides if installed.
Launch the game through SKSE.

CONTROLS
Activate a Night Eye ability, such as the Khajiit racial power.
Press F10 in game to open the separate Night Eye overlay. It is a standalone
window, separate from the graphics core's menu.
Adjust Visibility as desired. To enable eyeshine, open Eyeshine and increase
Eyeshine Strength above zero; fresh installations default to zero.
Actor Vision highlights visible living actors, including modded creatures.
Choose Infrared Style or Predator in the Actor Vision tab and adjust the colours,
highlight amount and brightness. Infrared Style uses simulated heat colours.
Anaglyph and Custom VFX are optional and initially disabled.
Change the shortcut in the Menu tab. Click Save to retain your settings.
The menu scales text and controls automatically for the display resolution:
1080p uses the original size, 1440p uses about 1.33x and 4K uses 2x. Adjust
Menu Scale in the Menu tab from 0.75x to 2x relative to that automatic size.
In Visibility, enable Remove Night Eye Blur to suppress the standard Night Eye
intro, active and ending blur. Reactivate Night Eye after changing this option.
It preserves other modifier settings. Glow remains a separate control.
Custom spells using different image-space modifiers are outside this toggle.

NIGHT EYE COMPATIBILITY
The addon observes active Night Eye magic effects and the standard Night Eye
intro and active visual modifiers. This also covers scripted abilities using
those modifiers, including Orum's Aquatic Escape from Arcane Accessories.
Spell activation, scripts, costs, duration, invisibility and waterbreathing are
unchanged. Fully custom scripts using unrelated visual modifiers need separate
compatibility support. The ending modifier alone does not activate the addon.

MENU LOGS AND HDR VALIDATION
The startup log identifies the addon version after SKSE initialization. Menu
diagnostics record the selected target route initially and when it changes.
On the reported CS 1.9.1 HDR setup, the corrected build should report
"redirected framebuffer UI" with RTV format 28 when the menu opens.
For HDR testing, leave HDR enabled, open the addon menu using your configured
shortcut, check its controls and the graphics core's menu, then close Skyrim.
Share the complete NightEye.log from your Windows Documents folder under
My Games/Skyrim Special Edition/SKSE. Documents may be redirected to OneDrive.
Live visibility of the corrected HDR menu remains unverified.

CUSTOM VFX
Edit Data/Shaders/NightEye/CustomVFX.hlsli, then click Reload Night Eye Shader
in the Custom VFX tab. The last working shader is retained if an edit fails.

EYESHINE COMPATIBILITY
Eyeshine supports identifiable eye meshes and included creature masks, including
mammoths, giants and mudcrabs. Some custom creature meshes need an additional
mask or eye identification; automatic eyeshine for every creature mod is not
included in version 1.4.

LICENSE AND SOURCE
GNU GPL version 3 or later, with the additional permissions in EXCEPTIONS.md.
See LICENSE.txt and THIRD_PARTY_NOTICES.md for the full terms and credits.
Matching source: Truly Night Eye 1.4 Source.zip, supplied alongside this preview.
It includes build instructions, editable eye masks and dependency source archives.
Project: https://github.com/Aswordmaster/Truly-Night-Eye
The published main source archive and stable 1.0 download still contain the
original build; they do not contain this local compatibility preview.
