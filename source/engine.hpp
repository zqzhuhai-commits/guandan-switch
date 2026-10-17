#pragma once
#include "storage.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <functional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>
namespace gd {
struct Card { int id; int rank()const{return id%54<52? id%54/4+2 : id%54-52+16;} int suit()const{return id%54<52?(id%54)%4:4;} };
inline bool wild(int id,int level){return Card{id}.rank()==level&&Card{id}.suit()==1;}
inline int power(int rank,int level){return rank>=16?rank:rank==level?15:rank;}
enum Kind{NONE,SINGLE,PAIR,TRIPLE,FULLHOUSE,STRAIGHT,PAIRS,TRIPLES,BOMB,FLUSH,JOKERS};
inline const char* kindName(Kind k){const char* a[]={"自由出牌","单张","对子","三张","三带二","顺子","三连对","钢板","炸弹","同花顺","四大天王"};return a[k];}
// Two decks of 54: ranks 2..A appear 8 times, each joker 2 times. Index by rank() (2..14,16,17).
inline constexpr std::array<int,18> TOTAL_RANK{0,0,8,8,8,8,8,8,8,8,8,8,8,8,8,0,2,2};
struct Move{Kind kind=NONE; int high=0; std::vector<int> cards; bool explosive()const{return kind>=BOMB;} };
inline int bombPower(const Move&m){if(m.kind==JOKERS)return 10000;if(m.kind==FLUSH)return 550+m.high;return int(m.cards.size())*100+m.high;}
inline bool beats(const Move&a,const Move&b){if(a.kind==NONE)return false;if(b.kind==NONE)return true;if(a.explosive())return !b.explosive()||bombPower(a)>bombPower(b);return !b.explosive()&&a.kind==b.kind&&a.cards.size()==b.cards.size()&&a.high>b.high;}
// Enumerate rank templates. A heart level card substitutes only for a non-joker.
inline std::vector<Move> moves(const std::vector<int>&hand,int level,const Move&target=Move{}){
 std::array<std::vector<int>,18> ranks;std::vector<int>w;
 for(int id:hand){if(wild(id,level))w.push_back(id);else ranks[Card{id}.rank()].push_back(id);}
 std::vector<Move> out;std::set<std::vector<int>>seen;
 auto add=[&](Kind k,int high,std::vector<int> cards){if(k==PAIR && cards.size()==2 && wild(cards[0],level) && wild(cards[1],level))high=power(level,level);Move m{k,high,cards};if(!beats(m,target))return;std::sort(cards.begin(),cards.end());if(seen.insert(cards).second)out.push_back(m);};
 if(target.kind==NONE||target.kind==SINGLE)for(int id:hand)add(SINGLE,power(Card{id}.rank(),level),{id});
 auto pattern=[&](Kind k,int high,const std::vector<std::pair<int,int>>&need,int suit=-1){
  std::vector<int> cards;int wi=0;
  for(auto [r,n]:need){int used=0;for(int id:ranks[r])if(suit<0||Card{id}.suit()==suit){cards.push_back(id);if(++used==n)break;}
   while(used<n){if(r>=16||wi>=int(w.size()))return;cards.push_back(w[wi++]);used++;}}
  add(k,high,cards);
 };
 for(int r=2;r<=17;r++){
  if(target.kind==NONE||target.kind==PAIR)pattern(PAIR,power(r,level),{{r,2}});
  if(target.kind==NONE||target.kind==TRIPLE)pattern(TRIPLE,power(r,level),{{r,3}});
  if(r<16)for(int n=4;n<=10;n++)pattern(BOMB,power(r,level),{{r,n}});
 }
 if(target.kind==NONE||target.kind==FULLHOUSE)for(int r=2;r<=14;r++)for(int s=2;s<=17;s++)if(s!=r)pattern(FULLHOUSE,power(r,level),{{r,3},{s,2}});
 for(int start=1;start<=10;start++){
  std::vector<std::pair<int,int>>need;for(int i=0;i<5;i++)need.push_back({start+i==1?14:start+i,1});
  if(target.kind==NONE||target.kind==STRAIGHT)pattern(STRAIGHT,start+4,need);
  for(int s=0;s<4;s++)pattern(FLUSH,start+4,need,s);
 }
 for(int count=2;count<=3;count++){Kind k=count==2?PAIRS:TRIPLES;int len=count==2?3:2;if(target.kind!=NONE&&target.kind!=k)continue;
  for(int start=1;start<=15-len;start++){std::vector<std::pair<int,int>>need;for(int i=0;i<len;i++)need.push_back({start+i==1?14:start+i,count});pattern(k,start+len-1,need);}}
 std::vector<int>js;for(int id:hand)if(Card{id}.rank()>=16)js.push_back(id);if(js.size()==4)add(JOKERS,99,js);
 // A same-suit sequence is always a straight flush, even if a straight template found it first.
 for(auto&m:out)if(m.kind==STRAIGHT){int s=-1;bool same=true;for(int id:m.cards)if(!wild(id,level)){if(s<0)s=Card{id}.suit();else if(s!=Card{id}.suit())same=false;}if(same)m.kind=FLUSH;}
 out.erase(std::remove_if(out.begin(),out.end(),[&](const Move&m){return !beats(m,target);}),out.end());
 return out;
}
// Exact recognition for arbitrary player-selected cards, including deliberate wildcard use.
inline Move identify(const std::vector<int>&cards,int level,const Move&target=Move{}){
 auto ms=moves(cards,level,target);Move best;for(auto&m:ms)if(m.cards.size()==cards.size()&&(best.kind==NONE|| (m.explosive()&&!best.explosive()) || (m.explosive()==best.explosive()&&m.high<best.high)))best=m;return best;
}
struct TurnRecord { int player=0; Move move; };
struct Game{
 std::array<std::vector<int>,4> hands;std::array<int,2> levels{2,2};int level=2,turn=0,last=-1,passes=0,mode=0,difficulty=1,round=0,first=0;bool over=false,champion=false;
 Move table;std::vector<int>finish,previous;std::vector<std::string>log;std::array<Move,4> shown;std::array<bool,4>passed{};std::string notice;uint32_t seed=0;
 std::vector<TurnRecord> history;
 std::vector<int>powerLog;std::vector<bool>bombLog;std::vector<std::array<int,4>>snapshots; // 复盘：每手牌力/是否炸弹/四家手牌快照
 std::array<int,18> used{}; // cards already played this deal, by rank
 int remaining(int r)const{int own=0;for(int id:hands[0])if(Card{id}.rank()==r)own++;int n=TOTAL_RANK[r]-used[r]-own;return n<0?0:n;}
 void note(const std::string&s){notice=s;log.push_back(s);if(log.size()>80)log.erase(log.begin());}
 void sort(){for(auto&h:hands)std::sort(h.begin(),h.end(),[&](int a,int b){int pa=power(Card{a}.rank(),level),pb=power(Card{b}.rank(),level);return pa!=pb?pa<pb:a<b;});}
 void newMatch(int m,uint32_t s){*this=Game{};mode=m;seed=s;deal();}
 int next(int p)const{for(int n=1;n<=4;n++)if(!hands[(p+n)%4].empty())return(p+n)%4;return p;}
 void exchange(int from,int to){
  auto&h=hands[from];int best=-1;for(int id:h)if(!wild(id,level)&&(best<0||power(Card{id}.rank(),level)>power(Card{best}.rank(),level)))best=id;
  if(best<0){return;}h.erase(std::find(h.begin(),h.end(),best));auto&t=hands[to];
  int ret=-1;for(int id:t)if(!wild(id,level)&&Card{id}.rank()<=10&&(ret<0||Card{id}.rank()<Card{ret}.rank()))ret=id;
  if(ret<0)for(int id:t)if(!wild(id,level)&&(ret<0||power(Card{id}.rank(),level)<power(Card{ret}.rank(),level)))ret=id;
  if(ret<0){ret=t.front();}t.erase(std::find(t.begin(),t.end(),ret));t.push_back(best);h.push_back(ret);
 }
 void deal(){round++;table=Move{};used={};finish.clear();shown={};passed={};last=-1;passes=0;over=false;history.clear();log.clear();powerLog.clear();bombLog.clear();snapshots.clear();for(auto&h:hands)h.clear();
  std::vector<int>deck(108);for(int i=0;i<108;i++)deck[i]=i;std::mt19937 rng(seed+uint32_t(round)*7919u);std::shuffle(deck.begin(),deck.end(),rng);for(int i=0;i<108;i++)hands[i%4].push_back(deck[i]);
  turn=first;notice="新牌局开始";
  if(mode==1&&previous.size()==4){
   int win=previous[0],lose=previous[3];bool dbl=previous[1]%2==win%2;int red=0;for(int p=0;p<4;p++)if((dbl&&p%2!=win%2)||(!dbl&&p==lose))for(int id:hands[p])if(Card{id}.rank()==17)red++;
   if(red==2){turn=win;notice="双大王抗贡 · 头游先出";}else{
    if(dbl){int a=previous[2],b=previous[3];auto highest=[&](int p){int x=0;for(int id:hands[p])if(!wild(id,level))x=std::max(x,power(Card{id}.rank(),level));return x;};if(highest(b)>highest(a))std::swap(a,b);exchange(a,win);exchange(b,previous[1]);turn=a;}
    else{exchange(lose,win);turn=lose;}notice="已自动进贡还贡 · 还最低非逢人配牌";
   }
  }sort();note(notice);
 }
 void complete(){
  over=true;for(int p=0;p<4;p++)if(std::find(finish.begin(),finish.end(),p)==finish.end())finish.push_back(p);previous=finish;
  int team=finish[0]%2,place=0;for(int i=1;i<4;i++)if(finish[i]%2==team)place=i;
  int up=4-place;bool onAce=level==14&&levels[team]==14&&first%2==team;
  if(mode==1){champion=onAce&&place<3;levels[team]=std::min(14,levels[team]+up);level=levels[team];}
  first=finish[0];sort();note((team==0?"我方获胜":"对方获胜")+(mode==1?std::string(" · 升 ")+std::to_string(up)+" 级":std::string(" · 本局结束"))+(champion?" · 成功过 A！":""));
 }
 bool play(int p,const std::vector<int>&cards){
  if(over||p!=turn){return false;}std::set<int> unique(cards.begin(),cards.end());if(unique.size()!=cards.size())return false;for(int id:cards)if(std::find(hands[p].begin(),hands[p].end(),id)==hands[p].end())return false;
  Move m=identify(cards,level,table);if(m.kind==NONE)return false;
  for(int id:cards){auto&h=hands[p];h.erase(std::find(h.begin(),h.end(),id));}for(int id:m.cards)used[Card{id}.rank()]++;table=m;shown[p]=m;passed[p]=false;last=p;passes=0;note(std::string(p==0?"你":p==2?"队友":"对手")+"打出"+kindName(m.kind));
  history.push_back({p,m});
  powerLog.push_back(m.explosive()?bombPower(m):m.high);bombLog.push_back(m.explosive());std::array<int,4>sc{};for(int i=0;i<4;i++)sc[i]=int(hands[i].size());snapshots.push_back(sc);
  if(hands[p].empty()){finish.push_back(p);if(finish.size()==3||(finish.size()==2&&finish[0]%2==finish[1]%2)){complete();return true;}}
  turn=next(p);return true;
 }
 bool pass(int p){if(over||p!=turn||table.kind==NONE)return false;history.push_back({p,Move{}});note(std::string(p==0?"你":p==2?"队友":"对手")+"不出");passes++;shown[p]=Move{};passed[p]=true;int active=0;for(auto&h:hands)if(!h.empty())active++;
  int required=active-(hands[last].empty()?0:1);
  if(passes>=required){int lead=hands[last].empty()?(!hands[(last+2)%4].empty()?(last+2)%4:next(last)):last;turn=lead;table=Move{};last=-1;passes=0;shown={};passed={};note("新一轮 · 自由领出");}
  else {turn=next(p);}return true;
 }
 Move hint(int p)const{
  auto ms=moves(hands[p],level,table);if(ms.empty())return Move{};for(auto&m:ms)if(m.cards.size()==hands[p].size())return m;
  bool partner=last>=0&&last%2==p%2;bool urgent=last>=0&&hands[last].size()<=4;
  if(partner&&difficulty>0 && (hands[last].size()<=6 || hands[last].size()<=hands[p].size()))return Move{};
  auto score=[&](const Move&m){double s=0;std::array<int,18>before{},after{};for(int id:hands[p])before[Card{id}.rank()]++;after=before;
   for(int id:m.cards){after[Card{id}.rank()]--;if(wild(id,level))s+=9;}
   for(int r=2;r<=17;r++){if(before[r]>=4&&after[r]>0&&after[r]<4)s+=difficulty?18:1;if(after[r]==1)s+=difficulty==0?1.0:difficulty==1?2.8:4.8; if(difficulty==2 && after[r]==2)s+=1.0;}
   if(table.kind==NONE){s-=m.cards.size()*4;s+=m.high*.55;if(m.explosive())s+=35;}
   else{s+=m.high*.7;if(m.explosive())s+=urgent?5:45;s-=m.cards.size()*.3;}
   if(difficulty==2){ for(int r=3;r<=13;r++)if(after[r]==1 && after[r-1]>0 && after[r+1]>0)s-=2.0; if(table.kind!=NONE && !urgent && m.high>=15)s+=8; }
   if(partner){s+=m.explosive()?70:10;}return s;
  };
  return *std::min_element(ms.begin(),ms.end(),[&](const Move&a,const Move&b){return score(a)<score(b);});
 }
 void autoStep(){Move m=hint(turn);if(m.kind==NONE){if(table.kind==NONE){auto ms=moves(hands[turn],level);if(!ms.empty())play(turn,ms.front().cards);}else pass(turn);}else play(turn,m.cards);}
 bool valid()const{std::set<int>s;for(auto&h:hands)for(int id:h)if(id<0||id>=108||!s.insert(id).second)return false;return turn>=0&&turn<4&&level>=2&&level<=14;}
 bool save(const std::string&path)const{
  std::ofstream f(path+".tmp");if(!f)return false;f<<"GD5 "<<seed<<' '<<round<<' '<<mode<<' '<<difficulty<<' '<<level<<' '<<levels[0]<<' '<<levels[1]<<' '<<turn<<' '<<last<<' '<<passes<<' '<<first<<' '<<over<<' '<<champion<<'\n';
  for(int r=2;r<=17;r++){f<<used[r]<<' ';}
  f<<'\n';
  auto vec=[&](const std::vector<int>&v){f<<v.size();for(int x:v)f<<' '<<x;f<<'\n';};for(auto&h:hands)vec(h);vec(finish);vec(previous);f<<int(table.kind)<<' '<<table.high<<'\n';vec(table.cards);
  f<<history.size()<<'\n';for(const auto&r:history){f<<r.player<<' '<<int(r.move.kind)<<' '<<r.move.high<<'\n';vec(r.move.cards);}
  f<<powerLog.size()<<'\n';for(size_t i=0;i<powerLog.size();i++){f<<powerLog[i]<<' '<<bombLog[i];for(int n:snapshots[i])f<<' '<<n;f<<'\n';}
  f.close();if(!f)return false;return commitTemporaryFile(path);
 }
 bool load(const std::string&path){std::ifstream f(path);std::string magic;Game g;if(!(f>>magic)||(magic!="GD3"&&magic!="GD4"&&magic!="GD5"))return false;f>>g.seed>>g.round>>g.mode>>g.difficulty>>g.level>>g.levels[0]>>g.levels[1]>>g.turn>>g.last>>g.passes>>g.first>>g.over>>g.champion;
  if(magic!="GD3"){for(int r=2;r<=17;r++){int v;if(!(f>>v)||v<0||v>16){f.setstate(std::ios::failbit);return false;}g.used[r]=v;}}
  auto vec=[&](std::vector<int>&v){int n;if(!(f>>n)||n<0||n>108){f.setstate(std::ios::failbit);return;}for(int i=0;i<n;i++){int x=0;if(!(f>>x))return;v.push_back(x);}};for(auto&h:g.hands)vec(h);vec(g.finish);vec(g.previous);int k=0;if(!(f>>k>>g.table.high))return false;if(k<0||k>10)return false;g.table.kind=Kind(k);vec(g.table.cards);if(!f||!g.valid()||g.last < -1||g.last>3||g.mode<0||g.mode>3)return false;if(g.round<1||g.round>1000000||g.first<0||g.first>3||g.difficulty<0||g.difficulty>2||g.passes<0||g.passes>4||g.levels[0]<2||g.levels[0]>14||g.levels[1]<2||g.levels[1]>14)return false;
  for(auto&h:g.hands)if(h.size()>27)return false;
  auto places=[](const std::vector<int>&v){std::set<int>s;for(int p:v)if(p<0||p>3||!s.insert(p).second)return false;return true;};
  if(!places(g.finish)||!places(g.previous)||(!g.previous.empty()&&g.previous.size()!=4)||(g.over&&g.finish.size()!=4)||(!g.over&&g.hands[g.turn].empty()))return false;
  if(g.table.kind!=NONE&&(g.last<0||g.table.cards.empty()||g.table.cards.size()>10))return false;
  for(int id:g.table.cards)if(id<0||id>=108)return false;
  if(magic=="GD3"){g.used={};for(auto&h:g.hands)for(int id:h)g.used[Card{id}.rank()]++;for(int r=2;r<=17;r++)g.used[r]=TOTAL_RANK[r]>g.used[r]?TOTAL_RANK[r]-g.used[r]:0;}
  if(magic=="GD5"){
   int count=0;if(!(f>>count)||count<0||count>512)return false;
   for(int i=0;i<count;i++){TurnRecord r;int kind;if(!(f>>r.player>>kind>>r.move.high)||r.player<0||r.player>3||kind<0||kind>10)return false;r.move.kind=Kind(kind);vec(r.move.cards);if(!f||r.move.cards.size()>10)return false;std::set<int>ids;for(int id:r.move.cards)if(id<0||id>=108||!ids.insert(id).second)return false;if((kind==NONE)!=r.move.cards.empty())return false;g.history.push_back(r);}
   if(!(f>>count)||count<0||count>108)return false;
   for(int i=0;i<count;i++){int pw,b;std::array<int,4>sc{};if(!(f>>pw>>b)||pw<0||pw>10000||(b!=0&&b!=1))return false;for(int&n:sc)if(!(f>>n)||n<0||n>27)return false;g.powerLog.push_back(pw);g.bombLog.push_back(b!=0);g.snapshots.push_back(sc);}
   // Rebuild visible table feedback from the last trick, without replaying any actions.
   if(g.table.kind!=NONE){g.shown[g.last]=g.table;for(auto it=g.history.rbegin();it!=g.history.rend();++it){if(it->move.kind!=NONE)break;g.passed[it->player]=true;}}
  }
  g.sort();g.notice="已恢复上次对局";*this=g;return true;}
};
}
