#!/usr/bin/env python3
"""Exercise startup and disk recovery through the actual executable."""
from pathlib import Path
from tempfile import TemporaryDirectory
import os
import subprocess
ROOT=Path(__file__).resolve().parents[1]
with TemporaryDirectory(prefix='guandan-runtime-') as temp:
    root=Path(temp);data=root/'new sd root/switch/GuandanSwitch'
    common=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
    def run(data,mode,env):
        output=root/'capture.bmp'
        p=subprocess.run([str(ROOT/'guandan-desktop'),'--data-dir',str(data),mode,str(output)],cwd=ROOT,env=env,text=True,capture_output=True,timeout=30)
        assert p.returncode==0,p.stderr
        assert output.stat().st_size>100000
        return p.stderr
    badAudio=dict(common,SDL_AUDIODRIVER='guandan-intentionally-missing-driver')
    log=run(data,'--capture-game',badAudio)
    assert 'audio init failed (game continues)' in log and 'main loop ready' in log
    assert (data/'session.dat').read_text().startswith('GD5 ')
    for _ in range(2):run(data,'--capture-game',common)
    assert (data/'session.dat.bak').exists() and (data/'profile.txt.bak').exists()
    (data/'session.dat').write_text('interrupted invalid data')
    log=run(data,'--capture',common)
    assert 'Recovered session from .bak' in log
    blocked=root/'not-a-directory';blocked.write_text('keep this file')
    log=run(blocked,'--capture-game',badAudio)
    assert 'create data directory' in log and 'main loop ready' in log
    assert blocked.read_text()=='keep this file'
print('PASS: first-run nested data path, repeat saves, recovered backup, missing audio driver and blocked storage all render without exiting early')
