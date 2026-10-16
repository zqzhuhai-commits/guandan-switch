#include "../source/engine.hpp"
#include <cassert>
#include <iostream>
using namespace gd;
int c(int r,int s=0,int d=0){return d*54+(r-2)*4+s;}
int main(){
 for(int r=2;r<=14;r++)for(int s=0;s<4;s++)assert(Card{c(r,s,0)}.suit()==Card{c(r,s,1)}.suit());
 assert(identify({c(3)},2).kind==SINGLE);
 assert(identify({c(4),c(4,1)},2).kind==PAIR);
 assert(identify({c(4),c(2,1)},2).kind==PAIR);
 assert(identify({c(4),c(4,1),c(4,2),c(8),c(8,1)},2).kind==FULLHOUSE);
 assert(identify({c(3),c(4,1),c(5,2),c(6,3),c(7)},2).kind==STRAIGHT);
 assert(identify({c(3),c(4),c(5),c(6),c(7)},2).kind==FLUSH);
 assert(identify({c(14),c(2,2),c(3,1),c(4),c(5,3)},7).high==5);
 assert(identify({c(12),c(13),c(14),c(2),c(3)},7).kind==NONE);
 assert(identify({c(3),c(3,1),c(4),c(4,1),c(5),c(5,1)},2).kind==PAIRS);
 assert(identify({c(3),c(3,1),c(3,2),c(4),c(4,1),c(4,2)},2).kind==TRIPLES);
 assert(identify({52,53,106,107},2).kind==JOKERS);
 assert(identify({52,c(2,1)},2).kind==NONE);
 Move fl=identify({c(3),c(4),c(5),c(6),c(7)},2);
 Move b5=identify({c(8),c(8,1),c(8,2),c(8,3),c(8,0,1)},2);
 Move b6=identify({c(8),c(8,1),c(8,2),c(8,3),c(8,0,1),c(8,1,1)},2);
 assert(beats(fl,b5));assert(beats(b6,fl));
 assert(identify({c(9,1),c(9,1,1)},9).high==15);
 std::vector<int> ten;for(int d=0;d<2;d++)for(int suit=0;suit<4;suit++)ten.push_back(c(8,suit,d));ten.push_back(c(2,1));ten.push_back(c(2,1,1));
 Move decuple=identify(ten,2);assert(decuple.kind==BOMB&&decuple.cards.size()==10);assert(beats(identify({52,53,106,107},2),decuple));
 Game g;g.newMatch(0,12);assert(!g.play(0,{g.hands[0][0],g.hands[0][0]}));assert(!g.pass(0));
 // Finished leader transfers a cleared trick to their partner.
 g.hands={std::vector<int>{},std::vector<int>{0},std::vector<int>{4},std::vector<int>{8}};g.table={SINGLE,17,{53}};g.last=0;g.turn=1;g.passes=0;assert(g.pass(1));assert(g.pass(2));assert(g.pass(3));assert(g.turn==2&&g.table.kind==NONE);
 // Complete hundreds of deals and continuous tribute/level transitions.
 int deals=0,actions=0;
 for(int mode=0;mode<4;mode++)for(int seed=1;seed<=16;seed++){
  Game x;x.newMatch(mode,seed);x.difficulty=seed%3;
  for(int r=0;r<4;r++){
   std::set<int> ids;for(auto&h:x.hands)for(int id:h)ids.insert(id);assert(ids.size()==108);
   for(int n=0;!x.over&&n<1600;n++){int before=0;for(auto&h:x.hands)before+=h.size();int p=x.turn;Move m=x.hint(p);if(m.kind!=NONE){assert(beats(m,x.table));assert(x.play(p,m.cards));int after=0;for(auto&h:x.hands)after+=h.size();assert(after==before-int(m.cards.size()));}else assert(x.pass(p));assert(x.valid());actions++;}
   assert(x.over);assert(x.finish.size()==4);assert(x.save("/tmp/gd-save-test"));Game y;assert(y.load("/tmp/gd-save-test"));assert(y.hands==x.hands&&y.level==x.level&&y.finish==x.finish);deals++;x.deal();
  }
 }
 std::cout<<"PASS: recognition, wildcard, bomb hierarchy, illegal actions, lead transfer, save/load; "<<deals<<" AI deals, "<<actions<<" actions\n";
}
