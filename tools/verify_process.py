#!/usr/bin/env python3
"""Verify actual compiled NPDM application capabilities, not just JSON inputs."""
from pathlib import Path
import struct,json
for name,tid in [('native',0x010000abcd000000),('forwarder',0x010000abcd001000)]:
 b=(Path('dist')/name/'main.npdm').read_bytes();assert b[:4]==b'META'
 aci=struct.unpack_from('<I',b,0x70)[0];assert struct.unpack_from('<Q',b,aci+0x10)[0]==tid
 ko,ks=struct.unpack_from('<II',b,aci+0x30);caps=struct.unpack_from('<'+'I'*(ks//4),b,aci+ko)
 app=[v>>14&7 for v in caps if v&0x3fff==0x1fff];assert app==[1],(name,'application_type missing or wrong',app)
 assert b[0xe]==44 and b[0xf]==0
 assert struct.unpack_from('<I',b,0x1c)[0]>=0x100000
 print('PASS compiled NPDM:',name,'application_type=1, title ID, priority, CPU and stack')
