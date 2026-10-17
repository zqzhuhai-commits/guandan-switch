// 掼蛋时光 · Guandan Time — Switch native homebrew (also builds on desktop)
// UI re-styled after the feel of Tencent's 大掼蛋 (clean-room: no Tencent assets).
// Rules engine lives in engine.hpp. This file is the SDL2 presentation layer.
#include "engine.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <sys/stat.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#ifdef __SWITCH__
#include <switch.h>
#endif
using namespace gd;

static constexpr int W=1280,H=720;
static SDL_Renderer*ren=nullptr;static SDL_Window*window=nullptr;
static SDL_Texture*avTex[4]={nullptr,nullptr,nullptr,nullptr};static SDL_Texture*bgTex[3]={nullptr,nullptr,nullptr};
static std::map<int,TTF_Font*>fonts;
struct TextTex{SDL_Texture*t;int w,h;};static std::map<std::string,TextTex>cache;
struct Col{Uint8 r,g,b,a=255;};
const Col INK{38,40,34},CREAM{255,248,228},GOLD{246,198,74},GOLD_D{198,150,40},
  MUTED{158,190,176},WHITE{255,255,250},RED{212,64,64},DARK{8,12,16},
  PANEL{12,16,20,240},PANEL2{8,12,16,244},GOLD_BORDER{246,198,74,120};
struct Skin{Col top,bot,card,btnHover;};
static Skin skins[3]={ {{12,82,52},{3,32,20},{240,236,222},{26,30,36,235}},   // 翡翠绿
                      {{16,48,92},{6,18,46},{238,240,246},{34,76,124}}, // 深海蓝
                      {{58,34,88},{24,14,42},{246,240,250},{84,54,118}}}; // 午夜紫
static std::string base="./",fontPath;static int skin=0,speed=1;static bool sound=true,quit=false,autoplay=false,recorder=true,timerOn=true,organized=false;
static SDL_AudioDeviceID audioDevice=0;static int musicVolume=50,sfxVolume=80;static Game game;static int screen=0,modal=0,focus=0,cursor=0;static bool selected[108]{};
// 背景音乐引擎 - 程序化中国风五声音阶旋律
static int musicStep=0;static Uint32 musicNextTick=0;static bool musicPlaying=false;
static const int MUSIC_BPM=72;static const int MUSIC_STEP_MS=60000/MUSIC_BPM/2; // 八分音符
// 五声音阶频率 (C D E G A C5 D5 E5 G5)
static const double PENTA[]={261.63,293.66,329.63,392.00,440.00,523.25,587.33,659.25,783.99};
// 旋律: 0-8=音阶索引, -1=休止, 高8位标记低音区
static const int8_t MELODY[]={
  4,2,0,2, 4,5,4,2, 0,2,4,2, 0,-1,0,-1,
  5,4,2,4, 5,7,5,4, 2,4,5,4, 2,-1,0,-1,
  4,2,0,2, 4,5,4,2, 0,2,4,2, 0,2,0,-1,
  2,4,5,4, 2,0,-1,2, 4,2,0,-1, 0,-1,-1,-1,
};
static const int MELODY_LEN=sizeof(MELODY)/sizeof(MELODY[0]);
static const int8_t BASS[]={0,-1,0,-1, 4,-1,4,-1, 0,-1,0,-1, 2,-1,2,-1};
static const int BASS_LEN=sizeof(BASS)/sizeof(BASS[0]);
static Uint32 lastAI=0,toastUntil=0;static std::string toast;static int total=0,wins=0,bombs=0,statRound=-1;static bool hasSave=false;
static constexpr Uint32 TURN_MS=18000;
static Uint32 pausedAt=0;static int pendingMode=0,historyPage=0,hintIndex=-1;
static std::vector<Move> hintOptions;static size_t hintAction=0;static uint32_t hintSeed=0;static int hintRound=0;
struct Area { int x,y,w,h;bool contains(int px,int py)const{return px>=x&&px<x+w&&py>=y&&py<y+h;} };
static const Area modeAreas[4]={{460,206,350,210},{830,206,350,210},{460,436,350,118},{830,436,350,118}};
static const Area navAreas[4]={{60,600,270,70},{346,600,270,70},{632,600,270,70},{918,600,270,70}};
void setModal(int next){Uint32 now=SDL_GetTicks();if(!modal&&next)pausedAt=now;if(modal&&!next)lastAI+=now-pausedAt;modal=next;}
std::vector<int> selection(){std::vector<int> out;for(int id:game.hands[0])if(selected[id])out.push_back(id);return out;}
void resetHints(){hintIndex=-1;hintOptions.clear();}

enum FxType{FX_PLAY,FX_BOMB,FX_PASS,FX_EMOTE,FX_WIN};
struct Fx{int type;Uint32 t0;int dur;int sx,sy,dx,dy;std::vector<int> cards;std::string text;};
static std::vector<Fx> fxList;
static bool lightPanel=false;
struct LightPanelScope {bool previous=lightPanel;LightPanelScope(){lightPanel=true;}~LightPanelScope(){lightPanel=previous;}};

static std::array<Move,4> lastShown{};static std::array<bool,4> lastPassed{};static bool lastOver=false;

void color(Col c){SDL_SetRenderDrawColor(ren,c.r,c.g,c.b,c.a);}
void rect(int x,int y,int w,int h,Col c){color(c);SDL_Rect r{x,y,w,h};SDL_RenderFillRect(ren,&r);}
void line(int x,int y,int xx,int yy,Col c){color(c);SDL_RenderDrawLine(ren,x,y,xx,yy);}
void hline(int x,int y,int w,Col c){color(c);SDL_RenderDrawLine(ren,x,y,x+w,y);}
void vline(int x,int y,int h,Col c){color(c);SDL_RenderDrawLine(ren,x,y,x,y+h);}
void ring(int cx,int cy,int r,int th,Col c){color(c);for(int i=0;i<360;i+=3){float a=i*0.0174533f;int x1=cx+int(r*std::cos(a)),y1=cy+int(r*std::sin(a));int x2=cx+int((r-th)*std::cos(a)),y2=cy+int((r-th)*std::sin(a));SDL_RenderDrawLine(ren,x1,y1,x2,y2);}}
void arc(int cx,int cy,int r,int th,float a0,float a1,Col c){color(c);for(float a=a0;a<a1;a+=0.03f){int x1=cx+int(r*std::cos(a)),y1=cy+int(r*std::sin(a));int x2=cx+int((r-th)*std::cos(a)),y2=cy+int((r-th)*std::sin(a));SDL_RenderDrawLine(ren,x1,y1,x2,y2);}}
void circle(int x,int y,int r,Col c){color(c);for(int i=-r;i<=r;i++){int dx=int(std::sqrt(double(r*r-i*i)));SDL_RenderDrawLine(ren,x-dx,y+i,x+dx,y+i);}}
void vgrad(int x,int y,int w,int h,Col a,Col b){for(int i=0;i<h;i++){double t=h>1?i/double(h-1):0;rect(x,y+i,w,1,{Uint8(a.r*(1-t)+b.r*t),Uint8(a.g*(1-t)+b.g*t),Uint8(a.b*(1-t)+b.b*t)});}}
void box(int x,int y,int w,int h,int r,Col c){r=std::min(r,std::min(w,h)/2);color(c);for(int row=0;row<h;row++){int inset=0;if(row<r){int dy=r-row;inset=r-int(std::sqrt(double(r*r-dy*dy)));}else if(row>=h-r){int dy=row-(h-r-1);inset=r-int(std::sqrt(double(r*r-dy*dy)));}SDL_RenderDrawLine(ren,x+inset,y+row,x+w-inset-1,y+row);}}
void roundedGradient(int x,int y,int w,int h,int r,Col a,Col b){
 for(int row=0;row<h;row++){int dy=row<r?r-row:row>=h-r?row-(h-r-1):0;int inset=dy?r-int(std::sqrt(double(r*r-dy*dy))):0;float t=row/float(std::max(1,h-1));rect(x+inset,y+row,w-inset*2,1,{Uint8(a.r*(1-t)+b.r*t),Uint8(a.g*(1-t)+b.g*t),Uint8(a.b*(1-t)+b.b*t),Uint8(a.a*(1-t)+b.a*t)});}
}
void boxOutline(int x,int y,int w,int h,int r,Col c){box(x,y,w,h,r,c);}

void text(const std::string&s,int x,int y,int size,Col c=CREAM,bool centered=false){if(s.empty())return;
 if(lightPanel){auto same=[&](Col x){return c.r==x.r&&c.g==x.g&&c.b==x.b;};Uint8 alpha=c.a;
  if(same(CREAM)||same(WHITE))c={44,57,91};else if(same(MUTED))c={101,116,147};else if(same(GOLD))c={159,108,25};else if(same(Col{207,221,204}))c={88,105,139};c.a=alpha;
 }
 std::string key=std::to_string(size)+":"+std::to_string(c.r)+","+std::to_string(c.g)+","+std::to_string(c.b)+":"+s;
 auto it=cache.find(key);if(it==cache.end()){if(!fonts.count(size))fonts[size]=TTF_OpenFont(fontPath.c_str(),size);if(!fonts[size])return;SDL_Color sc{c.r,c.g,c.b,255};SDL_Surface*sf=TTF_RenderUTF8_Blended(fonts[size],s.c_str(),sc);if(!sf)return;TextTex t{SDL_CreateTextureFromSurface(ren,sf),sf->w,sf->h};SDL_FreeSurface(sf);it=cache.emplace(key,t).first;}
 auto&t=it->second;SDL_SetTextureAlphaMod(t.t,c.a);SDL_Rect d{x-(centered?t.w/2:0),y,t.w,t.h};SDL_RenderCopy(ren,t.t,nullptr,&d);}
void trimCache(){if(cache.size()>650){for(auto&kv:cache)SDL_DestroyTexture(kv.second.t);cache.clear();}}
std::string rankName(int r){return r==14?"A":r==13?"K":r==12?"Q":r==11?"J":r==17?"大王":r==16?"小王":std::to_string(r);}
std::string playerName(int p){return p==0?"你":p==1?"南风":p==2?"小满":"青竹";}
void message(const std::string&s){toast=s;toastUntil=SDL_GetTicks()+2800;}
void queueTone(int freq,int ms,int vol,bool noise=false){if(!audioDevice||!sound||sfxVolume<=0)return;double gain=sfxVolume/100.0;int n=44100/1000*ms;std::vector<int16_t>s(n*2);for(int i=0;i<n;i++){double t=i/44100.0;double env=1.0-i/double(n);double v=std::sin(6.2831853*freq*t);if(noise)v+=((std::rand()%2001)/1000.0-1.0)*0.8;int16_t x=int16_t(v*vol*env*gain);s[i*2]=x;s[i*2+1]=x;}SDL_QueueAudio(audioDevice,s.data(),s.size()*2);}
void queueMusicNote(int freq,int ms,int vol){if(!audioDevice||!sound||musicVolume<=0)return;double gain=musicVolume/100.0;int n=44100/1000*ms;std::vector<int16_t>s(n*2);for(int i=0;i<n;i++){double t=i/44100.0;double env=std::min(1.0,i/600.0)*std::min(1.0,(n-i)/800.0);double v=std::sin(6.2831853*freq*t)*0.6+std::sin(6.2831853*freq*2*t)*0.2+std::sin(6.2831853*freq*0.5*t)*0.15;int16_t x=int16_t(v*vol*env*gain);s[i*2]=x;s[i*2+1]=x;}SDL_QueueAudio(audioDevice,s.data(),s.size()*2);}
void sfx(int kind){if(!audioDevice||!sound)return;
 if(kind==0){queueTone(880,40,800);queueTone(1320,30,500);} // 按钮点击
 else if(kind==1){queueTone(520,60,1600);queueTone(380,50,1200);} // 出牌
 else if(kind==2){queueTone(330,70,1000);queueTone(220,60,800);} // 不要/过牌
 else if(kind==3){queueTone(90,300,2400,true);queueTone(70,200,1800,true);queueTone(150,150,1200);} // 炸弹
 else if(kind==4){queueTone(523,120,1400);queueTone(659,120,1400);queueTone(784,120,1400);queueTone(1047,200,1600);} // 胜利
 else if(kind==5){queueTone(200,150,1000,true);queueTone(150,100,800,true);} // 超时
 else if(kind==6){queueTone(440,30,600);queueTone(550,30,600);queueTone(660,30,600);queueTone(880,60,800);} // 发牌
 else if(kind==7){queueTone(196,200,1200);queueTone(147,200,1000);} // 失败
 else if(kind==8){queueTone(660,80,1000);queueTone(880,80,1000);queueTone(1100,120,1200);} // 同花顺/大牌型
}
void musicStart(){musicStep=0;musicNextTick=SDL_GetTicks();musicPlaying=true;}
void musicStop(){musicPlaying=false;}
void musicUpdate(){if(!musicPlaying||!audioDevice||!sound||musicVolume<=0)return;Uint32 now=SDL_GetTicks();if(now<musicNextTick)return;if(SDL_GetQueuedAudioSize(audioDevice)>44100*4)return;int idx=musicStep%MELODY_LEN;int note=MELODY[idx];if(note>=0)queueMusicNote((int)PENTA[note],MUSIC_STEP_MS,900);int bassIdx=musicStep%BASS_LEN;int bnote=BASS[bassIdx];if(bnote>=0)queueMusicNote((int)(PENTA[bnote]*0.5),MUSIC_STEP_MS*2,600);musicStep++;musicNextTick=now+MUSIC_STEP_MS;}
void clearSelection(){std::fill(std::begin(selected),std::end(selected),false);}
void saveStats(){std::ofstream f(base+"profile.txt");f<<total<<' '<<wins<<' '<<bombs<<' '<<skin<<' '<<speed<<' '<<sound<<' '<<recorder<<' '<<game.difficulty<<' '<<timerOn<<' '<<musicVolume<<' '<<sfxVolume;}
void checkpoint(){hasSave=game.save(base+"session.dat");if(!hasSave)message("存档写入失败，请检查 SD 卡空间");}
void recordResult(){if(game.over&&statRound!=game.round){statRound=game.round;total++;if(game.finish[0]%2==0)wins++;saveStats();}}
void start(int mode,uint32_t seed=0){int diff=game.difficulty;game.newMatch(mode,seed?seed:mode==3?20261016u:uint32_t(std::time(nullptr)));game.difficulty=diff;screen=1;modal=0;autoplay=false;organized=false;cursor=0;clearSelection();statRound=-1;resetHints();lastOver=false;lastAI=SDL_GetTicks();fxList.clear();lastShown={};lastPassed={};checkpoint();if(sound&&musicVolume>0)musicStart();sfx(6);}

// ---------------- cards ----------------
void cardFace(int id,int x,int y,int w,int h,bool sel,bool focused,bool up=true){
 if(!up){box(x+2,y+5,w,h,7,{0,0,0,55});Col c=skins[skin].card;box(x,y,w,h,7,{0,0,0});box(x+1,y+1,w-2,h-2,6,{Uint8(c.r*0.72),Uint8(c.g*0.72),Uint8(c.b*0.72)});box(x+4,y+4,w-8,h-8,5,{255,255,255,26});int cx=x+w/2,cy=y+h/2;ring(cx,cy,std::min(w,h)/4,3,GOLD_BORDER);line(x+8,y+h-10,x+w-10,y+8,{255,255,255,22});line(x+8,y+8,x+w-10,y+h-10,{255,255,255,22});return;}
 if(sel){y-=18;}box(x+2,y+5,w,h,7,{0,0,0,60});if(sel||focused)box(x-3,y-3,w+6,h+6,9,focused?GOLD:Col{80,200,160});box(x,y,w,h,8,{0,0,0});box(x+1,y+1,w-2,h-2,7,sel?Col{255,246,214}:skins[skin].card);
 hline(x+8,y+h-4,w-16,{176,172,160,65});
 int r=Card{id}.rank(),s=Card{id}.suit();Col c=(s==1||s==2||r==17)?RED:INK;std::string suits[]={"♠","♥","♦","♣",""};
 int rs=w<46?12:w<62?15:w<82?19:24,ss=w<46?11:w<62?14:w<82?17:22;
 if(r>=16){int fs=w<62?16:w<82?20:26;text(r==17?"大":"小",x+4,y+3,fs,c);text("王",x+4,y+3+(w<62?18:24),fs,c);if(w>=62)text("★",x+w-28,y+h-42,24,c);if(w>=62)text("★",x+w-26,y+6,12,c);}
 else{text(rankName(r),x+4,y+2,rs,c);text(suits[s],x+4,y+2+(rs>15?rs-4:rs),ss,c);if(w>=62){text(rankName(r),x+w-4-int(rs*0.6)-8,y+h-rs-4,rs,c);text(suits[s],x+w-4-int(ss*0.6)-8,y+h-ss-4,ss,c);}else{text(suits[s],x+w-26,y+h-38,ss,c);}}
 if(wild(id,game.level)){box(x+w-21,y+4,17,21,4,GOLD);text("配",x+w-19,y+5,11,INK);}
}
// ---------------- avatars ----------------
void loadAvatars(){
 const std::string root=fontPath.rfind("romfs:",0)==0?"romfs:/":"romfs/";
 int aw=0,ah=0,an=0;unsigned char*atlas=stbi_load((root+"characters.png").c_str(),&aw,&ah,&an,4);
 const char*names[]={"avatar0.png","avatar1.png","avatar2.png","avatar3.png"};
 for(int i=0;i<4;i++){
  int w=0,h=0,n=0;unsigned char*px=nullptr;std::vector<unsigned char> portrait;
  if(atlas&&aw>=4&&ah>0){
   // Read each portrait from the supplied atlas at runtime; preserve the original asset.
   int cell=aw/4;w=h=std::min(cell,ah);portrait.resize(w*h*4);
   for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)for(int c=0;c<4;c++)portrait[(yy*w+xx)*4+c]=atlas[(yy*aw+i*cell+xx)*4+c];
   px=portrait.data();
  }else px=stbi_load((root+names[i]).c_str(),&w,&h,&n,4);
  if(!px){fprintf(stderr,"Avatar %s failed: %s\n",names[i],stbi_failure_reason());continue;}
  for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){float dx=(xx-w/2.0f)/(w*.49f),dy=(yy-h/2.0f)/(h*.49f);if(dx*dx+dy*dy>1)px[(yy*w+xx)*4+3]=0;}
  SDL_Surface*sf=SDL_CreateRGBSurfaceWithFormatFrom(px,w,h,32,w*4,SDL_PIXELFORMAT_RGBA32);
  if(sf){avTex[i]=SDL_CreateTextureFromSurface(ren,sf);SDL_SetTextureBlendMode(avTex[i],SDL_BLENDMODE_BLEND);SDL_FreeSurface(sf);}
  if(portrait.empty())stbi_image_free(px);
 }
 stbi_image_free(atlas);
}
void loadBg(){
 const char*names[]={"bghome.png","bgtable.png","lobby-seaside.png"};
 for(int i=0;i<3;i++){
  std::string p=fontPath.size()>6&&fontPath[5]==':'?std::string("romfs:/")+names[i]:std::string("romfs/")+names[i];
  int w=0,h=0,n=0;unsigned char*px=stbi_load(p.c_str(),&w,&h,&n,4);
  if(!px){fprintf(stderr,"Bg %s failed: %s\n",names[i],stbi_failure_reason());continue;}
  SDL_Surface*sf=SDL_CreateRGBSurfaceWithFormatFrom(px,w,h,32,w*4,SDL_PIXELFORMAT_RGBA32);
  if(sf){bgTex[i]=SDL_CreateTextureFromSurface(ren,sf);SDL_FreeSurface(sf);}
  stbi_image_free(px);
 }
}
void face(int x,int y,int r,int who){
 Col skinC=who==1?Col{242,208,172}:who==3?Col{214,172,134}:Col{236,198,160};
 circle(x,y,r,skinC);
 if(who==2){circle(x,y,r,{250,212,66});circle(x-r/2-2,y-r/2,4,{255,255,255,90});circle(x+r/2+2,y-r/2,4,{255,255,255,90});
  for(int i=-3;i<=3;i++)circle(x+i*7,y-r+2,6,{255,252,240});
  circle(x,y+r/2,8,{250,212,66});circle(x,y+int(r*0.45),8,{250,212,66});line(x,y,x,y+int(r*0.55),{250,212,66});
  for(int i=0;i<6;i++)hline(x+2,y+i+r*0.16,i<3?i:6-i,{250,150,50});
  circle(x-7,y-4,2,INK);circle(x+7,y-4,2,INK);circle(x-11,y+2,3,{255,170,170,120});circle(x+11,y+2,3,{255,170,170,120});
  hline(x-7,y+10,x+7,{255,255,255});return;}
 if(who==0){rect(x-r,y-r+6,r*2,8,{56,46,38});rect(x-r,y-r,r*2,6,{66,54,44});}
 if(who==1){circle(x-r,y-r+2,7,{70,52,40});circle(x+r,y-r+2,7,{70,52,40});rect(x-r+2,y-r+8,r*2-4,9,{70,52,40});}
 if(who==3){rect(x-r,y-r+4,r*2,7,{150,150,150});rect(x-r,y-r+2,r*2,3,{160,160,160});circle(x-r/2+4,y+2,5,{200,210,220,180});circle(x+r/2-4,y+2,5,{200,210,220,180});line(x-r/2,y+2,x+r/2,y+2,{90,100,110,140});}
 circle(x-7,y-5,3,INK);circle(x+7,y-5,3,INK);circle(x-8,y-6,1,WHITE);circle(x+6,y-6,1,WHITE);
 for(int i=0;i<4;i++)line(x-5+i,y+7,x-4+i,y+4,INK);
 circle(x-11,y+3,4,{255,160,160,110});circle(x+11,y+3,4,{255,160,160,110});
}
void drawAvatar(int p,int x,int y,int r,bool active,bool passed,bool finished){
 Uint32 now=SDL_GetTicks();float pulse=active?1.0f+0.06f*std::sin(now/80.0f):1.0f;
 if(active||finished)ring(x,y,int(r+7*pulse),4,finished?GOLD:GOLD);
 if(avTex[p]){SDL_Rect dst{x-r,y-r,2*r,2*r};SDL_RenderCopy(ren,avTex[p],nullptr,&dst);}
 else face(x,y,int(r*pulse),p);
 if(passed){circle(x,y,int(r*0.92),{10,30,24,150});text("过",x,y-9,22,MUTED,true);}
 if(finished){for(int i=-2;i<=2;i++){rect(x+i*8-5,y-r-10,10,7,{255,255,255});line(x+i*8,y-r-10,x+i*8-5,y-r-17,GOLD);line(x+i*8,y-r-10,x+i*8+5,y-r-17,GOLD);}text("已出完",x,y+r+16,15,CREAM,true);}
 else if(p!=0)text(playerName(p),x,y+r+12,17,CREAM,true);
}
// ---------------- FX ----------------
void addFx(int type,int sx,int sy,int dx,int dy,std::vector<int> cards,std::string t=""){Fx f{type,SDL_GetTicks(),type==FX_PLAY?320:(type==FX_BOMB?620:(type==FX_EMOTE?2800:(type==FX_WIN?2200:800))),sx,sy,dx,dy,std::move(cards),t};fxList.push_back(f);if(fxList.size()>14)fxList.erase(fxList.begin());}
void drawFx(Uint32 now){
 for(auto&f:fxList){float t=(now-f.t0)/float(f.dur);if(t>=1)continue;t=std::max(0.0f,t);
  if(f.type==FX_PLAY){int n=f.cards.size();for(int i=0;i<n;i++){float ti=std::max(0.0f,std::min(1.0f,t*1.4f-i*0.04f));float ei=1.0f-std::pow(1.0f-ti,3.0f);int x=f.sx+(f.dx-f.sx)*ei,y=f.sy+(f.dy-f.sy)*ei;cardFace(f.cards[i],x,y,48,69,false,false);}}
  if(f.type==FX_BOMB){int cx=f.dx,cy=f.dy;float r=24+130*t;ring(cx,cy,int(r),int(3*(1-t)+1),{GOLD.r,GOLD.g,GOLD.b,Uint8(230*(1-t))});ring(cx,cy,int(r*0.62),int(3*(1-t)+1),{255,190,80,Uint8(200*(1-t))});
   for(int i=0;i<16;i++){float a=i*0.3927f;float rr=50+150*t;int px=cx+int(rr*std::cos(a)),py=cy+int(rr*std::sin(a));rect(px-3,py-3,6,6,{GOLD.r,GOLD.g,GOLD.b,Uint8(255*(1-t))});}
   text("炸 弹",cx,cy-int(30*(1-t))-22,38,{255,255,255,Uint8(255*(1-t))},true);}
  if(f.type==FX_PASS){int w=84,h=30;float a=(1-t);box(f.dx-w/2,f.dy-h/2,w,h,10,{10,30,24,Uint8(200*a)});text("不 出",f.dx,f.dy-8,17,{MUTED.r,MUTED.g,MUTED.b,Uint8(255*a)},true);}
  if(f.type==FX_EMOTE){int w=200+8*(f.text.size()>6?f.text.size()-6:0),h=40;float a=t<0.85?1.0f:1.0f-(t-0.85f)/0.15f;int bx=f.dx-w/2,by=f.dy-h/2;box(bx,by,w,h,12,{16,40,32,Uint8(235*a)});line(bx+w/2-8,by+h,bx+w/2+8,by+h,{16,40,32,Uint8(235*a)});text(f.text,f.dx,by+7,18,{GOLD.r,GOLD.g,GOLD.b,Uint8(255*a)},true);}
  if(f.type==FX_WIN){for(int i=0;i<46;i++){float p=std::fmod(t+i*0.013f,1.0f);int px=int(i*27.8f)+int(40*std::sin(t*6+i))*3,py=int(p*300)-40;Col cc=(i%3==0)?GOLD:(i%3==1)?Col{255,120,120}:Col{120,200,255};rect(px,py,5,9,{cc.r,cc.g,cc.b,Uint8(255*p)});}}
 }}
void clearFx(Uint32 now){fxList.erase(std::remove_if(fxList.begin(),fxList.end(),[&](Fx&f){return now-f.t0>Uint32(f.dur);}),fxList.end());}
// ---------------- screens ----------------
// The lobby is drawn natively so labels remain crisp on handheld and TV output.
void lobbyScrim(){
 // Smooth alpha ramps protect edge controls while leaving the central scene clear.
 for(int y=0;y<190;y++){float t=y/189.0f;float fade=1-t*t*(3-2*t);rect(0,y,W,1,{8,22,46,Uint8(224*fade)});}
 for(int y=535;y<H;y++){float t=(y-535)/184.0f;float fade=t*t*(3-2*t);rect(0,y,W,1,{8,22,46,Uint8(222*fade)});}
}
void bgHome(){
 if(bgTex[2]){SDL_Rect d{0,0,W,H};SDL_RenderCopy(ren,bgTex[2],nullptr,&d);}
 else{vgrad(0,0,W,H,{39,63,125},{14,19,53});for(int i=0;i<9;i++){int y=130+i*58;line(0,y,1280,y-180,{137,174,255,13});}}
 lobbyScrim();
}
void bgTable(){
 if(bgTex[1]){SDL_Rect dst{0,0,W,H};SDL_RenderCopy(ren,bgTex[1],nullptr,&dst);}
 else vgrad(0,0,W,H,skins[skin].top,skins[skin].bot);
 // Themes tint the same edge-to-edge scene instead of replacing it with flat panels.
 if(skin==1)rect(0,0,W,H,{12,42,100,38});
 if(skin==2)rect(0,0,W,H,{70,20,100,38});
}
void panelFrame(int x,int y,int w,int h,bool close=true){
 box(x+7,y+12,w,h,26,{6,18,45,85});box(x-4,y-4,w+8,h+8,28,{106,95,169});box(x-2,y-2,w+4,h+4,26,{232,200,120});
 roundedGradient(x,y,w,h,24,{254,253,249},{231,239,255});
 int divider=h<400?96:76;hline(x+34,y+divider,w-68,{195,205,226});box(x+44,y+divider-1,64,3,1,{224,172,62});
 if(!close)return;
 circle(x+w-44,y+40,22,{119,111,181});circle(x+w-44,y+40,19,{249,251,255});
 line(x+w-51,y+33,x+w-37,y+47,{90,91,147});line(x+w-51,y+47,x+w-37,y+33,{90,91,147});text("B",x+w-90,y+28,17,MUTED);
}
void button(int x,int y,int w,const std::string&label,bool active=false,bool enabled=true){
 if(lightPanel){box(x,y+4,w,48,14,active?Col{204,146,41}:Col{190,201,226});box(x-1,y-1,w+2,50,15,active?Col{227,177,67}:Col{191,204,234});roundedGradient(x,y,w,48,14,active?Col{255,227,132}:Col{255,255,255},active?Col{249,190,62}:Col{232,240,255});hline(x+18,y+3,w-36,{255,255,255,165});text(label,x+w/2,y+9,20,active?INK:enabled?CREAM:MUTED,true);return;}
 box(x,y,w,48,14,active?GOLD:Col{255,255,255,enabled?Uint8(18):Uint8(7)});text(label,x+w/2,y+9,20,active?INK:enabled?CREAM:MUTED,true);
}
void choiceRow(int x,int y,int w,const std::string&label,const std::string&value,bool active){
 box(x-2,y-2,w+4,52,14,active?Col{225,176,66}:Col{211,221,242});box(x,y,w,48,12,{255,255,255});
 if(active){box(x+1,y+9,4,30,2,{231,180,59});}
 text(label,x+24,y+10,20,CREAM);
 box(x+w-254,y+5,244,38,10,active?Col{255,237,184}:Col{237,242,254});
 if(value=="开启"||value=="关闭"){text(value,x+w-220,y+11,18,CREAM);bool on=value=="开启";box(x+w-90,y+12,60,26,13,on?Col{97,136,224}:Col{185,195,214});circle(x+w-(on?43:77),y+25,10,WHITE);}
 else{text(value,x+w-140,y+11,18,CREAM,true);text("›",x+w-32,y+7,22,MUTED,true);}
}
// Outlined floating labels stay legible over the scene without header/footer panels.
void floatingText(const std::string&label,int x,int y,int size,Col c=WHITE,bool centered=false){
 for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(dx||dy)text(label,x+dx,y+dy,size,{8,20,40,210},centered);
 text(label,x,y,size,c,centered);
}
void navIcon(int type,int x,int y,Col c){
 ring(x,y,16,3,{15,29,53,220});ring(x,y,15,1,c);
 auto glyph=[&](int dx,int dy,Col cc){
  if(type==0){vline(x-4+dx,y-7+dy,14,cc);line(x-4+dx,y-7+dy,x+7+dx,y+dy,cc);line(x+7+dx,y+dy,x-4+dx,y+7+dy,cc);}
  if(type==1)for(int i=0;i<3;i++){vline(x-7+i*7+dx,y+6-(i+1)*5+dy,(i+1)*5,cc);vline(x-6+i*7+dx,y+6-(i+1)*5+dy,(i+1)*5,cc);}
  if(type==2){ring(x+dx,y+dy,7,2,cc);for(int i=0;i<8;i++){float a=i*.785398f;line(x+dx+int(8*std::cos(a)),y+dy+int(8*std::sin(a)),x+dx+int(11*std::cos(a)),y+dy+int(11*std::sin(a)),cc);}}
 };
 glyph(1,1,{15,29,53,230});glyph(0,0,c);if(type==3)floatingText("?",x,y-15,21,c,true);
}
void homeButtons(){
 const char*labels[]={"继续上次对局","本地战绩","偏好设置","玩法说明"};
 for(int i=0;i<4;i++){auto a=navAreas[i];bool active=!modal&&focus==i+4,enabled=i!=0||hasSave;Col c=active?GOLD:enabled?WHITE:Col{209,218,228};
  box(a.x+28,a.y-1,a.w-56,72,18,active?GOLD:Col{207,224,255,65});
  box(a.x+29,a.y,a.w-58,70,17,active?Col{29,58,97,200}:Col{9,27,53,135});
  navIcon(i,a.x+a.w/2,a.y+15,c);floatingText(i==0&&!hasSave?"暂无进行中对局":labels[i],a.x+a.w/2,a.y+35,20,c,true);
  if(active){hline(a.x+a.w/2-58,a.y+65,116,GOLD);hline(a.x+a.w/2-58,a.y+66,116,GOLD);}
 }
 floatingText("方向键 选择   A 进入   − 规则",60,682,15);floatingText("离线单机 · 无需联网",1004,682,15);
}
void home(){
 bgHome();circle(82,46,23,GOLD);text("掼",82,28,26,{32,43,82},true);floatingText("掼蛋时光",120,23,31);
 floatingText("离线俱乐部",355,35,15,WHITE,true);
 floatingText("累计对局  "+std::to_string(total),824,32,20);floatingText("我方胜率  "+(total?std::to_string(wins*100/total)+"%":"—"),1022,32,20,GOLD);
 floatingText("好牌好搭档，随时开一桌",484,127,27);floatingText("双副牌 · 逢人配",1088,137,15,GOLD,true);
 if(bgTex[2]){floatingText("好配合，就是好手气",247,517,23,WHITE,true);floatingText("掼蛋时光 · 离线俱乐部",247,550,16,GOLD,true);}
 else{box(60,206,374,348,24,{13,27,66,180});if(avTex[0]){SDL_Rect d{160,258,174,174};SDL_RenderCopy(ren,avTex[0],nullptr,&d);}text("好配合，就是好手气",247,450,26,WHITE,true);text("经典升级 · 默契搭档 · 从 2 打到 A",247,494,17,{171,194,235},true);}
 const char* titles[]={"快速开局","经典升级","明牌练习","定局挑战"};const char* tags[]={"QUICK PLAY","CLASSIC MATCH","OPEN HANDS","CHALLENGE"};
 const char* desc[]={"随机发牌，一局尽兴","从 2 打到 A，进贡还贡","看清四家 · 练习配合","固定发牌 · 反复推演"};
 Col tops[4]={{65,114,201,235},{101,78,173,235},{37,103,146,235},{100,67,143,235}};Col bots[4]={{32,56,124,242},{53,37,113,242},{21,53,87,242},{47,31,85,242}};
 for(int i=0;i<4;i++){auto a=modeAreas[i];bool f=modal==8?pendingMode==i:!modal&&focus==i;box(a.x+3,a.y+6,a.w,a.h,20,{4,9,35,90});box(a.x-2,a.y-2,a.w+4,a.h+4,22,f?GOLD:Col{146,174,249,80});
  roundedGradient(a.x,a.y,a.w,a.h,20,tops[i],bots[i]);hline(a.x+22,a.y+3,a.w-44,{255,255,255,70});
  text(tags[i],a.x+24,a.y+16,13,{217,227,255});text(titles[i],a.x+24,a.y+(i<2?48:38),i<2?34:27,WHITE);text(desc[i],a.x+24,a.y+(i<2?99:80),17,{217,227,255});
  if(i<2){for(int j=0;j<3;j++)cardFace((i==0?36:40)+j,a.x+211+j*27,a.y+107-j*8,48,69,false,false);box(a.x+24,a.y+148,122,36,18,f?GOLD:Col{255,255,255,32});text(i==0?"即刻开桌  ›":"开始升级  ›",a.x+85,a.y+155,17,f?INK:WHITE,true);}
  else text("›",a.x+a.w-35,a.y+40,32,GOLD);
 }
 floatingText("选择玩法后，可调整 AI 难度、节奏和倒计时",478,565,15);
}
void showMove(int p,int cx,int y){auto&m=game.shown[p];if(game.passed[p]){text("不 出",cx,y+16,24,MUTED,true);return;}if(m.kind==NONE)return;int n=m.cards.size(),step=std::min(34,230/std::max(1,n));int x=cx-(step*(n-1)+48)/2;for(int i=0;i<n;i++)cardFace(m.cards[i],x+step*i,y,48,69,false,false);text(kindName(m.kind),cx,y+74,14,GOLD,true);}
void counterPanel(){
 if(!recorder||game.over||game.mode==2)return;
 box(16,74,306,124,12,{0,0,0,62});text("记牌器",28,80,15,GOLD);
 const char*lab[]={"2","3","4","5","6","7","8","9","10","J","Q","K","A","小王","大王"};int rr[]={2,3,4,5,6,7,8,9,10,11,12,13,14,16,17};
 for(int i=0;i<15;i++){int col=i%8,row=i/8;int x=26+col*37,y=102+row*37;box(x,y,34,32,6,{255,255,255,10});text(lab[i],x+4,y+1,14,CREAM);int n=game.remaining(rr[i]);text(std::to_string(n),x+22,y+12,17,n==0?RED:GOLD);}
}
struct HandPos{int x,y,w,h;};
struct OrganizedGroup{std::vector<int>cards;std::string label;};
static std::vector<OrganizedGroup> orgGroups;
static std::vector<int> orgGroupStart;
void computeOrganizedGroups(){
 auto&hand=game.hands[0];int level=game.level;
 std::array<std::vector<int>,18>rk;std::vector<int>jokers;
 for(int id:hand){int r=Card{id}.rank();if(r>=16)jokers.push_back(id);else rk[r].push_back(id);}
 for(int r=2;r<=14;r++)std::sort(rk[r].begin(),rk[r].end(),[](int a,int b){return Card{a}.suit()<Card{b}.suit();});
 orgGroups.clear();orgGroupStart.clear();
 // 1. Bombs (4+ same rank)
 for(int r=2;r<=14;r++)if(rk[r].size()>=4){orgGroups.push_back({rk[r],std::to_string(rk[r].size())+"炸"});rk[r].clear();}
 // 2. Straight flushes (greedy per suit)
 for(int s=0;s<4;s++)for(int st=1;st<=10;st++){
  std::vector<int>gc;bool ok=true;
  for(int i=0;i<5;i++){int r=st+i==1?14:st+i;
   auto it=std::find_if(rk[r].begin(),rk[r].end(),[&](int id){return Card{id}.suit()==s;});
   if(it==rk[r].end()){ok=false;break;}gc.push_back(*it);rk[r].erase(it);}
  if(!ok)for(int id:gc){for(int r=2;r<=14;r++){auto it=std::find(rk[r].begin(),rk[r].end(),id);if(it!=rk[r].end()){rk[r].insert(it,id);break;}}}}
 // 3. Straights (5 consecutive)
 for(int st=1;st<=10;st++){
  std::vector<int>gc;bool ok=true;
  for(int i=0;i<5;i++){int r=st+i==1?14:st+i;
   if(rk[r].empty()){ok=false;break;}gc.push_back(rk[r][0]);rk[r].erase(rk[r].begin());}
  if(ok)orgGroups.push_back({gc,"顺子"});
  else for(int id:gc)for(int r=2;r<=14;r++){if(!std::count(rk[r].begin(),rk[r].end(),id)){auto it=std::find_if(rk[r].begin(),rk[r].end(),[&](int x){return power(Card{x}.rank(),level)>=power(Card{id}.rank(),level);});rk[r].insert(it,id);break;}}
 }
 // 4. Consecutive pairs (三连对)
 for(int st=1;st<=12;st++){
  bool ok=true;std::vector<int>gc;
  for(int i=0;i<3;i++){int r=st+i==1?14:st+i;
   if(rk[r].size()<2){ok=false;break;}
   gc.push_back(rk[r][0]);gc.push_back(rk[r][1]);rk[r].erase(rk[r].begin(),rk[r].begin()+2);}
  if(ok)orgGroups.push_back({gc,"三连对"});
  else for(int id:gc)for(int r=2;r<=14;r++){if(!std::count(rk[r].begin(),rk[r].end(),id))rk[r].push_back(id);}
 }
 // 5. Remaining by rank
 for(int r=2;r<=14;r++)if(!rk[r].empty()){std::string lbl=rk[r].size()==3?"三带":"";orgGroups.push_back({rk[r],lbl});}
 if(!jokers.empty())orgGroups.push_back({jokers,""});
 // Rebuild hand in group order
 hand.clear();
 for(auto&g:orgGroups)for(int id:g.cards)hand.push_back(id);
 // Compute group start indices
 orgGroupStart.clear();int idx=0;
 for(auto&g:orgGroups){orgGroupStart.push_back(idx);idx+=g.cards.size();}
 orgGroupStart.push_back(idx);
}
std::vector<HandPos> organizedPositions(){
 int n=game.hands[0].size();std::vector<HandPos>pos(n);
 int ng=orgGroupStart.size()-1;
 int cw=80,ch=118;
 int colStep=std::max(56,std::min(78,1120/std::max(1,ng)));
 int vStep=42;int totalW=(ng-1)*colStep+cw;int sx=(W-totalW)/2;int baseBottom=598;
 for(int gi=0;gi<ng;gi++){int st=orgGroupStart[gi],en=orgGroupStart[gi+1];
  for(int ci=st;ci<en;ci++){int layer=ci-st;pos[ci]={sx+gi*colStep,baseBottom-layer*vStep-ch,cw,ch};}}
 return pos;
}
void table(){
 bgTable();
 floatingText("掼蛋时光",26,14,24,GOLD);
 const char*modes[]={"快速开局","经典升级","明牌练习","定局挑战"};floatingText(modes[game.mode],176,21,17);floatingText("第 "+std::to_string(game.round)+" 局",330,21,17);
 floatingText("本局打 "+rankName(game.level),646,17,26,GOLD,true);
 floatingText("我方 "+rankName(game.levels[0])+"   :   对方 "+rankName(game.levels[1]),880,22,18);floatingText("+ 暂停",1174,22,15);floatingText("ZL 记录",1097,20,17,WHITE,true);
 counterPanel();
 Uint32 now=SDL_GetTicks();
 auto fin=[&](int p){return game.hands[p].empty();};
 drawAvatar(2,game.mode==2?320:640,game.mode==2?108:118,26,game.turn==2&&!game.over,game.passed[2],fin(2));
 text(std::to_string(game.hands[2].size())+" 张",game.mode==2?300:706,game.mode==2?166:104,18,GOLD);
 drawAvatar(1,96,game.mode==2?372:300,game.mode==2?26:36,game.turn==1&&!game.over,game.passed[1],fin(1));
 text(std::to_string(game.hands[1].size())+" 张",96,game.mode==2?434:380,18,GOLD,true);
 drawAvatar(3,1184,game.mode==2?372:300,game.mode==2?26:36,game.turn==3&&!game.over,game.passed[3],fin(3));
 text(std::to_string(game.hands[3].size())+" 张",1184,game.mode==2?434:380,18,GOLD,true);
 if(game.mode==2){for(int p=1;p<4;p++){int x=p==1?26:p==3?1066:365,y=p==2?88:96;for(int i=0;i<int(game.hands[p].size());i++){int cols=p==2?27:6,col=i%cols,row=i/cols;cardFace(game.hands[p][i],x+col*(p==2?20:28),y+row*50,35,49,false,false);}}}
 else for(int p:{1,3}){int x=p==1?60:1160;for(int i=0;i<5;i++){int xx=x+i*5;box(xx,206+i*3,34,52,5,{40,80,64});line(xx+8,216+i*3,xx+26,248+i*3,GOLD_BORDER);}}
 showMove(2,640,196);showMove(1,250,288);showMove(3,1030,288);showMove(0,640,330);
 if(game.mode!=2)floatingText("对家：小满",443,80,15,{164,218,255},true);
 if(game.table.kind==NONE)text("自 由 领 出",640,262,22,{207,221,204,150},true);
 text("掼蛋时光",640,296,26,{246,198,74,18},true);
 if(!game.over){std::string st=game.turn==0?(autoplay?"正在托管 · 自动出牌":"轮到你出牌"):playerName(game.turn)+" 正在思考";text(st,640,424,21,game.turn==0?GOLD:MUTED,true);if(game.turn!=0){for(int i=0;i<3;i++)circle(640-30+i*30,457,3,i==int(now/350)%3?GOLD:Col{70,108,99});}}
 drawAvatar(0,260,452,30,game.turn==0&&!game.over&&!autoplay,game.passed[0],fin(0));
 text("你 · "+std::to_string(game.hands[0].size())+" 张",296,442,17,CREAM);
 if(timerOn&&screen&&!modal&&!game.over&&!autoplay&&game.turn==0){float el=float(now-lastAI)/float(TURN_MS);if(el<1){arc(197,452,23,4,-1.57f,-1.57f+6.2832f*el,GOLD);text(std::to_string(std::max(0,int((TURN_MS-(now-lastAI))/1000))),197,443,17,GOLD,true);}}
 auto chosen=selection();Move preview=identify(chosen,game.level,game.table);if(!chosen.empty()){std::string label="已选 "+std::to_string(chosen.size())+" 张 · "+(preview.kind==NONE?"无法出牌":std::string(kindName(preview.kind)));text(label,640,478,17,preview.kind==NONE?Col{255,151,151}:GOLD,true);}
 for(int p=1;p<4;p++)if(!fin(p)&&game.hands[p].size()<=5)text("余 "+std::to_string(game.hands[p].size())+" 张！",p==1?96:p==2?790:1184,p==2?166:(game.mode==2?458:374),18,{255,180,106},true);
 int n=game.hands[0].size();
 if(organized&&n>0){auto pos=organizedPositions();
  for(int gi=0;gi<(int)orgGroupStart.size()-1;gi++){
   for(int ci=orgGroupStart[gi+1]-1;ci>=orgGroupStart[gi];ci--){
    int id=game.hands[0][ci];int x=pos[ci].x,y=pos[ci].y;
    if(selected[id])y-=18;bool focused=ci==cursor&&game.turn==0&&!autoplay&&!game.over;
    cardFace(id,x,y,pos[ci].w,pos[ci].h,selected[id],focused);}
   // Draw group label below column
   if(!orgGroups[gi].label.empty()){
    int lx=pos[orgGroupStart[gi]].x+pos[orgGroupStart[gi]].w/2;
    text(orgGroups[gi].label,lx,606,13,{180,220,255},true);}
  }
 }else{int step=std::min(58,1138/std::max(1,n-1)),x=(W-(step*std::max(0,n-1)+66))/2;
  for(int i=0;i<n;i++)cardFace(game.hands[0][i],x+i*step,513,66,104,selected[game.hands[0][i]],i==cursor&&game.turn==0&&!autoplay&&!game.over);}
 if(game.mode==2)text("明牌练习 · 其余三家手牌可见",640,502,15,{207,221,204,130},true);
 drawFx(now);
 auto btn=[&](int bx,int by,int bw,const std::string&l,bool en,bool act){box(bx,by,bw,48,14,act?GOLD:en?Col{255,255,255,20}:Col{255,255,255,7});text(l,bx+bw/2,by+9,20,act?INK:en?CREAM:MUTED,true);};
 bool my=game.turn==0&&!game.over&&!autoplay;
 btn(210,630,140,"L 不出",game.table.kind!=NONE&&my,false);btn(360,630,140,"Y 提示",my,false);btn(510,630,180,"X 出牌",my&&preview.kind!=NONE,my&&preview.kind!=NONE);btn(700,630,120,organized?"恢复平铺":"一键理牌",true,false);btn(830,630,150,autoplay?"ZR 接管":"ZR 托管",!game.over,!game.over&&autoplay);btn(990,630,120,"表情",true,false);
 floatingText("← → 移动   A 选牌   B 清选   R 理牌",24,693,14);floatingText("− 规则   ZL 记录",1096,693,14);
 recordResult();
 if(game.over){LightPanelScope palette;rect(0,0,W,H,{2,10,16,60});panelFrame(380,150,520,340,false);text(game.finish[0]%2==0?"配合漂亮！":"再来一局",640,180,42,GOLD,true);text(game.notice,640,244,20,CREAM,true);
  int teamUp=0;if(game.mode==1){int tf=game.finish[0]%2;int place=0;for(int i=1;i<4;i++)if(game.finish[i]%2==tf)place=i;teamUp=4-place;}
  for(int i=0;i<4;i++)text(std::to_string(i+1)+"  "+playerName(game.finish[i])+(game.finish[i]%2==0?"  ★":""),440+(i%2)*215,285+(i/2)*38,19,game.finish[i]%2==0?GOLD:MUTED);
  text(game.mode!=1?"单局结束 · 下一局重新发牌":game.champion?"成功过 A · 新赛季开启！":"获胜方升 "+std::to_string(teamUp)+" 级",640,396,18,{207,221,204},true);
  button(395,428,165,game.champion?"A 新赛季":"A 下一局",true);button(580,428,135,"B 大厅");button(735,428,150,"ZL 复盘");}
 if(now<toastUntil){box(390,112,500,50,14,{10,22,16,238});text(toast,640,124,20,GOLD,true);}
}
void overlay(){
 if(!modal)return;
 LightPanelScope palette;rect(0,0,W,H,{2,10,16,60});panelFrame(210,84,860,548);
 if(modal==1){text("暂停片刻",254,116,34,GOLD);const char*items[]={"继续对局","保存并返回大厅","偏好设置","玩法说明"};for(int i=0;i<4;i++)button(340,196+i*80,600,items[i],focus==i);}
 if(modal==2){text("偏好设置",254,116,34,GOLD);const char*labels[]={"AI 难度","牌桌主题","出牌节奏","总音效","记牌器","出牌倒计时","背景音乐","音效音量"};std::string values[]={game.difficulty==0?"休闲":game.difficulty==1?"标准":"进阶",skin==0?"原色夜景":skin==1?"深海蓝":"午夜紫",speed==0?"舒缓":speed==1?"标准":"快速",sound?"开启":"关闭",recorder?"开启":"关闭",timerOn?"开启":"关闭",std::to_string(musicVolume)+"%",std::to_string(sfxVolume)+"%"};for(int i=0;i<8;i++){int y=160+i*62;if(i<6){choiceRow(315,y,650,labels[i],values[i],focus==i);}else{box(315,y+4,650,44,10,{8,16,28,200});text(labels[i],340,y+15,18,focus==i?GOLD:WHITE);int barX=560,barW=360,barH=10;box(barX,y+20,barW,barH,5,{40,50,70,255});int fillW=barW*(i==6?musicVolume:sfxVolume)/100;box(barX,y+20,fillW,barH,5,{80,200,160,255});text(values[i],930,y+15,18,{180,220,200},true);if(focus==i)box(312,y+1,656,50,12,GOLD);}}text("A 切换 / ←→ 调音量   B 返回",640,670,17,MUTED,true);}
 if(modal==3){text("玩法与操作",254,110,32,GOLD);const char*rules[]={"四人两副牌，对家组队。红桃级牌（逢人配）可代替非王牌。","牌型：单张 / 对子 / 三张 / 三带二 / 五张顺子 / 三连对 / 钢板。","炸弹：四至五张 < 同花顺 < 六张及以上；四王最大。","顺子中级牌回归原点数；A 可在开头或末尾，不循环。","头游一方按队友名次升 3 / 2 / 1 级；打 A 且队友非末游获胜。","升级赛自动进贡并还最低非配牌；双大王可抗贡。","← → 移动光标，A 选牌，X 出牌，B 清选，Y 切换提示，R 理牌。","L 不出，ZR 托管，ZL 出牌记录，+ 暂停，− 本页。","掌机支持点按选牌和按钮；倒计时结束自动出牌。","进阶 AI 为规则启发式策略；定局挑战可重试相同发牌。"};for(int i=0;i<10;i++)text(rules[i],254,168+i*37,18,i<6?CREAM:MUTED);}
 if(modal==4){text("本地战绩",254,116,34,GOLD);std::string v[]={std::to_string(total),std::to_string(wins),total?std::to_string(wins*100/total)+"%":"—",std::to_string(bombs)};const char*l[]={"完成对局","我方获胜","本地胜率","打出炸弹"};for(int i=0;i<4;i++){int x=330+(i%2)*340,y=200+(i/2)*150;box(x-28,y-12,280,126,16,{255,255,255,205});circle(x+206,y+24,17,{233,223,185});text("★",x+206,y+11,18,GOLD,true);text(v[i],x,y,46,CREAM);text(l[i],x,y+63,20,MUTED);}text("B 返回   ·   数据仅保存在本机",640,570,17,MUTED,true);}
 if(modal==5){text("出牌记录",254,116,34,GOLD);int pages=std::max(1,(int(game.history.size())+4)/5);historyPage=std::clamp(historyPage,0,pages-1);text("第 "+std::to_string(historyPage+1)+" / "+std::to_string(pages)+" 页",840,132,17,MUTED);
  if(game.history.empty())text("尚无出牌记录 · 旧版本存档从恢复后开始记录",640,300,21,CREAM,true);
  int end=int(game.history.size())-historyPage*5,start=std::max(0,end-5);for(int i=start;i<end;i++){auto&r=game.history[i];int y=178+(i-start)*72;box(248,y,784,65,10,{255,255,255,210});text(std::to_string(i+1),263,y+20,15,MUTED);text(playerName(r.player),310,y+17,20,r.player%2==0?GOLD:CREAM);text(r.move.kind==NONE?"不出":kindName(r.move.kind),390,y+21,16,MUTED);for(int j=0;j<int(r.move.cards.size());j++)cardFace(r.move.cards[j],500+j*46,y+5,40,54,false,false);}
  button(290,562,250,"← 较早记录");button(740,562,250,"较新记录 →");}
 if(modal==8){const char*modes[]={"快速开局","经典升级","明牌练习","定局挑战"};text(modes[pendingMode],254,116,34,GOLD);text("开局设置",892,128,18,MUTED);const char*labels[]={"AI 难度","出牌节奏","18 秒倒计时"};std::string values[]={game.difficulty==0?"休闲":game.difficulty==1?"标准":"进阶",speed==0?"舒缓":speed==1?"标准":"快速",timerOn?"开启":"关闭"};for(int i=0;i<3;i++)choiceRow(315,190+i*78,650,labels[i],values[i],focus==i);button(315,424,650,"开始对局  ›",true);text(pendingMode==3?"同一副首局可重试，适合练习拆牌思路":"四人双副牌 · 对家组队 · 红桃级牌为逢人配",640,534,18,MUTED,true);text("↑ ↓ 选择   A 切换 / 开始   B 返回",640,574,17,MUTED,true);}
 if(modal==6){text("快捷表情",254,116,34,GOLD);const char*em[]={"打得漂亮！","不要走，决战到天亮","快点吧，等得花儿都谢了","你太厉害了！","稳住，我们能赢！","这牌，绝了！","别急，容我想想","配合默契，双喜临门！"};for(int i=0;i<8;i++){int col=i%2,row=i/2;button(330+col*330,196+row*84,300,em[i],focus==i);}text("方向键选择   A 发送   B 返回",640,562,17,MUTED,true);}
 if(modal==7){text("复盘报告",254,116,34,GOLD);text("牌力统计 · 非 AI 评估",650,132,17,MUTED);int n=int(game.powerLog.size());int bombs=0,maxPow=0;for(int i=0;i<n;i++){if(game.bombLog[i])bombs++;maxPow=std::max(maxPow,game.powerLog[i]);}
  int cx=252,cy=168,cw=776,ch=206;box(cx,cy,cw,ch,10,{255,255,255,220});text("本局共 "+std::to_string(n)+" 手 · 炸弹 "+std::to_string(bombs)+" 次 · 最大牌力 "+std::to_string(maxPow),278,180,18,GOLD);
  const int grid[4]={100,75,50,25};for(int g=0;g<4;g++){int gy=cy+ch-26-grid[g]*(ch-70)/100;hline(cx+10,gy,cw-20,{180,194,221,130});text(std::to_string(grid[g]),cx-22,gy-7,14,MUTED);}
  int px0=cx+16,py0=cy+ch-26,px1=cx+cw-16,py1=cy+44;
  if(n>1){int lx=px0,ly=py0-(maxPow?int(double(game.powerLog[0])*100/maxPow):0)*(py0-py1)/100;for(int i=0;i<n;i++){int x=px0+(px1-px0)*i/(n-1);int sc=maxPow?std::min(100,int(double(game.powerLog[i])*100.0/double(maxPow))):0;int y=py0-sc*(py0-py1)/100;line(lx,ly,x,y,{180,120,24});circle(x,y,2,{180,120,24});lx=x;ly=y;}}
  text("牌力曲线 · 第 1 → 第 "+std::to_string(std::max(1,n))+" 手",px0,py0+8,14,MUTED);
  int qx=cx,qy=py0+38,qw=cw,qh=H-qy-170;box(qx,qy,qw,qh,10,{255,255,255,220});text("局势图 · 四家手牌余量",278,qy+8,18,GOLD);
  if(!game.snapshots.empty()){Col cols[4]={Col{180,120,24},Col{36,142,116},{127,86,187},{188,100,68}};const char*ps[4]={"你","南风","小满","青竹"};for(int p=0;p<4;p++){int lx=qx+16,ly=qy+qh-18-int(game.snapshots[0][p])*(qh-52)/27;for(int i=1;i<int(game.snapshots.size());i++){int x=qx+16+(qw-112)*i/(int(game.snapshots.size())-1);int y=qy+qh-18-int(game.snapshots[i][p])*(qh-52)/27;line(lx,ly,x,y,cols[p]);lx=x;ly=y;}text(ps[p],qx+qw-40,qy+44+23*p,14,cols[p]);}}
  text("B 返回",940,568,17,MUTED);}
}
// ---------------- actions ----------------
void doPlay(){if(game.turn!=0||game.over||autoplay)return;std::vector<int>cards;for(int id:game.hands[0])if(selected[id])cards.push_back(id);if(cards.empty()){message("先选中要打出的牌");return;}Move m=identify(cards,game.level,game.table);if(game.play(0,cards)){if(m.explosive())bombs++;sfx(m.explosive()?3:1);clearSelection();cursor=std::min(cursor,int(game.hands[0].size())-1);lastAI=SDL_GetTicks();checkpoint();}else message("牌型不合法，或无法压过上一手");}
void doHint(){if(game.turn!=0||game.over||autoplay)return;
 if(hintIndex<0||hintAction!=game.history.size()||hintSeed!=game.seed||hintRound!=game.round){hintOptions=moves(game.hands[0],game.level,game.table);std::stable_sort(hintOptions.begin(),hintOptions.end(),[](const Move&a,const Move&b){if(a.explosive()!=b.explosive())return !a.explosive();if(a.cards.size()!=b.cards.size())return a.cards.size()>b.cards.size();return a.high<b.high;});Move recommended=game.hint(0);if(recommended.kind==NONE&&game.table.kind!=NONE)hintOptions.insert(hintOptions.begin(),recommended);else{auto it=std::find_if(hintOptions.begin(),hintOptions.end(),[&](const Move&m){auto a=m.cards,b=recommended.cards;std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());return a==b;});if(it!=hintOptions.end())std::rotate(hintOptions.begin(),it,it+1);}hintAction=game.history.size();hintSeed=game.seed;hintRound=game.round;hintIndex=-1;}
 clearSelection();if(hintOptions.empty()){message("没有可压过的牌，按 L 不出");return;}hintIndex=(hintIndex+1)%int(hintOptions.size());auto&m=hintOptions[hintIndex];for(int id:m.cards)selected[id]=true;message(std::to_string(hintIndex+1)+" / "+std::to_string(hintOptions.size())+" · "+(m.kind==NONE?std::string("建议不出，按 L 执行"):std::string(kindName(m.kind))+"，按 X 出牌")+" · Y 换一组");}
void doAutoMove(){Move m=game.hint(0);if(m.kind==NONE){if(game.table.kind==NONE){auto ms=moves(game.hands[0],game.level);if(!ms.empty())game.play(0,ms.front().cards);}else game.pass(0);}else if(game.play(0,m.cards)&&m.explosive())bombs++;clearSelection();cursor=0;}
enum Action{LEFT,RIGHT,UP,DOWN,CONFIRM,BACK,PLAY,HINT,PASS,SORT,AUTO,PAUSE,HELP,HISTORY,EMOTE};
void act(Action a){
 if(modal){
  if(a==BACK||a==PAUSE){setModal(0);focus=0;sfx(2);return;}
  int max=modal==1?4:modal==2?8:modal==6?8:modal==8?4:1;
  if(modal==5){int pages=std::max(1,(int(game.history.size())+4)/5);if(a==LEFT||a==UP)historyPage=std::min(pages-1,historyPage+1);if(a==RIGHT||a==DOWN)historyPage=std::max(0,historyPage-1);return;}
  if(modal==6){if(a==LEFT||a==UP)focus=(focus+7)%8;if(a==RIGHT||a==DOWN)focus=(focus+1)%8;}
  else if(modal==2&&(a==LEFT||a==RIGHT)){if(focus==6){musicVolume=std::clamp(musicVolume+(a==RIGHT?10:-10),0,100);if(musicVolume>0&&!musicPlaying)musicStart();}if(focus==7){sfxVolume=std::clamp(sfxVolume+(a==RIGHT?10:-10),0,100);sfx(0);}saveStats();return;}
  else{if(a==UP)focus=(focus+max-1)%max;if(a==DOWN)focus=(focus+1)%max;}
  if(a==CONFIRM){
   if(modal==1){if(focus==0)setModal(0);else if(focus==1){checkpoint();screen=0;setModal(0);focus=4;}else if(focus==2){setModal(2);focus=0;}else{setModal(3);focus=0;}}
   else if(modal==2){if(focus==0)game.difficulty=(game.difficulty+1)%3;if(focus==1)skin=(skin+1)%3;if(focus==2)speed=(speed+1)%3;if(focus==3){sound=!sound;if(sound&&musicVolume>0)musicStart();if(!sound)musicStop();}if(focus==4)recorder=!recorder;if(focus==5)timerOn=!timerOn;if(focus==6){musicVolume=(musicVolume+10)%110;if(musicVolume==0)musicVolume=100;if(musicVolume>0&&!musicPlaying)musicStart();}if(focus==7){sfxVolume=(sfxVolume+10)%110;if(sfxVolume==0)sfxVolume=100;sfx(0);}saveStats();if(screen)checkpoint();sfx(0);}
   else if(modal==8){if(focus==0)game.difficulty=(game.difficulty+1)%3;if(focus==1)speed=(speed+1)%3;if(focus==2)timerOn=!timerOn;saveStats();if(focus==3){setModal(0);start(pendingMode);}sfx(0);}
   else if(modal==6){const char*em[]={"打得漂亮！","不要走，决战到天亮","快点吧，等得花儿都谢了","你太厉害了！","稳住，我们能赢！","这牌，绝了！","别急，容我想想","配合默契，双喜临门！"};addFx(FX_EMOTE,0,0,300,540,{},em[focus]);sfx(0);setModal(0);focus=0;}
  }
  return;
 }
 if(!screen){if(a==LEFT)focus=focus<4?(focus^1):4+(focus-4+3)%4;if(a==RIGHT)focus=focus<4?(focus^1):4+(focus-4+1)%4;if(a==UP)focus=focus<2?4+focus:focus<4?focus-2:2+(focus-4)/2;if(a==DOWN)focus=focus<2?focus+2:focus<4?4+(focus-2)*2:(focus-4)/2;if(a==PAUSE){quit=true;return;}if(a==HELP){setModal(3);focus=0;return;}
  if(a==CONFIRM){sfx(0);if(focus<4){pendingMode=focus;setModal(8);focus=3;}else if(focus==4){if(hasSave){Game g;if(g.load(base+"session.dat")){game=g;screen=1;autoplay=false;organized=false;clearSelection();cursor=0;lastAI=SDL_GetTicks();statRound=game.over?game.round:-1;resetHints();lastOver=game.over;fxList.clear();lastShown={};lastPassed={};}else message("存档无法读取");}}else{setModal(focus==5?4:focus==6?2:3);focus=0;}}return;}
 if(a==PAUSE){checkpoint();setModal(1);focus=0;sfx(0);return;}if(a==HELP){setModal(3);focus=0;return;}if(a==HISTORY){setModal(game.over?7:5);historyPage=0;focus=0;return;}if(a==EMOTE){if(game.over)return;setModal(6);focus=0;return;}
 if(game.over){if(a==CONFIRM){if(game.champion)start(game.mode);else{game.deal();statRound=-1;resetHints();lastOver=false;organized=false;clearSelection();cursor=0;checkpoint();lastAI=SDL_GetTicks();fxList.clear();lastShown={};lastPassed={};}}if(a==BACK){screen=0;focus=0;}return;}
 if(a==AUTO){autoplay=!autoplay;clearSelection();message(autoplay?"托管已开启，按 ZR 取消":"已取消托管");sfx(0);lastAI=SDL_GetTicks();return;}
 if(game.turn!=0||autoplay){return;}int n=game.hands[0].size();if(a==LEFT&&n)cursor=(cursor+n-1)%n;if(a==RIGHT&&n)cursor=(cursor+1)%n;if(a==CONFIRM&&n){selected[game.hands[0][cursor]]=!selected[game.hands[0][cursor]];sfx(0);}if(a==BACK)clearSelection();if(a==HINT)doHint();if(a==PLAY)doPlay();if(a==PASS){if(game.pass(0)){clearSelection();lastAI=SDL_GetTicks();sfx(2);checkpoint();}else message("自由领出时必须出牌");}if(a==SORT){game.sort();organized=!organized;if(organized)computeOrganizedGroups();clearSelection();cursor=0;message(organized?"一键理牌：炸弹/同花顺/顺子已分组标注":"已恢复平铺排列");sfx(0);}
}
void click(int x,int y){
 if(modal){
  if(Area{945,96,112,54}.contains(x,y)){act(BACK);return;}
  if(modal==5){if(Area{290,562,250,48}.contains(x,y))act(LEFT);else if(Area{740,562,250,48}.contains(x,y))act(RIGHT);return;}
  int count=modal==1?4:modal==2?8:modal==8?4:modal==6?8:0;
  for(int i=0;i<count;i++){
   Area area=modal==1?Area{340,196+i*80,600,48}:modal==2?Area{315,160+i*62,650,48}:modal==8?Area{315,190+i*78,650,48}:Area{330+(i%2)*330,196+(i/2)*84,300,48};
   if(area.contains(x,y)){focus=i;act(CONFIRM);return;}
  }
  return;
 }
 if(!screen){for(int i=0;i<4;i++){if(modeAreas[i].contains(x,y)){focus=i;act(CONFIRM);return;}}for(int i=0;i<4;i++)if(navAreas[i].contains(x,y)){focus=i+4;act(CONFIRM);return;}return;}
 if(game.over){if(Area{380,426,185,60}.contains(x,y))act(CONFIRM);else if(Area{570,426,155,60}.contains(x,y))act(BACK);else if(Area{730,426,170,60}.contains(x,y))act(HISTORY);return;}if(Area{1040,14,114,34}.contains(x,y)){act(HISTORY);return;}if(x>1140&&y<66){act(PAUSE);return;}
 struct BtnGeom{int x,y,w;}bs[6]={{210,630,140},{360,630,140},{510,630,180},{700,630,120},{830,630,150},{990,630,120}};Action acts[6]={PASS,HINT,PLAY,SORT,AUTO,EMOTE};
 for(int i=0;i<6;i++)if(x>=bs[i].x&&x<bs[i].x+bs[i].w&&y>=bs[i].y&&y<bs[i].y+48){act(acts[i]);return;}
 if(game.turn==0&&!autoplay){
  if(organized){auto pos=organizedPositions();int n=game.hands[0].size();
   for(int gi=(int)orgGroupStart.size()-2;gi>=0;gi--){
    for(int ci=orgGroupStart[gi];ci<orgGroupStart[gi+1];ci++){
     int id=game.hands[0][ci];int yy=pos[ci].y-(selected[id]?18:0);
     if(x>=pos[ci].x&&x<pos[ci].x+pos[ci].w&&y>=yy&&y<yy+pos[ci].h){cursor=ci;act(CONFIRM);return;}}}}
  else{int n=game.hands[0].size(),step=std::min(58,1138/std::max(1,n-1)),xx=(W-(step*std::max(0,n-1)+66))/2;
   for(int i=n-1;i>=0;i--){int yy=513-(selected[game.hands[0][i]]?18:0);
    if(x>=xx+i*step&&x<xx+i*step+66&&y>=yy&&y<yy+104){cursor=i;act(CONFIRM);return;}}}}
}
int main(int argc,char**argv){bool capture=false,captureGame=false,captureOver=false,smoke=false,reviewMode=false;int captureMode=0,captureModal=0;std::string capturePath="preview.bmp";
 for(int i=1;i<argc;i++){std::string s=argv[i];if(s=="--capture-training"){captureMode=2;s="--capture-game";}if(s=="--capture-help"){captureModal=3;s="--capture";}if(s=="--capture-stats"){captureModal=4;s="--capture";}if(s=="--capture-pause"){captureModal=1;s="--capture-game";}if(s=="--capture-settings"){captureModal=2;s="--capture";}if(s=="--capture-emote"){captureModal=6;s="--capture";}if(s=="--capture-setup"){captureModal=8;s="--capture";}if(s=="--capture-history"){captureModal=5;captureGame=true;s="--capture-game";}if(s=="--capture-review"){reviewMode=true;s="--capture-over";}if(s=="--capture-over"){capture=true;captureGame=true;captureOver=true;if(i+1<argc)capturePath=argv[++i];}if(s=="--capture"||s=="--capture-game"){capture=true;captureGame=s=="--capture-game";if(i+1<argc)capturePath=argv[++i];}if(s=="--smoke")smoke=true;if(s=="--data-dir"&&i+1<argc)base=std::string(argv[++i])+"/";}
#ifdef __SWITCH__
 {Result r=romfsInit();if(R_FAILED(r)){fprintf(stderr,"romfsInit failed: 0x%x\n",r);}base="sdmc:/switch/GuandanSwitch/";mkdir(base.c_str(),0777);fontPath="romfs:/font.otf";}
#else
 fontPath="romfs/font.otf";
#endif
 if(SDL_Init(SDL_INIT_VIDEO)<0){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}
 window=SDL_CreateWindow("掼蛋时光 | Guandan Switch",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
 ren=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);if(!ren){fprintf(stderr,"Renderer: %s\n",SDL_GetError());return 1;}
 SDL_RenderSetLogicalSize(ren,W,H);SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
 // 最早的画面：纯色填充，证明渲染器已启动
 SDL_SetRenderDrawColor(ren,20,40,80,255);SDL_RenderClear(ren);SDL_RenderPresent(ren);
 if(TTF_Init()<0){fprintf(stderr,"TTF: %s\n",TTF_GetError());}
 fonts[20]=TTF_OpenFont(fontPath.c_str(),20);if(!fonts[20])fprintf(stderr,"Font load failed: %s\n",TTF_GetError());
 // 显示加载中
 if(fonts[20]){SDL_SetRenderDrawColor(ren,8,16,28,255);SDL_RenderClear(ren);text("加载中...",640,340,36,{200,220,255},true);SDL_RenderPresent(ren);}
 // 手柄初始化（放在加载画面之后，便于定位是否卡在这里）
#ifdef __SWITCH__
 padConfigureInput(1,HidNpadStyleSet_NpadStandard);PadState pad;padInitializeDefault(&pad);
#endif
 loadAvatars();loadBg();
 // 音频：默认跳过避免模拟器挂起，存在 enable_audio 文件时才尝试初始化
 {bool tryAudio=false;std::ifstream af(base+"enable_audio");if(af.good())tryAudio=true;if(tryAudio){SDL_AudioSpec spec{};spec.freq=44100;spec.format=AUDIO_S16LSB;spec.channels=2;spec.samples=2048;SDL_AudioSpec obtained{};audioDevice=SDL_OpenAudioDevice(nullptr,0,&spec,&obtained,SDL_AUDIO_ALLOW_ANY_CHANGE);if(audioDevice){fprintf(stderr,"Audio: %dHz %dch\n",obtained.freq,obtained.channels);SDL_PauseAudioDevice(audioDevice,0);}else fprintf(stderr,"Audio unavailable: %s\n",SDL_GetError());}}
 std::ifstream profile(base+"profile.txt");profile>>total>>wins>>bombs>>skin>>speed>>sound>>recorder;int savedDifficulty=1;if(profile>>savedDifficulty)game.difficulty=std::clamp(savedDifficulty,0,2);int savedTimer=1;if(profile>>savedTimer)timerOn=savedTimer!=0;int savedMusicVol=50;if(profile>>savedMusicVol)musicVolume=std::clamp(savedMusicVol,0,100);int savedSfxVol=80;if(profile>>savedSfxVol)sfxVolume=std::clamp(savedSfxVol,0,100);skin=(skin%3+3)%3;speed=(speed%3+3)%3;{Game old;hasSave=old.load(base+"session.dat");}if(sound&&musicVolume>0)musicStart();
 if(captureGame||smoke){start(smoke?2:captureMode,20261016u);if(captureOver)autoplay=true;}if(captureModal){if(captureModal==5)for(int i=0;i<14&&!game.over;i++)game.autoStep();if(captureModal==8)focus=3;setModal(captureModal);}Uint32 frame=0; while(!quit){
#ifdef __SWITCH__
 if(!appletMainLoop()){break;}padUpdate(&pad);u64 down=padGetButtonsDown(&pad);u64 held=padGetButtons(&pad);static Uint32 repeat=0;u64 dirs=HidNpadButton_Left|HidNpadButton_Right|HidNpadButton_Up|HidNpadButton_Down;if(down&HidNpadButton_StickLLeft)down|=HidNpadButton_Left;if(down&HidNpadButton_StickLRight)down|=HidNpadButton_Right;if(down&HidNpadButton_StickLUp)down|=HidNpadButton_Up;if(down&HidNpadButton_StickLDown)down|=HidNpadButton_Down;if(held&HidNpadButton_StickLLeft)held|=HidNpadButton_Left;if(held&HidNpadButton_StickLRight)held|=HidNpadButton_Right;if(held&HidNpadButton_StickLUp)held|=HidNpadButton_Up;if(held&HidNpadButton_StickLDown)held|=HidNpadButton_Down;if(down&dirs)repeat=SDL_GetTicks()+320;else if((held&dirs)&&SDL_GetTicks()>repeat){down|=held&dirs;repeat=SDL_GetTicks()+95;}
 struct Binding{u64 bit;Action a;};Binding bindings[]={{HidNpadButton_Left,LEFT},{HidNpadButton_Right,RIGHT},{HidNpadButton_Up,UP},{HidNpadButton_Down,DOWN},{HidNpadButton_A,CONFIRM},{HidNpadButton_B,BACK},{HidNpadButton_X,PLAY},{HidNpadButton_Y,HINT},{HidNpadButton_L,PASS},{HidNpadButton_R,SORT},{HidNpadButton_ZR,AUTO},{HidNpadButton_ZL,HISTORY},{HidNpadButton_Plus,PAUSE},{HidNpadButton_Minus,HELP}};for(auto b:bindings)if(down&b.bit)act(b.a);
#endif
 SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_QUIT)quit=true;if(e.type==SDL_KEYDOWN){switch(e.key.keysym.sym){case SDLK_LEFT:act(LEFT);break;case SDLK_RIGHT:act(RIGHT);break;case SDLK_UP:act(UP);break;case SDLK_DOWN:act(DOWN);break;case SDLK_SPACE:act(CONFIRM);break;case SDLK_RETURN:act(screen&&!modal&&!game.over?PLAY:CONFIRM);break;case SDLK_ESCAPE:act(modal?BACK:PAUSE);break;case SDLK_BACKSPACE:act(BACK);break;case SDLK_h:act(HINT);break;case SDLK_p:act(PASS);break;case SDLK_r:act(SORT);break;case SDLK_t:act(AUTO);break;case SDLK_F1:act(HELP);break;case SDLK_l:act(HISTORY);break;case SDLK_e:act(EMOTE);break;}}
  if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.which!=SDL_TOUCH_MOUSEID){float mx,my;SDL_RenderWindowToLogical(ren,e.button.x,e.button.y,&mx,&my);click(int(mx),int(my));}if(e.type==SDL_FINGERDOWN)click(int(e.tfinger.x*W),int(e.tfinger.y*H));}
 Uint32 now=SDL_GetTicks();
 if(screen&&!modal){
  for(int p=0;p<4;p++){Move cur=game.shown[p];Move old=lastShown[p];
   bool fresh=cur.kind!=NONE&&(old.kind!=cur.kind||old.cards.empty()||old.cards.front()!=cur.cards.front());
   bool passed=game.passed[p]&&!lastPassed[p];
   if(fresh){int sx=0,sy=0,dx=0,dy=0;if(p==0){sx=640;sy=540;}else if(p==1){sx=96;sy=300;}else if(p==2){sx=640;sy=120;}else{sx=1184;sy=300;}
    if(p==0){dx=640;dy=330;}else if(p==1){dx=250;dy=300;}else if(p==2){dx=640;dy=205;}else{dx=1030;dy=300;}
    addFx(FX_PLAY,sx,sy,dx,dy,cur.cards);if(cur.explosive())addFx(FX_BOMB,0,0,dx,dy,{});
   }
   if(passed)addFx(FX_PASS,0,0,p==0?300:p==1?96:p==2?640:1184,(p==0?420:p==1?260:p==2?90:260),{});
  }
  lastShown=game.shown;lastPassed=game.passed;
  if(game.over&&!lastOver){addFx(FX_WIN,0,0,0,0,{});sfx(game.champion?4:7);}
  lastOver=game.over;
 }
 if(screen&&!modal&&!game.over&&(game.turn!=0||autoplay)&&now-lastAI>Uint32(capture&&captureOver?0:speed==0?1500:speed==1?850:320)&&(!capture||captureOver)){int p=game.turn;Move m=game.hint(p);if(m.kind==NONE)game.pass(p);else game.play(p,m.cards);if(p==0&&m.explosive())bombs++;clearSelection();cursor=0;lastAI=now;if(m.explosive())sfx(3);else sfx(1);checkpoint();}
 if(timerOn&&screen&&!modal&&!game.over&&!autoplay&&game.turn==0&&!capture&&now-lastAI>TURN_MS){doAutoMove();sfx(5);message("时间到，已自动出牌");lastAI=now;checkpoint();}
 if(smoke){if(frame==1){act(HELP);act(BACK);act(HINT);act(PLAY);}if(frame==2){act(PAUSE);act(BACK);act(AUTO);}if(frame>20)quit=true;}
 if(reviewMode&&game.over&&!modal&&frame>10)modal=7;
 trimCache();musicUpdate();if(screen)table();else{home();homeButtons();}overlay();clearFx(now);
 if(now<toastUntil){box(390,112,500,50,14,{10,22,16,238});text(toast,640,124,20,GOLD,true);}
 if((capture&&!captureOver&&frame==2)||(captureOver&&game.over&&frame>10&&!reviewMode)||(reviewMode&&game.over&&modal==7&&frame>18)){SDL_Surface*sf=SDL_CreateRGBSurfaceWithFormat(0,W,H,32,SDL_PIXELFORMAT_ARGB8888);SDL_RenderReadPixels(ren,nullptr,sf->format->format,sf->pixels,sf->pitch);SDL_SaveBMP(sf,capturePath.c_str());SDL_FreeSurface(sf);quit=true;}
 SDL_RenderPresent(ren);frame++;SDL_Delay(16);
 }
 if(screen){checkpoint();}saveStats();for(auto&kv:cache)SDL_DestroyTexture(kv.second.t);for(auto&kv:fonts)TTF_CloseFont(kv.second);for(auto t:avTex){if(t)SDL_DestroyTexture(t);}
 for(auto t:bgTex){if(t)SDL_DestroyTexture(t);}
 if(audioDevice){SDL_CloseAudioDevice(audioDevice);}
 SDL_DestroyRenderer(ren);SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();
#ifdef __SWITCH__
 romfsExit();
#endif
 return 0;
}
