#!/usr/bin/env python3
"""Reject stale/non-native release files and verify embedded NRO asset bounds."""
from pathlib import Path
import hashlib
import struct
import sys
p = Path(sys.argv[1])
b = p.read_bytes()
assert b[0x10:0x14] == b'NRO0', 'Not an NRO'
size = struct.unpack_from('<I', b, 0x18)[0]
assert 0x80 < size < len(b) and b[size:size+4] == b'ASET', 'Missing embedded assets'
for offset in (size+8, size+24, size+40):
    start, length = struct.unpack_from('<QQ', b, offset)
    assert length > 0 and size+start+length <= len(b), 'Invalid embedded asset bounds'
romfs_offset, romfs_size = struct.unpack_from('<QQ', b, size+40)
r = memoryview(b)[size+romfs_offset:size+romfs_offset+romfs_size]
assert len(r) >= 80
file_meta, file_meta_size, file_data = struct.unpack_from('<QQQ', r, 0x38)
assert file_meta+file_meta_size <= len(r) and file_data <= len(r)
entries = {}
entry = 0
while entry < file_meta_size:
    pos = file_meta+entry
    data_offset, data_size, _, name_len = struct.unpack_from('<QQII', r, pos+8)
    assert 0 < name_len <= 255 and pos+32+name_len <= file_meta+file_meta_size
    name = bytes(r[pos+32:pos+32+name_len]).decode('utf-8')
    assert file_data+data_offset+data_size <= len(r)
    entries[name] = r[file_data+data_offset:file_data+data_offset+data_size]
    entry += 32+((name_len+3)//4)*4
for name in ('lobby-seaside.png', 'characters.png', 'font.otf', 'music.wav', 'voice.wav', 'voice.idx'):
    assert name in entries, f'Missing embedded resource: {name}'
    assert hashlib.sha256(entries[name]).digest() == hashlib.sha256((Path('romfs')/name).read_bytes()).digest(), f'Stale embedded asset: {name}'
elf = Path('GuandanSwitch.elf').read_bytes()
assert elf[:4] == b'\x7fELF' and struct.unpack_from('<H',elf,18)[0] == 183, 'Not an AArch64 ELF'
assert b'sdmc:/switch/GuandanSwitch/' in b, 'Missing application data directory'
print('PASS: NRO0, embedded icon/NACP/RomFS bounds, current visual assets, data directory')
print('SHA256:', hashlib.sha256(b).hexdigest())
