"""Convert editable grayscale masks to the runtime's uncompressed R8 DDS format."""

import argparse
from pathlib import Path
import struct

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
MAXIMUM_MASK_DIMENSION = 2048
DDS_HEADER_SIZE = 124
DDS_PIXELFORMAT_SIZE = 32
DDS_HEADER_FLAGS = 0x0002100F
DDS_PIXELFORMAT_FOURCC = 0x00000004
DDS_TEXTURE_CAPS = 0x00001000
DXGI_FORMAT_R8_UNORM = 61
D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3


def convert(path):
    with Image.open(path) as image:
        if image.mode != 'L':
            raise ValueError(f'{path.name}: a grayscale 8-bit PNG is required')
        width, height = image.size
        if not (0 < width <= MAXIMUM_MASK_DIMENSION and
                0 < height <= MAXIMUM_MASK_DIMENSION):
            raise ValueError(f'{path.name}: mask dimensions must be 1 to {MAXIMUM_MASK_DIMENSION}')
        pixels = image.tobytes()
    header = [DDS_HEADER_SIZE, DDS_HEADER_FLAGS, height, width, width, 1, 1]
    header.extend([0] * 11)
    header.extend([DDS_PIXELFORMAT_SIZE, DDS_PIXELFORMAT_FOURCC,
                   int.from_bytes(b'DX10', 'little'), 0, 0, 0, 0, 0])
    header.extend([DDS_TEXTURE_CAPS, 0, 0, 0, 0])
    extended_header = [DXGI_FORMAT_R8_UNORM, D3D10_RESOURCE_DIMENSION_TEXTURE2D, 0, 1, 0]
    return b'DDS ' + struct.pack('<31I', *header) + struct.pack('<5I', *extended_header) + pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-dir', type=Path, default=ROOT / 'assets/eyeshine')
    parser.add_argument('--output-dir', type=Path,
                        default=ROOT / 'package/Shaders/NightEye/Eyeshine')
    parser.add_argument('--check', action='store_true',
                        help='Compare with existing DDS files without writing')
    args = parser.parse_args()
    sources = sorted(args.source_dir.glob('*.png'))
    if not sources:
        raise ValueError(f'No PNG masks found in {args.source_dir}')
    if not args.check:
        args.output_dir.mkdir(parents=True, exist_ok=True)
    for source in sources:
        target = args.output_dir / source.with_suffix('.dds').name
        converted = convert(source)
        if args.check:
            if not target.is_file() or target.read_bytes() != converted:
                raise ValueError(f'{target}: generated DDS differs')
        else:
            target.write_bytes(converted)
    print(f'{"Verified" if args.check else "Converted"} {len(sources)} eyeshine masks.')


if __name__ == '__main__':
    main()
