"""Stage the addon alone, without host binaries, shaders or user settings."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import argparse
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def allowed(path):
    name = path.as_posix()
    return (name == 'SKSE/Plugins/NightEye.dll' or name.startswith('SKSE/Plugins/NightEye/')
            or name.startswith('Shaders/NightEye/')
            or name in ('README.txt', 'LICENSE.txt', 'EXCEPTIONS.md', 'THIRD_PARTY_NOTICES.md'))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/Release')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    dll = args.build_dir / 'Release/NightEye.dll'
    output = args.output_dir / 'Truly Night Eye 1.0'
    if not dll.is_file():
        raise RuntimeError('The standalone Release DLL has not been built')
    output.mkdir(parents=True, exist_ok=True)
    files = {Path('SKSE/Plugins/NightEye.dll'): dll,
             Path('LICENSE.txt'): ROOT / 'LICENSE',
             Path('EXCEPTIONS.md'): ROOT / 'EXCEPTIONS.md',
             Path('THIRD_PARTY_NOTICES.md'): ROOT / 'THIRD_PARTY_NOTICES.md'}
    license_files = sorted((ROOT / 'LICENSES').rglob('*'))
    if not any(path.is_file() for path in license_files):
        raise RuntimeError('Missing third-party license files')
    for source in license_files:
        if source.is_file():
            relative = source.relative_to(ROOT / 'LICENSES')
            files[Path('SKSE/Plugins/NightEye/Licenses') / relative] = source
    for source in files.values():
        if not source.is_file():
            raise RuntimeError(f'Missing dependency notice: {source}')
    for directory in (ROOT / 'package', ROOT / 'Shaders'):
        for path in directory.rglob('*'):
            if path.is_file():
                relative = path.relative_to(directory)
                if directory.name == 'Shaders':
                    relative = Path('Shaders') / relative
                if not allowed(relative):
                    raise RuntimeError(f'Unexpected addon asset: {relative}')
                if relative in files:
                    raise RuntimeError(f'Duplicate addon asset: {relative}')
                files[relative] = path
    for relative, source in files.items():
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    notice_path = output / 'THIRD_PARTY_NOTICES.md'
    notice = notice_path.read_text(encoding='utf-8')
    notice = notice.replace('(LICENSE)', '(LICENSE.txt)')
    notice = notice.replace('(LICENSES/', '(SKSE/Plugins/NightEye/Licenses/')
    notice = notice.replace('(LICENSES)', '(SKSE/Plugins/NightEye/Licenses)')
    notice = notice.replace('(BUILDING.md)', '(https://github.com/Aswordmaster/Truly-Night-Eye/blob/main/BUILDING.md)')
    notice = notice.replace('`assets/eyeshine`', '[assets/eyeshine](https://github.com/Aswordmaster/Truly-Night-Eye/tree/main/assets/eyeshine)')
    notice_path.write_text(notice, encoding='utf-8')
    readme = '''Truly Night Eye 1.0

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

LICENSE AND SOURCE
GNU GPL version 3 or later, with the additional permissions in EXCEPTIONS.md.
See LICENSE.txt and THIRD_PARTY_NOTICES.md for the full terms and credits.
Source: https://github.com/Aswordmaster/Truly-Night-Eye
'''
    (output / 'README.txt').write_text(readme, encoding='utf-8')
    actual = {path.relative_to(output): path for path in output.rglob('*') if path.is_file()}
    expected = set(files) | {Path('README.txt'), Path('Manifest.json')}
    for relative in actual:
        if relative not in expected or (relative.name != 'Manifest.json' and not allowed(relative)):
            raise RuntimeError(f'Unrelated file in staged addon: {relative}')
        if relative.name == 'Settings.json' or 'ShaderCache' in relative.parts or relative.suffix.lower() in ('.log', '.pso', '.vso', '.cso', '.pdb', '.tmp', '.dmp'):
            raise RuntimeError(f'User data or development output in staged addon: {relative}')
    manifest = {'packageVersion': '1.0', 'runtime': '1.6.1170',
                'hosts': ['Community Shaders 1.8.4', 'Open Shaders 2.17.0'],
                'stockFilesReplaced': [],
                'files': {path.as_posix(): {'sha256': sha(file), 'bytes': file.stat().st_size}
                          for path, file in sorted(actual.items()) if path.name != 'Manifest.json'}}
    (output / 'Manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    archive_path = output.with_name(output.name + '.zip')
    with ZipFile(archive_path, 'w', ZIP_DEFLATED) as archive:
        for file in sorted(output.rglob('*')):
            if file.is_file():
                archive.write(file, file.relative_to(output).as_posix())
    with ZipFile(archive_path) as archive:
        if archive.testzip() is not None:
            raise RuntimeError('Addon archive failed its CRC check')
    print(json.dumps({'folder': str(output), 'archive': str(archive_path),
                      'runtimeFiles': len(files), 'archiveSHA256': sha(archive_path),
                      'stockFilesReplaced': []}, indent=2))

if __name__ == '__main__':
    main()
