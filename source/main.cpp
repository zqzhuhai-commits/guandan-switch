#include "engine.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <ctime>
#include <map>
#include <sys/stat.h>
#ifdef __SWITCH__
#include <switch.h>
#endif
using namespace gd;
static constexpr int W=1280,H=720;
static SDL_Renderer*ren;static SDL_Window*window;static std::map<int,TTF_Font*>fonts;
struct TextTex{SDL_Texture*t;int w,h;};static std::map<std::string,TextTex>cache;
struct Col{Uint8 r,g,b,a=255;};
const Col ink{29,46,51},cream{248,242,226},gold{225,188,112},muted{151,182,174},white{255,255,250},red{185,57,63};
static std::string base="./",fontPath;static int skin=0,speed=1;static bool sound=true,quit=false,autoplay=false,recorder=true;
static SDL_AudioDeviceID audioDevice=0;static Game game;static int screen=0,modal=0,focus=0,cursor=0;static bool selected[108]{};static Uint32 lastAI=0,toastUntil=0;static std::string toast;static int total=0,wins=0,bombs=0,statRound=-1;static bool hasSave=false;
void color(Col c){SDL_SetRenderDrawColor(ren,c.r,c.g,c.b,c.a);}
void rect(int x,int y,int w,int h,Col c){color(c);SDL_Rect r{x,y,w,h};SDL_RenderFillRect(ren,&r);}
void line(int x,int y,int xx,int yy,Col c){color(c);SDL_RenderDrawLine(ren,x,y,xx,yy);}
void circle(int x,int y,int r,Col c){color(c);for(int i=-r;i<=r;i++){int dx=int(std::sqrt(double(r*r-i*i)));SDL_RenderDrawLine(ren,x-dx,y+i,x+dx,y+i);}}
void box(int x,int y,int w,int h,int r,Col c){r=std::min(r,std::min(w,h)/2);color(c);for(int row=0;row<h;row++){int inset=0;if(row<r){int dy=r-row;inset=r-int(std::sqrt(double(r*r-dy*dy)));}else if(row>=h-r){int dy=row-(h-r-1);inset=r-int(std::sqrt(double(r*r-dy*dy)));}SDL_RenderDrawLine(ren,x+inset,y+row,x+w-inset-1,y+row);}}

void text(const std::string&s,int x,int y,int size,Col c=cream,bool centered=false){if(s.empty())return;std::string key=std::to_string(size)+":"+std::to_string(c.r)+","+std::to_string(c.g)+","+std::to_string(c.b)+":"+s;
 auto it=cache.find(key);if(it==cache.end()){if(!fonts.count(size))fonts[size]=TTF_OpenFont(fontPath.c_str(),size);if(!fonts[size])return;SDL_Color sc{c.r,c.g,c.b,255};SDL_Surface*sf=TTF_RenderUTF8_Blended(fonts[size],s.c_str(),sc);if(!sf)return;TextTex t{SDL_CreateTextureFromSurface(ren,sf),sf->w,sf->h};SDL_FreeSurface(sf);it=cache.emplace(key,t).first;}
 auto&t=it->second;SDL_SetTextureAlphaMod(t.t,c.a);SDL_Rect d{x-(centered?t.w/2:0),y,t.w,t.h};SDL_RenderCopy(ren,t.t,nullptr,&d);
}
void trimCache(){if(cache.size()>600){for(auto&kv:cache)SDL_DestroyTexture(kv.second.t);cache.clear();}}
std::string rankName(int r){return r==14?"A":r==13?"K":r==12?"Q":r==11?"J":r==17?"大王":r==16?"小王":std::to_string(r);}
std::string playerName(int p){return p==0?"你":p==1?"南风":p==2?"小满 · 队友":"青竹";}
void message(const std::string&s){toast=s;toastUntil=SDL_GetTicks()+2800;}
void beep(bool bomb=false){if(!audioDevice||!sound)return;std::vector<int16_t>samples(4800);for(int i=0;i<4800;i++)samples[i]=int16_t(std::sin(i*6.2831853*(bomb?220:660)/48000)*1800*(1-i/4800.));SDL_QueueAudio(audioDevice,samples.data(),samples.size()*2);}
void clearSelection(){std::fill(std::begin(selected),std::end(selected),false);}
void saveStats(){std::ofstream f(base+"profile.txt");f<<total<<' '<<wins<<' '<<bombs<<' '<<skin<<' '<<speed<<' '<<sound<<' '<<recorder<<' '<<game.difficulty;}
void checkpoint(){hasSave=game.save(base+"session.dat");if(!hasSave)message("存档写入失败，请检查 SD 卡空间");}
void recordResult(){if(game.over&&statRound!=game.round){statRound=game.round;total++;if(game.finish[0]%2==0)wins++;saveStats();}}
void start(int mode){int diff=game.difficulty;game.newMatch(mode,mode==3?20261016u:uint32_t(std::time(nullptr)));game.difficulty=diff;screen=1;modal=0;autoplay=false;cursor=0;clearSelection();statRound=-1;lastAI=SDL_GetTicks();checkpoint();}
void card(int id,int x,int y,int w=66,int h=96,bool sel=false,bool focused=false){
 if(sel){y-=20;}box(x+2,y+5,w,h,7,{0,0,0,55});if(sel||focused)box(x-3,y-3,w+6,h+6,9,focused?gold:Col{79,207,169});box(x,y,w,h,7,sel?Col{255,248,211}:white);
 line(x,y+8,x,y+h-9,{213,218,207});int r=Card{id}.rank(),s=Card{id}.suit();Col c=(s==1||s==2||r==17)?red:ink;std::string suits[]={"♠","♥","♦","♣",""};
 if(r>=16){text(r==17?"大":"小",x+5,y+5,w<50?15:21,c);text("王",x+5,y+(w<50?23:31),w<50?14:18,c);if(w>=50)text("★",x+w-24,y+h-35,24,c);}
 else{text(rankName(r),x+5,y+2,w<50?18:24,c);text(suits[s],x+5,y+(w<50?24:34),w<50?17:22,c);if(w>=50)text(suits[s],x+w-25,y+h-35,26,c);}
 if(wild(id,game.level)){box(x+w-19,y+4,15,19,4,gold);text("配",x+w-18,y+4,12,ink);}
}
void background(bool home=false){
 Col top=skin==1?Col{25,44,78}:skin==2?Col{42,32,59}:Col{12,57,52};Col bottom=skin==1?Col{10,23,44}:skin==2?Col{18,16,31}:Col{7,29,33};
 for(int y=0;y<H;y++){double t=y/double(H);rect(0,y,W,1,{Uint8(top.r*(1-t)+bottom.r*t),Uint8(top.g*(1-t)+bottom.g*t),Uint8(top.b*(1-t)+bottom.b*t)});}
 for(int i=0;i<28;i++){int x=(i*139+37)%W,y=(i*73+21)%H;circle(x,y,1,{210,188,130,45});}
 if(home){for(int i=0;i<6;i++){int x=965+i*42;line(x,58,x+165,410,{123,157,136,20});}text("GUANDAN  /  OFFLINE CLUB",70,40,17,gold);}
}
void button(int x,int y,int w,const std::string&label,bool active=false,bool enabled=true){box(x,y,w,48,13,active?gold:Col{255,255,255,enabled?Uint8(20):Uint8(6)});text(label,x+w/2,y+9,20,active?ink:enabled?cream:muted,true);}
void avatar(int p,int x,int y,bool active){if(active)circle(x,y,34,gold);circle(x,y,30,p%2==0?Col{78,131,116}:Col{80,106,133});circle(x,y-6,10,{235,212,175});circle(x,y+17,18,p%2==0?Col{39,80,72}:Col{40,61,91});circle(x,y-10,11,{59,48,44});circle(x,y-3,9,{235,212,175});line(x-5,y-3,x-3,y-3,ink);line(x+3,y-3,x+5,y-3,ink);text(playerName(p),x,y+42,18,cream,true);}
void home(){background(true);text("掼蛋时光",66,87,66,cream);text("好牌，好搭档。随时开一桌。",72,177,24,muted);box(74,232,385,35,11,{255,255,255,12});text("双副牌  ·  离线 AI  ·  手柄 / 触屏",91,236,17,gold);
 card(40,938,122,100,148);card(45,1032,143,100,148);card(49,1125,170,100,148);
 const char*titles[]={"快速开局","经典升级","明牌练习","定局挑战"};const char*desc[]={"一局尽兴  /  随机发牌","从 2 打到 A  /  自动进贡还贡","看清四家  /  拆牌与配合练习","固定种子  /  重试同一副牌"};const char*tag[]={"QUICK PLAY","CLASSIC MATCH","OPEN HANDS","SEED CHALLENGE"};
 for(int i=0;i<4;i++){int x=74+(i%2)*566,y=306+(i/2)*137;bool f=focus==i;if(f)box(x-3,y-3,546,122,22,gold);box(x,y,540,116,20,f?Col{41,91,79}:Col{24,63,59});text("0"+std::to_string(i+1),x+24,y+15,18,gold);text(tag[i],x+65,y+16,14,muted);text(titles[i],x+24,y+39,29,cream);text(desc[i],x+24,y+81,17,muted);text("›",x+492,y+31,38,gold);}
 button(74,605,220,hasSave?"继续上次对局":"暂无进行中对局",focus==4,hasSave);button(310,605,180,"本地战绩",focus==5);button(506,605,180,"偏好设置",focus==6);button(702,605,180,"玩法说明",focus==7);
 text("方向键选择    A 确认    + 退出",74,680,17,muted);text("独立单机版  ·  v0.1",1206,680,16,muted,true);
}
void shownCards(int p,int cx,int y){auto&m=game.shown[p];if(game.passed[p]){text("不 出",cx,y+18,25,muted,true);return;}if(m.kind==NONE)return;int n=m.cards.size(),step=std::min(36,240/std::max(1,n));int x=cx-(step*(n-1)+48)/2;for(int i=0;i<n;i++)card(m.cards[i],x+step*i,y,48,69);text(kindName(m.kind),cx,y+74,15,gold,true);}
void table(){background();rect(0,0,W,68,{0,0,0,44});text("掼蛋时光",28,16,26,gold);const char*modes[]={"快速开局","经典升级","明牌练习","定局挑战"};text(modes[game.mode],190,22,17,cream);text("第 "+std::to_string(game.round)+" 局",304,22,17,muted);box(540,14,194,39,14,{255,255,255,13});text("本局打 "+rankName(game.level),637,18,23,gold,true);text("我方 "+rankName(game.levels[0])+"   :   对方 "+rankName(game.levels[1]),900,22,18,cream);text("+ 暂停",1160,22,17,muted);
 // Quiet ellipse marks the playable area.
 color({118,174,140,28});for(int i=0;i<360;i++){double a=i*6.2831853/360;SDL_RenderDrawPoint(ren,int(640+445*cos(a)),int(292+154*sin(a)));}
 avatar(2,640,game.mode==2?101:111,game.turn==2&&!game.over);avatar(1,90,game.mode==2?380:267,game.turn==1&&!game.over);avatar(3,1190,game.mode==2?380:267,game.turn==3&&!game.over);
 text(std::to_string(game.hands[2].size())+" 张",740,107,19,gold);text(std::to_string(game.hands[1].size())+" 张",90,game.mode==2?453:343,20,gold,true);text(std::to_string(game.hands[3].size())+" 张",1190,game.mode==2?453:343,20,gold,true);
 if(game.mode==2){for(int p=1;p<4;p++){int x=p==1?20:p==3?1080:420,y=p==2?173:94;for(int i=0;i<int(game.hands[p].size());i++){int cols=p==2?14:6,col=i%cols,row=i/cols;card(game.hands[p][i],x+col*27,y+row*50,35,49);}}}
 else{for(int p:{1,3})for(int i=0;i<5;i++){int x=p==1?66:1166;box(x+i*4,190+i*3,32,49,5,{58,103,96});line(x+7+i*4,199+i*3,x+23+i*4,229+i*3,gold);}}
 shownCards(2,game.mode==2?480:640,game.mode==2?290:206);shownCards(1,283,300);shownCards(3,997,300);shownCards(0,game.mode==2?780:640,game.mode==2?350:327);
 if(game.table.kind==NONE)text("自 由 领 出",640,295,25,{207,221,204,140},true);
 if(!game.over){std::string status=game.turn==0?(autoplay?"正在托管":"轮到你出牌"):playerName(game.turn)+" 正在思考";text(status,640,game.mode==2?462:435,20,game.turn==0?gold:muted,true);if(game.turn!=0)for(int i=0;i<3;i++)circle(613+i*27,475,3,i==int(SDL_GetTicks()/350)%3?gold:Col{70,108,99});}
 if(recorder){std::array<int,18>count{};for(auto&h:game.hands)for(int id:h)count[Card{id}.rank()]++;box(195,73,269,33,10,{0,0,0,28});text("未出大王 "+std::to_string(count[17])+"  小王 "+std::to_string(count[16])+"  级牌 "+std::to_string(count[game.level]),207,78,15,muted);}
 int n=game.hands[0].size();int step=std::min(61,1138/std::max(1,n-1)),x=(W-(step*std::max(0,n-1)+66))/2;for(int i=0;i<n;i++)card(game.hands[0][i],x+i*step,513,66,104,selected[game.hands[0][i]],i==cursor&&game.turn==0&&!autoplay);
 if(n)text("你  ·  "+std::to_string(n)+" 张",50,480,17,muted);
 button(330,630,170,"L 不出",false,game.table.kind!=NONE&&game.turn==0);button(517,630,170,"Y 提示",false,game.turn==0);button(704,630,216,"X 出牌",game.turn==0&&!game.over);
 text("← → 移动   A 选牌   B 清选   R 理牌",24,693,14,muted);text(autoplay?"ZR 取消托管":"ZR 托管",1010,641,17,autoplay?gold:muted);text("− 规则   ZL 记录",1100,693,14,muted);
 recordResult();if(game.over){rect(0,68,W,552,{2,14,19,175});box(380,205,520,284,25,{27,63,58});text(game.finish[0]%2==0?"配合漂亮！":"再来一局",640,228,41,gold,true);text(game.notice,640,290,20,cream,true);for(int i=0;i<4;i++)text(std::to_string(i+1)+"  "+playerName(game.finish[i]),440+(i%2)*215,335+(i/2)*35,19,muted);text(game.champion?"A 开始新赛季    B 返回大厅":"A 下一局    B 返回大厅",640,444,21,cream,true);}
}
void overlay(){if(!modal)return;rect(0,0,W,H,{2,10,16,210});box(210,88,860,536,24,{24,50,50});
 if(modal==1){text("暂停片刻",254,116,34,gold);const char*items[]={"继续对局","返回大厅并保存","偏好设置","玩法说明"};for(int i=0;i<4;i++)button(340,200+i*73,600,items[i],focus==i);}
 if(modal==2){text("偏好设置",254,116,34,gold);std::string labels[]={"AI 策略："+std::string(game.difficulty==0?"休闲":game.difficulty==1?"标准":"进阶"),"牌桌主题："+std::string(skin==0?"翡翠绿":skin==1?"深海蓝":"午夜紫"),"AI 出牌速度："+std::string(speed==0?"舒缓":speed==1?"标准":"快速"),"操作音效："+std::string(sound?"开启":"关闭"),"余牌提示："+std::string(recorder?"开启":"关闭")};for(int i=0;i<5;i++)button(315,184+i*68,650,labels[i],focus==i);text("A 切换选项   B 返回",640,564,18,muted,true);}
 if(modal==3){text("玩法与操作",254,110,32,gold);const char*rules[]={"四人两副牌，对家组队。红桃级牌可代替非王牌。","单张 / 对子 / 三张 / 三带二 / 五张顺子 / 三连对 / 钢板。","炸弹：四至五张 < 同花顺 < 六张及以上；四王最大。","顺子中级牌回归原点数；A 可在开头或末尾，不循环。","头游一方按队友名次升 3 / 2 / 1 级；打 A 且队友非末游获胜。","升级赛自动进贡并还最低非配牌；双大王可抗贡。","← → 移动光标，A 选牌，X 出牌，B 清选，Y 提示。","L 不出，R 理牌，ZR 托管，ZL 出牌记录，+ 暂停。","掌机支持点按选牌和按钮。− 查看说明，B 关闭此页。","进阶 AI 为规则启发式策略；定局挑战可重试相同发牌。"};for(int i=0;i<10;i++)text(rules[i],254,171+i*37,18,i<6?cream:muted);}
 if(modal==4){text("本地战绩",254,116,34,gold);std::string v[]={std::to_string(total),std::to_string(wins),total?std::to_string(wins*100/total)+"%":"—",std::to_string(bombs)};const char*l[]={"完成对局","我方获胜","本地胜率","打出炸弹"};for(int i=0;i<4;i++){int x=330+(i%2)*340,y=205+(i/2)*145;text(v[i],x,y,46,cream);text(l[i],x,y+63,20,muted);}text("B 返回   ·   数据仅保存在本机",640,567,18,muted,true);}
 if(modal==5){text("出牌记录",254,116,34,gold);int start=std::max(0,int(game.log.size())-10);for(int i=start;i<int(game.log.size());i++)text(game.log[i],254,180+(i-start)*36,19,i==int(game.log.size())-1?gold:cream);text("B 返回",940,568,18,muted);}
}
void doPlay(){if(game.turn!=0||game.over||autoplay)return;std::vector<int>cards;for(int id:game.hands[0])if(selected[id])cards.push_back(id);if(cards.empty()){message("先选中要打出的牌");return;}Move m=identify(cards,game.level,game.table);if(game.play(0,cards)){if(m.explosive())bombs++;beep(m.explosive());clearSelection();cursor=std::min(cursor,int(game.hands[0].size())-1);lastAI=SDL_GetTicks();checkpoint();}else message("牌型不合法，或无法压过上一手");}
void doHint(){if(game.turn!=0||game.over)return;clearSelection();Move m=game.hint(0);if(m.kind==NONE){message(game.last>=0&&game.last%2==0?"建议让队友继续领牌，可按 L 不出":"建议不出，可按 L");return;}for(int id:m.cards)selected[id]=true;message(std::string("建议：")+kindName(m.kind)+"，按 X 打出");}
enum Action{LEFT,RIGHT,UP,DOWN,CONFIRM,BACK,PLAY,HINT,PASS,SORT,AUTO,PAUSE,HELP,HISTORY};
void act(Action a){beep();
 if(modal){if(a==BACK||a==PAUSE){modal=0;focus=0;return;}int max=modal==1?4:modal==2?5:1;if(a==UP)focus=(focus+max-1)%max;if(a==DOWN)focus=(focus+1)%max;
  if(a==CONFIRM){if(modal==1){if(focus==0)modal=0;else if(focus==1){checkpoint();screen=0;modal=0;focus=4;}else if(focus==2){modal=2;focus=0;}else{modal=3;focus=0;}}
   else if(modal==2){if(focus==0)game.difficulty=(game.difficulty+1)%3;if(focus==1)skin=(skin+1)%3;if(focus==2)speed=(speed+1)%3;if(focus==3)sound=!sound;if(focus==4)recorder=!recorder;saveStats();if(screen)checkpoint();}}
  return;
 }
 if(!screen){if(a==LEFT)focus=focus<4?(focus^1):4+(focus-4+3)%4;if(a==RIGHT)focus=focus<4?(focus^1):4+(focus-4+1)%4;if(a==UP)focus=focus<2?4+focus:focus<4?focus-2:2+(focus-4)/2;if(a==DOWN)focus=focus<2?focus+2:focus<4?4+(focus-2)*2:(focus-4)/2;if(a==PAUSE){quit=true;return;}if(a==HELP){modal=3;return;}
  if(a==CONFIRM){if(focus<4)start(focus);else if(focus==4){if(hasSave){Game g;if(g.load(base+"session.dat")){game=g;screen=1;autoplay=false;clearSelection();cursor=0;lastAI=SDL_GetTicks();statRound=game.over?game.round:-1;}else message("存档无法读取");}}else{modal=focus==5?4:focus==6?2:3;focus=0;}}return;}
 if(a==PAUSE){checkpoint();modal=1;focus=0;return;}if(a==HELP){modal=3;return;}if(a==HISTORY){modal=5;return;}
 if(game.over){if(a==CONFIRM){if(game.champion)start(game.mode);else{game.deal();statRound=-1;clearSelection();cursor=0;checkpoint();lastAI=SDL_GetTicks();}}if(a==BACK){screen=0;focus=0;}return;}
 if(a==AUTO){autoplay=!autoplay;clearSelection();message(autoplay?"托管已开启，按 ZR 取消":"已取消托管");return;}
 if(game.turn!=0||autoplay){return;}int n=game.hands[0].size();if(a==LEFT&&n)cursor=(cursor+n-1)%n;if(a==RIGHT&&n)cursor=(cursor+1)%n;if(a==CONFIRM&&n)selected[game.hands[0][cursor]]=!selected[game.hands[0][cursor]];if(a==BACK)clearSelection();if(a==HINT)doHint();if(a==PLAY)doPlay();if(a==PASS){if(game.pass(0)){clearSelection();lastAI=SDL_GetTicks();checkpoint();}else message("自由领出时必须出牌");}if(a==SORT){game.sort();clearSelection();cursor=0;message("已按点数理牌，级牌和王牌在右侧");}
}
void click(int x,int y){if(modal){if(modal==1&&x>340&&x<940&&y>=200&&y<492){focus=std::min(3,(y-200)/73);act(CONFIRM);}else if(modal==2&&x>315&&x<965&&y>=184&&y<524){focus=std::min(4,(y-184)/68);act(CONFIRM);}else act(BACK);return;}
 if(!screen){for(int i=0;i<4;i++){int xx=74+(i%2)*566,yy=306+(i/2)*137;if(x>=xx&&x<xx+540&&y>=yy&&y<yy+116){focus=i;act(CONFIRM);return;}}if(y>605&&y<655){if(x>=74&&x<294)focus=4;else if(x>=310&&x<490)focus=5;else if(x>=506&&x<686)focus=6;else if(x>=702&&x<882)focus=7;else return;act(CONFIRM);}return;}
 if(game.over){act(CONFIRM);return;}if(x>1140&&y<68){act(PAUSE);return;}if(y>=630&&y<=679){if(x>=330&&x<500)act(PASS);if(x>=517&&x<687)act(HINT);if(x>=704&&x<920)act(PLAY);if(x>=1000)act(AUTO);return;}
 if(game.turn==0&&!autoplay){int n=game.hands[0].size(),step=std::min(61,1138/std::max(1,n-1)),xx=(W-(step*std::max(0,n-1)+66))/2;for(int i=n-1;i>=0;i--){int yy=513-(selected[game.hands[0][i]]?20:0);if(x>=xx+i*step&&x<xx+i*step+66&&y>=yy&&y<yy+104){cursor=i;act(CONFIRM);return;}}}}
int main(int argc,char**argv){bool capture=false,captureGame=false,smoke=false;int captureMode=0,captureModal=0;std::string capturePath="preview.bmp";for(int i=1;i<argc;i++){std::string s=argv[i];if(s=="--capture-training"){captureMode=2;s="--capture-game";}if(s=="--capture-help"){captureModal=3;s="--capture";}if(s=="--capture-settings"){captureModal=2;s="--capture";}if(s=="--capture"||s=="--capture-game"){capture=true;captureGame=s=="--capture-game";if(i+1<argc)capturePath=argv[++i];}if(s=="--smoke")smoke=true;}
#ifdef __SWITCH__
 romfsInit();base="sdmc:/switch/GuandanSwitch/";mkdir(base.c_str(),0777);fontPath="romfs:/font.otf";padConfigureInput(1,HidNpadStyleSet_NpadStandard);PadState pad;padInitializeDefault(&pad);
#else
 fontPath="romfs/font.otf";
#endif
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER)<0){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}if(TTF_Init()<0){fprintf(stderr,"TTF: %s\n",TTF_GetError());return 1;}
 window=SDL_CreateWindow("掼蛋时光 | Guandan Switch",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);ren=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!ren)ren=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);if(!ren){fprintf(stderr,"Renderer: %s\n",SDL_GetError());return 1;}SDL_RenderSetLogicalSize(ren,W,H);SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
 fonts[20]=TTF_OpenFont(fontPath.c_str(),20);if(!fonts[20]){fprintf(stderr,"Font: %s\n",TTF_GetError());return 1;}
 SDL_AudioSpec spec{};spec.freq=48000;spec.format=AUDIO_S16SYS;spec.channels=1;spec.samples=1024;audioDevice=SDL_OpenAudioDevice(nullptr,0,&spec,nullptr,0);if(audioDevice)SDL_PauseAudioDevice(audioDevice,0);
 std::ifstream profile(base+"profile.txt");profile>>total>>wins>>bombs>>skin>>speed>>sound>>recorder;int savedDifficulty=1;if(profile>>savedDifficulty)game.difficulty=std::clamp(savedDifficulty,0,2);skin=(skin%3+3)%3;speed=(speed%3+3)%3;{Game old;hasSave=old.load(base+"session.dat");}
 if(captureGame||smoke){start(smoke?2:captureMode);}if(captureModal)modal=captureModal;Uint32 frame=0;
 while(!quit){
#ifdef __SWITCH__
 if(!appletMainLoop()){break;}padUpdate(&pad);u64 down=padGetButtonsDown(&pad);u64 held=padGetButtons(&pad);static Uint32 repeat=0;u64 dirs=HidNpadButton_Left|HidNpadButton_Right|HidNpadButton_Up|HidNpadButton_Down;if(down&HidNpadButton_StickLLeft)down|=HidNpadButton_Left;if(down&HidNpadButton_StickLRight)down|=HidNpadButton_Right;if(down&HidNpadButton_StickLUp)down|=HidNpadButton_Up;if(down&HidNpadButton_StickLDown)down|=HidNpadButton_Down;if(held&HidNpadButton_StickLLeft)held|=HidNpadButton_Left;if(held&HidNpadButton_StickLRight)held|=HidNpadButton_Right;if(held&HidNpadButton_StickLUp)held|=HidNpadButton_Up;if(held&HidNpadButton_StickLDown)held|=HidNpadButton_Down;if(down&dirs)repeat=SDL_GetTicks()+320;else if((held&dirs)&&SDL_GetTicks()>repeat){down|=held&dirs;repeat=SDL_GetTicks()+95;}
 struct Binding{u64 bit;Action a;};Binding bindings[]={{HidNpadButton_Left,LEFT},{HidNpadButton_Right,RIGHT},{HidNpadButton_Up,UP},{HidNpadButton_Down,DOWN},{HidNpadButton_A,CONFIRM},{HidNpadButton_B,BACK},{HidNpadButton_X,PLAY},{HidNpadButton_Y,HINT},{HidNpadButton_L,PASS},{HidNpadButton_R,SORT},{HidNpadButton_ZR,AUTO},{HidNpadButton_ZL,HISTORY},{HidNpadButton_Plus,PAUSE},{HidNpadButton_Minus,HELP}};for(auto b:bindings)if(down&b.bit)act(b.a);
#endif
 SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_QUIT)quit=true;if(e.type==SDL_KEYDOWN){switch(e.key.keysym.sym){case SDLK_LEFT:act(LEFT);break;case SDLK_RIGHT:act(RIGHT);break;case SDLK_UP:act(UP);break;case SDLK_DOWN:act(DOWN);break;case SDLK_SPACE:act(CONFIRM);break;case SDLK_RETURN:act(screen&&!game.over?PLAY:CONFIRM);break;case SDLK_ESCAPE:act(modal?BACK:PAUSE);break;case SDLK_BACKSPACE:act(BACK);break;case SDLK_h:act(HINT);break;case SDLK_p:act(PASS);break;case SDLK_r:act(SORT);break;case SDLK_t:act(AUTO);break;case SDLK_F1:act(HELP);break;case SDLK_l:act(HISTORY);break;}}
  if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.which!=SDL_TOUCH_MOUSEID){float x,y;SDL_RenderWindowToLogical(ren,e.button.x,e.button.y,&x,&y);click(int(x),int(y));}if(e.type==SDL_FINGERDOWN)click(int(e.tfinger.x*W),int(e.tfinger.y*H));}
 Uint32 now=SDL_GetTicks();if(screen&&!modal&&!game.over&&(game.turn!=0||autoplay)&&now-lastAI>Uint32(speed==0?1500:speed==1?850:320)&&!capture){int p=game.turn;Move m=game.hint(p);if(m.kind==NONE)game.pass(p);else game.play(p,m.cards);if(p==0&&m.explosive())bombs++;clearSelection();cursor=0;lastAI=now;beep(m.explosive());checkpoint();}
 if(smoke){if(frame==1){act(HELP);act(BACK);act(HINT);act(PLAY);}if(frame==2){act(PAUSE);act(BACK);act(AUTO);}if(frame>20)quit=true;}
 trimCache();if(screen)table();else home();overlay();if(now<toastUntil){box(260,115,760,52,16,{10,20,26,240});text(toast,640,127,21,gold,true);}if(capture&&frame==2){SDL_Surface*sf=SDL_CreateRGBSurfaceWithFormat(0,W,H,32,SDL_PIXELFORMAT_ARGB8888);SDL_RenderReadPixels(ren,nullptr,sf->format->format,sf->pixels,sf->pitch);SDL_SaveBMP(sf,capturePath.c_str());SDL_FreeSurface(sf);quit=true;}SDL_RenderPresent(ren);frame++;SDL_Delay(16);
 }
 if(screen){checkpoint();}saveStats();for(auto&kv:cache)SDL_DestroyTexture(kv.second.t);for(auto&kv:fonts)TTF_CloseFont(kv.second);if(audioDevice)SDL_CloseAudioDevice(audioDevice);SDL_DestroyRenderer(ren);SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();
#ifdef __SWITCH__
 romfsExit();
#endif
 return 0;
}
