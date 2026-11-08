"""Set Finder bundle metadata without changing either application fork."""
import binascii
from pathlib import Path

def fix_bundle(path):
    path = Path(path)
    before = path.read_bytes()
    assert len(before) >= 128 and before[65:73] == b'APPLSKFR'
    data = bytearray(before)
    flags = ((data[73] << 8) | data[101] | 0x2000) & ~0x0100
    data[73], data[101] = flags >> 8, flags & 255
    data[124:126] = binascii.crc_hqx(data[:124], 0).to_bytes(2, 'big')
    assert data[128:] == before[128:]
    path.write_bytes(data)

if __name__ == '__main__':
    import sys
    for name in sys.argv[1:]: fix_bundle(name)
