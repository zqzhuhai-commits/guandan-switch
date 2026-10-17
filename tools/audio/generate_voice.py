#!/usr/bin/env python3
"""Build an offline voice bank with MIT-licensed MeloTTS via sherpa-onnx.
Model: csukuangfj/vits-melo-tts-zh_en @ a0d5c6a264c0ef92d70d8661d8cc502d79627cd6
Install sherpa-onnx numpy; pass the downloaded model directory as argv[1].
The model and synthesis engine are build tools only, never shipped in the app.
"""
import json,sys,wave
from pathlib import Path
import numpy as np
import sherpa_onnx
ROOT=Path(__file__).resolve().parents[2]
m=Path(sys.argv[1])
vits=sherpa_onnx.OfflineTtsVitsModelConfig(model=str(m/'model.onnx'),lexicon=str(m/'lexicon.txt'),tokens=str(m/'tokens.txt'),dict_dir=str(m/'dict'))
config=sherpa_onnx.OfflineTtsConfig(model=sherpa_onnx.OfflineTtsModelConfig(vits=vits,num_threads=2),rule_fsts=','.join(str(m/n) for n in ['date.fst','number.fst','phone.fst','new_heteronym.fst']))
tts=sherpa_onnx.OfflineTts(config)
lines=json.loads((ROOT/'tools/audio/voice_lines.json').read_text())
bank=[];ranges=[];offset=0;rate=22050
for key,text in lines:
 audio=tts.generate(text,sid=0,speed=1.12)
 data=np.asarray(audio.samples,dtype=np.float32)
 # Trim leading/trailing silence and normalize short cues consistently.
 nz=np.flatnonzero(np.abs(data)>.012)
 if not len(nz):raise ValueError('Silent cue: '+key)
 pad=int(audio.sample_rate*.025);data=data[max(0,nz[0]-pad):min(len(data),nz[-1]+pad)]
 # Runtime bank is compact 22.05k mono; SDL resamples once at startup.
 n=round(len(data)*rate/audio.sample_rate)
 data=np.interp(np.arange(n)*audio.sample_rate/rate,np.arange(len(data)),data)
 peak=max(np.max(np.abs(data)),.01);data=np.clip(data*.70/peak,-1,1)
 pcm=(data*32767).astype('<i2');bank.append(pcm);ranges.append((offset,offset+len(pcm)));offset+=len(pcm)
 print(key,text,round(len(pcm)/rate,2),flush=True)
with wave.open(str(ROOT/'romfs/voice.wav'),'wb') as f:
 f.setnchannels(1);f.setsampwidth(2);f.setframerate(rate);f.writeframes(np.concatenate(bank).tobytes())
(ROOT/'romfs/voice.idx').write_text(f'{rate} {len(lines)}\n'+''.join(f'{a} {b}\n' for a,b in ranges))
