#!/usr/bin/env python3
"""Original 32-second swing card-room score, no copyrighted melody or samples.
Rebuild requires NumPy. Accordion/clarinet lead, plucked chords, walking bass,
and synthesized shuffle drums. 120 BPM, 16 bars, stereo 24 kHz PCM (resampled to 48 kHz at runtime).
"""
from pathlib import Path
import numpy as np
import wave
R=24000;BEAT=.5;N=int(32*R)
out=np.zeros((N,2),dtype=np.float64)
rng=np.random.default_rng(502)
def note(midi,start,duration,gain=1,timbre='lead',pan=0):
 first=int(start*R);n=min(int(duration*R),N-first)
 if n<=0:return
 t=np.arange(n)/R;f=440*2**((midi-69)/12)
 if timbre=='lead':
  phase=2*np.pi*f*t+.018*np.sin(2*np.pi*5*t)
  a=np.sin(phase)+.32*np.sin(phase*2)+.22*np.sin(phase*3)+.09*np.sin(phase*5)
  env=np.minimum(t/.012,1)*np.minimum((duration-t)/.06,1)*np.exp(-t*2.5)
 elif timbre=='pluck':
  a=np.sin(2*np.pi*f*t)+.5*np.sin(2*np.pi*f*2*t)+.14*np.sin(2*np.pi*f*4*t)
  env=np.minimum(t/.003,1)*np.exp(-t*11)
 else:
  a=np.sin(2*np.pi*f*t)+.2*np.sin(2*np.pi*f*2*t);env=np.minimum(t/.008,1)*np.exp(-t*7)
 a=a*np.clip(env,0,1)*gain
 out[first:first+n,0]+=a*(1-pan*.3);out[first:first+n,1]+=a*(1+pan*.3)
def drum(start,kind):
 first=int(start*R);n=min(int(.23*R),N-first);t=np.arange(n)/R
 noise=rng.normal(0,1,n)
 if kind=='kick':a=np.sin(2*np.pi*(60*t+8*(1-np.exp(-t*30))))*np.exp(-t*23)*.19
 elif kind=='snare':a=(noise*.55+np.sin(2*np.pi*185*t)*.25)*np.exp(-t*35)*.10
 else:a=(noise-np.roll(noise,1))*.5*np.exp(-t*90)*.026
 out[first:first+n]+=a[:,None]
# A new melody; no transcription, downloaded samples, or reference-game audio.
phrases=[[0,2,4,7,9,7,4,2],[4,7,9,12,9,7,4,-1],[2,4,7,4,2,0,2,4],[7,9,7,4,2,-1,0,-1],
 [9,7,4,2,4,7,9,12],[11,9,7,4,7,4,2,-1],[4,2,0,2,4,7,4,2],[2,4,2,0,2,-1,0,-1]]
chords=[(60,[60,64,67]),(57,[57,60,64]),(53,[53,57,60]),(55,[55,59,62])]
for bar in range(16):
 root,chord=chords[bar%4]
 phrase=phrases[bar%8]
 for k,pitch in enumerate(phrase):
  start=bar*2+(k//2)*BEAT+(BEAT*2/3 if k%2 else 0)
  if pitch>=0:note(72+pitch,start,.23 if k%2 else .31,.14,'lead',-.25)
 for beat in range(4):
  start=bar*2+beat*BEAT
  note(root-24+(7 if beat%2 else 0),start,.36,.20,'bass',-.12)
  drum(start,'kick' if beat%2==0 else 'snare')
  for h in [0,BEAT*2/3]:drum(start+h,'hat')
  for j,m in enumerate(chord):note(m+12,start+.28+j*.008,.20,.037,'pluck',.65)
 if bar%2:note(84+phrase[0],bar*2+1.7,.25,.04,'pluck',.8)
peak=np.max(np.abs(out));out*=.78/max(peak,.001)
fade=np.minimum(np.arange(N)/480,1)*np.minimum((N-1-np.arange(N))/480,1);out*=fade[:,None]
pcm=(out*32767).astype('<i2')
p=Path(__file__).resolve().parents[2]/'romfs/music.wav'
with wave.open(str(p),'wb') as f:f.setnchannels(2);f.setsampwidth(2);f.setframerate(R);f.writeframes(pcm.tobytes())
print('Generated original 32s swing loop; RMS',round(float(np.sqrt(np.mean(out*out))),4))
