// Reference-style table: one geometry model shared by painting, touch and controller.
struct HandPos {int x,y,w,h;};
static std::vector<gd::HandGroup> orgGroups;
static std::vector<int> orgGroupStart;
void computeOrganizedGroups(){
 orgGroups=gd::arrangeHand(game.hands[0],game.level,smartGrouped);orgGroupStart.clear();
 std::vector<int> reordered;
 for(const auto&g:orgGroups){orgGroupStart.push_back(int(reordered.size()));reordered.insert(reordered.end(),g.cards.begin(),g.cards.end());}
 orgGroupStart.push_back(int(reordered.size()));game.hands[0]=reordered;
 layoutHand=reordered;layoutLevel=game.level;layoutRound=game.round;layoutSmart=smartGrouped;
 cursor=std::clamp(cursor,0,std::max(0,int(reordered.size())-1));
}
void ensureGroups(){if(layoutHand!=game.hands[0]||layoutLevel!=game.level||layoutRound!=game.round||layoutSmart!=smartGrouped)computeOrganizedGroups();}
void restoreHand(){organized=false;smartGrouped=false;suitFilter=-1;flushIndex=-1;clearSelection();computeOrganizedGroups();cursor=0;message("已恢复横排，按点数排序");}
std::vector<HandPos> organizedPositions(){
 ensureGroups();int n=game.hands[0].size();std::vector<HandPos> pos(n);
 int ng=int(orgGroups.size());if(!ng)return pos;
 int cw=std::clamp(1010/std::max(1,ng),62,104),ch=138;
 int step=ng>1?std::min(cw+5,(1040-cw)/(ng-1)):0;
 int total=(ng-1)*step+cw,sx=170+(1040-total)/2;
 int maxRows=1;for(const auto&g:orgGroups)maxRows=std::max(maxRows,int(g.cards.size()));
 int dy=maxRows>1?std::min(39,(278-ch)/(maxRows-1)):39;
 for(int g=0;g<ng;g++){int size=orgGroupStart[g+1]-orgGroupStart[g];for(int i=0;i<size;i++)pos[orgGroupStart[g]+i]={sx+g*step,630-ch-(size-1-i)*dy,cw,ch};}
 return pos;
}
std::vector<HandPos> handPositions(){
 if(organized)return organizedPositions();
 int n=game.hands[0].size();std::vector<HandPos> pos(n);int step=std::min(67,1036/std::max(1,n-1)),sx=170+(1040-(step*std::max(0,n-1)+74))/2;
 for(int i=0;i<n;i++)pos[i]={sx+i*step,492,74,138};
 return pos;
}
int hitHand(int x,int y){auto pos=handPositions();for(int i=int(pos.size())-1;i>=0;i--){int id=game.hands[0][i],yy=pos[i].y-(selected[id]?18:0);if(Area{pos[i].x,yy,pos[i].w,pos[i].h}.contains(x,y))return i;}return -1;}
void beginHandGesture(int x,int y){
 gestureActive=false;if(!screen||modal||game.over||autoplay||(!captureStill&&dealStarted&&SDL_GetTicks()-dealStarted<1000))return;
 int hit=hitHand(x,y);if(hit<0)return;gestureActive=true;std::fill(std::begin(gestureVisited),std::end(gestureVisited),false);
 int id=game.hands[0][hit];gestureValue=!selected[id];gestureVisited[id]=true;selected[id]=gestureValue;cursor=hit;sfx(0);
}
void moveHandGesture(int x,int y){if(!gestureActive||modal||autoplay||game.over)return;int hit=hitHand(x,y);if(hit<0)return;int id=game.hands[0][hit];if(!gestureVisited[id]){gestureVisited[id]=true;selected[id]=gestureValue;cursor=hit;sfx(0);}}
void findStraightFlush(){
 auto ms=moves(game.hands[0],game.level);ms.erase(std::remove_if(ms.begin(),ms.end(),[&](const Move&m){if(m.kind!=FLUSH)return true;if(suitFilter<0)return false;for(int id:m.cards)if(!wild(id,game.level)&&Card{id}.suit()!=suitFilter)return true;return false;}),ms.end());
 if(ms.empty()){message(suitFilter<0?"当前手牌没有可组成的同花顺":"该花色暂无同花顺，可用花色按钮查看手牌");return;}
 std::sort(ms.begin(),ms.end(),[](const Move&a,const Move&b){return a.high<b.high;});flushIndex=(flushIndex+1)%int(ms.size());clearSelection();for(int id:ms[flushIndex].cards)selected[id]=true;
 message("同花顺 "+std::to_string(flushIndex+1)+" / "+std::to_string(ms.size())+" · 按 X 出牌，点击同花顺换组");
}
void fillPolygon(std::initializer_list<SDL_Point> input,Col c){
 std::vector<SDL_Point>p(input);int ymin=H,ymax=0;for(auto q:p){ymin=std::min(ymin,q.y);ymax=std::max(ymax,q.y);}color(c);
 for(int y=std::max(0,ymin);y<=std::min(H-1,ymax);y++){std::vector<int>xs;for(size_t i=0;i<p.size();i++){auto a=p[i],b=p[(i+1)%p.size()];if((a.y<=y&&b.y>y)||(b.y<=y&&a.y>y))xs.push_back(a.x+(y-a.y)*(b.x-a.x)/(b.y-a.y));}std::sort(xs.begin(),xs.end());for(size_t i=1;i<xs.size();i+=2)SDL_RenderDrawLine(ren,xs[i-1],y,xs[i],y);}
}
void referenceScene(){
 const char*themeNames[]={"原色夜景","深海蓝","午夜紫"};
 if(themeTex[skin]){SDL_Rect dst{0,0,W,H};SDL_RenderCopy(ren,themeTex[skin],nullptr,&dst);}
 else vgrad(0,0,W,H,skins[skin].top,skins[skin].bot);
 // Local, translucent gradients keep the full artwork visible; no solid header/footer.
 for(int y=0;y<92;y++)rect(0,y,W,1,{8,14,29,Uint8(115*(92-y)/92)});
 floatingText(std::string(themeNames[skin])+" · 新场景",790,24,15,WHITE);
 text("掼 蛋 时 光",640,213,35,{255,255,255,20},true);
 const char*mode[]={"休闲单局","经典升级","明牌练习","定局挑战"};text(std::string(mode[game.mode])+"  ·  对家组队",640,258,16,{255,255,255,34},true);
}
void pill(int x,int y,int w,int h,const std::string&label,bool gold,bool enabled=true,int fs=22){
 box(x+1,y+4,w,h,h/2,{17,44,85,60});box(x-1,y-1,w+2,h+2,h/2,enabled?Col{255,255,255,150}:Col{255,255,255,45});
 Col a=gold?Col{255,224,127}:skin==0?Col{67,145,115}:skin==1?Col{91,160,226}:Col{153,115,205},b=gold?Col{239,168,52}:skin==0?Col{28,91,70}:skin==1?Col{35,89,168}:Col{91,57,146};if(!enabled){a={121,147,170};b={96,118,150};}
 roundedGradient(x,y,w,h,h/2,a,b);hline(x+22,y+4,w-44,{255,255,255,90});text(label,x+w/2,y+(h-fs)/2-3,fs,gold?Col{103,59,15}:WHITE,true);
}
void tableSeat(int p,int x,int y,bool teammate=false){
 bool active=!game.over&&game.turn==p;box(x-34,y-34,68,68,10,active?GOLD:Col{219,238,251});box(x-31,y-31,62,62,8,{46,105,142});
 if(avTex[p]){SDL_Rect d{x-29,y-29,58,58};SDL_RenderCopy(ren,avTex[p],nullptr,&d);}else face(x,y,25,p);
 box(x-39,y+36,78,25,5,{17,41,70,180});text(playerName(p),x,y+37,16,WHITE,true);
 if(teammate){box(x-29,y+14,58,18,4,{46,143,212,240});text("队友",x,y+13,13,WHITE,true);}
 if(p!=0){box(x-23,y+65,46,22,5,{25,43,66,160});text(std::to_string(game.hands[p].size())+" 张",x,y+64,16,GOLD,true);}
 if(game.hands[p].empty())text("已出完",x,y-59,15,GOLD,true);
}
void clockBadge(int x,int y,Uint32 now){
 int sec=timerOn?std::max(0,18-int((now-lastAI)/1000)):0;circle(x,y,23,{203,138,47});circle(x,y,19,{255,249,218});ring(x,y,17,1,{234,193,112});
 text(timerOn?std::to_string(sec):"∞",x,y-14,25,{142,79,31},true);line(x-13,y-22,x-19,y-29,GOLD);line(x+13,y-22,x+19,y-29,GOLD);
}
void referenceCounter(){
 if(!counterOpen||!recorder)return;
 box(315,71,640,88,14,{18,50,89,234});text("记牌器 · 未出且不在你手中的牌",334,78,14,{197,220,248});
 int ranks[]={17,16,14,13,12,11,10,9,8,7,6,5,4,3,2};
 for(int i=0;i<15;i++){int x=335+i*41;std::string label=i==0?"大王":i==1?"小王":rankName(ranks[i]);text(label,x,100,15,WHITE);text(std::to_string(game.remaining(ranks[i])),x+10,123,19,game.remaining(ranks[i])?GOLD:Col{144,166,186},true);}
}
