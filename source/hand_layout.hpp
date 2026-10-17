#pragma once
#include "engine.hpp"
#include <map>
namespace gd {
struct HandGroup { std::vector<int> cards; std::string label; Kind kind=NONE; };
// Work on a copy; a failed pattern can never remove or duplicate a real card.
inline std::vector<HandGroup> arrangeHand(const std::vector<int>& hand,int level,bool smart){
 std::vector<int> rest=hand;std::vector<HandGroup> groups;
 auto take=[&](Kind kind){
  for(;;){auto candidates=moves(rest,level);candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](const Move&m){return m.kind!=kind;}),candidates.end());if(candidates.empty())break;
   auto cost=[&](const Move&m){int wc=0;for(int id:m.cards)wc+=wild(id,level);return wc*1000-int(m.cards.size())*30+m.high;};
   auto m=*std::min_element(candidates.begin(),candidates.end(),[&](const Move&a,const Move&b){return cost(a)<cost(b);});
   std::sort(m.cards.begin(),m.cards.end(),[&](int a,int b){int ra=Card{a}.rank(),rb=Card{b}.rank();return ra!=rb?ra>rb:a<b;});
   std::string label=kind==BOMB?std::to_string(m.cards.size())+"炸":kindName(kind);
   groups.push_back({m.cards,label,kind});
   for(int id:m.cards)rest.erase(std::find(rest.begin(),rest.end(),id));
  }
 };
 if(smart){take(JOKERS);take(FLUSH);take(BOMB);take(TRIPLES);take(PAIRS);take(STRAIGHT);take(FULLHOUSE);}
 std::map<int,std::vector<int>,std::greater<int>> ranks;
 for(int id:rest)ranks[power(Card{id}.rank(),level)].push_back(id);
 std::vector<int> singles;
 for(auto&entry:ranks){auto cards=entry.second;std::sort(cards.begin(),cards.end(),[](int a,int b){return Card{a}.suit()!=Card{b}.suit()?Card{a}.suit()<Card{b}.suit():a<b;});
  if(smart&&cards.size()==1){singles.push_back(cards[0]);continue;}
  Kind kind=cards.size()>=4?BOMB:cards.size()==3?TRIPLE:cards.size()==2?PAIR:SINGLE;
  if(cards[0]%54>=52&&cards.size()>2)kind=NONE;
  groups.push_back({cards,cards.size()>=4&&kind==BOMB?std::to_string(cards.size())+"炸":cards.size()==3?"三张":cards.size()==2?"对子":"",kind});
 }
 for(int id:singles)groups.push_back({{id},"",SINGLE});
 return groups;
}
}
