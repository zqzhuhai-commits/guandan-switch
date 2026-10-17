// Use libnx's startup order with SD diagnostics before the AM/HID service calls.
// Runtime initialization mirrors switchbrew/libnx nx/source/runtime/init.c (ISC).
#ifdef __SWITCH__
#include <switch.h>
#include <cstdio>
#include <sys/stat.h>
extern "C" void __libnx_init_time(void);
extern "C" void __nx_win_init(void) __attribute__((weak));
static void startupResult(const char*stage,Result rc){
 std::FILE*f=std::fopen("sdmc:/switch/GuandanSwitch/startup-v0.5.2.log","a");
 if(f){std::fprintf(f,"%s: 0x%08X\n",stage,unsigned(rc));std::fclose(f);}
}
extern "C" void __appInit(void){
 Result rc=smInitialize();if(R_FAILED(rc))diagAbortWithResult(rc);
 if(!hosversionGet()){rc=setsysInitialize();if(R_SUCCEEDED(rc)){SetSysFirmwareVersion fw;rc=setsysGetFirmwareVersion(&fw);if(R_SUCCEEDED(rc))hosversionSet(MAKEHOSVERSION(fw.major,fw.minor,fw.micro));setsysExit();}}
 rc=fsInitialize();if(R_FAILED(rc))diagAbortWithResult(rc);
 rc=fsdevMountSdmc();if(R_SUCCEEDED(rc)){mkdir("sdmc:/switch",0777);mkdir("sdmc:/switch/GuandanSwitch",0777);}
 startupResult("v0.5.2 SD mount",rc);
 rc=appletInitialize();startupResult("appletInitialize",rc);if(R_FAILED(rc))diagAbortWithResult(rc);
 if(appletGetAppletType()!=AppletType_None){rc=hidInitialize();startupResult("hidInitialize",rc);if(R_FAILED(rc))diagAbortWithResult(rc);}
 rc=timeInitialize();startupResult("timeInitialize",rc);if(R_FAILED(rc))diagAbortWithResult(rc);
 __libnx_init_time();if(&__nx_win_init)__nx_win_init();startupResult("libnx services ready",0);
}
#endif
