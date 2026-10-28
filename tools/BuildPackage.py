"""Stage the addon alone, without host binaries, shaders or user settings."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import argparse
import hashlib
import json
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
project_version = re.search(r'^project\(NightEye VERSION (\d+\.\d+\.\d+(?:\.\d+)?)\s+LANGUAGES\b',
                            (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8'), re.MULTILINE)
if not project_version:
    raise RuntimeError('Cannot resolve the Night Eye version from CMakeLists.txt')
PACKAGE_VERSION = project_version.group(1).removesuffix('.0')
PACKAGE_NAME = f'Truly Night Eye {PACKAGE_VERSION}'

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
    output = args.output_dir / PACKAGE_NAME
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
    notice = notice.replace('[BUILDING.md](BUILDING.md)', 'BUILDING.md in the accompanying source ZIP')
    notice = notice.replace('`assets/eyeshine`', '`assets/eyeshine` in the accompanying source ZIP')
    notice_path.write_text(notice, encoding='utf-8')
    readme = (ROOT / 'README.txt').read_text(encoding='utf-8')
    (output / 'README.txt').write_text(readme, encoding='utf-8')
    actual = {path.relative_to(output): path for path in output.rglob('*') if path.is_file()}
    expected = set(files) | {Path('README.txt'), Path('Manifest.json')}
    for relative in actual:
        if relative not in expected or (relative.name != 'Manifest.json' and not allowed(relative)):
            raise RuntimeError(f'Unrelated file in staged addon: {relative}')
        if relative.name == 'Settings.json' or 'ShaderCache' in relative.parts or relative.suffix.lower() in ('.log', '.pso', '.vso', '.cso', '.pdb', '.tmp', '.dmp'):
            raise RuntimeError(f'User data or development output in staged addon: {relative}')
    manifest = {'packageVersion': PACKAGE_VERSION, 'settingsSchemaVersion': 1, 'status': 'compatibility-preview',
                'platform': 'Steam',
                'runtimes': ['1.5.97', '1.6.1130', '1.6.1170', '1.7.104'],
                'hosts': ['Community Shaders 1.8.4', 'Community Shaders 1.9.1', 'Open Shaders 2.17.0'],
                'hostRestrictions': {'1.7.104': ['Community Shaders 1.9.1', 'Open Shaders 2.17.0']},
                'liveValidation': 'pending', 'unsupportedPlatforms': ['GOG', 'VR'],
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
