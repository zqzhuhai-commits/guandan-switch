#pragma once
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
class AudioEngine {
 struct Voice {double frequency=0,phase=0;int age=0,duration=0,delay=0;double gain=0;bool noise=false;uint32_t random=0x51f15e;};
 std::array<Voice,12> voices{};
 std::vector<int16_t> music;
 size_t musicFrame=0;uint64_t renderedFrames=0;
 bool master=true,suspended=false;int musicVolume=40,effectVolume=80;
 static void callback(void*user,Uint8*stream,int bytes){static_cast<AudioEngine*>(user)->render(reinterpret_cast<int16_t*>(stream),bytes/4);}
 void lock(){if(device)SDL_LockAudioDevice(device);}void unlock(){if(device)SDL_UnlockAudioDevice(device);}
 void tone(double frequency,int ms,double gain,int delay=0,bool noise=false){
  auto it=std::find_if(voices.begin(),voices.end(),[](const Voice&v){return v.age>=v.duration;});if(it==voices.end())it=voices.begin();
  *it={frequency,0,0,ms*48,delay*48,gain,noise,0x51f15e};
 }
public:
 static constexpr int Rate=48000;
 SDL_AudioDeviceID device=0;std::string error,musicError,driver;
 bool loadMusic(const std::string&path){
  SDL_AudioSpec spec{};Uint8*raw=nullptr;Uint32 length=0;
  if(!SDL_LoadWAV(path.c_str(),&spec,&raw,&length)){musicError=SDL_GetError();return false;}
  SDL_AudioCVT cv{};int needed=SDL_BuildAudioCVT(&cv,spec.format,spec.channels,spec.freq,AUDIO_S16SYS,2,Rate);
  if(needed<0){musicError=SDL_GetError();SDL_FreeWAV(raw);return false;}
  if(needed){std::vector<Uint8> converted(size_t(length)*cv.len_mult);std::memcpy(converted.data(),raw,length);cv.buf=converted.data();cv.len=int(length);
   if(SDL_ConvertAudio(&cv)<0){musicError=SDL_GetError();SDL_FreeWAV(raw);return false;}
   music.resize(size_t(cv.len_cvt)/2);std::memcpy(music.data(),converted.data(),music.size()*2);
  }else{music.resize(length/2);std::memcpy(music.data(),raw,music.size()*2);}
  SDL_FreeWAV(raw);music.resize(music.size()/2*2);musicFrame=0;
  if(music.empty()){musicError="Empty music track";return false;}musicError.clear();return true;
 }
 bool init(const std::string&path){
  if(device)return true;
  if(SDL_InitSubSystem(SDL_INIT_AUDIO)<0){error=SDL_GetError();return false;}
  loadMusic(path);
  SDL_AudioSpec want{};want.freq=Rate;want.format=AUDIO_S16SYS;want.channels=2;want.samples=1024;want.callback=callback;want.userdata=this;
  // SDL converts the requested stereo PCM to the hardware format if necessary.
  device=SDL_OpenAudioDevice(nullptr,0,&want,nullptr,0);
  if(!device){error=SDL_GetError();return false;}
  const char*name=SDL_GetCurrentAudioDriver();driver=name?name:"unknown";error.clear();SDL_PauseAudioDevice(device,0);return true;
 }
 void close(){if(device){SDL_CloseAudioDevice(device);device=0;}}
 void configure(bool enabled,int bgm,int sfx){lock();master=enabled;musicVolume=std::clamp(bgm,0,100);effectVolume=std::clamp(sfx,0,100);unlock();}
 void suspend(bool value){lock();suspended=value;unlock();}
 bool hasMusic()const{return !music.empty();}
 uint64_t frameCount(){lock();auto n=renderedFrames;unlock();return n;}
 void effect(int kind){
  lock();if(!master||suspended){unlock();return;}
  if(kind==0){tone(880,45,1700);tone(1175,35,950,35);}
  else if(kind==1){tone(540,75,2100);tone(400,60,1300,55);}
  else if(kind==2){tone(330,75,1400);tone(247,85,1000,65);}
  else if(kind==3){tone(95,270,5000,0,true);tone(140,190,2800,180,true);}
  else if(kind==4){tone(523,140,2800);tone(659,140,2800,120);tone(784,160,2800,240);tone(1047,260,2800,380);}
  else if(kind==5){tone(330,120,2400);tone(247,120,2100,150);}
  else if(kind==6){tone(440,55,1300);tone(660,60,1600,60);tone(880,85,1800,125);}
  unlock();
 }
 // One real mixer is used by SDL and by the offline PCM verification.
 void render(int16_t*out,int frames){
  std::memset(out,0,size_t(frames)*4);renderedFrames+=frames;if(!master||suspended)return;
  for(int i=0;i<frames;i++){
   double l=0,r=0;if(!music.empty()&&musicVolume){l=music[musicFrame*2]*musicVolume/100.0;r=music[musicFrame*2+1]*musicVolume/100.0;musicFrame=(musicFrame+1)%(music.size()/2);}
   double effect=0;for(auto&v:voices){if(v.age>=v.duration)continue;if(v.delay>0){v.delay--;continue;}
    double t=v.age/double(Rate),envelope=std::min(1.0,t/.004)*(1-v.age/double(v.duration));double wave=std::sin(v.phase*6.283185307179586);
    if(v.noise){v.random=v.random*1664525u+1013904223u;wave=wave*.35+(double(v.random>>8)/16777216.0*2-1)*.65;}
    effect+=wave*v.gain*envelope;v.phase+=v.frequency/Rate;v.phase-=std::floor(v.phase);v.age++;
   }
   effect*=effectVolume/100.0;out[i*2]=int16_t(std::clamp(l+effect,-32768.0,32767.0));out[i*2+1]=int16_t(std::clamp(r+effect,-32768.0,32767.0));
  }
 }
};
