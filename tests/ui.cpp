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
 // New table: default smart columns, reversible sorting, preselection and exact touch geometry.
 start(0,123);ensureGroups();assert(organized&&smartGrouped);
 auto original=game.hands[0];auto sortedOriginal=original;std::sort(sortedOriginal.begin(),sortedOriginal.end());
 act(RESTORE);assert(organized&&!smartGrouped);act(SORT);assert(organized&&smartGrouped);
 auto restored=game.hands[0];std::sort(restored.begin(),restored.end());assert(restored==sortedOriginal);
 auto pos=handPositions();for(auto a:pos){assert(a.x>=0&&a.x+a.w<=1280);assert(a.y>=340&&a.y+a.h<=630);}
 clearSelection();int index=0,id=game.hands[0][index];auto a=pos[index];
 beginHandGesture(a.x+8,a.y+4);assert(selected[id]);
 for(int i=0;i<20;i++)moveHandGesture(a.x+8,a.y-10);
 assert(selected[id]);gestureActive=false;
 beginHandGesture(a.x+8,a.y-10);assert(!selected[id]);gestureActive=false;
 // Arrangement and selection are available during an opponent's turn, while play remains gated.
 game.turn=1;act(SORT);act(CONFIRM);assert(!selection().empty());auto unchanged=game.hands[0];act(PLAY);assert(game.hands[0]==unchanged);
 act(BACK);assert(selection().empty());game.turn=0;
 click(1090,33);assert(counterOpen);click(1090,33);assert(!counterOpen);
 click(338,680);assert(suitFilter==0);click(338,680);assert(suitFilter==-1);
 click(600,681);assert(!smartGrouped&&organized);click(780,681);assert(smartGrouped&&organized);
 // A genuine same-suit run can be selected by the bottom tool, with no auto-play.
 game.hands[0]={4,8,12,16,20};game.level=2;game.table={};suitFilter=-1;flushIndex=-1;clearSelection();
 act(FLUSH_FIND);assert(selection().size()==5);assert(identify(selection(),2).kind==FLUSH);assert(game.history.empty());
 // Sound settings persist independent volumes and the diagnostics menu is reachable.
 start(0,124);act(PAUSE);focus=2;act(CONFIRM);assert(modal==2);focus=6;musicVolume=40;act(RIGHT);assert(musicVolume==50);act(LEFT);assert(musicVolume==40);act(CONFIRM);assert(musicVolume==50);
 click(600,164+7*51+20);assert(focus==7&&sfxVolume==90);act(BACK);act(PAUSE);focus=4;act(CONFIRM);assert(modal==11);act(BACK);assert(!modal);
 // Remaining rendering is covered by real screenshot capture.
 std::remove((base+"session.dat").c_str());std::remove((base+"profile.txt").c_str());SDL_Quit();
 std::cout<<"PASS: mode setup, touch actions, hint cycling, legal play, nested pause clock, history paging, result navigation\n";
}
