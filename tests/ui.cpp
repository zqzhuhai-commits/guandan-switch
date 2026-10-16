// Exercise the actual controller/touch handlers with SDL's headless drivers.
#define main guandan_app_main
#include "../source/main.cpp"
#undef main
#include <cassert>
#include <iostream>
int main(){
 assert(SDL_Init(SDL_INIT_TIMER)==0);
 base="/tmp/gd-ui-test-";sound=false;
 screen=0;focus=0;modal=0;
 click(500,260);assert(modal==8&&pendingMode==0&&focus==3);
 focus=0;int oldDifficulty=game.difficulty;act(CONFIRM);assert(game.difficulty==(oldDifficulty+1)%3);
 click(600,444);assert(screen==1&&modal==0&&game.difficulty==(oldDifficulty+1)%3);
 // Invalid selection cannot mutate a hand. Hints cycle and stay legal.
 game.newMatch(0,123);resetHints();clearSelection();auto hand=game.hands[0];doPlay();assert(game.hands[0]==hand);
 doHint();auto first=selection();assert(!first.empty()&&identify(first,game.level,game.table).kind!=NONE);
 doHint();assert(hintIndex==1);auto second=selection();assert(first!=second);assert(identify(second,game.level,game.table).kind!=NONE);
 act(PLAY);assert(game.hands[0].size()==hand.size()-second.size());assert(!game.history.empty());
 // Pausing, including navigation between dialogs, preserves the turn's elapsed time.
 lastAI=SDL_GetTicks();SDL_Delay(12);act(PAUSE);Uint32 elapsed=pausedAt-lastAI;SDL_Delay(24);
 focus=2;act(CONFIRM);assert(modal==2);SDL_Delay(12);act(BACK);Uint32 resumed=SDL_GetTicks()-lastAI;assert(resumed>=elapsed&&resumed<=elapsed+8);
 // Real history button and both page directions work by touch and controller.
 for(int i=0;i<30&&!game.over;i++){game.autoStep();}
 assert(game.history.size()>5);
 act(HISTORY);assert(modal==5&&historyPage==0);click(400,580);assert(historyPage==1);act(RIGHT);assert(historyPage==0);click(1004,112);assert(modal==0);
 // Result page touch actions must distinguish review, lobby, and next deal.
 while(!game.over){game.autoStep();}
 click(800,450);assert(modal==7);act(BACK);click(640,450);assert(screen==0);
 // Every lobby mode maps to its own configuration and starts that mode.
 for(int i=0;i<4;i++){auto a=modeAreas[i];click(a.x+30,a.y+40);assert(modal==8&&pendingMode==i);act(CONFIRM);assert(screen==1&&game.mode==i);act(PAUSE);focus=1;act(CONFIRM);assert(!screen&&!modal);}
 // Training selection cannot overlap the card recorder (render covered separately).
 std::remove((base+"session.dat").c_str());std::remove((base+"profile.txt").c_str());SDL_Quit();
 std::cout<<"PASS: mode setup, touch actions, hint cycling, legal play, nested pause clock, history paging, result navigation\n";
}
