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
            if(isHandleOpened(beacon)) continue;
            //this is actually windows specific, ignore for now
            sprintf(path, "\\\\?\\%5s", coms[serialIndex]);
            if(openHandle(path, beacon)){
                if(xbs2_detectAndSetStandardBaudRate(beacon)){
                    if (!xbs2_initModule(beacon)){
                        closeHandle(beacon);
                        return false;
                    }
                    if (!xbs2_readValues(beacon)){
                        closeHandle(beacon);
                        return false;
                    }if(!strcmp_n(coordinator, beacon->sidLower, 8)){
                        state->coordinator = beacon;
                    }
                    continue;
                }
            }
            return false;
        }
        
        //find coordinator and reset network
        while(!xbs2_initNetwork(state->coordinator));
        
        char channelMask[5];
        /**
        bit flag (channel)
        0 (0x0B) 4 (0x0F) 8 (0x13) 12 (0x17)
            1 (0x0C) 5 (0x10) 9 (0x14) 13 (0x18)
            2 (0x0D) 6 (0x11) 10 (0x15) 14 (0x19)
            3 (0x0E) 7 (0x12) 11 (0x16) 15 (0x1A)
            
            */
        if(!strcmp_n(state->coordinator->channel, "B", 2)){
            strcpy(channelMask, "0001");
        }else if(!strcmp_n(state->coordinator->channel, "C", 2)){
            strcpy(channelMask, "0002");
        }else if(!strcmp_n(state->coordinator->channel, "D", 2)){
            strcpy(channelMask, "0004");
        }else if(!strcmp_n(state->coordinator->channel, "E", 2)){
            strcpy(channelMask, "0008");
        }else if(!strcmp_n(state->coordinator->channel, "F", 2)){
            strcpy(channelMask, "0010");
        }else if(!strcmp_n(state->coordinator->channel, "10", 2)){
            strcpy(channelMask, "0020");
        }else if(!strcmp_n(state->coordinator->channel, "11", 2)){
            strcpy(channelMask, "0040");
        }else if(!strcmp_n(state->coordinator->channel, "12", 2)){
            strcpy(channelMask, "0080");
        }else if(!strcmp_n(state->coordinator->channel, "13", 2)){
            strcpy(channelMask, "0100");
        }else if(!strcmp_n(state->coordinator->channel, "14", 2)){
            strcpy(channelMask, "0200");
        }else if(!strcmp_n(state->coordinator->channel, "15", 2)){
            strcpy(channelMask, "0400");
        }else if(!strcmp_n(state->coordinator->channel, "16", 2)){
            strcpy(channelMask, "0800");
        }else if(!strcmp_n(state->coordinator->channel, "17", 2)){
            strcpy(channelMask, "1000");
        }else if(!strcmp_n(state->coordinator->channel, "18", 2)){
            strcpy(channelMask, "2000");
        }else if(!strcmp_n(state->coordinator->channel, "19", 2)){
            strcpy(channelMask, "4000");
        }else if(!strcmp_n(state->coordinator->channel, "1A", 2)){
            strcpy(channelMask, "8000");
        }
        
        //reset network on others
        for(uint8 serialIndex = 0; serialIndex < ARRAYSIZE(coms); serialIndex++){
            XBS2Handle * beacon = &state->serials[serialIndex];
            if(beacon != state->coordinator){
                while(!xbs2_initNetwork(beacon, channelMask) || (strcmp_n(state->coordinator->pan, beacon->pan, 5) != 0 || strcmp_n(state->coordinator->channel, beacon->channel, 3) != 0));
                beacon->frequency = state->coordinator->frequency;
            }
        }
        //all set
        //check network topology?
        state->inited = true;
        
    }
    return state->inited;
}

extern "C" __declspec(dllexport) void iterateDomain(){
    
    
    int i = 0;
    
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
