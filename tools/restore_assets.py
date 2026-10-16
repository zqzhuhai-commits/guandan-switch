#!/usr/bin/env python3
"""Restore the exact tested release, font and NRO from verified repository parts."""
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
manifest = json.loads((ROOT / 'release/manifest.json').read_text())
archive = ROOT / manifest['archive']
archive.parent.mkdir(parents=True, exist_ok=True)
tmp = archive.with_suffix('.zip.tmp')
try:
    with tmp.open('wb') as target:
        for part in manifest['parts']:
            data = (ROOT / part['path']).read_bytes()
            if len(data) != part['size'] or hashlib.sha256(data).hexdigest() != part['sha256']:
                raise ValueError('Release part checksum mismatch: ' + part['path'])
            target.write(data)
    data = tmp.read_bytes()
    if len(data) != manifest['size'] or hashlib.sha256(data).hexdigest() != manifest['sha256']:
        raise ValueError('Full release checksum mismatch')
    tmp.replace(archive)
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None:
            raise ValueError('Invalid ZIP contents')
        for source, dest in [
            ('GuandanSwitch-v0.1/source-project/romfs/font.otf', 'romfs/font.otf'),
            ('GuandanSwitch-v0.1/switch/GuandanSwitch/GuandanSwitch.nro', 'switch/GuandanSwitch/GuandanSwitch.nro'),
        ]:
            output = ROOT / dest
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(z.read(source))
            print('Restored:', dest)
    nro = (ROOT / 'switch/GuandanSwitch/GuandanSwitch.nro').read_bytes()
    if nro[16:20] != b'NRO0':
        raise ValueError('Invalid NRO header')
    print('Release ZIP SHA256:', manifest['sha256'])
except Exception:
    tmp.unlink(missing_ok=True)
    raise
