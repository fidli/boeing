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
    XBS2Handle serials[4];
    
};

State * state;

extern "C" __declspec(dllexport) bool initDomain(void * platformMemory){
    initMemory(platformMemory);
    if(!initTime()) return false;
    state = (State *) platformMemory;
    ASSERT(sizeof(State) <= PERSISTENT_MEM);
    
    char coms[4][6] = {"COM5", "COM6", "COM7", "COM8"};
    char coordinator[] = "400A3EF2";
    
    char path[10];
    for(uint8 serialIndex = 0; serialIndex < ARRAYSIZE(coms); serialIndex++){
        XBS2Handle * beacon = &state->serials[serialIndex];
        //this is actually windows specific, ignore for now
        sprintf(path, "\\\\?\\%5s", coms[serialIndex]);
        if(openHandle(path, beacon)){
            if(setRate(beacon, 9600)){
                if(clearSerialPort(beacon)){
                    xbs2_setup(beacon);
                    continue;
                }
            }
            
        }
        return false;
    }
    return true;
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
