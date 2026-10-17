#!/usr/bin/env python3
"""Restore the locally verified homebrew NSP without ever handling build keys."""
from pathlib import Path
import hashlib
import json
import struct
ROOT = Path(__file__).resolve().parents[1]
RELEASE = ROOT/'release/v0.5.1'
manifest = json.loads((RELEASE/'native-nsp.json').read_text())
data = bytearray()
for entry in manifest['parts']:
    assert Path(entry['name']).name == entry['name']
    part = (RELEASE/entry['name']).read_bytes()
    assert len(part) == entry['size']
    assert hashlib.sha256(part).hexdigest() == entry['sha256']
    data.extend(part)
assert len(data) == manifest['nsp_size']
assert hashlib.sha256(data).hexdigest() == manifest['nsp_sha256']
assert data[:4] == b'PFS0'
count, strings = struct.unpack_from('<II', data, 4)
assert count == 3
base = 16+count*24+strings
for i in range(count):
    pos = 16+i*24
    off, size, name_off = struct.unpack_from('<QQI', data, pos)
    assert base+off+size <= len(data)
    start = 16+count*24+name_off
    end = data.index(0, start, base)
    name = data[start:end].decode('ascii')
    assert name.startswith(hashlib.sha256(data[base+off:base+off+size]).hexdigest()[:32])
(ROOT/'dist').mkdir(exist_ok=True)
(ROOT/'dist/GuandanTime.nsp').write_bytes(data)
print('PASS: restored NSP part hashes, complete SHA-256 and three NCA hashes')
