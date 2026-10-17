#include "../source/audio_engine.hpp"
#include <cassert>
#include <algorithm>
#include <iostream>
static double energy(const std::vector<int16_t>&pcm){double s=0;for(auto v:pcm)s+=double(v)*v;return s/pcm.size();}
int main(){
 assert(SDL_Init(SDL_INIT_TIMER)==0);
 AudioEngine offline;assert(offline.loadMusic("romfs/music.wav"));std::vector<int16_t> pcm(48000*2);
 offline.configure(true,100,0);offline.render(pcm.data(),48000);assert(energy(pcm)>100000); // Real PCM must contain audible music.
 offline.configure(false,100,100);offline.render(pcm.data(),48000);assert(energy(pcm)==0);
 offline.configure(true,0,100);offline.effect(4);offline.render(pcm.data(),48000);assert(energy(pcm)>100000);
 offline.configure(true,0,0);offline.render(pcm.data(),48000);assert(energy(pcm)==0);
 assert(offline.loadSpeech("romfs/voice.wav","romfs/voice.idx"));assert(offline.hasSpeech());
 offline.configure(true,0,0,100);for(int cue=0;cue<speech::COUNT;cue++){assert(offline.say(cue,4));offline.render(pcm.data(),48000);assert(energy(pcm)>10000);}offline.configure(true,0,0,0);offline.configure(true,0,0,100);assert(offline.say(speech::BOMB,3));assert(!offline.say(speech::WAIT,0));offline.render(pcm.data(),48000);assert(energy(pcm)>100000);
 offline.configure(true,0,0,0);offline.render(pcm.data(),48000);assert(energy(pcm)==0&&!offline.speaking());assert(!offline.say(speech::WAIT));
 offline.configure(true,0,0,100);assert(offline.say(speech::WAIT));offline.configure(false,100,100,100);assert(!offline.speaking());
 AudioEngine live;assert(live.init("romfs/music.wav"));live.configure(true,40,80);live.effect(0);SDL_Delay(120);assert(live.frameCount()>0);assert(live.hasMusic());live.close();
 SDL_QuitSubSystem(SDL_INIT_AUDIO);SDL_setenv("SDL_AUDIODRIVER","guandan-intentionally-missing-driver",1);AudioEngine missing;assert(!missing.init("romfs/music.wav"));assert(!missing.error.empty());assert(SDL_WasInit(SDL_INIT_TIMER));
 SDL_Quit();std::cout<<"PASS: non-silent BGM and effects, independent volume/mute, real SDL callback and audio failure without exiting the game\n";
}
