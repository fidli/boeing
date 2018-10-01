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

#undef ASSERT
#define ASSERT(expression)  if(!(expression)) {printf("ASSERT failed on line %d file %s\n", __LINE__, __FILE__); *(int *)0 = 0;}


#include "util_mem.h"
#include "util_time.h"
#include "util_string.cpp"
#include "xbs2.cpp"

#include "util_io.cpp"
#include "util_string.cpp"


#include "messages.h"

#include "util_net.h"

#include "beacons_common.h"

LocalTime lt;
char logbuffer[1024];
#define LOG(message,...)  lt = getLocalTime(); sprintf(logbuffer, "[%02hu.%02hu.%04hu %02hu:%02hu:%02hu] %900s\r\n", lt.day, lt.month, lt.year, lt.hour, lt.minute, lt.second, (message)); printf(logbuffer, __VA_ARGS__);

struct State : Common{
    XBS2Handle * coordinator;
    
    struct{
        XBS2Handle serial; //31 bytes
        uint64 tick[2];
        int64 fifoData[2][4*2000];
        int16 head[2];
        int16 tail[2];
        uint16 fifoCount[2];
    } beacons[4];
    
    char sendBuffer[60000];
    
    bool  inited;
    
    FileWatchHandle configFileWatch;
    char coms[4][6];
    char coordinatorSid[9];
};

State * state;

bool inited = false;

#include "util_config.cpp"

const char * configPath = "data/beacons.config";




static bool parseConfig(const char * line){
    if(!strncmp("ip", line, 2)){
        memset(state->ip, 0, 16);
        memset(state->port, 0, 6);
        return sscanf(line, "ip %16[^ ] %6[^ \r\n]", state->ip, state->port) == 2;
    }else if(!strncmp("com", line, 3)){
        memset(state->coms[0], 0, 6);
        memset(state->coms[1], 0, 6);
        memset(state->coms[2], 0, 6);
        memset(state->coms[3], 0, 6);
        return sscanf(line, "com %6[^ ] %6[^ ] %6[^ ] %6[^ \r\n]", &state->coms[0], &state->coms[1], &state->coms[2], &state->coms[3]) == 4;
    }
    else if(!strncmp("coord", line, 5)){
        memset(state->coordinatorSid, 0, 9);
        return sscanf(line, "coord %9[^\r\n ]", &state->coordinatorSid) == 1;
    }
    return true;
}

static bool connectToServer(){
    
    closeSocket(&state->beaconsSocket);
    
    NetSocketSettings connectionSettings;
    connectionSettings.reuseAddr = true;
    connectionSettings.blocking = true;
    LOG("opening socket");
    if(!openSocket(&state->beaconsSocket, &connectionSettings)){
        LOG("opening socket failed");
        return false;
    }
    
    LOG("PRE error: %d", WSAGetLastError());
    LOG("connecting to server");
    if(!tcpConnect(&state->beaconsSocket, state->ip, state->port)){
        LOG("connecting to server failed %d", WSAGetLastError());
        return false;
    }
    LOG("POST error: %d", WSAGetLastError());
    
    LOG("connection successfull. Sending handshake message");
    
    Message handshake;
    
    handshake.type = MessageType_Init;
    handshake.init.clientType = ClientType_Beacon;
    handshake.init.beacon.frequencyKhz = state->coordinator->frequency;
    handshake.init.beacon.timeDivisor = getTickDivisor();
    strncpy(handshake.init.beacon.channel, state->coordinator->channel, 3);
    strncpy(handshake.init.beacon.pan, state->coordinator->pan, 5);
    for(uint8 beaconIndex = 0; beaconIndex < ARRAYSIZE(state->beacons); beaconIndex++){
        strncpy(handshake.init.beacon.sidLower[beaconIndex], state->beacons[beaconIndex].serial.sidLower, 9);
    }
    
    NetSendSource message;
    message.buffer = (char*)&handshake;
    message.bufferLength = sizeof(Message);
    
    
    
    if(netSend(&state->beaconsSocket, &message) != NetResultType_Ok){
        LOG("failed to send handshake message");
        return false;
    }
    return true;
}

/*
extern "C" __declspec(dllexport) void softResetBeacons(){
    if(!inited || !domainState->inited) return;
    printf("soft reset inited\n");
}
*/
extern "C" __declspec(dllexport) void initDomainRoutine(void * platformMemory){
    initMemory(platformMemory);
    if(!initIo()){
        return;
    }
    if(!initTime()) return;
    if(!initNet()) return;
    state = (State *) platformMemory;
    ASSERT(sizeof(State) <= PERSISTENT_MEM);
    
    char path[10];
    if(!state->inited){
        bool result = true;
        result &= watchFile(configPath, &state->configFileWatch);
        ASSERT(result);
        if(hasFileChanged(&state->configFileWatch)){
            result &= loadConfig(configPath, parseConfig);
        }else{
            ASSERT(false);
        }
        
#if 1
        LOG("initing network");
        for(uint8 serialIndex = 0; serialIndex < ARRAYSIZE(state->coms); serialIndex++){
            XBS2Handle * beacon = &state->beacons[serialIndex].serial;
            if(isHandleOpened(beacon)) continue;
            //this is actually windows specific, ignore for now
            LOG("opening handle");
            sprintf(path, "\\\\?\\%5s", state->coms[serialIndex]);
            LOG(path);
            if(openHandle(path, beacon)){
                LOG("handle opened. detecting baudrate");
                if(xbs2_detectAndSetStandardBaudRate(beacon)){
                    LOG("baudrate detected %u", beacon->baudrate);
                    LOG("initing module");
                    while(!xbs2_initModule(beacon)){
                        LOG("failed to init module");
                        wait(10);
                        LOG("retrying");
                    }
                    LOG("reading values");
                    while(!xbs2_readValues(beacon)){
                        LOG("failed to read module values");
                        wait(10);
                        LOG("retrying");
                    }
                    if(!strncmp(state->coordinatorSid, beacon->sidLower, 8)){
                        LOG("this is coordinator");
                        state->coordinator = beacon;
                    }
                    LOG("handle opened and module inited");
                    continue;
                }
            }
            LOG("Failed to open handle");
            return;
        }
        ASSERT(state->coordinator != NULL);
        LOG("finding channel");
        //find coordinator and reset network
        while(!xbs2_initNetwork(state->coordinator));
        
        char channelMask[5];
        ASSERT(xbs2_getChannelMask(state->coordinator->channel, channelMask));
        
        LOG("found channel: %s", state->coordinator->channel);
        LOG("pan id: %s", state->coordinator->pan);
        
        //reset network on others
        for(uint8 serialIndex = 0; serialIndex < ARRAYSIZE(state->coms); serialIndex++){
            XBS2Handle * beacon = &state->beacons[serialIndex].serial;
            if(beacon != state->coordinator){
                LOG("joining network with %s", beacon->sidLower);
                while(!xbs2_initNetwork(beacon, channelMask) || (strncmp(state->coordinator->pan, beacon->pan, 5) != 0 || strncmp(state->coordinator->channel, beacon->channel, 3) != 0));
                beacon->frequency = state->coordinator->frequency;
                LOG("success");
            }
        }
        //all set
        //check network topology?
#endif
        LOG("XBS2 set");
        
        if(!connectToServer()){
            return;
        }
        LOG("all set.");
        state->inited = true;
    }
    inited = true;
}

extern "C" __declspec(dllexport) void beaconDomainRoutine(int index){
    if(!inited || !state->inited) return;
    //poll the xbs
    char response;
    int32 size = waitForAnyByte(&state->beacons[index].serial, &response, 3);
    if(size > 0){
        uint8 moduleId = response - '1'; 
        uint64 oldTick = state->beacons[index].tick[moduleId];
        state->beacons[index].tick[moduleId] = getTick();
        uint64 res = state->beacons[index].tick[moduleId] - oldTick;
        
        state->beacons[index].fifoData[moduleId][state->beacons[index].head[moduleId]] = res;
        state->beacons[index].head[moduleId] = (state->beacons[index].head[moduleId]+1) % ARRAYSIZE(state->beacons[index].fifoData[moduleId]);
        FETCH_AND_ADD(&state->beacons[index].fifoCount[moduleId], 1);
        //printf("[%8s] %lf\n", state->beacons[index].serial.sidLower, res);
        //xbs2_readLatestReceiveInfo(&state->beacons[index].serial);
    }else{
        //printf("[%d][%f] no response for 3 seconds\n", index, getProcessCurrentTime());
    }
}

static void sendAndReconnect(const NetSendSource * source){
    NetResultType result;
    result = netSend(&state->beaconsSocket, source);
    if(result != NetResultType_Ok){
        printf("send error, trying reconnect\n");
        connectToServer();
        Sleep(1000);
    }
    
}

extern "C" __declspec(dllexport) void iterateDomainRoutine(){
    if(!inited || !state->inited) return;
    //forward to server
    for(uint8 boeingIndex = 0; boeingIndex < 2; boeingIndex++){
        uint16 count = -1;
        
        for(uint8 beaconIndex = 0; beaconIndex < ARRAYSIZE(state->beacons); beaconIndex++){
            count = MIN(state->beacons[beaconIndex].fifoCount[boeingIndex], count);
        }
        ASSERT(count*32 < ARRAYSIZE(state->sendBuffer));
        
        if(count > 0){
            for(uint8 beaconIndex = 0; beaconIndex < ARRAYSIZE(state->beacons); beaconIndex++){
                uint16 size = ARRAYSIZE(state->beacons[beaconIndex].fifoData[boeingIndex]);
                uint16 tail = (state->beacons[beaconIndex].tail[boeingIndex] + count) % size;
                
                for(uint16 i = state->beacons[beaconIndex].tail[boeingIndex], j = beaconIndex; i != tail; i = (i+1) % size, j += ARRAYSIZE(state->beacons))
                {
                    ((uint64*)state->sendBuffer)[j] = state->beacons[beaconIndex].fifoData[boeingIndex][i];
                }
                state->beacons[beaconIndex].tail[boeingIndex] += count;
                state->beacons[beaconIndex].tail[boeingIndex] %= size;
                FETCH_AND_ADD(&state->beacons[beaconIndex].fifoCount[boeingIndex], -count);
            }
            
            
            
            Message header;
            header.type = MessageType_Data;
            header.data.length = count*32;
            header.data.boeingId = boeingIndex;
            
            NetSendSource source;
            source.bufferLength = sizeof(Message);
            source.buffer = (char*)&header;
            
            
            sendAndReconnect(&source);
            
            source.bufferLength = header.data.length;
            source.buffer = state->sendBuffer;
            
            sendAndReconnect(&source);
            
        }else{
            //heartbeat to redetermine connection
            Message header;
            header.type = MessageType_Data;
            header.data.length = 0;
            header.data.boeingId = boeingIndex;
            NetSendSource source;
            source.bufferLength = sizeof(Message);
            source.buffer = (char*)&header;
            
            sendAndReconnect(&source);
            Sleep(1000);
        }
    }
    
    
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
