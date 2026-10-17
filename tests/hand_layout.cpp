#include "../source/hand_layout.hpp"
#include <cassert>
#include <iostream>
using namespace gd;
void check(const std::vector<int>&hand,int level,bool smart){
 auto groups=arrangeHand(hand,level,smart);std::vector<int>out;
 for(auto&g:groups){assert(!g.cards.empty());out.insert(out.end(),g.cards.begin(),g.cards.end());if(g.kind!=NONE){auto m=identify(g.cards,level);assert(m.kind!=NONE);if(g.kind==FLUSH)assert(m.kind==FLUSH);}}
 auto expected=hand;std::sort(expected.begin(),expected.end());std::sort(out.begin(),out.end());assert(expected==out);
}
int main(){
 // A failed straight-flush attempt must return every card, including duplicate decks.
 for(unsigned seed=1;seed<=512;seed++){Game g;g.newMatch(0,seed);for(int l:{2,4,14})for(const auto&h:g.hands){check(h,l,true);check(h,l,false);}}
 for(unsigned seed=1;seed<=24;seed++){Game g;g.newMatch(1,seed);int turns=0;while(!g.over&&turns++<512){auto groups=arrangeHand(g.hands[g.turn],g.level,true);std::vector<int>h;for(auto&group:groups)h.insert(h.end(),group.cards.begin(),group.cards.end());g.hands[g.turn]=h;assert(g.valid());g.autoStep();}assert(g.over);}
 std::cout<<"PASS: 12288 hand layouts preserve every card; 24 full games after repeated regrouping\n";
}
