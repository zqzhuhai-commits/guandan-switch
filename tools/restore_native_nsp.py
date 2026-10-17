#!/usr/bin/env python3
"""Restore both locally verified homebrew NSPs; no keys are handled here."""
from pathlib import Path
import hashlib,json,struct
ROOT=Path(__file__).resolve().parents[1];RELEASE=ROOT/'release/v0.5.2'
m=json.loads((RELEASE/'native-nsp.json').read_text());(ROOT/'dist').mkdir(exist_ok=True)
for output,prefix in [('GuandanTime.nsp',''),('GuandanTime-Forwarder.nsp','forwarder_')]:
 data=bytearray()
 for e in m[prefix+'parts']:
  assert Path(e['name']).name==e['name'];b=(RELEASE/e['name']).read_bytes();assert len(b)==e['size'];assert hashlib.sha256(b).hexdigest()==e['sha256'];data.extend(b)
 assert len(data)==m[prefix+'nsp_size'];assert hashlib.sha256(data).hexdigest()==m[prefix+'nsp_sha256'];assert data[:4]==b'PFS0'
 count,strings=struct.unpack_from('<II',data,4);assert count==3;base=16+count*24+strings
 for i in range(count):
  pos=16+i*24;off,size,no=struct.unpack_from('<QQI',data,pos);assert base+off+size<=len(data)
  start=16+count*24+no;end=data.index(0,start,base);name=data[start:end].decode('ascii');assert name.startswith(hashlib.sha256(data[base+off:base+off+size]).hexdigest()[:32])
 (ROOT/'dist'/output).write_bytes(data);print('PASS restored NSP, all part/container/NCA hashes:',output)
