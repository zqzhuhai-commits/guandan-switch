#!/usr/bin/env python3
"""Original looping pentatonic score. No samples or third-party recordings."""
from pathlib import Path
import math
import wave
import array
RATE=48000
STEP=.375
MELODY=[4,2,0,2,4,5,4,2,0,2,4,2,0,-1,0,-1,
        5,4,2,4,5,7,5,4,2,4,5,4,2,-1,0,-1,
        4,2,0,2,4,5,4,2,0,2,4,2,0,2,0,-1,
        2,4,5,4,2,0,-1,2,4,2,0,-1,0,-1,-1,-1]
SCALE=[261.626,293.665,329.628,391.995,440.0,523.251,587.33,659.255,783.991]
frames=int(len(MELODY)*STEP*RATE)
left=[0.0]*frames;right=[0.0]*frames
for k,note in enumerate(MELODY):
 if note<0:continue
 start=int(k*STEP*RATE);freq=SCALE[note];length=min(int(.7*RATE),frames-start)
 for i in range(length):
  t=i/RATE;env=min(1,t/.012)*math.exp(-t*5.5)*min(1,(length-i)/960)
  v=2200*env*(math.sin(math.tau*freq*t)+.28*math.sin(math.tau*freq*2*t)+.12*math.sin(math.tau*freq*3*t))
  left[start+i]+=v*.92;right[start+i]+=v*.83
for k in range(0,len(MELODY),4):
 start=int(k*STEP*RATE);freq=SCALE[[0,4,0,2][(k//4)%4]]/2;length=min(int(STEP*4*RATE),frames-start)
 for i in range(length):
  t=i/RATE;env=math.sin(math.pi*i/length)**2
  v=1100*env*(math.sin(math.tau*freq*t)+.3*math.sin(math.tau*freq*1.5*t))
  left[start+i]+=v;right[start+i]+=v*.93
pcm=array.array('h')
for a,b in zip(left,right):pcm.extend((int(max(-32768,min(32767,a))),int(max(-32768,min(32767,b)))))
import sys
if sys.byteorder!='little':pcm.byteswap()
p=Path(__file__).resolve().parents[2]/'romfs/music.wav'
with wave.open(str(p),'wb') as out:out.setnchannels(2);out.setsampwidth(2);out.setframerate(RATE);out.writeframes(pcm.tobytes())
print('Generated original 24-second stereo music loop:',p.name)
