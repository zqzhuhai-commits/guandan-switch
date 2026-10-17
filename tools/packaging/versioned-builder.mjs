// hacbrewpack 0.0.3 omits titleVersion when calling its CNMT builder.
// Patch that single call reproducibly, retaining the installed MIT implementation.
import { readFile, writeFile } from 'node:fs/promises';
const entry = import.meta.resolve('@tootallnate/hacbrewpack');
const text = await readFile(new URL(entry), 'utf8');
const needle = 'buildCnmt({\n        titleId,\n        contentRecords,';
if (text.split(needle).length !== 2) throw new Error('Unexpected pinned hacbrewpack source; refusing unverified patch');
const patched = text.replace(needle, 'buildCnmt({\n        titleId,\n        titleVersion: options.titleVersion ?? 0,\n        contentRecords,');
const url = new URL('./guandan-versioned-index.js', entry);
await writeFile(url, patched);
export const { buildNsp } = await import(url.href);
