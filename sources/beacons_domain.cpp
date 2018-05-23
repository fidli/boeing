extern "C" void * __cdecl memset(void *, int, size_t);
#pragma intrinsic(memset)
extern "C" void * __cdecl memcpy(void *, const void *, size_t);
#pragma intrinsic(memcpy)

extern "C"{
#pragma function(memset)
    void * memset(void * dest, int c, size_t count)
    {
        char * bytes = (char *) dest;
        while (count--)
        {
            (*bytes) = (char) c;
            (*bytes++);
        }
        return dest;
    }
    
#pragma function(memcpy)
    void *memcpy(void *dest, const void *src, size_t count)
    {
        char *dest8 = (char *)dest;
        const char *src8 = (const char *)src;
        while (count--)
        {
            *dest8++ = *src8++;
        }
        return dest;
    }
}
extern "C"{
    int _fltused;
};

#include "common.h"
#define PERSISTENT_MEM MEGABYTE(1)
#define TEMP_MEM MEGABYTE(98)
#define STACK_MEM MEGABYTE(1)



#include "util_mem.h"
#include "util_time.h"
#include "util_string.cpp"
#include "xbs2.cpp"




struct State{
    XBS2Handle * coordinator;
    XBS2Handle serials[4];
    bool  inited;
};

State * state;

extern "C" __declspec(dllexport) bool initDomain(void * platformMemory){
    initMemory(platformMemory);
    if(!initTime()) return false;
    state = (State *) platformMemory;
    ASSERT(sizeof(State) <= PERSISTENT_MEM);
    
    char coms[4][6] = {"COM5", "COM6", "COM8", "COM9"};
    char coordinator[] = "400A3EF2";
    
    char path[10];
    if(!state->inited){
        for(uint8 serialIndex = 0; serialIndex < ARRAYSIZE(coms); serialIndex++){
            XBS2Handle * beacon = &state->serials[serialIndex];
            //this is actually windows specific, ignore for now
            sprintf(path, "\\\\?\\%5s", coms[serialIndex]);
            if(openHandle(path, beacon)){
                if(xbs2_detectAndSetStandardBaudRate(beacon)){
                    if (!xbs2_initModule(beacon)) return false;
                    if (!xbs2_readValues(beacon)) return false;
                    if(!strcmp_n(coordinator, beacon->sidLower, 8)){
                        state->coordinator = beacon;
                    }
                    continue;
                }
            }
            return false;
        }
    }
    state->inited = true;
    if(state->inited){
        //find coordinator and reset network
        while(!xbs2_initNetwork(state->coordinator));
        
        //reset network on others
        for(uint8 serialIndex = 0; serialIndex < ARRAYSIZE(coms); serialIndex++){
            XBS2Handle * beacon = &state->serials[serialIndex];
            if(beacon != state->coordinator){
                while(!xbs2_initNetwork(state->coordinator));
            }
        }
        //all set
        //check network topology?
    }
    return state->inited;
}

extern "C" __declspec(dllexport) void iterateDomain(){
    /*
    char buffer[10] = {};
    
    xbs2_transmitMessage(&serial2, "400A3EF2", "ahoj\r");
    
    waitForAnyMessage(&serial, buffer);
    
    int i = 0;
    
    */
    
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL,DWORD fdwReason,LPVOID lpReserved)
{
    
    return 1;
}


BOOL WINAPI _DllMainCRTStartup(HINSTANCE hinstDLL,DWORD fdwReason,LPVOID lpReserved)
{
    return 1;
    return DllMain(hinstDLL,fdwReason,lpReserved);
}
