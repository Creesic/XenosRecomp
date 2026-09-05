"""Synthetic prepatched vfetch regression, without title shader assets."""
import argparse
import struct
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True)
    parser.add_argument('--common', required=True)
    parser.add_argument('--work', required=True, type=Path)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    source = args.work / 'baked.bin'
    output = args.work / 'baked.hlsl'
    data = bytearray(96 + 48)

    def word(offset, value):
        struct.pack_into('>I', data, offset, value)

    word(0, 0x102A1141)
    word(4, 96)
    word(8, 48)
    word(24, 36)
    word(40, 48)
    word(64, 3)
    for i, usage in enumerate((0, 3, 5)):
        word(72 + i * 4, (usage << 12) | (i + 1))
    # EXEC_END: three vfetch instructions, followed by an unused NOP CF.
    word(96, 1 | (3 << 12) | (0x15 << 16))
    word(100, 2 << 12)
    full = (31 << 20) | (2 << 25) | (1 << 19)
    for i, (fmt, attributes, swizzle, offset) in enumerate((
            (57, 3, 0xA88, 0), (7, 1, 0xA88, 3), (25, 3, 0xF01, 4))):
        word(108 + i * 12, full | (i << 12))
        word(112 + i * 12, (fmt << 16) | (attributes << 12) | swizzle |
             (0x40000000 if i else 0))
        word(116 + i * 12, 5 | (offset << 8))

    def translate(ok=True):
        source.write_bytes(data)
        result = subprocess.run([args.exe, str(source), str(output), args.common],
                                capture_output=True, text=True)
        assert (result.returncode == 0) == ok, result.stderr
        return output.read_text() if ok else result.stderr

    body = translate().split('float textureLod = 0.0;')[-1]
    assert 'tfetchBakedFloat3(input.iPosition0)' in body
    assert 'tfetchBakedPacked(asuint(input.iNormal0).x, 7u, true, true, false, 0)' in body
    assert 'tfetchBakedPacked(asuint(input.iTexCoord0).x, 25u, true, false, false, 0)).yx' in body
    assert 'swapFloats(' not in body and 'tfetchR11G11B10(' not in body
    word(124, (7 << 16) | (1 << 12) | (1 << 14) | (2 << 24) | 0xA88)
    assert ', 7u, true, true, true, 2)' in translate()
    word(124, (7 << 16) | (2 << 12) | 0xA88)
    assert ', 7u, false, false, false, 0)' in translate()
    # Removing the prepatched flag retains the existing dynamic-declaration path.
    word(0, 0x102A1101)
    body = translate().split('float textureLod = 0.0;')[-1]
    assert 'tfetchBaked' not in body and 'tfetchR11G11B10(' in body
    word(0, 0x102A1141)
    word(124, (63 << 16) | 0xA88)
    assert 'unsupported baked vertex fetch format 63' in translate(False)
    print('Baked fetch decoding, swizzle ownership, flags and rejection passed')


if __name__ == '__main__':
    main()
