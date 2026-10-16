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
static SDL_Texture*avTex[4]={nullptr,nullptr,nullptr,nullptr};
static std::map<int,TTF_Font*>fonts;
struct TextTex{SDL_Texture*t;int w,h;};static std::map<std::string,TextTex>cache;
struct Col{Uint8 r,g,b,a=255;};
const Col INK{38,40,34},CREAM{255,248,228},GOLD{246,198,74},GOLD_D{198,150,40},
  MUTED{158,190,176},WHITE{255,255,250},RED{212,64,64},DARK{10,36,26},
  PANEL{20,54,42,244},PANEL2{14,38,31,246},GOLD_BORDER{246,198,74,120};
struct Skin{Col top,bot,card,btnHover;};
static Skin skins[3]={ {{12,82,52},{3,32,20},{240,236,222},{34,98,66}},   // 翡翠绿
                      {{16,48,92},{6,18,46},{238,240,246},{34,76,124}}, // 深海蓝
                      {{58,34,88},{24,14,42},{246,240,250},{84,54,118}}}; // 午夜紫
static std::string base="./",fontPath;static int skin=0,speed=1;static bool sound=true,quit=false,autoplay=false,recorder=true,timerOn=true;
static SDL_AudioDeviceID audioDevice=0;static Game game;static int screen=0,modal=0,focus=0,cursor=0;static bool selected[108]{};
static Uint32 lastAI=0,toastUntil=0;static std::string toast;static int total=0,wins=0,bombs=0,statRound=-1;static bool hasSave=false;
static constexpr Uint32 TURN_MS=18000;
enum FxType{FX_PLAY,FX_BOMB,FX_PASS,FX_EMOTE,FX_WIN};
struct Fx{int type;Uint32 t0;int dur;int sx,sy,dx,dy;std::vector<int> cards;std::string text;};
static std::vector<Fx> fxList;
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
void boxOutline(int x,int y,int w,int h,int r,Col c){box(x,y,w,h,r,c);}

void text(const std::string&s,int x,int y,int size,Col c=CREAM,bool centered=false){if(s.empty())return;std::string key=std::to_string(size)+":"+std::to_string(c.r)+","+std::to_string(c.g)+","+std::to_string(c.b)+":"+s;
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
void start(int mode){int diff=game.difficulty;game.newMatch(mode,mode==3?20261016u:uint32_t(std::time(nullptr)));game.difficulty=diff;screen=1;modal=0;autoplay=false;cursor=0;clearSelection();statRound=-1;lastAI=SDL_GetTicks();fxList.clear();lastShown={};lastPassed={};checkpoint();}

// ---------------- cards ----------------
void cardFace(int id,int x,int y,int w,int h,bool sel,bool focused,bool up=true){
 if(!up){box(x+2,y+5,w,h,7,{0,0,0,55});Col c=skins[skin].card;box(x,y,w,h,7,{Uint8(c.r*0.72),Uint8(c.g*0.72),Uint8(c.b*0.72)});box(x+4,y+4,w-8,h-8,5,{255,255,255,26});int cx=x+w/2,cy=y+h/2;ring(cx,cy,std::min(w,h)/4,3,GOLD_BORDER);line(x+8,y+h-10,x+w-10,y+8,{255,255,255,22});line(x+8,y+8,x+w-10,y+h-10,{255,255,255,22});return;}
 if(sel){y-=18;}box(x+2,y+5,w,h,7,{0,0,0,60});if(sel||focused)box(x-3,y-3,w+6,h+6,9,focused?GOLD:Col{80,200,160});box(x,y,w,h,8,sel?Col{255,246,214}:skins[skin].card);
 line(x+7,y+7,x+w-8,y+h-8,{200,192,170,70});line(x+7,y+h-8,x+w-8,y+7,{200,192,170,45});
 int r=Card{id}.rank(),s=Card{id}.suit();Col c=(s==1||s==2||r==17)?RED:INK;std::string suits[]={"♠","♥","♦","♣",""};
 int rs=w<46?12:w<62?15:w<82?19:24,ss=w<46?11:w<62?14:w<82?17:22;
 if(r>=16){int fs=w<62?16:w<82?20:26;text(r==17?"大":"小",x+4,y+3,fs,c);text("王",x+4,y+3+(w<62?18:24),fs,c);if(w>=62)text("★",x+w-28,y+h-42,24,c);if(w>=62)text("★",x+w-26,y+6,12,c);}
 else{text(rankName(r),x+4,y+2,rs,c);text(suits[s],x+4,y+2+(rs>15?rs-4:rs),ss,c);if(w>=62){text(rankName(r),x+w-4-int(rs*0.6)-8,y+h-rs-4,rs,c);text(suits[s],x+w-4-int(ss*0.6)-8,y+h-ss-4,ss,c);}else{text(suits[s],x+w-26,y+h-38,ss,c);}}
 if(wild(id,game.level)){box(x+w-21,y+4,17,21,4,GOLD);text("配",x+w-19,y+5,11,INK);}
}
// ---------------- avatars ----------------
void loadAvatars(){
 const char* names[]={"avatar0.png","avatar1.png","avatar2.png","avatar3.png"};
 for(int i=0;i<4;i++){
  std::string p=fontPath.size()>6&&fontPath[5]==':'?std::string("romfs:/")+names[i]:std::string("romfs/")+names[i];
  int w=0,h=0,n=0;unsigned char*px=stbi_load(p.c_str(),&w,&h,&n,4);
  if(!px){fprintf(stderr,"Avatar %s failed: %s\n",names[i],stbi_failure_reason());continue;}
  SDL_Surface*sf=SDL_CreateRGBSurfaceWithFormatFrom(px,w,h,32,w*4,SDL_PIXELFORMAT_RGBA8888);
  if(sf){avTex[i]=SDL_CreateTextureFromSurface(ren,sf);SDL_SetTextureBlendMode(avTex[i],SDL_BLENDMODE_BLEND);SDL_FreeSurface(sf);}
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
 else text(playerName(p),x,y+r+12,17,CREAM,true);
}
// ---------------- FX ----------------
void addFx(int type,int sx,int sy,int dx,int dy,std::vector<int> cards,std::string t=""){Fx f{type,SDL_GetTicks(),type==FX_PLAY?320:(type==FX_BOMB?620:(type==FX_EMOTE?2800:(type==FX_WIN?2200:800))),sx,sy,dx,dy,std::move(cards),t};fxList.push_back(f);if(fxList.size()>14)fxList.erase(fxList.begin());}
void drawFx(Uint32 now){
 for(auto&f:fxList){float t=(now-f.t0)/float(f.dur);if(t>=1)continue;t=std::max(0.0f,t);
  if(f.type==FX_PLAY){float e=1.0f-std::pow(1.0f-t,3.0f);int n=f.cards.size();for(int i=0;i<n;i++){float ti=std::max(0.0f,std::min(1.0f,t*1.4f-i*0.04f));float ei=1.0f-std::pow(1.0f-ti,3.0f);int x=f.sx+(f.dx-f.sx)*ei,y=f.sy+(f.dy-f.sy)*ei;cardFace(f.cards[i],x,y,48,69,false,false);}}
  if(f.type==FX_BOMB){int cx=f.dx,cy=f.dy;float r=24+130*t;ring(cx,cy,int(r),int(3*(1-t)+1),{GOLD.r,GOLD.g,GOLD.b,Uint8(230*(1-t))});ring(cx,cy,int(r*0.62),int(3*(1-t)+1),{255,190,80,Uint8(200*(1-t))});
   for(int i=0;i<16;i++){float a=i*0.3927f;float rr=50+150*t;int px=cx+int(rr*std::cos(a)),py=cy+int(rr*std::sin(a));rect(px-3,py-3,6,6,{GOLD.r,GOLD.g,GOLD.b,Uint8(255*(1-t))});}
   text("炸 弹",cx,cy-int(30*(1-t))-22,38,{255,255,255,Uint8(255*(1-t))},true);}
  if(f.type==FX_PASS){int w=84,h=30;float a=(1-t);box(f.dx-w/2,f.dy-h/2,w,h,10,{10,30,24,Uint8(200*a)});text("不 出",f.dx,f.dy-8,17,{MUTED.r,MUTED.g,MUTED.b,Uint8(255*a)},true);}
  if(f.type==FX_EMOTE){int w=200+8*(f.text.size()>6?f.text.size()-6:0),h=40;float a=t<0.85?1.0f:1.0f-(t-0.85f)/0.15f;int bx=f.dx-w/2,by=f.dy-h/2;box(bx,by,w,h,12,{16,40,32,Uint8(235*a)});line(bx+w/2-8,by+h,bx+w/2+8,by+h,{16,40,32,Uint8(235*a)});text(f.text,f.dx,by+7,18,{GOLD.r,GOLD.g,GOLD.b,Uint8(255*a)},true);}
  if(f.type==FX_WIN){for(int i=0;i<46;i++){float p=std::fmod(t+i*0.013f,1.0f);int px=int(i*27.8f)+int(40*std::sin(t*6+i))*3,py=int(p*300)-40;Col cc=(i%3==0)?GOLD:(i%3==1)?Col{255,120,120}:Col{120,200,255};rect(px,py,5,9,{cc.r,cc.g,cc.b,Uint8(255*p)});}}
 }}
void clearFx(Uint32 now){fxList.erase(std::remove_if(fxList.begin(),fxList.end(),[&](Fx&f){return now-f.t0>Uint32(f.dur);}),fxList.end());}
// ---------------- screens ----------------
void bgHome(){
 vgrad(0,0,W,H,skins[skin].top,skins[skin].bot);
 for(int i=0;i<30;i++){int x=(i*137+37)%W,y=(i*73+21)%H;circle(x,y,1+((i*7)%2),{210,188,130,50});}
 int mx=1130,my=180;if(avTex[2]){SDL_Rect dst{mx-64,my-64,128,128};SDL_RenderCopy(ren,avTex[2],nullptr,&dst);}else face(mx,my,64,2);
 for(int i=0;i<3;i++)cardFace((i*2+10)*4+i,mx-30+i*20,my+70,40,58,false,false);
 for(int i=0;i<6;i++){int x=940+i*42;line(x,52,x+150,398,{123,157,136,20});}
 text("GUANDAN  /  OFFLINE CLUB",66,44,16,GOLD);
}
void bgTable(){
 vgrad(0,0,W,H,skins[skin].top,skins[skin].bot);
 for(int r=420;r>40;r-=38){circle(W/2,290,r,{255,255,255,Uint8(4)});}
 for(int i=0;i<24;i++){int x=(i*139+37)%W,y=(i*73+21)%H;circle(x,y,1,{210,188,130,40});}
 boxOutline(10,74,W-20,H-96,18,{0,0,0,50});
}
void button(int x,int y,int w,const std::string&label,bool active=false,bool enabled=true){box(x,y,w,48,14,active?GOLD:Col{255,255,255,enabled?Uint8(18):Uint8(7)});text(label,x+w/2,y+9,20,active?INK:enabled?CREAM:MUTED,true);}
void homeButtons(){button(74,605,220,hasSave?"继续上次对局":"暂无进行中对局",focus==4,hasSave);button(310,605,180,"本地战绩",focus==5);button(506,605,180,"偏好设置",focus==6);button(702,605,180,"玩法说明",focus==7);}
void home(){
 bgHome();
 text("掼蛋时光",86,92,72,{GOLD.r,GOLD.g,GOLD.b,255});
 text("掼蛋时光",89,95,72,{10,30,22,160});
 text("好牌，好搭档。随时开一桌。",90,196,25,MUTED);
 box(90,244,432,36,12,{255,255,255,14});text("双副牌  ·  离线 AI  ·  手柄 / 触屏",107,249,17,GOLD);
 const char*titles[]={"快速开局","经典升级","明牌练习","定局挑战"};const char*desc[]={"一局尽兴  /  随机发牌","从 2 打到 A  /  自动进贡还贡","看清四家  /  拆牌与配合练习","固定种子  /  重试同一副牌"};const char*tag[]={"QUICK PLAY","CLASSIC MATCH","OPEN HANDS","SEED CHALLENGE"};
 for(int i=0;i<4;i++){int x=74+(i%2)*566,y=306+(i/2)*137;bool f=focus==i;if(f)box(x-3,y-3,546,122,22,GOLD);box(x,y,540,116,20,f?skins[skin].btnHover:Col{16,52,38,235});text("0"+std::to_string(i+1),x+24,y+15,17,GOLD);text(tag[i],x+66,y+16,14,MUTED);text(titles[i],x+24,y+38,29,CREAM);text(desc[i],x+24,y+80,17,MUTED);text("›",x+492,y+30,38,GOLD);}
}
void showMove(int p,int cx,int y){auto&m=game.shown[p];if(game.passed[p]){text("不 出",cx,y+16,24,MUTED,true);return;}if(m.kind==NONE)return;int n=m.cards.size(),step=std::min(34,230/std::max(1,n));int x=cx-(step*(n-1)+48)/2;for(int i=0;i<n;i++)cardFace(m.cards[i],x+step*i,y,48,69,false,false);text(kindName(m.kind),cx,y+74,14,GOLD,true);}
void counterPanel(){
 if(!recorder||game.over)return;
 box(16,74,306,124,12,{0,0,0,62});text("记牌器",28,80,15,GOLD);
 const char*lab[]={"2","3","4","5","6","7","8","9","10","J","Q","K","A","小王","大王"};int rr[]={2,3,4,5,6,7,8,9,10,11,12,13,14,16,17};
 for(int i=0;i<15;i++){int col=i%8,row=i/8;int x=26+col*37,y=102+row*37;box(x,y,34,32,6,{255,255,255,10});text(lab[i],x+4,y+1,14,CREAM);int n=game.remaining(rr[i]);text(std::to_string(n),x+22,y+12,17,n==0?RED:GOLD);}
}
void table(){
 bgTable();
 rect(0,0,W,66,{0,0,0,62});
 text("掼蛋时光",26,14,24,GOLD);
 const char*modes[]={"快速开局","经典升级","明牌练习","定局挑战"};text(modes[game.mode],176,21,17,CREAM);text("第 "+std::to_string(game.round)+" 局",330,21,17,MUTED);
 box(566,11,160,40,13,{255,255,255,14});text("本局打 "+rankName(game.level),646,17,26,GOLD,true);
 text("我方 "+rankName(game.levels[0])+"   :   对方 "+rankName(game.levels[1]),880,22,18,CREAM);text("+ 暂停",1174,22,15,MUTED);
 counterPanel();
 Uint32 now=SDL_GetTicks();
 auto fin=[&](int p){return std::find(game.finish.begin(),game.finish.end(),p)!=game.finish.end();};
 drawAvatar(2,640,game.mode==2?106:118,26,game.turn==2&&!game.over,game.passed[2],fin(2));
 text(std::to_string(game.hands[2].size())+" 张",706,104,18,GOLD);
 drawAvatar(1,96,game.mode==2?372:300,26,game.turn==1&&!game.over,game.passed[1],fin(1));
 text(std::to_string(game.hands[1].size())+" 张",96,344,18,GOLD,true);
 drawAvatar(3,1184,game.mode==2?372:300,26,game.turn==3&&!game.over,game.passed[3],fin(3));
 text(std::to_string(game.hands[3].size())+" 张",1184,344,18,GOLD,true);
 if(game.mode==2){for(int p=1;p<4;p++){int x=p==1?26:p==3?1066:370,y=p==2?168:96;for(int i=0;i<int(game.hands[p].size());i++){int cols=p==2?13:6,col=i%cols,row=i/cols;cardFace(game.hands[p][i],x+col*28,y+row*50,35,49,false,false);}}}
 else for(int p:{1,3}){int x=p==1?60:1160;for(int i=0;i<5;i++){int xx=x+i*5;box(xx,206+i*3,34,52,5,{40,80,64});line(xx+8,216+i*3,xx+26,248+i*3,GOLD_BORDER);}}
 showMove(2,640,196);showMove(1,250,288);showMove(3,1030,288);showMove(0,640,330);
 if(game.table.kind==NONE)text("自 由 领 出",640,262,22,{207,221,204,150},true);
 text("掼蛋时光",640,296,26,{246,198,74,18},true);
 if(!game.over){std::string st=game.turn==0?(autoplay?"正在托管 · 自动出牌":"轮到你出牌"):playerName(game.turn)+" 正在思考";text(st,640,448,22,game.turn==0?GOLD:MUTED,true);if(game.turn!=0){for(int i=0;i<3;i++)circle(640-30+i*30,470,3,i==int(now/350)%3?GOLD:Col{70,108,99});}}
 drawAvatar(0,74,452,24,game.turn==0&&!game.over&&!autoplay,game.passed[0],fin(0));
 text("你 · "+std::to_string(game.hands[0].size())+" 张",104,442,17,CREAM);
 if(timerOn&&screen&&!modal&&!game.over&&!autoplay&&game.turn==0){float el=float(now-lastAI)/float(TURN_MS);if(el<1){arc(74,452,29,4,-1.57f,-1.57f+6.2832f*el,GOLD);text(std::to_string(std::max(0,int((TURN_MS-(now-lastAI))/1000))),74,443,17,GOLD,true);}}
 int n=game.hands[0].size();int step=std::min(58,1138/std::max(1,n-1)),x=(W-(step*std::max(0,n-1)+66))/2;
 for(int i=0;i<n;i++)cardFace(game.hands[0][i],x+i*step,513,66,104,selected[game.hands[0][i]],i==cursor&&game.turn==0&&!autoplay&&!game.over);
 if(game.mode==2)text("明牌练习 · 其余三家手牌可见",640,502,15,{207,221,204,130},true);
 drawFx(now);
 auto btn=[&](int bx,int by,int bw,const std::string&l,bool en,bool act){box(bx,by,bw,48,14,act?GOLD:en?Col{255,255,255,20}:Col{255,255,255,7});text(l,bx+bw/2,by+9,20,act?INK:en?CREAM:MUTED,true);};
 bool my=game.turn==0&&!game.over;
 btn(210,630,140,"L 不出",game.table.kind!=NONE&&my,false);btn(360,630,140,"Y 提示",my,false);btn(510,630,180,"X 出牌",my,true);btn(700,630,120,"R 理牌",my,false);btn(830,630,150,"ZR 托管",true,autoplay);btn(990,630,120,"表情",true,false);
 text("← → 移动   A 选牌   B 清选   R 理牌",24,693,14,MUTED);text(autoplay?"ZR 取消托管":"ZR 托管",1020,641,16,autoplay?GOLD:MUTED);text("− 规则   ZL 记录",1096,693,14,MUTED);
 recordResult();
 if(game.over){rect(0,66,W,H-66,{2,10,16,180});box(380,150,520,340,24,{24,58,46});text(game.finish[0]%2==0?"配合漂亮！":"再来一局",640,180,42,GOLD,true);text(game.notice,640,244,20,CREAM,true);
  int teamUp=0;if(game.mode==1){int tf=game.finish[0]%2;int place=0;for(int i=1;i<4;i++)if(game.finish[i]%2==tf)place=i;teamUp=4-place;}
  for(int i=0;i<4;i++)text(std::to_string(i+1)+"  "+playerName(game.finish[i])+(game.finish[i]%2==0?"  ★":""),440+(i%2)*215,285+(i/2)*38,19,game.finish[i]%2==0?GOLD:MUTED);
  text(game.champion?"成功过 A · 新赛季开启！":"本局升 "+std::to_string(teamUp)+" 级",640,396,18,{207,221,204},true);
  text(game.champion?"A 开始新赛季    B 返回大厅    ZL 复盘":"A 下一局    B 返回大厅    ZL 复盘",640,440,21,CREAM,true);}
 if(now<toastUntil){box(390,112,500,50,14,{10,22,16,238});text(toast,640,124,20,GOLD,true);}
}
void overlay(){
 if(!modal)return;rect(0,0,W,H,{2,10,16,205});box(210,84,860,548,24,{20,52,42});
 if(modal==1){text("暂停片刻",254,116,34,GOLD);const char*items[]={"继续对局","保存并返回大厅","偏好设置","玩法说明"};for(int i=0;i<4;i++)button(340,196+i*80,600,items[i],focus==i);}
 if(modal==2){text("偏好设置",254,116,34,GOLD);std::string labels[]={"AI 策略："+std::string(game.difficulty==0?"休闲":game.difficulty==1?"标准":"进阶"),"牌桌主题："+std::string(skin==0?"翡翠绿":skin==1?"深海蓝":"午夜紫"),"AI 出牌速度："+std::string(speed==0?"舒缓":speed==1?"标准":"快速"),"操作音效："+std::string(sound?"开启":"关闭"),"记牌器："+std::string(recorder?"开启":"关闭"),"出牌倒计时："+std::string(timerOn?"开启":"关闭")};for(int i=0;i<6;i++)button(315,180+i*70,650,labels[i],focus==i);text("A 切换选项   B 返回",640,574,17,MUTED,true);}
 if(modal==3){text("玩法与操作",254,110,32,GOLD);const char*rules[]={"四人两副牌，对家组队。红桃级牌（逢人配）可代替非王牌。","牌型：单张 / 对子 / 三张 / 三带二 / 五张顺子 / 三连对 / 钢板。","炸弹：四至五张 < 同花顺 < 六张及以上；四王最大。","顺子中级牌回归原点数；A 可在开头或末尾，不循环。","头游一方按队友名次升 3 / 2 / 1 级；打 A 且队友非末游获胜。","升级赛自动进贡并还最低非配牌；双大王可抗贡。","← → 移动光标，A 选牌，X 出牌，B 清选，Y 提示，R 理牌。","L 不出，ZR 托管，ZL 出牌记录，+ 暂停，− 本页。","掌机支持点按选牌和按钮；倒计时结束自动出牌。","进阶 AI 为规则启发式策略；定局挑战可重试相同发牌。"};for(int i=0;i<10;i++)text(rules[i],254,168+i*37,18,i<6?CREAM:MUTED);}
 if(modal==4){text("本地战绩",254,116,34,GOLD);std::string v[]={std::to_string(total),std::to_string(wins),total?std::to_string(wins*100/total)+"%":"—",std::to_string(bombs)};const char*l[]={"完成对局","我方获胜","本地胜率","打出炸弹"};for(int i=0;i<4;i++){int x=330+(i%2)*340,y=200+(i/2)*150;text(v[i],x,y,46,CREAM);text(l[i],x,y+63,20,MUTED);}text("B 返回   ·   数据仅保存在本机",640,570,17,MUTED,true);}
 if(modal==5){text("出牌记录",254,116,34,GOLD);int start=std::max(0,int(game.log.size())-10);for(int i=start;i<int(game.log.size());i++)text(game.log[i],254,180+(i-start)*36,19,i==int(game.log.size())-1?GOLD:CREAM);text("B 返回",940,568,17,MUTED);}
 if(modal==6){text("快捷表情",254,116,34,GOLD);const char*em[]={"打得漂亮！","不要走，决战到天亮","快点吧，等得花儿都谢了","你太厉害了！","稳住，我们能赢！","这牌，绝了！","别急，容我想想","配合默契，双喜临门！"};for(int i=0;i<8;i++){int col=i%2,row=i/2;button(330+col*330,196+row*84,300,em[i],focus==i);}text("方向键选择   A 发送   B 返回",640,562,17,MUTED,true);}
 if(modal==7){text("复盘报告",254,116,34,GOLD);int n=int(game.powerLog.size());int bombs=0,maxPow=0;for(int i=0;i<n;i++){if(game.bombLog[i])bombs++;maxPow=std::max(maxPow,game.powerLog[i]);}
  int cx=252,cy=168,cw=776,ch=206;box(cx,cy,cw,ch,10,{0,0,0,78});text("本局共 "+std::to_string(n)+" 手 · 炸弹 "+std::to_string(bombs)+" 次 · 最大牌力 "+std::to_string(maxPow),278,180,18,GOLD);
  const int grid[4]={100,75,50,25};for(int g=0;g<4;g++){int gy=cy+ch-26-(100-grid[g])*(ch-70)/100;hline(cx+10,gy,cx+cw-20,{255,255,255,26});text(std::to_string(grid[g]),cx-22,gy-7,14,MUTED);}
  int px0=cx+16,py0=cy+ch-26,px1=cx+cw-16,py1=cy+26;
  if(n>1){int lx=px0,ly=py0;for(int i=0;i<n;i++){int x=px0+(px1-px0)*i/(n-1);int sc=maxPow?std::min(100,int(double(game.powerLog[i])*100.0/double(maxPow))):0;int y=py0-sc*(py0-py1)/100;line(lx,ly,x,y,GOLD);circle(x,y,2,GOLD);lx=x;ly=y;}}
  text("评分曲线 · 第 1 → 第 "+std::to_string(std::max(1,n))+" 手",px0,py0+8,14,MUTED);
  int qx=cx,qy=py0+38,qw=cw,qh=H-qy-170;box(qx,qy,qw,qh,10,{0,0,0,78});text("局势图 · 四家手牌余量",278,qy+8,18,GOLD);
  if(!game.snapshots.empty()){Col cols[4]={GOLD,Col{120,200,160},{150,120,220},{190,150,120}};const char*ps[4]={"你","队友","南风","青竹"};for(int p=0;p<4;p++){int lx=qx+16,ly=qy+qh-18-int(game.snapshots[0][p])*(qh-52)/27;for(int i=1;i<int(game.snapshots.size());i++){int x=qx+16+(qw-32)*i/(int(game.snapshots.size())-1);int y=qy+qh-18-int(game.snapshots[i][p])*(qh-52)/27;line(lx,ly,x,y,cols[p]);lx=x;ly=y;}text(ps[p],qx+qw-40,qy+qh-14-32*p,14,cols[p]);}}
  text("B 返回",940,568,17,MUTED);}
}
// ---------------- actions ----------------
void doPlay(){if(game.turn!=0||game.over||autoplay)return;std::vector<int>cards;for(int id:game.hands[0])if(selected[id])cards.push_back(id);if(cards.empty()){message("先选中要打出的牌");return;}Move m=identify(cards,game.level,game.table);if(game.play(0,cards)){if(m.explosive())bombs++;sfx(m.explosive()?3:1);clearSelection();cursor=std::min(cursor,int(game.hands[0].size())-1);lastAI=SDL_GetTicks();checkpoint();}else message("牌型不合法，或无法压过上一手");}
void doHint(){if(game.turn!=0||game.over)return;clearSelection();Move m=game.hint(0);if(m.kind==NONE){message(game.last>=0&&game.last%2==0?"建议让队友继续领牌，可按 L 不出":"建议不出，可按 L");return;}for(int id:m.cards)selected[id]=true;message(std::string("建议：")+kindName(m.kind)+"，按 X 打出");}
void doAutoMove(){Move m=game.hint(0);if(m.kind==NONE){if(game.table.kind==NONE){auto ms=moves(game.hands[0],game.level);if(!ms.empty())game.play(0,ms.front().cards);}else game.pass(0);}else game.play(0,m.cards);clearSelection();cursor=0;}
enum Action{LEFT,RIGHT,UP,DOWN,CONFIRM,BACK,PLAY,HINT,PASS,SORT,AUTO,PAUSE,HELP,HISTORY,EMOTE};
void act(Action a){
 if(modal){
  if(a==BACK||a==PAUSE){modal=0;focus=0;sfx(2);return;}
  int max=modal==1?4:modal==2?6:modal==6?8:1;
  if(modal==6){if(a==LEFT||a==UP)focus=(focus+7)%8;if(a==RIGHT||a==DOWN)focus=(focus+1)%8;}
  else{if(a==UP)focus=(focus+max-1)%max;if(a==DOWN)focus=(focus+1)%max;}
  if(a==CONFIRM){
   if(modal==1){if(focus==0)modal=0;else if(focus==1){checkpoint();screen=0;modal=0;focus=4;}else if(focus==2){modal=2;focus=0;}else{modal=3;focus=0;}}
   else if(modal==2){if(focus==0)game.difficulty=(game.difficulty+1)%3;if(focus==1)skin=(skin+1)%3;if(focus==2)speed=(speed+1)%3;if(focus==3)sound=!sound;if(focus==4)recorder=!recorder;if(focus==5)timerOn=!timerOn;saveStats();if(screen)checkpoint();sfx(0);}
   else if(modal==6){const char*em[]={"打得漂亮！","不要走，决战到天亮","快点吧，等得花儿都谢了","你太厉害了！","稳住，我们能赢！","这牌，绝了！","别急，容我想想","配合默契，双喜临门！"};addFx(FX_EMOTE,0,0,300,540,{},em[focus]);sfx(0);modal=0;focus=0;}
  }
  return;
 }
 if(!screen){if(a==LEFT)focus=focus<4?(focus^1):4+(focus-4+3)%4;if(a==RIGHT)focus=focus<4?(focus^1):4+(focus-4+1)%4;if(a==UP)focus=focus<2?4+focus:focus<4?focus-2:2+(focus-4)/2;if(a==DOWN)focus=focus<2?focus+2:focus<4?4+(focus-2)*2:(focus-4)/2;if(a==PAUSE){quit=true;return;}if(a==HELP){modal=3;focus=0;return;}
  if(a==CONFIRM){sfx(0);if(focus<4)start(focus);else if(focus==4){if(hasSave){Game g;if(g.load(base+"session.dat")){game=g;screen=1;autoplay=false;clearSelection();cursor=0;lastAI=SDL_GetTicks();statRound=game.over?game.round:-1;fxList.clear();lastShown={};lastPassed={};}else message("存档无法读取");}}else{modal=focus==5?4:focus==6?2:3;focus=0;}}return;}
 if(a==PAUSE){checkpoint();modal=1;focus=0;sfx(0);return;}if(a==HELP){modal=3;focus=0;return;}if(a==HISTORY){modal=game.over?7:5;focus=0;return;}if(a==EMOTE){if(game.over)return;modal=6;focus=0;return;}
 if(game.over){if(a==CONFIRM){if(game.champion)start(game.mode);else{game.deal();statRound=-1;clearSelection();cursor=0;checkpoint();lastAI=SDL_GetTicks();fxList.clear();lastShown={};lastPassed={};}}if(a==BACK){screen=0;focus=0;}return;}
 if(a==AUTO){autoplay=!autoplay;clearSelection();message(autoplay?"托管已开启，按 ZR 取消":"已取消托管");sfx(0);lastAI=SDL_GetTicks();return;}
 if(game.turn!=0||autoplay){return;}int n=game.hands[0].size();if(a==LEFT&&n)cursor=(cursor+n-1)%n;if(a==RIGHT&&n)cursor=(cursor+1)%n;if(a==CONFIRM&&n){selected[game.hands[0][cursor]]=!selected[game.hands[0][cursor]];sfx(0);}if(a==BACK)clearSelection();if(a==HINT)doHint();if(a==PLAY)doPlay();if(a==PASS){if(game.pass(0)){clearSelection();lastAI=SDL_GetTicks();sfx(2);checkpoint();}else message("自由领出时必须出牌");}if(a==SORT){game.sort();clearSelection();cursor=0;message("已按点数理牌，级牌和王牌在右侧");sfx(0);}
}
void click(int x,int y){if(modal){if(modal==1&&x>340&&x<940&&y>=196&&y<516){focus=std::min(3,(y-196)/80);act(CONFIRM);}else if(modal==2&&x>315&&x<965&&y>=180&&y<600){focus=std::min(5,(y-180)/70);act(CONFIRM);}else if(modal==6&&y>=196&&y<532){int col=x<630?0:1;int row=(y-196)/84;if(row<4){focus=row*2+col;act(CONFIRM);}}else act(BACK);return;}
 if(!screen){for(int i=0;i<4;i++){int xx=74+(i%2)*566,yy=306+(i/2)*137;if(x>=xx&&x<xx+540&&y>=yy&&y<yy+116){focus=i;act(CONFIRM);return;}}if(y>605&&y<655){if(x>=74&&x<294)focus=4;else if(x>=310&&x<490)focus=5;else if(x>=506&&x<686)focus=6;else if(x>=702&&x<882)focus=7;else return;act(CONFIRM);}return;}
 if(game.over){act(CONFIRM);return;}if(x>1140&&y<66){act(PAUSE);return;}
 struct BtnGeom{int x,y,w;}bs[6]={{210,630,140},{360,630,140},{510,630,180},{700,630,120},{830,630,150},{990,630,120}};Action acts[6]={PASS,HINT,PLAY,SORT,AUTO,EMOTE};
 for(int i=0;i<6;i++)if(x>=bs[i].x&&x<bs[i].x+bs[i].w&&y>=bs[i].y&&y<bs[i].y+48){act(acts[i]);return;}
 if(game.turn==0&&!autoplay){int n=game.hands[0].size(),step=std::min(58,1138/std::max(1,n-1)),xx=(W-(step*std::max(0,n-1)+66))/2;for(int i=n-1;i>=0;i--){int yy=513-(selected[game.hands[0][i]]?18:0);if(x>=xx+i*step&&x<xx+i*step+66&&y>=yy&&y<yy+104){cursor=i;act(CONFIRM);return;}}}
}
int main(int argc,char**argv){bool capture=false,captureGame=false,captureOver=false,smoke=false,reviewMode=false;int captureMode=0,captureModal=0;std::string capturePath="preview.bmp";
 for(int i=1;i<argc;i++){std::string s=argv[i];if(s=="--capture-training"){captureMode=2;s="--capture-game";}if(s=="--capture-help"){captureModal=3;s="--capture";}if(s=="--capture-settings"){captureModal=2;s="--capture";}if(s=="--capture-emote"){captureModal=6;s="--capture";}if(s=="--capture-review"){reviewMode=true;s="--capture-over";}if(s=="--capture-over"){capture=true;captureGame=true;captureOver=true;if(i+1<argc)capturePath=argv[++i];}if(s=="--capture"||s=="--capture-game"){capture=true;captureGame=s=="--capture-game";if(i+1<argc)capturePath=argv[++i];}if(s=="--smoke")smoke=true;}
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
 loadAvatars();
 SDL_AudioSpec spec{};spec.freq=48000;spec.format=AUDIO_S16SYS;spec.channels=1;spec.samples=1024;audioDevice=SDL_OpenAudioDevice(nullptr,0,&spec,nullptr,0);if(audioDevice)SDL_PauseAudioDevice(audioDevice,0);
 std::ifstream profile(base+"profile.txt");profile>>total>>wins>>bombs>>skin>>speed>>sound>>recorder;int savedDifficulty=1;if(profile>>savedDifficulty)game.difficulty=std::clamp(savedDifficulty,0,2);int savedTimer=1;if(profile>>savedTimer)timerOn=savedTimer!=0;skin=(skin%3+3)%3;speed=(speed%3+3)%3;{Game old;hasSave=old.load(base+"session.dat");}
 if(captureGame||smoke){start(smoke?2:captureMode);if(captureOver)autoplay=true;}if(captureModal)modal=captureModal;Uint32 frame=0; while(!quit){
#ifdef __SWITCH__
 if(!appletMainLoop()){break;}padUpdate(&pad);u64 down=padGetButtonsDown(&pad);u64 held=padGetButtons(&pad);static Uint32 repeat=0;u64 dirs=HidNpadButton_Left|HidNpadButton_Right|HidNpadButton_Up|HidNpadButton_Down;if(down&HidNpadButton_StickLLeft)down|=HidNpadButton_Left;if(down&HidNpadButton_StickLRight)down|=HidNpadButton_Right;if(down&HidNpadButton_StickLUp)down|=HidNpadButton_Up;if(down&HidNpadButton_StickLDown)down|=HidNpadButton_Down;if(held&HidNpadButton_StickLLeft)held|=HidNpadButton_Left;if(held&HidNpadButton_StickLRight)held|=HidNpadButton_Right;if(held&HidNpadButton_StickLUp)held|=HidNpadButton_Up;if(held&HidNpadButton_StickLDown)held|=HidNpadButton_Down;if(down&dirs)repeat=SDL_GetTicks()+320;else if((held&dirs)&&SDL_GetTicks()>repeat){down|=held&dirs;repeat=SDL_GetTicks()+95;}
 struct Binding{u64 bit;Action a;};Binding bindings[]={{HidNpadButton_Left,LEFT},{HidNpadButton_Right,RIGHT},{HidNpadButton_Up,UP},{HidNpadButton_Down,DOWN},{HidNpadButton_A,CONFIRM},{HidNpadButton_B,BACK},{HidNpadButton_X,PLAY},{HidNpadButton_Y,HINT},{HidNpadButton_L,PASS},{HidNpadButton_R,SORT},{HidNpadButton_ZR,AUTO},{HidNpadButton_ZL,HISTORY},{HidNpadButton_Plus,PAUSE},{HidNpadButton_Minus,HELP}};for(auto b:bindings)if(down&b.bit)act(b.a);
#endif
 SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_QUIT)quit=true;if(e.type==SDL_KEYDOWN){switch(e.key.keysym.sym){case SDLK_LEFT:act(LEFT);break;case SDLK_RIGHT:act(RIGHT);break;case SDLK_UP:act(UP);break;case SDLK_DOWN:act(DOWN);break;case SDLK_SPACE:act(CONFIRM);break;case SDLK_RETURN:act(screen&&!game.over?PLAY:CONFIRM);break;case SDLK_ESCAPE:act(modal?BACK:PAUSE);break;case SDLK_BACKSPACE:act(BACK);break;case SDLK_h:act(HINT);break;case SDLK_p:act(PASS);break;case SDLK_r:act(SORT);break;case SDLK_t:act(AUTO);break;case SDLK_F1:act(HELP);break;case SDLK_l:act(HISTORY);break;case SDLK_e:act(EMOTE);break;}}
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
  if(game.over&&!lastOver){addFx(FX_WIN,0,0,0,0,{});sfx(4);}
  lastOver=game.over;
 }
 if(screen&&!modal&&!game.over&&(game.turn!=0||autoplay)&&now-lastAI>Uint32(speed==0?1500:speed==1?850:320)&&(!capture||captureOver)){int p=game.turn;Move m=game.hint(p);if(m.kind==NONE)game.pass(p);else game.play(p,m.cards);if(p==0&&m.explosive())bombs++;clearSelection();cursor=0;lastAI=now;if(m.explosive())sfx(3);else sfx(1);checkpoint();}
 if(timerOn&&screen&&!modal&&!game.over&&!autoplay&&game.turn==0&&!capture&&now-lastAI>TURN_MS){doAutoMove();sfx(5);message("时间到，已自动出牌");lastAI=now;checkpoint();}
 if(smoke){if(frame==1){act(HELP);act(BACK);act(HINT);act(PLAY);}if(frame==2){act(PAUSE);act(BACK);act(AUTO);}if(frame>20)quit=true;}
 if(reviewMode&&game.over&&!modal&&frame>10)modal=7;
 trimCache();if(screen)table();else{home();homeButtons();}overlay();clearFx(now);
 if(now<toastUntil){box(390,112,500,50,14,{10,22,16,238});text(toast,640,124,20,GOLD,true);}
 if((capture&&!captureOver&&frame==2)||(captureOver&&game.over&&frame>10&&!reviewMode)||(reviewMode&&game.over&&modal==7&&frame>18)){SDL_Surface*sf=SDL_CreateRGBSurfaceWithFormat(0,W,H,32,SDL_PIXELFORMAT_ARGB8888);SDL_RenderReadPixels(ren,nullptr,sf->format->format,sf->pixels,sf->pitch);SDL_SaveBMP(sf,capturePath.c_str());SDL_FreeSurface(sf);quit=true;}
 SDL_RenderPresent(ren);frame++;SDL_Delay(16);
 }
 if(screen){checkpoint();}saveStats();for(auto&kv:cache)SDL_DestroyTexture(kv.second.t);for(auto&kv:fonts)TTF_CloseFont(kv.second);if(audioDevice)SDL_CloseAudioDevice(audioDevice);SDL_DestroyRenderer(ren);SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();
#ifdef __SWITCH__
 romfsExit();
#endif
 return 0;
}
