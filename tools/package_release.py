#!/usr/bin/env python3
"""Bundle the newly compiled NRO and source; never copy the old root NSP."""
from pathlib import Path
import hashlib
import shutil
import subprocess
import zipfile
ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT/'dist'
PACKAGE = DIST/'GuandanSwitch-v0.4.1'
PACKAGE.mkdir(exist_ok=True)
shutil.copy2(DIST/'GuandanSwitch.nro', PACKAGE/'GuandanSwitch.nro')
notes = (ROOT/'release/notes-v0.4.md').read_text()
if (DIST/'GuandanTime.nsp').exists():
    shutil.copy2(DIST/'GuandanTime.nsp', PACKAGE/'GuandanTime.nsp')
    notes += '\n本次已生成原生单文件 NSP，游戏和资源全部内嵌。\n'
else:
    notes += '\n本次附件为新版独立 NRO（全部资源内嵌）和完整源码包。原生单文件 NSP 因缺少 NSP_KEYS 构建输入未生成；仓库根目录旧 NSP 不属于本版。\n'
(DIST/'RELEASE-NOTES.md').write_text(notes)
(PACKAGE/'发行说明.md').write_text(notes)
files = subprocess.check_output(['git', 'ls-files'], cwd=ROOT, text=True).splitlines()
for name in files:
    if name.startswith(('release/GuandanSwitch-', 'release/manifest.json')) or name == 'GuandanTime.nsp' or (name.startswith('release/v0.4.1/') and '.part' in name):
        continue
    src = ROOT/name
    if src.is_file():
        out = PACKAGE/'source-project'/name
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, out)
font = PACKAGE/'source-project/romfs/font.otf'
font.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(ROOT/'romfs/font.otf', font)
if (DIST/'BUILD-SOURCE-COMMIT.txt').exists():
    shutil.copy2(DIST/'BUILD-SOURCE-COMMIT.txt', PACKAGE/'BUILD-SOURCE-COMMIT.txt')
subprocess.run(['git','rev-parse','HEAD'], cwd=ROOT, check=True, stdout=(PACKAGE/'SOURCE-COMMIT.txt').open('w'))
archive = DIST/'GuandanSwitch-v0.4.1.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for p in sorted(PACKAGE.rglob('*')):
        if p.is_file(): z.write(p, p.relative_to(DIST))
with zipfile.ZipFile(archive) as z: assert z.testzip() is None
assets = [archive, DIST/'GuandanSwitch.nro']
if (DIST/'GuandanTime.nsp').exists(): assets.append(DIST/'GuandanTime.nsp')
(DIST/'SHA256SUMS.txt').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in assets))
print((DIST/'SHA256SUMS.txt').read_text())
