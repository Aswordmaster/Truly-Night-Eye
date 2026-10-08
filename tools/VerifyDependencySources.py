"""Check the bundled dependency source and vcpkg recipe archive hashes."""

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1] / 'dependency-sources'


def verify(name, expected):
    path = ROOT / name
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError(f'{name}: source archive SHA-256 differs')


def main():
    manifest = json.loads((ROOT / 'Manifest.json').read_text())
    count = 0
    for package in manifest['packages']:
        verify(package['bundledArchive'], package['bundledArchiveSHA256'])
        count += 1
        if 'vcpkgPortArchive' in package:
            verify(package['vcpkgPortArchive'], package['vcpkgPortArchiveSHA256'])
            count += 1
    print(f'Verified {count} dependency source and build recipe archives.')


if __name__ == '__main__':
    main()
