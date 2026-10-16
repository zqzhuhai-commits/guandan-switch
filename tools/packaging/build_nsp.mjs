// Optional native NSP builder. Keys come only from the caller, never from the repository.
import { readFile, readdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { buildNsp } from '@tootallnate/hacbrewpack';
import { initializeKeySet, parseNca } from '@tootallnate/nca';
import { decode } from '@tootallnate/romfs';
import { createHash } from 'node:crypto';
const root = resolve(import.meta.dirname, '../..');
const input = process.env.NSP_KEYS;
if (!input) throw new Error('NSP_KEYS must contain the caller-provided build keys');
const bin = async p => new Uint8Array(await readFile(resolve(root,p)));
const main = await bin('dist/native/main');
if (Buffer.from(main.subarray(0,4)).toString() !== 'NSO0') throw new Error('Not a native NSO');
const romfs = {};
for (const f of await readdir(resolve(root,'romfs'))) if(!f.endsWith('.gz')) romfs[f] = new Blob([await bin('romfs/'+f)]);
const keys = await initializeKeySet(input,globalThis.crypto,1);
const result = await buildNsp({keys,plaintext:true,noLogo:true,noPatchNacpLogo:true,titleId:'010000abcd000000',
 exefs:new Map([['main',main],['main.npdm',await bin('dist/native/main.npdm')]]),
 control:new Map([['control.nacp',await bin('dist/native/control.nacp')],['icon_AmericanEnglish.dat',await bin('icon.jpg')]]),romfs});
const bytes = Buffer.from(await result.nsp.arrayBuffer());
if(bytes.toString('ascii',0,4)!=='PFS0') throw new Error('Invalid NSP container');
const count=bytes.readUInt32LE(4), strings=bytes.readUInt32LE(8), dataStart=16+24*count+strings;
if(count!==3) throw new Error('Expected Program, Control and Meta NCAs');
let checkedResources=false;
for(let i=0;i<count;i++){
 const pos=16+24*i, off=Number(bytes.readBigUInt64LE(pos)), len=Number(bytes.readBigUInt64LE(pos+8));
 if(dataStart+off+len>bytes.length) throw new Error('NCA exceeds NSP bounds');
 const nameStart=16+24*count+bytes.readUInt32LE(pos+16), name=bytes.toString('ascii',nameStart,bytes.indexOf(0,nameStart));
 const nca=bytes.subarray(dataStart+off,dataStart+off+len);
 if(!name.startsWith(createHash('sha256').update(nca).digest('hex').slice(0,32)))throw new Error('NCA content hash mismatch');
 const parsed=await parseNca(new Blob([nca]),{keys});
 if(parsed.contentType===0){const section=parsed.sections.find(s=>s.romfsData);if(!section)throw new Error('Missing native RomFS');const files=await decode(section.romfsData);for(const name of ['font.otf','lobby-seaside.png','characters.png']){if(!(files[name] instanceof Blob)||files[name].size!==romfs[name].size)throw new Error('Missing embedded '+name);}checkedResources=true;}
}
if(!checkedResources)throw new Error('Program NCA not verified');
await writeFile(resolve(root,'dist/GuandanTime.nsp'),bytes);
console.log('PASS: native NSP PFS0 bounds, three NCA content hashes, headers, and embedded current assets');
