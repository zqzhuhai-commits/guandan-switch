#!/usr/bin/env python3
"""Capture real SDL views headlessly without touching the player's save files."""
from pathlib import Path
from tempfile import TemporaryDirectory
import os
import subprocess
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
SCREENS = {'home':'--capture', 'game':'--capture-game', 'training':'--capture-training',
           'setup':'--capture-setup', 'history':'--capture-history', 'settings':'--capture-settings',
           'help':'--capture-help', 'emote':'--capture-emote',
           'over':'--capture-over', 'review':'--capture-review',
           'pause':'--capture-pause', 'stats':'--capture-stats',
           'restored':'--capture-restored', 'selected':'--capture-selected',
           'counter':'--capture-counter', 'flat':'--capture-flat', 'training-zoom':'--capture-training-zoom', 'diagnostics':'--capture-diagnostics', 'organized':'--capture-organized',
           'theme-night':'--capture-theme-night', 'theme-ocean':'--capture-theme-ocean', 'theme-purple':'--capture-theme-purple'}
with TemporaryDirectory(prefix='guandan-preview-') as tmp:
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    for name, flag in SCREENS.items():
        raw = Path(tmp) / (name + '.bmp')
        subprocess.run([str(ROOT/'guandan-desktop'), '--data-dir', tmp, flag, str(raw)],
                       cwd=ROOT, env=env, check=True, timeout=30)
        with Image.open(raw) as im:
            assert im.size == (1280, 720)
            output = ROOT/'preview'/(name+'.png')
            pending = output.with_suffix('.png.tmp')
            im.save(pending, format='PNG')
        with Image.open(pending) as check:
            check.verify()
        pending.replace(output)
        print('Captured:', name)

# Sample actual runtime animation frames (not a mocked storyboard).
with TemporaryDirectory(prefix='guandan-motion-') as tmp:
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    subprocess.run([str(ROOT/'guandan-desktop'), '--data-dir', tmp, '--capture-motion', tmp], cwd=ROOT, env=env, check=True, timeout=45)
    frames=[]
    for p in sorted(Path(tmp).glob('frame-*.bmp'),key=lambda p:int(p.stem.split('-')[-1])):
        with Image.open(p) as im:frames.append(im.resize((768,432)).convert('P',palette=Image.Palette.ADAPTIVE))
    assert len(frames)==38 and frames[0].tobytes()!=frames[12].tobytes()
    frames[0].save(ROOT/'preview/motion.gif',save_all=True,append_images=frames[1:],duration=60,loop=0,optimize=False)
    print('Captured: live deal and legal play animation (38 frames)')
