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



#include "util_mem.h"
#include "util_time.h"
#include "util_string.cpp"
#include "xbs2.cpp"

#include "util_io.cpp"
#include "util_string.cpp"


#include "messages.h"

#include "util_net.h"

LocalTime lt;
char logbuffer[1024];
#define LOG(message)  lt = getLocalTime(); sprintf(logbuffer, "[%2hu.%2hu.%4hu %2hu:%2hu:%2hu] %900s\r\n", lt.day, lt.month, lt.year, lt.hour, lt.minute, lt.second, (message)); print(logbuffer);

struct State{
    XBS2Handle * coordinator;
    XBS2Handle serials[4];
    bool  inited;
    NetSocket serverConnection;
};

State * state;

extern "C" __declspec(dllexport) bool initDomain(void * platformMemory){
    initMemory(platformMemory);
    if(!initIo()){
        return false;
    }
    if(!initTime()) return false;
    if(!initNet()) return false;
    state = (State *) platformMemory;
    ASSERT(sizeof(State) <= PERSISTENT_MEM);
    
    char coms[4][6] = {"COM3", "COM5", "COM7", "COM8"};
    char coordinator[] = "400A3EF2";
    
    char path[10];
    if(!state->inited){
#if 1
        LOG("initing network");
        for(uint8 serialIndex = 0; serialIndex < ARRAYSIZE(coms); serialIndex++){
            XBS2Handle * beacon = &state->serials[serialIndex];
            if(isHandleOpened(beacon)) continue;
            //this is actually windows specific, ignore for now
            LOG("opening handle");
            sprintf(path, "\\\\?\\%5s", coms[serialIndex]);
            LOG(path);
            if(openHandle(path, beacon)){
                if(xbs2_detectAndSetStandardBaudRate(beacon)){
                    if (!xbs2_initModule(beacon)){
                        LOG("failed to init module");
                        closeHandle(beacon);
                        return false;
                    }
                    if (!xbs2_readValues(beacon)){
                        LOG("failed to read module values");
                        closeHandle(beacon);
                        return false;
                    }
                    if(!strncmp(coordinator, beacon->sidLower, 8)){
                        state->coordinator = beacon;
                    }
                    LOG("handle opened and module inited");
                    continue;
                }
            }
            LOG("Failed to open handle");
            return false;
        }
        
        LOG("finding channel");
        //find coordinator and reset network
        while(!xbs2_initNetwork(state->coordinator));
        
        char channelMask[5];
        ASSERT(xbs2_getChannelMask(state->coordinator->channel, channelMask));
        
        LOG("found channel");
        LOG(state->coordinator->channel);
        LOG("pan id");
        LOG(state->coordinator->pan);
        
        //reset network on others
        for(uint8 serialIndex = 0; serialIndex < ARRAYSIZE(coms); serialIndex++){
            XBS2Handle * beacon = &state->serials[serialIndex];
            if(beacon != state->coordinator){
                LOG("joining network with");
                LOG(beacon->sidLower);
                while(!xbs2_initNetwork(beacon, channelMask) || (strncmp(state->coordinator->pan, beacon->pan, 5) != 0 || strncmp(state->coordinator->channel, beacon->channel, 3) != 0));
                beacon->frequency = state->coordinator->frequency;
                LOG("success");
            }
        }
        //all set
        //check network topology?
#endif
        LOG("XBS2 set. Connecting to the server");
        //todo connect to server, annonce settings
        NetSocketSettings connectionSettings;
        connectionSettings.reuseAddr = true;
        connectionSettings.blocking = true;
        LOG("opening socket");
        if(!openSocket(&state->serverConnection, &connectionSettings)){
            LOG("opening socket failed");
            return false;
        }
        LOG("connecting to server");
        if(!tcpConnect(&state->serverConnection, "10.0.0.10", "25555")){
            LOG("connecting to server failed");
            return false;
        }
        
        LOG("connection successfull. Sending handshake message");
        
        Message handshake;
        
        handshake.type = MessageType_Init;
        handshake.init.clientType = ClientType_Beacon;
        handshake.init.beacon.frequency = state->coordinator->frequency;
        strncpy(handshake.init.beacon.channel, state->coordinator->channel, 3);
        strncpy(handshake.init.beacon.pan, state->coordinator->pan, 5);
        
        NetSendSource message;
        message.buffer = (char*)&handshake;
        message.bufferLength = sizeof(Message);
        
        if(netSend(&state->serverConnection, &message) != NetResultType_Ok){
            LOG("failed to send handshake message");
        }
        
        LOG("all set.");
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
