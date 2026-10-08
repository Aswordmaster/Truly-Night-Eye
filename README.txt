Truly Night Eye 1.0

An adjustable Night Eye effect with its own in-game configuration menu.

EFFECTS
- Visibility: exposure, shadow gamma, shadow lift and transition speed.
- Appearance: tint colour and strength, glow, vignette and grain.
- Eyeshine: adjustable strength and colour for human and creature eyes.
- Anaglyph: adjustable colour split, channel colours, separation and desaturation.
- Actor Vision: simulated infrared colours or Predator relationship colours.
  Predator defaults to red enemies, green allies and grey neutral actors.
- Custom VFX: editable shader effects with an adjustable strength.

REQUIREMENTS
- Windows 10/11, 64-bit.
- Skyrim Special Edition, Steam version 1.6.1170.
  The paid Anniversary Upgrade is not required.
- SKSE64 2.2.6, Steam build matching Skyrim 1.6.1170.
- Address Library for SKSE Plugins containing versionlib-1-6-1170-0.bin.
- Latest Microsoft Visual C++ v14 Redistributable, x64.
- Exactly one enabled stock graphics core: Community Shaders 1.8.4
  OR Open Shaders 2.17.0, with that core's own requirements installed.

Dependency pages:
SKSE: https://skse.silverlock.org/ (Archived builds, Steam 2.2.6)
Address Library: https://www.nexusmods.com/skyrimspecialedition/mods/32444
Visual C++: https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist

Effects 11 and SkyUI are not required.

INSTALLATION
Install the ZIP as a separate mod in your mod manager and enable it alongside
ONE supported graphics core. Disable the older Night Eye Native integration
and other Night Eye shader overrides if installed. Launch the game through SKSE.

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

CUSTOM VFX
Edit Data/Shaders/NightEye/CustomVFX.hlsli, then click Reload Night Eye Shader
in the Custom VFX tab. The last working shader is retained if an edit fails.

EYESHINE COMPATIBILITY
Eyeshine supports identifiable eye meshes and included creature masks, including
mammoths, giants and mudcrabs. Some custom creature meshes need an additional
mask or eye identification; automatic eyeshine for every creature mod is not
included in version 1.0.
