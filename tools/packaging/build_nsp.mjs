// Native homebrew NSP. Caller-provided keys stay in memory and are never bundled.
import { readFile, readdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { createHash } from 'node:crypto';
import assert from 'node:assert/strict';
import { buildNsp } from '@tootallnate/hacbrewpack';
import { initializeKeySet, parseNca } from '@tootallnate/nca';
import { decode } from '@tootallnate/romfs';

const root = resolve(import.meta.dirname, '../..');
const input = process.env.NSP_KEYS;
if (!input) throw new Error('NSP_KEYS must contain the caller-provided build keys');
const bin = async p => new Uint8Array(await readFile(resolve(root, p)));
const digest = b => createHash('sha256').update(b).digest('hex');
const blobDigest = async b => digest(Buffer.from(await b.arrayBuffer()));
const main = await bin('dist/native/main');
assert.equal(Buffer.from(main.subarray(0, 4)).toString(), 'NSO0');
const romfs = {};
for (const f of await readdir(resolve(root, 'romfs'))) {
  if (!f.endsWith('.gz')) romfs[f] = new Blob([await bin('romfs/' + f)]);
}
const nacp = await bin('dist/native/control.nacp');
const keys = await initializeKeySet(input, globalThis.crypto, 1);
const result = await buildNsp({
  keys, plaintext: true, noLogo: false, noPatchNacpLogo: true,
  titleId: '010000abcd000000',
  exefs: new Map([['main', main], ['main.npdm', await bin('dist/native/main.npdm')]]),
  control: new Map([['control.nacp', nacp], ['icon_AmericanEnglish.dat', await bin('icon.jpg')]]),
  logo: new Map([['NintendoLogo.png', await bin('NintendoLogo.png')]]),
  romfs,
});

function pfsFiles(bytes) {
  assert.equal(bytes.toString('ascii', 0, 4), 'PFS0');
  const count = bytes.readUInt32LE(4), strings = bytes.readUInt32LE(8);
  const table = 16 + 24 * count, dataStart = table + strings;
  assert.ok(dataStart <= bytes.length);
  const files = new Map();
  for (let i = 0; i < count; i++) {
    const pos = 16 + 24 * i;
    const off = Number(bytes.readBigUInt64LE(pos)), len = Number(bytes.readBigUInt64LE(pos + 8));
    const nameStart = table + bytes.readUInt32LE(pos + 16);
    const nameEnd = bytes.indexOf(0, nameStart);
    assert.ok(nameStart >= table && nameEnd < dataStart && nameEnd >= nameStart);
    assert.ok(dataStart + off + len <= bytes.length);
    const name = bytes.toString('ascii', nameStart, nameEnd);
    assert.ok(!files.has(name));
    files.set(name, bytes.subarray(dataStart + off, dataStart + off + len));
  }
  return files;
}
const bytes = Buffer.from(await result.nsp.arrayBuffer());
const files = pfsFiles(bytes);
assert.equal(files.size, 3, 'Expected Program, Control and Meta NCAs');
const records = new Map();
let cnmt;
for (const [name, nca] of files) {
  const hash = digest(nca);
  assert.ok(name.startsWith(hash.slice(0, 32)), 'NCA content hash mismatch');
  const parsed = await parseNca(new Blob([nca]), { keys });
  assert.equal(parsed.titleId, 0x010000abcd000000n);
  assert.equal(parsed.ncaSize, BigInt(nca.length));
  assert.equal(parsed.hasRightsId, false, 'Homebrew must not require title keys/tickets');
  records.set(hash.slice(0, 32), { hash, size: nca.length, type: parsed.contentType });
  if (parsed.contentType === 0) {
    const exe = pfsFiles(Buffer.from(await parsed.sections.find(s => s.pfs0Data).pfs0Data.arrayBuffer()));
    assert.equal(digest(exe.get('main')), digest(main), 'NSO must match this native build');
    const npdm = exe.get('main.npdm');
    assert.equal(npdm.toString('ascii', 0, 4), 'META');
    assert.equal(npdm.readBigUInt64LE(npdm.readUInt32LE(0x70) + 0x10), 0x010000abcd000000n);
    const assets = await decode(parsed.sections.find(s => s.romfsData).romfsData);
    for (const name of Object.keys(romfs)) {
      assert.ok(assets[name] instanceof Blob, 'Missing embedded ' + name);
      assert.equal(await blobDigest(assets[name]), await blobDigest(romfs[name]), 'Stale asset ' + name);
    }
  } else if (parsed.contentType === 2) {
    const control = await decode(parsed.sections.find(s => s.romfsData).romfsData);
    assert.equal(await blobDigest(control['control.nacp']), digest(nacp));
    assert.equal(await blobDigest(control['icon_AmericanEnglish.dat']), digest(await bin('icon.jpg')));
    console.log('Control display version:', Buffer.from(nacp).toString('utf8', 0x3060, 0x3070).replace(/\0.*$/s, ''));
  } else if (parsed.contentType === 1) {
    const meta = pfsFiles(Buffer.from(await parsed.sections.find(s => s.pfs0Data).pfs0Data.arrayBuffer()));
    cnmt = [...meta.values()][0];
  }
}
assert.equal(cnmt.readBigUInt64LE(0), 0x010000abcd000000n);
assert.equal(cnmt.readUInt16LE(0x10), 2);
const start = 0x20 + cnmt.readUInt16LE(0x0e);
for (let i = 0; i < 2; i++) {
  const pos = start + 0x38 * i;
  const record = records.get(cnmt.subarray(pos + 0x20, pos + 0x30).toString('hex'));
  assert.ok(record, 'CNMT references absent NCA');
  assert.equal(record.hash, cnmt.subarray(pos, pos + 0x20).toString('hex'));
  assert.equal(record.size, cnmt.readUIntLE(pos + 0x30, 6));
  assert.equal(cnmt[pos + 0x36], record.type === 0 ? 1 : 3);
}
await writeFile(resolve(root, 'dist/GuandanTime.nsp'), bytes);
console.log('PASS: NSP container, all NCA hashes, title ID, NSO, NPDM, NACP, icon, all RomFS assets and CNMT references');
console.log('NSP SHA256:', digest(bytes));
