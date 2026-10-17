// 掼蛋时光 · Guandan Time — Switch native homebrew (also builds on desktop)
// UI re-styled after the feel of Tencent's 大掼蛋 (clean-room: no Tencent assets).
// Rules engine lives in engine.hpp. This file is the SDL2 presentation layer.
#include "engine.hpp"
#include "hand_layout.hpp"
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
static std::string base="./",fontPath;static int skin=0,speed=1;static bool sound=true,quit=false,autoplay=false,recorder=true,timerOn=true;
static bool organized=true,smartGrouped=true,counterOpen=false;
static int suitFilter=-1,flushIndex=-1;
static std::vector<int> layoutHand;
static int layoutLevel=0,layoutRound=0;static bool layoutSmart=false;
static bool gestureActive=false,gestureValue=false;static bool gestureVisited[108]{};
void computeOrganizedGroups();void ensureGroups();void restoreHand();
static SDL_AudioDeviceID audioDevice=0;static Game game;static int screen=0,modal=0,focus=0,cursor=0;static bool selected[108]{};
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
void queueTone(int freq,int ms,int vol,bool noise=false){if(!audioDevice||!sound)return;int n=48000/1000*ms;std::vector<int16_t>s(n);for(int i=0;i<n;i++){double t=i/48000.0;double env=1.0-i/double(n);double v=std::sin(6.2831853*freq*t);if(noise)v+=((std::rand()%2001)/1000.0-1.0)*0.8;int16_t x=int16_t(v*vol*env);s[i]=x;}SDL_QueueAudio(audioDevice,s.data(),s.size()*2);}
void sfx(int kind){if(!audioDevice||!sound)return;if(kind==0)queueTone(660,55,1200);if(kind==1)queueTone(440,80,1400);if(kind==2)queueTone(300,55,1100);if(kind==3){queueTone(110,260,2000,true);queueTone(90,180,1500,true);}if(kind==4){queueTone(660,110,1400);queueTone(880,110,1400);queueTone(990,160,1400);}if(kind==5)queueTone(220,120,900,true);}
void beep(){sfx(0);}
void clearSelection(){std::fill(std::begin(selected),std::end(selected),false);}
void saveStats(){std::ofstream f(base+"profile.txt");f<<total<<' '<<wins<<' '<<bombs<<' '<<skin<<' '<<speed<<' '<<sound<<' '<<recorder<<' '<<game.difficulty<<' '<<timerOn;}
void checkpoint(){hasSave=game.save(base+"session.dat");if(!hasSave)message("存档写入失败，请检查 SD 卡空间");}
void recordResult(){if(game.over&&statRound!=game.round){statRound=game.round;total++;if(game.finish[0]%2==0)wins++;saveStats();}}
void start(int mode,uint32_t seed=0){int diff=game.difficulty;game.newMatch(mode,seed?seed:mode==3?20261016u:uint32_t(std::time(nullptr)));game.difficulty=diff;screen=1;modal=0;autoplay=false;organized=true;smartGrouped=true;suitFilter=-1;layoutHand.clear();cursor=0;clearSelection();statRound=-1;resetHints();lastOver=false;lastAI=SDL_GetTicks();fxList.clear();lastShown={};lastPassed={};checkpoint();}

// ---------------- cards ----------------
void cardFace(int id,int x,int y,int w,int h,bool sel,bool focused,bool up=true){
 if(sel)y-=18;
 box(x+2,y+3,w,h,7,{14,30,49,50});if(sel||focused)box(x-2,y-2,w+4,h+4,9,sel?Col{255,211,91}:Col{249,223,145});
 box(x,y,w,h,7,{0,0,0});box(x+1,y+1,w-2,h-2,6,up?(sel?Col{255,248,219}:Col{255,253,244}):Col{54,115,163});
 if(!up){box(x+5,y+5,w-10,h-10,4,{255,255,255,40});text("掼",x+w/2,y+h/2-13,std::max(12,w/3),WHITE,true);return;}
 int r=Card{id}.rank(),s=Card{id}.suit();Col c=(s==1||s==2||r==17)?Col{183,35,39}:Col{23,26,32};
 const char*suits[]={"♠","♥","♦","♣","★"};int fs=w<40?13:w<65?20:std::min(29,w/3),ss=w<40?12:w<65?18:fs;
 text(r>=16?(r==17?"大":"小"):rankName(r),x+4,y+1,fs,c);
 text(suits[s],x+(r==10?fs+9:fs+2),y+1,ss,c);
 if(r>=16){text("王",x+4,y+fs,fs,c);if(w>=62)text("★",x+w/2,y+h-65,40,c,true);}
 else if(w>=62){text(suits[s],x+w/2+2,y+h-69,std::min(58,w*3/5),c,true);text(rankName(r),x+w-18,y+h-25,17,c,true);}
 if(wild(id,game.level)){box(x+w-25,y+h-27,21,23,4,{245,181,60});text("配",x+w-15,y+h-25,14,{130,61,18},true);}
 else if(r==game.level&&w>=62){box(x+4,y+h-23,18,18,3,{66,163,143});text("级",x+13,y+h-23,12,WHITE,true);}
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
void showMove(int p,int cx,int y){auto&m=game.shown[p];if(game.passed[p]){text("不 出",cx,y+16,24,MUTED,true);return;}if(m.kind==NONE)return;int n=m.cards.size(),step=std::min(34,230/std::max(1,n));int x=cx-(step*(n-1)+48)/2;for(int i=0;i<n;i++)cardFace(m.cards[i],x+step*i,y,48,69,false,false);text(kindName(m.kind),cx,y+71,12,WHITE,true);}
void counterPanel(){
 if(!recorder||game.over||game.mode==2)return;
 box(16,74,306,124,12,{0,0,0,62});text("记牌器",28,80,15,GOLD);
 const char*lab[]={"2","3","4","5","6","7","8","9","10","J","Q","K","A","小王","大王"};int rr[]={2,3,4,5,6,7,8,9,10,11,12,13,14,16,17};
 for(int i=0;i<15;i++){int col=i%8,row=i/8;int x=26+col*37,y=102+row*37;box(x,y,34,32,6,{255,255,255,10});text(lab[i],x+4,y+1,14,CREAM);int n=game.remaining(rr[i]);text(std::to_string(n),x+22,y+12,17,n==0?RED:GOLD);}
}
#include "table_ui.hpp"
void table(){
 referenceScene();Uint32 now=SDL_GetTicks();ensureGroups();
 box(20,14,155,42,8,{255,250,226,245});text("本局打",33,21,22,{151,103,39});text(rankName(game.level),145,16,32,{157,77,26},true);
 int bombCount=0;for(const auto&r:game.history)if(r.move.explosive())bombCount++;
 box(185,14,146,42,8,{22,51,92,170});text("炸弹倍数",199,24,16,WHITE);text("×"+std::to_string(1<<std::min(bombCount,10)),297,19,24,GOLD,true);
 floatingText("第 "+std::to_string(game.round)+" 局",350,24,18,WHITE);
 pill(1048,14,108,42,"记牌器",false,true,18);pill(1170,14,90,42,"更多",false,true,18);
 time_t tmNow=std::time(nullptr);auto*local=std::localtime(&tmNow);char tmText[12]="";if(local)std::strftime(tmText,sizeof(tmText),"%H:%M",local);floatingText(tmText,1210,68,17,WHITE,true);
 floatingText("我方 "+rankName(game.levels[0])+"  :  对方 "+rankName(game.levels[1]),82,72,16,WHITE,true);
 tableSeat(2,460,91,true);tableSeat(1,72,232);tableSeat(3,1208,232);
 if(!game.over){if(game.turn==1)clockBadge(158,227,now);else if(game.turn==2)clockBadge(536,87,now);else if(game.turn==3)clockBadge(1125,227,now);}
 showMove(2,705,game.mode==2?185:94);showMove(1,263,205);showMove(3,1012,205);
 // Own last move stays in a corner; it cannot cover the playable hand.
 if(game.shown[0].kind!=NONE||game.passed[0])showMove(0,88,438);
 if(game.mode==2){
  for(int p=1;p<4;p++){int n=game.hands[p].size(),sx=p==2?500:p==1?18:916;
   for(int i=0;i<n;i++)cardFace(game.hands[p][i],sx+i*12,125,30,44,false,false);}
 }

 auto chosen=selection();Move preview=identify(chosen,game.level,game.table);bool my=game.turn==0&&!game.over&&!autoplay;
 if(my){clockBadge(403,289,now);pill(445,266,125,46,"L 不出",false,game.table.kind!=NONE);pill(586,266,125,46,"Y 提示",false);pill(728,266,150,46,"X 出牌",true,preview.kind!=NONE);pill(894,266,94,46,"清空",false,!chosen.empty(),18);}
 else if(!game.over){floatingText(autoplay?"正在托管 · ZR 接管":playerName(game.turn)+"正在出牌 · 可预选手牌",705,287,19,WHITE,true);}
 if(!chosen.empty())floatingText("已选 "+std::to_string(chosen.size())+" 张 · "+(preview.kind==NONE?"请按合法牌型选择":std::string(kindName(preview.kind))),705,322,16,preview.kind==NONE?Col{255,226,196}:WHITE,true);
 else if(my)floatingText(game.table.kind==NONE?"自由领出 · 请选择手牌":"轮到你出牌",705,322,16,WHITE,true);
 auto pos=handPositions();
 for(int i=0;i<int(pos.size());i++){
  int id=game.hands[0][i];auto a=pos[i];cardFace(id,a.x,a.y,a.w,a.h,selected[id],i==cursor&&!autoplay&&!game.over);
  if(suitFilter>=0&&Card{id}.suit()!=suitFilter&&!wild(id,game.level))box(a.x+1,a.y-(selected[id]?18:0)+1,a.w-2,a.h-2,7,{41,65,95,102});
 }
 if(organized){for(size_t g=0;g<orgGroups.size();g++){auto a=pos[orgGroupStart[g]];if(!orgGroups[g].label.empty()){int tw=std::min(a.w,88);box(a.x,632,tw,21,4,orgGroups[g].kind>=BOMB?Col{120,85,167,232}:Col{32,131,141,225});text(orgGroups[g].label,a.x+tw/2,633,13,WHITE,true);}}}
 tableSeat(0,57,615);floatingText("手牌 "+std::to_string(game.hands[0].size())+" 张",112,638,16,WHITE,true);
 // A gradient keeps the complete scene visible behind the toolbar.
 for(int y=652;y<H;y++)rect(0,y,W,1,{17,35,63,Uint8(80+(y-652)*2)});
 floatingText("同花顺",270,671,22,WHITE,true);
 const char*suits[]={"♠","♥","♦","♣"};
 for(int i=0;i<4;i++){int x=338+i*55;if(suitFilter==i)box(x-22,660,44,43,12,{255,255,255,100});text(suits[i],x,662,31,i==1||i==2?Col{254,167,143}:WHITE,true);}
 pill(548,663,120,42,"恢复",true,true,21);pill(683,663,176,42,"R 一键理牌",false,true,21);
 pill(875,663,113,42,autoplay?"接管":"托管",false,!game.over,20);pill(1004,663,88,42,"表情",true,!game.over,20);pill(1106,663,150,42,"ZL 出牌记录",false,true,17);
 floatingText("↑↓ 选层  ←→ 换列",130,707,11,WHITE);floatingText("A 选牌  B 清选  RS 恢复  LS 花色",629,707,11,WHITE,true);
 referenceCounter();
 recordResult();
 if(game.over){LightPanelScope palette;rect(0,0,W,H,{2,10,16,60});panelFrame(380,150,520,340,false);text(game.finish[0]%2==0?"配合漂亮！":"再来一局",640,180,42,GOLD,true);text(game.notice,640,244,20,CREAM,true);
  int teamUp=0;if(game.mode==1){int tf=game.finish[0]%2;int place=0;for(int i=1;i<4;i++)if(game.finish[i]%2==tf)place=i;teamUp=4-place;}
  for(int i=0;i<4;i++)text(std::to_string(i+1)+"  "+playerName(game.finish[i])+(game.finish[i]%2==0?"  ★":""),440+(i%2)*215,285+(i/2)*38,19,game.finish[i]%2==0?GOLD:MUTED);
  text(game.mode!=1?"单局结束 · 下一局重新发牌":game.champion?"成功过 A · 新赛季开启！":"获胜方升 "+std::to_string(teamUp)+" 级",640,396,18,{207,221,204},true);
  button(395,428,165,game.champion?"A 新赛季":"A 下一局",true);button(580,428,135,"B 大厅");button(735,428,150,"ZL 复盘");}
 if(now<toastUntil){box(340,218,600,38,16,{10,22,16,238});text(toast,640,224,17,GOLD,true);}
}
void overlay(){
 if(!modal)return;
 LightPanelScope palette;rect(0,0,W,H,{2,10,16,60});panelFrame(210,84,860,548);
 if(modal==10){int player=focus+1;text("明牌手牌 · "+playerName(player),254,116,32,GOLD);text(player==2?"你的队友":"对手",895,128,18,MUTED);auto hand=game.hands[player];for(int i=0;i<int(hand.size());i++)cardFace(hand[i],256+(i%14)*55,190+(i/14)*150,50,112,false,false);button(290,542,250,"← 上一家");button(740,542,250,"下一家 →");}
 if(modal==1){text("暂停片刻",254,116,34,GOLD);const char*items[]={"继续对局","保存并返回大厅","偏好设置","玩法说明"};for(int i=0;i<4;i++)button(340,196+i*80,600,items[i],focus==i);}
 if(modal==2){text("偏好设置",254,116,34,GOLD);const char*labels[]={"AI 难度","牌桌主题","出牌节奏","操作音效","记牌器","出牌倒计时"};std::string values[]={game.difficulty==0?"休闲":game.difficulty==1?"标准":"进阶",skin==0?"原色夜景":skin==1?"深海蓝":"午夜紫",speed==0?"舒缓":speed==1?"标准":"快速",sound?"开启":"关闭",recorder?"开启":"关闭",timerOn?"开启":"关闭"};for(int i=0;i<6;i++)choiceRow(315,180+i*70,650,labels[i],values[i],focus==i);text("A 切换选项   B 返回",640,600,17,MUTED,true);}
 if(modal==3){text("玩法与操作",254,110,32,GOLD);const char*rules[]={"四人两副牌，对家组队。红桃级牌（逢人配）可代替非王牌。","牌型：单张 / 对子 / 三张 / 三带二 / 五张顺子 / 三连对 / 钢板。","炸弹：四至五张 < 同花顺 < 六张及以上；四王最大。","顺子中级牌回归原点数；A 可在开头或末尾，不循环。","头游一方按队友名次升 3 / 2 / 1 级；打 A 且队友非末游获胜。","升级赛自动进贡并还最低非配牌；双大王可抗贡。","↑↓ 选层，←→ 换列，A 选牌，X 出牌，B 清选，Y 提示。","R 一键理牌，RS 恢复按点数，LS 花色，L 不出，ZR 托管。","点按牌型标签可整组选择；滑动多选；底部可查找同花顺。","进阶 AI 为规则启发式策略；定局挑战可重试相同发牌。"};for(int i=0;i<10;i++)text(rules[i],254,168+i*37,18,i<6?CREAM:MUTED);}
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
enum Action{LEFT,RIGHT,UP,DOWN,CONFIRM,BACK,PLAY,HINT,PASS,SORT,AUTO,PAUSE,HELP,HISTORY,EMOTE,RESTORE,SUIT,FLUSH_FIND,COUNTER};
void act(Action a){
 if(modal){
  if(a==BACK||a==PAUSE){setModal(0);focus=0;sfx(2);return;}
  int max=modal==1?4:modal==2?6:modal==6?8:modal==8?4:1;
  if(modal==10){if(a==LEFT||a==UP)focus=(focus+2)%3;if(a==RIGHT||a==DOWN)focus=(focus+1)%3;return;}
  if(modal==5){int pages=std::max(1,(int(game.history.size())+4)/5);if(a==LEFT||a==UP)historyPage=std::min(pages-1,historyPage+1);if(a==RIGHT||a==DOWN)historyPage=std::max(0,historyPage-1);return;}
  if(modal==6){if(a==LEFT||a==UP)focus=(focus+7)%8;if(a==RIGHT||a==DOWN)focus=(focus+1)%8;}
  else{if(a==UP)focus=(focus+max-1)%max;if(a==DOWN)focus=(focus+1)%max;}
  if(a==CONFIRM){
   if(modal==1){if(focus==0)setModal(0);else if(focus==1){checkpoint();screen=0;setModal(0);focus=4;}else if(focus==2){setModal(2);focus=0;}else{setModal(3);focus=0;}}
   else if(modal==2){if(focus==0)game.difficulty=(game.difficulty+1)%3;if(focus==1)skin=(skin+1)%3;if(focus==2)speed=(speed+1)%3;if(focus==3)sound=!sound;if(focus==4)recorder=!recorder;if(focus==5)timerOn=!timerOn;saveStats();if(screen)checkpoint();sfx(0);}
   else if(modal==8){if(focus==0)game.difficulty=(game.difficulty+1)%3;if(focus==1)speed=(speed+1)%3;if(focus==2)timerOn=!timerOn;saveStats();if(focus==3){setModal(0);start(pendingMode);}sfx(0);}
   else if(modal==6){const char*em[]={"打得漂亮！","不要走，决战到天亮","快点吧，等得花儿都谢了","你太厉害了！","稳住，我们能赢！","这牌，绝了！","别急，容我想想","配合默契，双喜临门！"};addFx(FX_EMOTE,0,0,300,540,{},em[focus]);sfx(0);setModal(0);focus=0;}
  }
  return;
 }
 if(!screen){if(a==LEFT)focus=focus<4?(focus^1):4+(focus-4+3)%4;if(a==RIGHT)focus=focus<4?(focus^1):4+(focus-4+1)%4;if(a==UP)focus=focus<2?4+focus:focus<4?focus-2:2+(focus-4)/2;if(a==DOWN)focus=focus<2?focus+2:focus<4?4+(focus-2)*2:(focus-4)/2;if(a==PAUSE){quit=true;return;}if(a==HELP){setModal(3);focus=0;return;}
  if(a==CONFIRM){sfx(0);if(focus<4){pendingMode=focus;setModal(8);focus=3;}else if(focus==4){if(hasSave){Game g;if(g.load(base+"session.dat")){game=g;screen=1;autoplay=false;clearSelection();cursor=0;lastAI=SDL_GetTicks();statRound=game.over?game.round:-1;resetHints();lastOver=game.over;fxList.clear();lastShown={};lastPassed={};}else message("存档无法读取");}}else{setModal(focus==5?4:focus==6?2:3);focus=0;}}return;}
 if(a==PAUSE){checkpoint();setModal(1);focus=0;sfx(0);return;}if(a==HELP){setModal(3);focus=0;return;}if(a==COUNTER){if(!recorder){recorder=true;saveStats();}counterOpen=!counterOpen;return;}if(a==HISTORY){setModal(game.over?7:5);historyPage=0;focus=0;return;}if(a==EMOTE){if(game.over)return;setModal(6);focus=0;return;}
 if(game.over){if(a==CONFIRM){if(game.champion)start(game.mode);else{game.deal();organized=true;smartGrouped=true;suitFilter=-1;layoutHand.clear();statRound=-1;resetHints();lastOver=false;clearSelection();cursor=0;checkpoint();lastAI=SDL_GetTicks();fxList.clear();lastShown={};lastPassed={};}}if(a==BACK){screen=0;focus=0;}return;}
 if(a==AUTO){autoplay=!autoplay;clearSelection();message(autoplay?"托管已开启，按 ZR 取消":"已取消托管");sfx(0);lastAI=SDL_GetTicks();return;}
 if(a==RESTORE){restoreHand();sfx(0);return;}
 if(a==SORT){organized=true;smartGrouped=true;suitFilter=-1;clearSelection();computeOrganizedGroups();cursor=0;message("一键理牌：同花顺、炸弹、连对与顺子已分列");sfx(0);return;}
 if(a==SUIT){suitFilter=suitFilter==3?-1:suitFilter+1;flushIndex=-1;return;}
 if(a==FLUSH_FIND){if(!autoplay)findStraightFlush();return;}
 if(a==BACK){clearSelection();return;}
 if(autoplay)return;
 ensureGroups();int n=int(game.hands[0].size());
 if(organized&&n){int g=0;while(g+1<int(orgGroupStart.size())-1&&cursor>=orgGroupStart[g+1])g++;int row=cursor-orgGroupStart[g];
  if(a==UP)cursor=std::max(orgGroupStart[g],cursor-1);
  if(a==DOWN)cursor=std::min(orgGroupStart[g+1]-1,cursor+1);
  if(a==LEFT||a==RIGHT){int dest=(g+(a==RIGHT?1:int(orgGroups.size())-1))%int(orgGroups.size());cursor=orgGroupStart[dest]+std::min(row,orgGroupStart[dest+1]-orgGroupStart[dest]-1);}
 }else{if(a==LEFT&&n)cursor=(cursor+n-1)%n;if(a==RIGHT&&n)cursor=(cursor+1)%n;}
 if(a==CONFIRM&&n){selected[game.hands[0][cursor]]=!selected[game.hands[0][cursor]];sfx(0);}
 if(game.turn!=0)return;
 if(a==HINT)doHint();
 if(a==PLAY)doPlay();
 if(a==PASS){if(game.pass(0)){clearSelection();lastAI=SDL_GetTicks();sfx(2);checkpoint();}else message("自由领出时必须出牌");}

}
void click(int x,int y){
 if(modal){
  if(Area{945,96,112,54}.contains(x,y)){act(BACK);return;}
  if(modal==10){if(Area{290,542,250,48}.contains(x,y))act(LEFT);if(Area{740,542,250,48}.contains(x,y))act(RIGHT);return;}
  if(modal==5){if(Area{290,562,250,48}.contains(x,y))act(LEFT);else if(Area{740,562,250,48}.contains(x,y))act(RIGHT);return;}
  int count=modal==1?4:modal==2?6:modal==8?4:modal==6?8:0;
  for(int i=0;i<count;i++){
   Area area=modal==1?Area{340,196+i*80,600,48}:modal==2?Area{315,180+i*70,650,48}:modal==8?Area{315,190+i*78,650,48}:Area{330+(i%2)*330,196+(i/2)*84,300,48};
   if(area.contains(x,y)){focus=i;act(CONFIRM);return;}
  }
  return;
 }
 if(!screen){for(int i=0;i<4;i++){if(modeAreas[i].contains(x,y)){focus=i;act(CONFIRM);return;}}for(int i=0;i<4;i++)if(navAreas[i].contains(x,y)){focus=i+4;act(CONFIRM);return;}return;}
 if(game.over){if(Area{380,426,185,60}.contains(x,y))act(CONFIRM);else if(Area{570,426,155,60}.contains(x,y))act(BACK);else if(Area{730,426,170,60}.contains(x,y))act(HISTORY);return;}if(Area{1048,14,108,42}.contains(x,y)){act(COUNTER);return;}if(Area{1170,14,90,42}.contains(x,y)){act(PAUSE);return;}
 if(game.mode==2){const Area seats[]={{20,194,105,130},{421,50,78,125},{1155,194,110,130}};for(int i=0;i<3;i++)if(seats[i].contains(x,y)){setModal(10);focus=i;return;}}
 const Area mainButtons[]={{445,266,125,46},{586,266,125,46},{728,266,150,46},{894,266,94,46}};
 const Action mainActions[]={PASS,HINT,PLAY,BACK};
 if(game.turn==0&&!autoplay)for(int i=0;i<4;i++)if(mainButtons[i].contains(x,y)){act(mainActions[i]);return;}
 if(Area{211,660,103,46}.contains(x,y)){act(FLUSH_FIND);return;}
 for(int i=0;i<4;i++)if(Area{316+i*55,660,44,43}.contains(x,y)){suitFilter=suitFilter==i?-1:i;flushIndex=-1;sfx(0);return;}
 const Area bottom[]={{548,663,120,42},{683,663,176,42},{875,663,113,42},{1004,663,88,42},{1106,663,150,42}};
 const Action bottomActions[]={RESTORE,SORT,AUTO,EMOTE,HISTORY};for(int i=0;i<5;i++)if(bottom[i].contains(x,y)){act(bottomActions[i]);return;}
 if(organized&&y>=632&&y<654&&!autoplay){ensureGroups();auto pos=organizedPositions();for(size_t g=0;g<orgGroups.size();g++){auto a=pos[orgGroupStart[g]];if(Area{a.x,632,std::min(a.w,88),21}.contains(x,y)){bool all=true;for(int id:orgGroups[g].cards)all&=selected[id];for(int id:orgGroups[g].cards)selected[id]=!all;sfx(0);return;}}}
 beginHandGesture(x,y);

}
int main(int argc,char**argv){bool capture=false,captureGame=false,captureOver=false,smoke=false,reviewMode=false;int layoutCapture=0;int captureMode=0,captureModal=0;std::string capturePath="preview.bmp";
 for(int i=1;i<argc;i++){std::string s=argv[i];if(s=="--capture-restored"){layoutCapture=1;s="--capture-game";}if(s=="--capture-selected"){layoutCapture=2;s="--capture-game";}if(s=="--capture-counter"){layoutCapture=3;s="--capture-game";}if(s=="--capture-flat"){layoutCapture=4;s="--capture-game";}if(s=="--capture-training-zoom"){captureModal=10;captureMode=2;s="--capture-game";}if(s=="--capture-training"){captureMode=2;s="--capture-game";}if(s=="--capture-help"){captureModal=3;s="--capture";}if(s=="--capture-stats"){captureModal=4;s="--capture";}if(s=="--capture-pause"){captureModal=1;s="--capture-game";}if(s=="--capture-settings"){captureModal=2;s="--capture";}if(s=="--capture-emote"){captureModal=6;s="--capture-game";}if(s=="--capture-setup"){captureModal=8;s="--capture";}if(s=="--capture-history"){captureModal=5;captureGame=true;s="--capture-game";}if(s=="--capture-review"){reviewMode=true;s="--capture-over";}if(s=="--capture-over"){capture=true;captureGame=true;captureOver=true;if(i+1<argc)capturePath=argv[++i];}if(s=="--capture"||s=="--capture-game"){capture=true;captureGame=s=="--capture-game";if(i+1<argc)capturePath=argv[++i];}if(s=="--smoke")smoke=true;if(s=="--data-dir"&&i+1<argc)base=std::string(argv[++i])+"/";}
#ifdef __SWITCH__
 romfsInit();base="sdmc:/switch/GuandanSwitch/";mkdir(base.c_str(),0777);fontPath="romfs:/font.otf";padConfigureInput(1,HidNpadStyleSet_NpadStandard);PadState pad;padInitializeDefault(&pad);
#else
 fontPath="romfs/font.otf";
#endif
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER)<0){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}if(TTF_Init()<0){fprintf(stderr,"TTF: %s\n",TTF_GetError());return 1;}
 window=SDL_CreateWindow("掼蛋时光 | Guandan Switch",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
 ren=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!ren)ren=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);if(!ren){fprintf(stderr,"Renderer: %s\n",SDL_GetError());return 1;}
 SDL_RenderSetLogicalSize(ren,W,H);SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
 fonts[20]=TTF_OpenFont(fontPath.c_str(),20);if(!fonts[20]){fprintf(stderr,"Font: %s\n",TTF_GetError());return 1;}
 loadAvatars();loadBg();
 SDL_AudioSpec spec{};spec.freq=48000;spec.format=AUDIO_S16SYS;spec.channels=1;spec.samples=1024;audioDevice=SDL_OpenAudioDevice(nullptr,0,&spec,nullptr,0);if(audioDevice)SDL_PauseAudioDevice(audioDevice,0);
 std::ifstream profile(base+"profile.txt");profile>>total>>wins>>bombs>>skin>>speed>>sound>>recorder;int savedDifficulty=1;if(profile>>savedDifficulty)game.difficulty=std::clamp(savedDifficulty,0,2);int savedTimer=1;if(profile>>savedTimer)timerOn=savedTimer!=0;skin=(skin%3+3)%3;speed=(speed%3+3)%3;{Game old;hasSave=old.load(base+"session.dat");}
 if(captureGame||smoke){start(smoke?2:captureMode,20261016u);if(captureOver)autoplay=true;ensureGroups();if(layoutCapture==1)restoreHand();if(layoutCapture==2)doHint();if(layoutCapture==3)counterOpen=true;if(layoutCapture==4)organized=false;}if(captureModal){if(captureModal==5)for(int i=0;i<14&&!game.over;i++)game.autoStep();if(captureModal==8)focus=3;setModal(captureModal);}Uint32 frame=0; while(!quit){
#ifdef __SWITCH__
 if(!appletMainLoop()){break;}padUpdate(&pad);u64 down=padGetButtonsDown(&pad);u64 held=padGetButtons(&pad);static Uint32 repeat=0;u64 dirs=HidNpadButton_Left|HidNpadButton_Right|HidNpadButton_Up|HidNpadButton_Down;if(down&HidNpadButton_StickLLeft)down|=HidNpadButton_Left;if(down&HidNpadButton_StickLRight)down|=HidNpadButton_Right;if(down&HidNpadButton_StickLUp)down|=HidNpadButton_Up;if(down&HidNpadButton_StickLDown)down|=HidNpadButton_Down;if(held&HidNpadButton_StickLLeft)held|=HidNpadButton_Left;if(held&HidNpadButton_StickLRight)held|=HidNpadButton_Right;if(held&HidNpadButton_StickLUp)held|=HidNpadButton_Up;if(held&HidNpadButton_StickLDown)held|=HidNpadButton_Down;if(down&dirs)repeat=SDL_GetTicks()+320;else if((held&dirs)&&SDL_GetTicks()>repeat){down|=held&dirs;repeat=SDL_GetTicks()+95;}
 struct Binding{u64 bit;Action a;};Binding bindings[]={{HidNpadButton_Left,LEFT},{HidNpadButton_Right,RIGHT},{HidNpadButton_Up,UP},{HidNpadButton_Down,DOWN},{HidNpadButton_A,CONFIRM},{HidNpadButton_B,BACK},{HidNpadButton_X,PLAY},{HidNpadButton_Y,HINT},{HidNpadButton_L,PASS},{HidNpadButton_R,SORT},{HidNpadButton_StickR,RESTORE},{HidNpadButton_StickL,SUIT},{HidNpadButton_ZR,AUTO},{HidNpadButton_ZL,HISTORY},{HidNpadButton_Plus,PAUSE},{HidNpadButton_Minus,HELP}};for(auto b:bindings)if(down&b.bit)act(b.a);
#endif
 SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_QUIT)quit=true;if(e.type==SDL_KEYDOWN){switch(e.key.keysym.sym){case SDLK_LEFT:act(LEFT);break;case SDLK_RIGHT:act(RIGHT);break;case SDLK_UP:act(UP);break;case SDLK_DOWN:act(DOWN);break;case SDLK_SPACE:act(CONFIRM);break;case SDLK_RETURN:act(screen&&!modal&&!game.over?PLAY:CONFIRM);break;case SDLK_ESCAPE:act(modal?BACK:PAUSE);break;case SDLK_BACKSPACE:act(BACK);break;case SDLK_h:act(HINT);break;case SDLK_p:act(PASS);break;case SDLK_r:act(SORT);break;case SDLK_c:act(RESTORE);break;case SDLK_f:act(SUIT);break;case SDLK_g:act(FLUSH_FIND);break;case SDLK_k:act(COUNTER);break;case SDLK_t:act(AUTO);break;case SDLK_F1:act(HELP);break;case SDLK_l:act(HISTORY);break;case SDLK_e:act(EMOTE);break;}}
  if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.which!=SDL_TOUCH_MOUSEID){float mx,my;SDL_RenderWindowToLogical(ren,e.button.x,e.button.y,&mx,&my);click(int(mx),int(my));}
 if(e.type==SDL_MOUSEMOTION&&e.motion.which!=SDL_TOUCH_MOUSEID&&(e.motion.state&SDL_BUTTON_LMASK)){float mx,my;SDL_RenderWindowToLogical(ren,e.motion.x,e.motion.y,&mx,&my);moveHandGesture(int(mx),int(my));}
 if(e.type==SDL_MOUSEBUTTONUP||e.type==SDL_FINGERUP)gestureActive=false;
 if(e.type==SDL_FINGERDOWN||e.type==SDL_FINGERMOTION){int ww,hh;SDL_GetWindowSize(window,&ww,&hh);float mx,my;SDL_RenderWindowToLogical(ren,int(e.tfinger.x*ww),int(e.tfinger.y*hh),&mx,&my);if(e.type==SDL_FINGERDOWN)click(int(mx),int(my));else moveHandGesture(int(mx),int(my));}
 }

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
  if(game.over&&!lastOver){addFx(FX_WIN,0,0,0,0,{});sfx(4);}
  lastOver=game.over;
 }
 if(screen&&!modal&&!game.over&&(game.turn!=0||autoplay)&&now-lastAI>Uint32(capture&&captureOver?0:speed==0?1500:speed==1?850:320)&&(!capture||captureOver)){int p=game.turn;Move m=game.hint(p);if(m.kind==NONE)game.pass(p);else game.play(p,m.cards);if(p==0&&m.explosive())bombs++;if(p==0){clearSelection();cursor=0;}lastAI=now;if(m.explosive())sfx(3);else sfx(1);checkpoint();}
 if(timerOn&&screen&&!modal&&!game.over&&!autoplay&&game.turn==0&&!capture&&now-lastAI>TURN_MS){doAutoMove();sfx(5);message("时间到，已自动出牌");lastAI=now;checkpoint();}
 if(smoke){if(frame==1){act(HELP);act(BACK);act(HINT);act(PLAY);}if(frame==2){act(PAUSE);act(BACK);act(AUTO);}if(frame>20)quit=true;}
 if(reviewMode&&game.over&&!modal&&frame>10)modal=7;
 trimCache();if(screen)table();else{home();homeButtons();}overlay();clearFx(now);
 if(now<toastUntil){box(340,218,600,38,16,{10,22,16,238});text(toast,640,224,17,GOLD,true);}
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
