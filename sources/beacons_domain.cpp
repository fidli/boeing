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
#include "algorithms.h"

struct State : Common{
    XBS2Handle * coordinator;
    
    struct Beacon{
        XBS2Handle serial; //31 bytes
        
        //NOTE(AK): this works for both methods
        
        uint64 tick[2];
        int64 fifoData[2][4*2000];
        int16 head[2];
        int16 tail[2];
        uint16 fifoCount[2];
        
    } beacons[4];
    
    struct Module{
        char sidLower[9];
        char id;
#if METHOD_XBPNG
        bool ping;
#endif
    } modules[2];
    
#if METHOD_XBPNG
    uint8 currentBeacon;
#endif
    
    uint32 serverMessageAccumulatedSize;
    Message serverMessage;
    
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


int getModuleIndexByName(char name){
    //'1' => 0
    //'2' => 1 ...
    return (int)(name - '1');
}

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
    connectionSettings.blocking = false;
    LOG("opening socket");
    uint8 i = 0;
    for(i = 0; i < 6; i++){
        if(openSocket(&state->beaconsSocket, &connectionSettings)) break;
        LOG("opening socket failed %d", WSAGetLastError());
        Sleep(500);
    }
    if(i == 6){
        LOG("too many tries.");
        return false;
    }
    
    LOG("PRE error: %d", WSAGetLastError());
    LOG("connecting to server");
    for(i = 0; i < 6; i++){
        if(tcpConnect(&state->beaconsSocket, state->ip, state->port)) break;
        LOG("connecting to server failed %d", WSAGetLastError());
        Sleep(500);
    }
    if(i == 6){
        LOG("too many tries.");
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
    
    
    
    for(i = 0; i < 6; i++){
        if(!(netSend(&state->beaconsSocket, &message) != NetResultType_Ok)) break;
        LOG("failed to send handshake message %d", WSAGetLastError());
        Sleep(500);
    }
    if(i == 6){
        LOG("too many tries.");
        return false;
    }
    return true;
}


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
        
#if METHOD_XBPNG
        for(uint8 moduleIndex = 0; moduleIndex < ARRAYSIZE(state->modules); moduleIndex++){
            state->modules[moduleIndex].ping = false;
        }
        state->currentBeacon = 0;
        
#endif
        state->serverMessageAccumulatedSize = 0;
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
                    XBS2InitSettings settings;
                    settings.prepareForBroadcast = true;
#if METHOD_XBPNG
                    settings.prepareForBroadcast = false;
#endif
                    while(!xbs2_initModule(beacon, &settings)){
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
        uint8 i = 0;
        for(; i < 6; i++){
            if(connectToServer()) break;
            Sleep(500);
        }
        if (i == 6){
            LOG("Connection failed.") return;
        }
        LOG("all set.");
        state->inited = true;
    }
    inited = true;
}

#if METHOD_XBPNG
extern "C" __declspec(dllexport) void pingDomainRoutine(){
    if(!inited || !state->inited) return;
    //1) timestamp
    //2) unicast ping message
    //3) await reply, timestamp and go to next beacon
    
    for(uint8 moduleIndex = 0; moduleIndex < ARRAYSIZE(state->modules); moduleIndex++){
        State::Module * module = &state->modules[moduleIndex];
        if(!module->ping) continue;
        for(uint8 beaconIndex = 0; beaconIndex < ARRAYSIZE(state->beacons); beaconIndex++){
            State::Beacon * beacon = &state->beacons[beaconIndex];
            
#if LOG_PING
            LOG("Pinging from %s to %s. Module id: %c", beacon->serial.sidLower, module->sidLower, module->id);
            LOG("Announcing ping process");
#endif
            while(!xbs2_changeAddress(&beacon->serial, module->sidLower)){
                LOG("Failed to change XB address");
                Sleep(1000);
            }
#if LOG_PING
            LOG("Addres changed.");
            LOG("Clearing pipe");
#endif
            while(!clearSerialPort(&beacon->serial));
            char msg[10];
            sprintf(msg, "%s\r", beacon->serial.sidLower);
#if LOG_PING
            LOG("Transmitting message: '%s'", beacon->serial.sidLower);
#endif
            while(!xbs2_transmitMessage(&beacon->serial, msg)){
                LOG("Failed to transmit message");
                Sleep(1000);
            }
#if LOG_PING
            LOG("Awaiting ACK. 3 seconds timeout");
#endif
            char response[2] = {'-', 0};
            //@Robustness we are not checking the respondent address, this could be anyone talking :/ lets use module id as the message and assume, that everyone else is silent
            xbs2_waitForAnyByte(&beacon->serial, response, 3);
            uint64 totalTicks;
            if(response[0] != module->id){
#if LOG_PING
                LOG("Response is not valid, sample failed, response is '%c'", response[0]);
#endif
                totalTicks = 0;
            }else{
#if LOG_PING
                LOG("Got ACK message. Sending actual ping. Logging silence.");
#endif
                response[0] = '-';
                uint64 startTick = getTick();
                xbs2_transmitByte(&beacon->serial, module->id);
                int32 length = xbs2_waitForAnyByte(&beacon->serial, response, 3);
                if(response[0] == module->id){
                    totalTicks = getTick() - startTick;
#if LOG_PING
                    LOG("Got ping. Message '%c'. Ticks elapsed: %llu", response[0], totalTicks);
#endif
                }else{
#if LOG_PING
                    if(length == 0){
                        LOG("Ping timed out.");
                    }else{
                        LOG("Bad message. Recevied: '%c' Length %d", response[0], length);
                    }
#endif
                    totalTicks = 0;
                }
                beacon->fifoData[moduleIndex][beacon->head[moduleIndex]] = totalTicks;
                beacon->head[moduleIndex] = (beacon->head[moduleIndex]+1) % ARRAYSIZE(beacon->fifoData[moduleIndex]);
                FETCH_AND_ADD(&beacon->fifoCount[moduleIndex], 1);
                
                
            }
            
        }
    }
    
    
}
#endif

#if METHOD_XBSP
extern "C" __declspec(dllexport) void beaconDomainRoutine(int index){
    if(!inited || !state->inited) return;
    //poll the xbs
    char response;
    int32 size = xbs2_waitForAnyByte(&state->beacons[index].serial, &response, 3);
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
#endif

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
#if METHOD_XBPNG
    
    NetRecvResult result;
    result.bufferLength = sizeof(Message);
    Message * message = &state->serverMessage;
    result.buffer = (char*)message;
    NetResultType resultCode;
    do{
        result.bufferLength = sizeof(Message) - state->serverMessageAccumulatedSize;
        result.buffer = ((char *) message) + state->serverMessageAccumulatedSize;
        resultCode = netRecv(&state->beaconsSocket, &result);
        state->serverMessageAccumulatedSize += result.resultLength;
        if(state->serverMessageAccumulatedSize == sizeof(Message)){
            if(message->type == MessageType_Start){
                int moduleIndex = getModuleIndexByName(message->start.id);
                
                LOG("Got Start message, id %c, target %s", message->start.id, message->start.sidLower);
                
                bool found = false;
                for(uint8 moduleIndex = 0; moduleIndex < ARRAYSIZE(state->modules); moduleIndex++){
                    if(!state->modules[moduleIndex].ping){
                        
                        strncpy(state->modules[moduleIndex].sidLower, message->start.sidLower, 9);
                        state->modules[moduleIndex].id = message->start.id;
                        state->modules[moduleIndex].ping = true;
                        found = true;
                        break;
                    }
                }
                ASSERT(found);
            }else if(message->type == MessageType_Stop){
                LOG("Got Stop message, id %c", message->stop.id);
                for(uint8 moduleIndex = 0; moduleIndex < ARRAYSIZE(state->modules); moduleIndex++){
                    if(state->modules[moduleIndex].ping && state->modules[moduleIndex].id == message->stop.id){
                        state->modules[moduleIndex].ping = false;
                        
                    }
                }
                
                
            }else{
                INV;
            }
            
            
            state->serverMessageAccumulatedSize = 0;
        }
    }while(resultCode == NetResultType_Ok && state->serverMessageAccumulatedSize != 0);
#endif
    
    
    
#if METHOD_XBSP
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
            header.data.id = (char)boeingIndex;
            NetSendSource source;
            source.bufferLength = sizeof(Message);
            source.buffer = (char*)&header;
            
            sendAndReconnect(&source);
            Sleep(1000);
        }
    }
#endif
    
#if METHOD_XBPNG
    //forward to server
    for(uint8 moduleIndex = 0; moduleIndex < 2; moduleIndex++){
        int32 sendBufferIndex = 0;
        for(uint8 beaconIndex = 0; beaconIndex < ARRAYSIZE(state->beacons); beaconIndex++){
            State::Beacon * beacon = &state->beacons[beaconIndex];
            
            //NOTE(AK): send only the most recent, as this is full information
            uint16 count = beacon->fifoCount[moduleIndex];
            if(count > 0){
                
                
                uint16 size = ARRAYSIZE(beacon->fifoData[moduleIndex]);
                uint16 tail = (beacon->tail[moduleIndex] + count - 1) % size;
                
                uint64 lastTick = beacon->fifoData[moduleIndex][tail];
                
                beacon->tail[moduleIndex] += count;
                beacon->tail[moduleIndex] %= size;
                
                FETCH_AND_ADD(&beacon->fifoCount[moduleIndex], -count);
                
                uint32 * indexPlace = (uint32 * ) &state->sendBuffer[sendBufferIndex];
                uint64 * tickPlace = (uint64 *) &state->sendBuffer[sendBufferIndex + sizeof(uint32)];
                
                *indexPlace = (uint64) beaconIndex;
                *tickPlace = lastTick;
                
                sendBufferIndex += sizeof(uint32)+sizeof(uint64);
                
            }
        }
        
        if(sendBufferIndex){
            Message header;
            header.type = MessageType_Data;
            header.data.length = sendBufferIndex;
            header.data.id = state->modules[moduleIndex].id;
            
            NetSendSource source;
            source.bufferLength = sizeof(Message);
            source.buffer = (char*)&header;
            
            
            sendAndReconnect(&source);
            
            source.bufferLength = header.data.length;
            source.buffer = state->sendBuffer;
            
            sendAndReconnect(&source);
        }
    }
#endif
    
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
