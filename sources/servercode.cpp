#ifndef CRT_PRESENT
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
#endif

#include "winsock2.h"
#include "ws2tcpip.h"

#include "windows_types.h"
#include "common.h"

#include "servercode_memory.h"

#include "servercode_common.h"

#define sleep(n) Sleep((n)*1000);

#include "util_mem.h"

#include "util_string.cpp"
#include "util_io.cpp"
#include "util_net.h"
#include "util_thread.h"
#include "util_font.cpp"
#include "util_math.cpp"
#include "util_graphics.cpp"
#include "util_conv.cpp"
#include "mpu6050.cpp"
#include "messages.h"

#include "windows_net.cpp"
#include "windows_thread.cpp"
#include "windows_filesystem.cpp"
#include "windows_io.cpp"
#include "windows_time.cpp"


#include "server_input.h"

#include "util_physics.cpp"

#include "algorithms.h"

union MemsData{
    struct{
        int16 accX;
        int16 accY;
        int16 accZ;
        
        int16 gyroX;
        int16 gyroY;
        int16 gyroZ;
    };   
    struct{
        struct{
            int16 v[3];
        } acc;
        struct{
            int16 v[3];
        } gyro;
    };    
};

#pragma pack(push, 4)
struct XbData{
#if METHOD_XBSP
    int64 delay[4];
#endif
#if METHOD_XBPNG
    //NOTE(AK): padding
    uint32 beaconIndex;
    uint64 lastTick;
#endif
    float64 timeReceived;
};
#pragma pack(pop)


enum LocalisationType{
    LocalisationType_Invalid,
    
    LocalisationType_Mems_Ori,
    LocalisationType_Mems_Loco,
    LocalisationType_Mems_Comb,
    LocalisationType_Xb,
    
    LocalisationTypeCount
};

struct ProgramContext : Common{
    bool inited;
    BitmapFont font;
    NetSocket serverSocket;
    NetSocket boeingSocket[2];
    NetSocket beaconsSocket;
    struct Module{
        
        bool calibrated;
        
        char name;
        MPU6050Settings settings;
        char sidLower[9];
        
        MemsData memsData[30000];
        int32 memsTailIndex;
        int32 memsHeadIndex;
        int32 memsStepsAvailable;
        
        XbData xbData[2000];
        int32 xbTailIndex;
        int32 xbHeadIndex;
        int32 xbStepsAvailable;
        
        
        
#if METHOD_XBPNG
        uint32 xbFrames[4];
        uint64 lastTicks[4];
        uint64 calibrationTicks[4][1000];
        float64 hwBias[4];
        Box_64 xbAABB;
#endif
        dv3_64 calibrationAcc64[20000];
        dv3_64 calibrationGyro64[20000];
        uint32 calibrationCount;
        
        
        dv3_64 defaultGyroBias64;
        dv3_64 defaultAccBias64;
        dv3_64 defaultGyroVar64;
        dv3_64 defaultAccVar64;
        
                
        v3_64 rotationAngles64;
        v3_64 acceleration64;
        v3_64 velocity64;
        
        dv3_64 defaultAccSum64;
        dv3_64 defaultVelSum64;
        dv3_64 defaultGyroSum64;
        
        dv3_64 accSum64;
        dv3_64 velSum64;
        dv3_64 gyroSum64;
        
        
        v3_64 worldPosition64;
        v3_64 worldOrientation64;
        
        v3_64 defaultWorldOrientation64;
        v3_64 defaultWorldPosition64;
        
        dv3_64 gyroBias64;
        dv3_64 accBias64;
        dv3_64 gyroVar64;
        dv3_64 accVar64;
        
        uint32 physicalFrame;
        
#if METHOD_XBSP
        uint32 xbFrame;
        float32 xbPeriod;        
#endif
        
        char memsDataBuffer[4096];
        char xbDataBuffer[4096];
        
        
        
        Message lastMemsMessage;
        
        bool run;
        uint32 accumulatedSize;
        bool haltProcessing;
        bool processHalted;
        bool beaconsHalted;
        bool boeingHalted;
        
        float64 timeConnected;
    } modules[2];
    
    struct Beacon{
        uint16 frequencyKhz;
        uint64 timeDivisor;
        char channel[4];
        char pan[5];
        v3_64 worldPosition64;
        float64 moduleDistance64[2];
        char sidLower[9];
        
        
    } beacons[4];
    
    bool beaconsRun;
    
    bool record;
    bool wasRecord;
    
    bool drawHelp;
    
    bool replay;
    bool restartReplay;
    
    Image * renderingTarget;
    float32 accumulator;
    uint32 beaconsAccumulatedSize;
    Message lastBeaconsMessage;
    uint32 newClientAccumulatedSize;
    
    LocalisationType localisationType;
    
    uint8 activeModuleIndex;
    
    FileWatchHandle configFileWatch;
    
    struct Record{
        struct {
            v3_64 defaultWorldOrientation64;
            v3_64 defaultWorldPosition64;
            dv3_64 defaultAccSum64;
            dv3_64 defaultVelSum64;
            dv3_64 defaultGyroSum64;
            
            uint32 memsWarmedUpFrame;
            uint32 memsCalibrationFrame;
            uint32 xbWarmedUpFrame;
            uint32 xbCalibrationFrame;
            uint32 memsStartingFrame;
#if METHOD_XBSP
            uint32 xbStartingFrame;
#endif
#if METHOD_XBPNG
            uint32 xbStartingFrames[4];
#endif
        } defaultModule[2];
        LocalTime startTime;
        struct {
            uint32 recordDataMemsCount;
            uint32 recordDataXbCount;
            //1khz - 1000/second 5min record = 
            MemsData mems[300000];
            XbData xb[3000];

            int32 memsRecordHeadIndex;
            int32 xbRecordHeadIndex;
            
            //replay
            uint32 recordDataMemsIndex;
            uint32 recordDataXbIndex;
        } data[2];
        
    } recordData;
    
    char tempRecordContents[MEGABYTE(50)];
};


const uint32 memsCalibrationFrame = 30000;
const uint32 memsWarmedUpFrame = 20000;

#if METHOD_XBPNG
const uint32 xbWarmedUpFrame = 1;
const uint32 xbCalibrationFrame = 5;
#endif


ProgramContext * programContext;
bool inited = false;

void resetBeacons(){
    //programContext->beaconsAccumulatedSize = 0;
}

void resetModule(int index, bool haltBoeing = true, bool recalibrate = true){
    ProgramContext::Module * module = &programContext->modules[index];
    
    module->haltProcessing = true;
    if(!programContext->replay) while(!module->processHalted){};
    if(!programContext->replay) while(!module->beaconsHalted){};
    if(!programContext->replay && haltBoeing) while(!module->boeingHalted){};
    
    module->memsTailIndex = 0;
    module->memsHeadIndex = 0;
    module->memsStepsAvailable = 0;
    
    module->xbTailIndex = 0;
    module->xbHeadIndex = 0;
    module->xbStepsAvailable = 0;
    
    if(recalibrate || !module->calibrated){
        module->calibrated = false;
        module->physicalFrame = 0;
        module->accBias64 = module->defaultAccBias64;
        module->gyroBias64 = module->defaultGyroBias64;
        module->accVar64 = module->defaultAccVar64;
        module->gyroVar64 = module->defaultGyroVar64;
    }else{
        module->physicalFrame = memsCalibrationFrame + 1;
    }
    
#if METHOD_XBSP
    module->xbFrame = 0;
#endif
#if METHOD_XBPNG 
    for(uint8 i = 0; i < ARRAYSIZE(module->xbFrames); i++){
        module->xbFrames[i] = 0;
    }
    module->xbAABB.lowerCorner = module->xbAABB.upperCorner = V3_64(0, 0, 0);
#endif
    
    
    
    module->worldOrientation64 = module->defaultWorldOrientation64;
    module->worldPosition64 = module->defaultWorldPosition64;
    module->accSum64 = module->defaultAccSum64;
    module->velSum64 = module->defaultVelSum64;
    module->gyroSum64 = module->defaultGyroSum64;
    
    for(uint8 beaconIndex = 0; beaconIndex < ARRAYSIZE(programContext->beacons); beaconIndex++){
        programContext->beacons[beaconIndex].moduleDistance64[index] = length64(V2_64(module->worldPosition64.x, module->worldPosition64.y) - V2_64(programContext->beacons[beaconIndex].worldPosition64.x, programContext->beacons[beaconIndex].worldPosition64.y));
    }
    
    if(programContext->replay){
        module->timeConnected = 0;
    }
    
    programContext->recordData.data[index].recordDataXbIndex = 0;
    programContext->recordData.data[index].recordDataMemsIndex = 0;
    
    module->haltProcessing = false;
    
    
}

extern "C" __declspec(dllexport) void handleInputDomainRoutine(const ServerInput * input){
    if(input->boeing1){
        programContext->activeModuleIndex = 0;
    }
    if(input->boeing2){
        programContext->activeModuleIndex = 1;
    }
    
    if(input->method1){
        programContext->localisationType = LocalisationType_Mems_Ori;
    }
    if(input->method2){
        programContext->localisationType = LocalisationType_Mems_Loco;
    }
    if(input->method3){
        programContext->localisationType = LocalisationType_Mems_Comb;
    }
    if(input->method4){
        programContext->localisationType = LocalisationType_Xb;
    }
    if(input->reposition){
        if(programContext->modules[0].run){
         resetModule(0, true, false);
        }
        if(programContext->modules[1].run){
         resetModule(1, true, false);
        }
    }
    if(input->help){
        programContext->drawHelp = !programContext->drawHelp;
    }
    
    if(input->record){
        if(!programContext->replay){
            programContext->record = !programContext->record;
        }else{
            programContext->restartReplay = true;
        }
        
    }
}

extern "C" __declspec(dllexport) void boeingDomainRoutine(int index){
    if(!inited || !programContext->inited) return;
    if(programContext->replay){
        Sleep(1000);
        return;
    }
    NetRecvResult result;
    
    
    ProgramContext::Module * module = &programContext->modules[index];
    
    
    Message * wrap = &module->lastMemsMessage;
    
    
    if(programContext->keepRunning && module->run){
        
        if(module->haltProcessing){
            module->boeingHalted = true;
            while(module->haltProcessing){};
            module->boeingHalted = false;
        }
        
        
        result.bufferLength = sizeof(Message) - module->accumulatedSize;
        result.buffer = ((char *) wrap) + module->accumulatedSize;
        NetResultType resultCode = netRecv(&programContext->boeingSocket[index], &result);
        if(resultCode == NetResultType_Ok){
            if(result.resultLength != 0){
                module->accumulatedSize += result.resultLength;
                if(module->accumulatedSize == sizeof(Message)){
                    
                    if(wrap->type == MessageType_Reset){
                        resetModule(index, false);
                        module->accumulatedSize = 0;
                        return;
                    }
                    
                    if(wrap->type == MessageType_Ready ){
#if METHOD_XBPNG
                        
                        NetSendSource message;
                        message.bufferLength = sizeof(Message);
                        
                        
                        Message startCommand;
                        startCommand.type = MessageType_Start;
                        startCommand.start.id = module->name;
                        strncpy(startCommand.start.sidLower, module->sidLower, 9);
                        
                        message.buffer = (char*)&startCommand;
                        
                        while(netSend(&programContext->beaconsSocket, &message) != NetResultType_Ok){
                            
                        }
                        
                        
#endif
                        module->accumulatedSize = 0;
                        return;
                        
                    }
                    
                    ASSERT(wrap->data.length % sizeof(MemsData) == 0 && sizeof(MemsData) == 12);
                    
                    result.bufferLength = wrap->data.length; 
                    result.buffer = module->memsDataBuffer;
                    result.resultLength = 0;
                    uint16 accumulated = 0;
                    while(accumulated != wrap->data.length){
                        
                        
                        result.buffer = module->memsDataBuffer + accumulated;
                        result.bufferLength = wrap->data.length - accumulated;
                        result.resultLength = 0;
                        
                        resultCode = netRecv(&programContext->boeingSocket[index], &result);
                        accumulated += result.resultLength;
                        
                        if(resultCode != NetResultType_Ok){
                            break;
                        }
                    }
                                      
                    ASSERT(accumulated == wrap->data.length);
                    
                    for(uint32 offset = 0; offset < wrap->data.length; offset += sizeof(MemsData)){
                        
                        MemsData * target = &module->memsData[module->memsHeadIndex];
                        
                        uint32 suboffset = 0;
                        target->accX = ((int16)(*(module->memsDataBuffer + offset + suboffset)) << 8) + *(module->memsDataBuffer + offset + suboffset + 1);
                        suboffset += 2;
                        target->accY = ((int16)(*(module->memsDataBuffer + offset + suboffset)) << 8) + *(module->memsDataBuffer + offset + suboffset + 1);
                        suboffset += 2;
                        target->accZ = ((int16)(*(module->memsDataBuffer + offset + suboffset)) << 8) + *(module->memsDataBuffer + offset + suboffset + 1);
                        
                        suboffset += 2;
                        target->gyroX = ((int16)(*(module->memsDataBuffer + offset + suboffset)) << 8) + *(module->memsDataBuffer + offset + suboffset + 1);
                        suboffset += 2;
                        target->gyroY = ((int16)(*(module->memsDataBuffer + offset + suboffset)) << 8) + *(module->memsDataBuffer + offset + suboffset + 1);
                        suboffset += 2;
                        target->gyroZ = ((int16)(*(module->memsDataBuffer + offset + suboffset)) << 8) + *(module->memsDataBuffer + offset + suboffset + 1);
                        
                        
                        module->memsHeadIndex = (module->memsHeadIndex + 1) % ARRAYSIZE(module->memsData);
                        FETCH_AND_ADD(&module->memsStepsAvailable, 1);
                        
                        
                    }
                    module->accumulatedSize = 0;
                }
            }else{
                Sleep(500);
            }
        }else{
            closeSocket(&programContext->boeingSocket[index]);
            module->run = false;
            NetSendSource message;
            message.bufferLength = sizeof(Message);
            Message stopCommand;
            stopCommand.type = MessageType_Stop;
            stopCommand.stop.id = module->name;
            message.buffer = (char*)&stopCommand;
            
            while(netSend(&programContext->beaconsSocket, &message) != NetResultType_Ok){
                
            }
        }
    }else{
        Sleep(500);
    }
}


extern "C" __declspec(dllexport) void beaconsDomainRoutine(){
    if(!inited || !programContext->inited) return;
    if(programContext->replay){
        Sleep(1000);
        return;
    }
    for(uint8 moduleIndex = 0; moduleIndex < ARRAYSIZE(programContext->modules); moduleIndex++){
        ProgramContext::Module * module = &programContext->modules[moduleIndex];
        if(module->haltProcessing){
            module->beaconsHalted = true;
            while(module->haltProcessing){};
            module->beaconsHalted = false;
        }
    }
    NetRecvResult result;
    result.bufferLength = sizeof(Message);
    Message * message = &programContext->lastBeaconsMessage;
    if(programContext->keepRunning && programContext->beaconsRun){
        
        result.bufferLength = sizeof(Message) - programContext->beaconsAccumulatedSize;
        result.buffer = ((char *) message) + programContext->beaconsAccumulatedSize;
        NetResultType resultCode = netRecv(&programContext->beaconsSocket, &result);
        if(resultCode == NetResultType_Ok){
            if(result.resultLength != 0){
                programContext->beaconsAccumulatedSize += result.resultLength;
                if(programContext->beaconsAccumulatedSize == sizeof(Message)){
                    if(message->type == MessageType_Reset){
                        resetBeacons();
                        programContext->beaconsAccumulatedSize = 0;
                        return;
                    }else if(message->type == MessageType_Data){
                        uint8 boeingId = 0;
                        for(; boeingId < 2; boeingId++){
                            if(programContext->modules[boeingId].name == message->data.id) break;
                        }
                        ASSERT(boeingId <= 1);
                        ProgramContext::Module * module = &programContext->modules[boeingId];
                        
                        ASSERT(message->data.length <= ARRAYSIZE(module->xbDataBuffer));
                        
                        result.bufferLength = message->data.length; 
                        result.buffer = module->xbDataBuffer;
                        result.resultLength = 0;
                        uint16 accumulated = 0;
                        while(accumulated != message->data.length){
                            
                            
                            result.buffer += accumulated;
                            result.bufferLength = message->data.length - accumulated;
                            result.resultLength = 0;
                            
                            resultCode = netRecv(&programContext->beaconsSocket, &result);
                            accumulated += result.resultLength;
                            
                            if(resultCode != NetResultType_Ok){
                                break;
                            }
                        }
                        
                        //NOTE(AK): this works for both XB_METHODS
                        int32 targetSize = sizeof(XbData) - sizeof(module->xbData[module->xbHeadIndex].timeReceived);
                        ASSERT(accumulated == message->data.length);
                        ASSERT(accumulated % targetSize == 0);
                        ASSERT(accumulated / targetSize <= ARRAYSIZE(module->xbData) - module->xbStepsAvailable);
                        for(uint32 offset = 0; offset < message->data.length; offset += targetSize){
                            
                            module->xbData[module->xbHeadIndex] = *((XbData *) (module->xbDataBuffer + offset));
                            module->xbData[module->xbHeadIndex].timeReceived = getProcessCurrentTime();
                            module->xbHeadIndex = (module->xbHeadIndex + 1) % ARRAYSIZE(module->xbData);
                            FETCH_AND_ADD(&module->xbStepsAvailable, 1);
                            
                            
                        }
                        
                        
                    }
                    programContext->beaconsAccumulatedSize = 0;
                    
                }
            }else{
                Sleep(500);
            }
        }else{
            closeSocket(&programContext->beaconsSocket);
            programContext->beaconsRun = false;
        }
    }else{
        Sleep(500);
    }
}



extern "C" __declspec(dllexport) void serverDomainRoutine(){
    if(!inited || !programContext->inited) return;
    if(programContext->replay){
        Sleep(1000);
        return;
    }
    NetSocketSettings settings;
    settings.blocking = false;
    NetSocket socket;
    if(tcpAccept(&programContext->serverSocket, &socket, &settings)){
        NetRecvResult result;
        result.bufferLength = sizeof(Message);
        Message message;
        result.buffer = (char*)&message;
        programContext->newClientAccumulatedSize = 0;
        bool good = false;
        //is this boeing or beacons client ?
        NetResultType resultCode;
        do{
            result.bufferLength = sizeof(Message) - programContext->newClientAccumulatedSize;
            result.buffer = ((char *) &message) + programContext->newClientAccumulatedSize;
            resultCode = netRecv(&socket, &result);
            programContext->newClientAccumulatedSize += result.resultLength;
            if(programContext->newClientAccumulatedSize == sizeof(Message)){
                
                Message * wrap = &message;
                ASSERT(wrap->type == MessageType_Init);
                ClientType clientType = wrap->init.clientType;
                if(clientType == ClientType_Boeing){
                    if(!programContext->beaconsRun){
                        closeSocket(&socket);
                        return;
                    }
                    bool found = false;
                    uint32 i = 0;
                    for(; i < ARRAYSIZE(programContext->modules); i++){
                        if(!programContext->modules[i].run && (wrap->init.boeing.name - '1') == i){
                            found = true;
                            break;
                        }
                    }
                    ASSERT(found);
                    
                    
                    ProgramContext::Module * module = &programContext->modules[i];
                    
                    programContext->boeingSocket[i] = socket;
                    
                    
                    module->name = wrap->init.boeing.name;
                    module->settings = wrap->init.boeing.settings;
#if METHOD_XBSP
                    module->xbPeriod = wrap->init.boeing.xbPeriod;
#endif
                    strncpy(module->sidLower, wrap->init.boeing.sidLower, 9);
                    
                    ProgramContext::Beacon * aBeacon = &programContext->beacons[0];
                    
                    Message handshake;
                    handshake.type = MessageType_Init;
                    handshake.init.clientType = ClientType_Beacon;
                    handshake.init.beacon.frequencyKhz = aBeacon->frequencyKhz;
                    strncpy(handshake.init.beacon.channel, aBeacon->channel, 3);
                    strncpy(handshake.init.beacon.pan, aBeacon->pan, 5);
                    
                    NetSendSource message;
                    message.buffer = (char*)&handshake;
                    message.bufferLength = sizeof(Message);
                    
                    while(netSend(&programContext->boeingSocket[i], &message) != NetResultType_Ok){
                        
                    }
                    module->defaultAccSum64 = {};
                    module->defaultVelSum64 = {};
                    module->defaultGyroSum64 = {};
                    module->defaultGyroBias64 = {};
                    module->defaultGyroVar64 = {};
                    module->defaultAccBias64 = {};
                    module->defaultAccVar64 = {};
                    
                    
                    module->run = true;
                    module->timeConnected = getProcessCurrentTime();
                    //default attributes
                    resetModule(i);
                    
                    
                    
                }else if(clientType == ClientType_Beacon){
                    //reorder beacons according to the beacon client
                    for(uint8 sourceIndex = 0; sourceIndex < ARRAYSIZE(programContext->beacons); sourceIndex++){
                        uint8 targetIndex = 0;
                        for(; targetIndex < ARRAYSIZE(programContext->beacons); targetIndex++){
                            if(!strncmp(wrap->init.beacon.sidLower[sourceIndex], programContext->beacons[targetIndex].sidLower, 8)){
                                //it should land on source index
                                if(sourceIndex != targetIndex){
                                    //swap
                                    v3_64 tempPosition = programContext->beacons[sourceIndex].worldPosition64;
                                    programContext->beacons[sourceIndex].worldPosition64 = programContext->beacons[targetIndex].worldPosition64;
                                    programContext->beacons[targetIndex].worldPosition64 = tempPosition;
                                    //swap one sided, the sid is getting written later
                                    strncpy(programContext->beacons[targetIndex].sidLower, programContext->beacons[sourceIndex].sidLower, 9);
                                }
                                break;
                            }
                        }
                        //beacon not found
                        ASSERT(targetIndex != ARRAYSIZE(programContext->beacons));
                        
                        strncpy(programContext->beacons[sourceIndex].sidLower, wrap->init.beacon.sidLower[sourceIndex], 9);
                    }
                    for(uint8 i = 0; i < ARRAYSIZE(programContext->beacons); i++){
                        ProgramContext::Beacon * beacon = &programContext->beacons[i];
                        
                        strncpy(beacon->channel, wrap->init.beacon.channel, 3);
                        strncpy(beacon->pan, wrap->init.beacon.pan, 5);
                        beacon->frequencyKhz = wrap->init.beacon.frequencyKhz;
                        beacon->timeDivisor = wrap->init.beacon.timeDivisor;
                    }
                    programContext->beaconsSocket = socket;
                    resetBeacons();
                    programContext->beaconsRun = true;
                }else{
                    INV;
                }
                good = true;
                break;
            }
        }while(resultCode == NetResultType_Ok);
        if(!good) closeSocket(&socket);
    }else{
        sleep(1);
    }
    
}

#include "util_config.cpp"

const char * configPath = "data/server.config";

static bool parseConfig(const char * line){
    bool success = true;
    if(!strncmp("ip", line, 2)){
        memset(programContext->ip, 0, 16);
        memset(programContext->port, 0, 6);
        return sscanf(line, "ip %16[^ ] %6[^\r\n ]", programContext->ip, programContext->port) == 2;
    }else if(!strncmp("beacons", line, 7)){
        return sscanf(line, "beacons %9[^ \r\n] %9[^ \r\n] %9[^ \r\n] %9[^ \r\n]", programContext->beacons[0].sidLower, programContext->beacons[1].sidLower, programContext->beacons[2].sidLower, programContext->beacons[3].sidLower) == 4;
    }else if(!strncmp("bx", line, 2)){
        success = success && sscanf(line, "bx %lf %lf %lf %lf", &programContext->beacons[0].worldPosition64.x, &programContext->beacons[1].worldPosition64.x, &programContext->beacons[2].worldPosition64.x, &programContext->beacons[3].worldPosition64.x) == 4;
    }else if(!strncmp("by", line, 2)){
        success = success &&sscanf(line, "by %lf %lf %lf %lf", &programContext->beacons[0].worldPosition64.y, &programContext->beacons[1].worldPosition64.y, &programContext->beacons[2].worldPosition64.y, &programContext->beacons[3].worldPosition64.y) == 4;
    }else if(!strncmp("bz", line, 2)){
        success = success &&sscanf(line, "bz %lf %lf %lf %lf", &programContext->beacons[0].worldPosition64.z, &programContext->beacons[1].worldPosition64.z, &programContext->beacons[2].worldPosition64.z, &programContext->beacons[3].worldPosition64.z) == 4;
    }else if(!strncmp("mpx", line, 3)){
        success = success && sscanf(line, "mpx %lf %lf", &programContext->modules[0].defaultWorldPosition64.x, &programContext->modules[1].defaultWorldPosition64.x) == 2;
    }else if(!strncmp("mpy", line, 3)){
        success = success && sscanf(line, "mpy %lf %lf", &programContext->modules[0].defaultWorldPosition64.y, &programContext->modules[1].defaultWorldPosition64.y) == 2;
    }else if(!strncmp("mpz", line, 3)){
        success = success && sscanf(line, "mpz %lf %lf", &programContext->modules[0].defaultWorldPosition64.z, &programContext->modules[1].defaultWorldPosition64.z) == 2;
    }else if(!strncmp("mox", line, 3)){
        success = success && sscanf(line, "mox %lf %lf", &programContext->modules[0].defaultWorldOrientation64.x, &programContext->modules[1].defaultWorldOrientation64.x) == 2;
    }else if(!strncmp("moy", line, 3)){
        success = success && sscanf(line, "moy %lf %lf", &programContext->modules[0].defaultWorldOrientation64.y, &programContext->modules[1].defaultWorldOrientation64.y) == 2;
    }else if(!strncmp("moz", line, 3)){
        success = success && sscanf(line, "moz %lf %lf", &programContext->modules[0].defaultWorldOrientation64.z, &programContext->modules[1].defaultWorldOrientation64.z) == 2;
    }
    return true;
}


extern "C" __declspec(dllexport) void initDomainRoutine(void * memoryStart, Image * renderingTarget, char * replayFile){
    
    initMemory(memoryStart);
    
    programContext = (ProgramContext *)memoryStart;
    
    initTime();
    
    initIo();
    
    if(!programContext->inited){
        
        programContext->drawHelp = false;
        
        programContext->renderingTarget = renderingTarget;
        bool result = true;
        
        result = result && watchFile(configPath, &programContext->configFileWatch);
        ASSERT(result);
        if(hasFileChanged(&programContext->configFileWatch)){
            result = result && loadConfig(configPath, parseConfig);
        }else{
            ASSERT(false);
        }
        
        if(replayFile !=  NULL) programContext->replay = true;
        if(programContext->replay){
            FileContents contents;
            result = result && getFileSize(replayFile, &contents.size);
            ASSERT(result);
            contents.contents = programContext->tempRecordContents;
            result = result && readFile(replayFile, &contents);
            ASSERT(result);
            char line[1024];
           
            //beacon names
            result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line))  && sscanf(line, "%8s %8s %8s %8s", &programContext->beacons[0].sidLower, &programContext->beacons[1].sidLower, &programContext->beacons[2].sidLower, &programContext->beacons[3].sidLower) == 4;
            
            //beacon position x
            result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line));
            result = result && sscanf(line, "%lf %lf %lf %lf", &programContext->beacons[0].worldPosition64.x, &programContext->beacons[1].worldPosition64.x, &programContext->beacons[2].worldPosition64.x, &programContext->beacons[3].worldPosition64.x) == 4;
            
            //beacon position y
            result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line));
            result = result &&sscanf(line, "%lf %lf %lf %lf", &programContext->beacons[0].worldPosition64.y, &programContext->beacons[1].worldPosition64.y, &programContext->beacons[2].worldPosition64.y, &programContext->beacons[3].worldPosition64.y) == 4;
            
            //beacon position z
            result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line));
            result = result && sscanf(line, "%lf %lf %lf %lf", &programContext->beacons[0].worldPosition64.z, &programContext->beacons[1].worldPosition64.z, &programContext->beacons[2].worldPosition64.z, &programContext->beacons[3].worldPosition64.z) == 4;
            
            //beacons time divisor
            result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%llu", &programContext->beacons[0].timeDivisor) == 1;
            programContext->beacons[3].timeDivisor = programContext->beacons[2].timeDivisor = programContext->beacons[1].timeDivisor = programContext->beacons[0].timeDivisor; 
            
 
            uint64 moduleCount = 0;
            //module heads
            result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%llu", &moduleCount) == 1;
           
            
            for(uint64 moduleIndex = 0; moduleIndex < moduleCount; moduleIndex++){
                ProgramContext::Module * module = &programContext->modules[moduleIndex];
                //module name
                if(getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%1c", &module->name) == 1){
                    module->run = true;
                    //mems rate
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%hu", &module->settings.sampleRate) == 1;
#if METHOD_XBSP
                    //xbPeriod
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%f", &module->xbPeriod) == 1;
#endif
                    //acc
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%u", &module->settings.accPrecision) == 1;
                    //gyro
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%u", &module->settings.gyroPrecision) == 1;
                    
                    //default pos
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line));
                    result = result && sscanf(line, "%lf %lf %lf", &module->defaultWorldPosition64.x, &module->defaultWorldPosition64.y, &module->defaultWorldPosition64.z) == 3;
                    
                    //default World orientation
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line));
                    result = result && sscanf(line, "%lf %lf %lf", &module->defaultWorldOrientation64.x, &module->defaultWorldOrientation64.y, &module->defaultWorldOrientation64.z) == 3;
                    
                    //default acc raw
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line));
                    result = result && sscanf(line, "%lld %lld %lld", &module->defaultAccSum64.x, &module->defaultAccSum64.y, &module->defaultAccSum64.z) == 3;

                    //default vel raw
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line));
                    result = result && sscanf(line, "%lld %lld %lld", &module->defaultVelSum64.x, &module->defaultVelSum64.y, &module->defaultVelSum64.z) == 3;
                    
                    //default gyro raw
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line));
                    result = result && sscanf(line, "%lld %lld %lld", &module->defaultGyroSum64.x, &module->defaultGyroSum64.y, &module->defaultGyroSum64.z) == 3;
                    
                    uint32 memsWarmupFrame;
                    uint32 memsCalibrationFrame;
                    uint32 xbWarmupFrame;
                    uint32 xbCalibrationFrame;
                    
                    //mems warmupframe
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%u", &memsWarmupFrame) == 1;
                    
                    //mems calibration
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%u", &memsCalibrationFrame) == 1;
                    
                    //mems starting frame
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%u", &module->physicalFrame) == 1;
                    
                    //xb warmupframe
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%u", &xbWarmupFrame) == 1;
                    
                    //xb calibration
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%u", &xbCalibrationFrame) == 1;
                    
#if METHOD_XBSP
            //xb starting frame
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%u", &module->xbFrame) == 1;
#endif
#if METHOD_XBPNG
            //xb starting frames
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%u %u %u %u", &module->xbFrames[0], &module->xbFrames[1], &module->xbFrames[2], &module->xbFrames[3]) == 4;
#endif                    

                    
                    //acc expectncy raw
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line));
                    result = result && sscanf(line, "%lld %lld %lld", &module->defaultAccBias64.x, &module->defaultAccBias64.y, &module->defaultAccBias64.z) == 3;

                    //acc std dev raw
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line));
                    result = result && sscanf(line, "%lld %lld %lld", &module->defaultAccVar64.x, &module->defaultAccVar64.y, &module->defaultAccVar64.z) == 3;

                    //gyro expectncy raw
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line));
                    result = result && sscanf(line, "%lld %lld %lld", &module->defaultGyroBias64.x, &module->defaultGyroBias64.y, &module->defaultGyroBias64.z) == 3;

                    //gyro std dev raw
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line));
                    result = result && sscanf(line, "%lld %lld %lld", &module->defaultGyroVar64.x, &module->defaultGyroVar64.y, &module->defaultGyroVar64.z) == 3;

                    //mems data count
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%u", &programContext->recordData.data[moduleIndex].recordDataMemsCount) == 1;
                                    
                    //xb data count
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%u", &programContext->recordData.data[moduleIndex].recordDataXbCount) == 1;
                    
                    //#----
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line));
                }
            }
                //module heads
                result = result && getNextLine(&contents, line, ARRAYSIZE(line));
                
                for(uint64 moduleIndex = 0; moduleIndex < moduleCount; moduleIndex++){
                    ProgramContext::Module * module = &programContext->modules[moduleIndex];

                    //module name
                    char moduleName = 0;
                    if(getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%1c", &moduleName) == 1){
                        ASSERT(moduleName == module->name);
                        //mems data
                        result = result && getNextLine(&contents, line, ARRAYSIZE(line));
                        
                    //mems data
                    for(uint32 di = 0; di < programContext->recordData.data[moduleIndex].recordDataMemsCount; di++){
                        result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%hd %hd %hd %hd %hd %hd", &programContext->recordData.data[moduleIndex].mems[di].accX, &programContext->recordData.data[moduleIndex].mems[di].accY, &programContext->recordData.data[moduleIndex].mems[di].accZ, &programContext->recordData.data[moduleIndex].mems[di].gyroX, &programContext->recordData.data[moduleIndex].mems[di].gyroY, &programContext->recordData.data[moduleIndex].mems[di].gyroZ) == 6;
                    }
                    
                    //xb data
                    result = result && getNextLine(&contents, line, ARRAYSIZE(line));

                    //xb data
                    for(uint32 di = 0; di < programContext->recordData.data[moduleIndex].recordDataXbCount; di++){
#if METHOD_XBSP
                            result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%lf %llu %llu %llu %llu", &programContext->recordData.data[moduleIndex].xb[di].timeReceived, &programContext->recordData.data[moduleIndex].xb[di].delay[0], &programContext->recordData.data[moduleIndex].xb[di].delay[1], &programContext->recordData.data[moduleIndex].xb[di].delay[2], &programContext->recordData.data[moduleIndex].xb[di].delay[3]) == 5;
#endif
#if METHOD_XBPNG
                            result = result && getNextLine(&contents, line, ARRAYSIZE(line)) && sscanf(line, "%lf %u %llu",  &programContext->recordData.data[moduleIndex].xb[di].timeReceived, &programContext->recordData.data[moduleIndex].xb[di].beaconIndex, &programContext->recordData.data[moduleIndex].xb[di].lastTick) == 3;
#endif
                    }
                    
                
                    module->boeingHalted = true;
                    module->beaconsHalted = true;
                    module->processHalted = true;
                    resetModule(moduleIndex);
	                    module->boeingHalted = false;
                    module->beaconsHalted = false;
                    module->processHalted = false;
                }
                
                ASSERT(result);
            }
    
        }else{
            NetSocketSettings settings;
            settings.blocking = false;
            result &= initSocket(&programContext->serverSocket, programContext->ip, programContext->port, &settings);
            result &= tcpListen(&programContext->serverSocket, 10);
            
        }
        
        FileContents fontFile = {};
        result &= readFile("data\\font.bmp", &fontFile);
        Image source;
        result &= decodeBMP(&fontFile, &source);
        result &= flipY(&source);
        result &= initBitmapFont(&programContext->font, &source, source.info.width / 16);
        
        
        
        programContext->localisationType = LocalisationType_Mems_Ori;
        
        
        
        programContext->inited = result;
        
        
        ASSERT(programContext->inited);
    }
    inited = true;
}




extern "C" __declspec(dllexport) void processDomainRoutine(){
    if(!inited || !programContext->inited) return;
    
    float64 speedOfLight = 300000000.0f;
    
    float32 start = getProcessCurrentTime();
    bool record = programContext->record;
    //record beginning
    if(record && !programContext->wasRecord){
        programContext->recordData.startTime = getLocalTime();
        for(uint8 i = 0; i < 2; i++){
            if(programContext->modules[i].run){
            programContext->recordData.defaultModule[i].defaultAccSum64 = programContext->modules[i].accSum64;
            programContext->recordData.defaultModule[i].defaultVelSum64 = programContext->modules[i].velSum64;
            programContext->recordData.defaultModule[i].defaultGyroSum64 = programContext->modules[i].gyroSum64;
            
            programContext->recordData.data[i].memsRecordHeadIndex = programContext->modules[i].memsTailIndex;
            programContext->recordData.data[i].xbRecordHeadIndex = programContext->modules[i].xbTailIndex;
            
            programContext->recordData.defaultModule[i].defaultWorldPosition64 = programContext->modules[i].worldPosition64;
            programContext->recordData.defaultModule[i].defaultWorldOrientation64 = programContext->modules[i].worldOrientation64;
            programContext->recordData.data[i].recordDataXbCount = 0;
            programContext->recordData.data[i].recordDataMemsCount = 0;
            programContext->recordData.defaultModule[i].memsCalibrationFrame = memsCalibrationFrame;
            programContext->recordData.defaultModule[i].xbCalibrationFrame = xbCalibrationFrame;
            programContext->recordData.defaultModule[i].memsWarmedUpFrame = memsWarmedUpFrame;
            programContext->recordData.defaultModule[i].xbWarmedUpFrame = xbWarmedUpFrame;
            programContext->recordData.defaultModule[i].memsStartingFrame = programContext->modules[i].physicalFrame;
#if METHOD_XBSP
            programContext->recordData.data[i].xbStartingFrame = programContext->modules[i].xbFrame;
#endif
#if METHOD_XBPNG
            for(int32 j = 0; j < ARRAYSIZE(programContext->beacons); j++){
                programContext->recordData.defaultModule[i].xbStartingFrames[j] = programContext->modules[i].xbFrames[j];
            }
#endif
            }else{
                programContext->recordData.defaultModule[i].defaultAccSum64 = {};
                programContext->recordData.defaultModule[i].defaultVelSum64 = {};
                programContext->recordData.defaultModule[i].defaultGyroSum64 = {};
            
                programContext->recordData.data[i].memsRecordHeadIndex = {};
                programContext->recordData.data[i].xbRecordHeadIndex = {};
            
                programContext->recordData.defaultModule[i].defaultWorldPosition64 = {};
                programContext->recordData.defaultModule[i].defaultWorldOrientation64 = {};
            programContext->recordData.data[i].recordDataXbCount = 0;
            programContext->recordData.data[i].recordDataMemsCount = 0;
            programContext->recordData.defaultModule[i].memsCalibrationFrame = memsCalibrationFrame;
            programContext->recordData.defaultModule[i].xbCalibrationFrame = xbCalibrationFrame;
            programContext->recordData.defaultModule[i].memsWarmedUpFrame = memsWarmedUpFrame;
            programContext->recordData.defaultModule[i].xbWarmedUpFrame = xbWarmedUpFrame;
                programContext->recordData.defaultModule[i].memsStartingFrame = {};
#if METHOD_XBSP
            programContext->recordData.data[i].xbStartingFrame = programContext->modules[i].xbFrame;
#endif
#if METHOD_XBPNG
            for(int32 j = 0; j < ARRAYSIZE(programContext->beacons); j++){
                programContext->recordData.defaultModule[i].xbStartingFrames[j] = programContext->modules[i].xbFrames[j];
            }
#endif
            }
        }
    }else if(!record && programContext->wasRecord){
        
        //START OF RECORD SAVE
        FileContents contents;
        contents.contents = programContext->tempRecordContents;
        contents.size = 0;
        
        char line[1024];
        uint32 linesize = ARRAYSIZE(line);
        uint32 offset = 0;
        nint linelen = 0;
        
        //beacon names
        snprintf(line, linesize, "#------------------------------------\r\n#beacon names\r\n%8s %8s %8s %8s\r\n", programContext->beacons[0].sidLower, programContext->beacons[1].sidLower, programContext->beacons[2].sidLower, programContext->beacons[3].sidLower);
        linelen = strlen(line);
        strncpy(contents.contents + offset, line, linelen);
        offset += linelen;
        
        
        //beacon x
        snprintf(line, linesize, "#beacon position x\r\n%lf %lf %lf %lf\r\n", programContext->beacons[0].worldPosition64.x, programContext->beacons[1].worldPosition64.x, programContext->beacons[2].worldPosition64.x, programContext->beacons[3].worldPosition64.x);
        linelen = strlen(line);
        strncpy(contents.contents + offset, line, linelen);
        offset += linelen;
        
        //beacon y
        snprintf(line, linesize, "#beacon position y\r\n%lf %lf %lf %lf\r\n", programContext->beacons[0].worldPosition64.y, programContext->beacons[1].worldPosition64.y, programContext->beacons[2].worldPosition64.y, programContext->beacons[3].worldPosition64.y);
        linelen = strlen(line);
        strncpy(contents.contents + offset, line, linelen);
        offset += linelen;
        
        //beacon z
        snprintf(line, linesize, "#beacon position z\r\n%lf %lf %lf %lf\r\n", programContext->beacons[0].worldPosition64.z, programContext->beacons[1].worldPosition64.z, programContext->beacons[2].worldPosition64.z, programContext->beacons[3].worldPosition64.z);
        linelen = strlen(line);
        strncpy(contents.contents + offset, line, linelen);
        offset += linelen;
        
        //beacons time divisor
        snprintf(line, linesize, "#beacons time divisor\r\n%llu\r\n", programContext->beacons[0].timeDivisor);
        linelen = strlen(line);
        strncpy(contents.contents + offset, line, linelen);
        offset += linelen;
        
        
        //module heads
        snprintf(line, linesize, "#------------------------------------\r\n#module headers\r\n%llu\r\n", ARRAYSIZE(programContext->modules));
        linelen = strlen(line);
        strncpy(contents.contents + offset, line, linelen);
        offset += linelen;
        for(uint8 i = 0; i < 2; i++){
            ProgramContext::Module * module = &programContext->modules[i];
            
            //module name
            snprintf(line, linesize, "#------------------------------------\r\n#module name\r\n%c\r\n", module->name);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //sample rate
            snprintf(line, linesize, "#module mems sample rate\r\n%hu\r\n", module->settings.sampleRate);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
#if METHOD_XBSP
            //xb period
            snprintf(line, linesize, "#xb period\r\n%f\r\n", module->xbPeriod);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
#endif
            //acc
            snprintf(line, linesize, "#acc precision\r\n%u\r\n", module->settings.accPrecision);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            //gyro
            snprintf(line, linesize, "#gyro precision\r\n%u\r\n", module->settings.gyroPrecision);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            
            //module default position
            snprintf(line, linesize, "#default position\r\n%lf %lf %lf\r\n", programContext->recordData.defaultModule[i].defaultWorldPosition64.x, programContext->recordData.defaultModule[i].defaultWorldPosition64.y, programContext->recordData.defaultModule[i].defaultWorldPosition64.z);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //module default world orientation
            snprintf(line, linesize, "#default orientation\r\n%lf %lf %lf\r\n", programContext->recordData.defaultModule[i].defaultWorldOrientation64.x, programContext->recordData.defaultModule[i].defaultWorldOrientation64.y, programContext->recordData.defaultModule[i].defaultWorldOrientation64.z);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //module default acc raw
            snprintf(line, linesize, "#default acc sum raw\r\n%lld %lld %lld\r\n", programContext->recordData.defaultModule[i].defaultAccSum64.x, programContext->recordData.defaultModule[i].defaultAccSum64.y, programContext->recordData.defaultModule[i].defaultAccSum64.z);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //module default vel raw
            snprintf(line, linesize, "#default vel sum raw\r\n%lld %lld %lld\r\n", programContext->recordData.defaultModule[i].defaultVelSum64.x, programContext->recordData.defaultModule[i].defaultVelSum64.y, programContext->recordData.defaultModule[i].defaultVelSum64.z);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //module default gyro raw
            snprintf(line, linesize, "#default gyro sum raw\r\n%lld %lld %lld\r\n", programContext->recordData.defaultModule[i].defaultGyroSum64.x, programContext->recordData.defaultModule[i].defaultGyroSum64.y, programContext->recordData.defaultModule[i].defaultGyroSum64.z);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //calibration stuff
            //warmed up frames
            snprintf(line, linesize, "#mems warmed up frame\r\n%u\r\n", programContext->recordData.defaultModule[i].memsWarmedUpFrame);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //memsCalibrationFrame
            snprintf(line, linesize, "#mems calibration frame\r\n%u\r\n", programContext->recordData.defaultModule[i].memsCalibrationFrame);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //mems starting frame
            snprintf(line, linesize, "#mems first frame index\r\n%u\r\n", programContext->recordData.defaultModule[i].memsStartingFrame);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //xb warmed up frames
            snprintf(line, linesize, "#xb warmed up frame\r\n%u\r\n", programContext->recordData.defaultModule[i].xbWarmedUpFrame);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //xbCalibrationFrame
            snprintf(line, linesize, "#xb calibration frame\r\n%u\r\n", programContext->recordData.defaultModule[i].xbCalibrationFrame);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
#if METHOD_XBSP
            //xb starting frame
            snprintf(line, linesize, "#xb first frame index\r\n%u\r\n", programContext->recordData.defaultModule[i].xbStartingFrame);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
#endif
#if METHOD_XBPNG
            //xb starting frames
            snprintf(line, linesize, "#xb first frames indices\r\n%u %u %u %u\r\n", programContext->recordData.defaultModule[i].xbStartingFrames[0], programContext->recordData.defaultModule[i].xbStartingFrames[1],
                     programContext->recordData.defaultModule[i].xbStartingFrames[2],
                     programContext->recordData.defaultModule[i].xbStartingFrames[3]);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
#endif
            
            //acc expectancy
            snprintf(line, linesize, "#acc bias(EX)\r\n%lld %lld %lld\r\n", programContext->modules[i].accBias64.x, programContext->modules[i].accBias64.y, programContext->modules[i].accBias64.z);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //acc std deviation
            snprintf(line, linesize, "#acc std deviation(sqrt(varX)))\r\n%lld %lld %lld\r\n", programContext->modules[i].accVar64.x, programContext->modules[i].accVar64.y, programContext->modules[i].accVar64.z);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //gyro expectancy
            snprintf(line, linesize, "#gyro bias(EX)\r\n%lld %lld %lld\r\n", programContext->modules[i].gyroBias64.x, programContext->modules[i].gyroBias64.y, programContext->modules[i].gyroBias64.z);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;

            //gyro std deviation
            snprintf(line, linesize, "#gyro std deviation(sqrt(varX))\r\n%lld %lld %lld\r\n", programContext->modules[i].gyroVar64.x, programContext->modules[i].gyroVar64.y, programContext->modules[i].gyroVar64.z);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //mems data count
            snprintf(line, linesize, "#mems data count\r\n%u\r\n", programContext->recordData.data[i].recordDataMemsCount);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            //xb data count
            snprintf(line, linesize, "#xb data count\r\n%u\r\n#------------------------------------\r\n", programContext->recordData.data[i].recordDataXbCount);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
           
        }
        //module heads
        snprintf(line, linesize, "#module data\r\n");
        linelen = strlen(line);
        strncpy(contents.contents + offset, line, linelen);
        offset += linelen;
        for(uint8 i = 0; i < 2; i++){
            ProgramContext::Module * module = &programContext->modules[i];
            //module name
            snprintf(line, linesize, "#------------------------------------\r\n#module name\r\n%c\r\n", module->name);
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
            snprintf(line, linesize, "#mems data\r\n");
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            //mems data
            for(uint32 memsDataIndex = 0; memsDataIndex < programContext->recordData.data[i].recordDataMemsCount; memsDataIndex++){
                snprintf(line, linesize, "%hd %hd %hd %hd %hd %hd\r\n", programContext->recordData.data[i].mems[memsDataIndex].accX, programContext->recordData.data[i].mems[memsDataIndex].accY, programContext->recordData.data[i].mems[memsDataIndex].accZ, programContext->recordData.data[i].mems[memsDataIndex].gyroX, programContext->recordData.data[i].mems[memsDataIndex].gyroY, programContext->recordData.data[i].mems[memsDataIndex].gyroZ);
                linelen = strlen(line);
                strncpy(contents.contents + offset, line, linelen);
                offset += linelen;
            }
            
            snprintf(line, linesize, "#xb data\r\n");
            linelen = strlen(line);
            strncpy(contents.contents + offset, line, linelen);
            offset += linelen;
            
#if METHOD_XBSP
            //xb data
            for(uint32 xbDataIndex = 0; xbDataIndex < programContext->recordData.data[i].recordDataXbCount; xbDataIndex++){
                snprintf(line, linesize, "%lf %llu %llu %llu %llu\r\n",
                         programContext->recordData.data[i].xb[xbDataIndex].timeReceived - module->timeConnected, programContext->recordData.data[i].xb[xbDataIndex].delay[0], programContext->recordData.data[i].xb[xbDataIndex].delay[1], programContext->recordData.data[i].xb[xbDataIndex].delay[2], programContext->recordData.data[i].xb[xbDataIndex].delay[3]);
                linelen = strlen(line);
                strncpy(contents.contents + offset, line, linelen);
                offset += linelen;
            }
#endif
            
#if METHOD_XBPNG
            //xb data
            for(uint32 xbDataIndex = 0; xbDataIndex < programContext->recordData.data[i].recordDataXbCount; xbDataIndex++){
                snprintf(line, linesize, "%lf %u %llu\r\n",
                         programContext->recordData.data[i].xb[xbDataIndex].timeReceived - module->timeConnected,programContext->recordData.data[i].xb[xbDataIndex].beaconIndex,programContext->recordData.data[i].xb[xbDataIndex].lastTick);
                linelen = strlen(line);
                strncpy(contents.contents + offset, line, linelen);
                offset += linelen;
            }
#endif
        }
        contents.size = offset;
        
        char path[250];
        sprintf(path, "records\\%04hu_%02hu_%02hu-%02hu-%02hu-%02hu.rec", programContext->recordData.startTime.year, programContext->recordData.startTime.month, programContext->recordData.startTime.day, programContext->recordData.startTime.hour, programContext->recordData.startTime.minute, programContext->recordData.startTime.second);
        bool saveFileResult = saveFile(path, &contents);
        ASSERT(saveFileResult);
        //END OF RECORD SAVE
    }
    programContext->wasRecord = record;
    
    int32 memsSteps[2];
    int32 xbSteps[2];
    
    
    for(uint8 i = 0; i < 2; i++){
        ProgramContext::Module * module = &programContext->modules[i];
        if(module->run){
            
            if(module->haltProcessing){
                module->processHalted = true;
                while(module->haltProcessing){};
                module->processHalted = false;
            }
            
            
            
            if(programContext->replay){
                memsSteps[i] = programContext->recordData.data[i].recordDataMemsCount - programContext->recordData.data[i].recordDataMemsIndex;
                xbSteps[i] = programContext->recordData.data[i].recordDataXbCount - programContext->recordData.data[i].recordDataXbIndex;
            }else{
                memsSteps[i] = module->memsStepsAvailable;
                xbSteps[i] = module->xbStepsAvailable;
            }
            
            ASSERT(memsSteps[i] >= 0);
            ASSERT(memsSteps[i] <= ARRAYSIZE(module->memsData));
            ASSERT(xbSteps[i] >= 0);
            ASSERT(xbSteps[i] <= ARRAYSIZE(module->xbData));
            if(record){
                int32 memsTargetHead = module->memsHeadIndex;
                ASSERT((module->memsHeadIndex - module->memsTailIndex + ARRAYSIZE(module->memsData)) % ARRAYSIZE(module->memsData) <= module->memsStepsAvailable);
                ASSERT((module->xbHeadIndex - module->xbTailIndex + ARRAYSIZE(module->xbData)) % ARRAYSIZE(module->xbData) <= module->xbStepsAvailable);
                for(; programContext->recordData.data[i].memsRecordHeadIndex != memsTargetHead; programContext->recordData.data[i].memsRecordHeadIndex = (programContext->recordData.data[i].memsRecordHeadIndex + 1) % ARRAYSIZE(module->memsData)){
                    programContext->recordData.data[i].mems[programContext->recordData.data[i].recordDataMemsCount] = module->memsData[programContext->recordData.data[i].memsRecordHeadIndex];
                    programContext->recordData.data[i].recordDataMemsCount++;
                    ASSERT(programContext->recordData.data[i].recordDataMemsCount < ARRAYSIZE(programContext->recordData.data[i].mems));
                }
                int32 xbTargetHead = module->xbHeadIndex;
                for(; programContext->recordData.data[i].xbRecordHeadIndex != xbTargetHead; programContext->recordData.data[i].xbRecordHeadIndex = (programContext->recordData.data[i].xbRecordHeadIndex + 1) % ARRAYSIZE(module->xbData)){
                    programContext->recordData.data[i].xb[programContext->recordData.data[i].recordDataXbCount] = module->xbData[programContext->recordData.data[i].xbRecordHeadIndex];
                    programContext->recordData.data[i].recordDataXbCount++;
                    ASSERT(programContext->recordData.data[i].recordDataXbCount < ARRAYSIZE(programContext->recordData.data[i].xb));
                }
            }
        }
    }
    
    if(programContext->localisationType == LocalisationType_Mems_Ori || programContext->localisationType == LocalisationType_Mems_Loco || programContext->localisationType == LocalisationType_Mems_Comb){
        
        float64 dt = 0;
        const float64 g = mpu6050_g64;
        int32 stepsAmount = 0;
        
        
        for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
            ProgramContext::Module * module = &programContext->modules[i];
            
            if(module->run){
                //wash out xb steps
                FETCH_AND_ADD(&module->xbStepsAvailable, -xbSteps[i]);
                module->xbTailIndex = (module->xbTailIndex + xbSteps[i]) % ARRAYSIZE(module->xbData); 

                if(dt == 0){
                    //NOTE(AK): assuming all modules have same sampling frequency
                    stepsAmount = memsSteps[i];
                }else{
                    stepsAmount = MIN(stepsAmount, memsSteps[i]);
                }
                dt = mpu6050_getTimeDelta64(module->settings.sampleRate);
            }
        }
        
        if(programContext->restartReplay || stepsAmount == 0){
            if(programContext->replay){
                for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
                    //reset
                    resetModule(i);
                    programContext->accumulator = 0;
                    programContext->restartReplay = false;
                    Sleep(100);
                }
                return;               
            }else{
                Sleep(500);
            }
        }
        stepsAmount = MIN(stepsAmount, (uint16)(programContext->accumulator / dt));
        
        for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
            ProgramContext::Module * module = &programContext->modules[i];
            
            if(module->run){
                
                int32 tDivisor = module->settings.sampleRate;
                
                int32 accDivisor = mpu6050_getAccDivisor(&module->settings);
                int32 velDivisor = tDivisor * accDivisor;
                int32 pDivisor = tDivisor * tDivisor * 2 * accDivisor;
                
                int32 degDivisor = tDivisor * mpu6050_getGyroDivisorTimes10(&module->settings);
                int32 degMultiplier = 10;
                int32 degModulo = 360 * degDivisor / degMultiplier;
                
                
                for(uint16 stepIndex = 0; stepIndex < stepsAmount; stepIndex++){
                    
                    int32 size = ARRAYSIZE(module->memsData);
                    
                    MemsData * data;
                    if(programContext->replay){
                        data = &programContext->recordData.data[i].mems[programContext->recordData.data[i].recordDataMemsIndex++];
                    }else{
                        data = &module->memsData[module->memsTailIndex];
                    }
                    
                    //NOTE(AK): this is hardcoded rotation for our default orientation in the map
                    //this will change, when default orientation change
                    //TODO(): implement when maps include different orientiaiton
                    //NOTE(AK): gyro data are left handed rotation and we use right handed
                    dv3_64 accDataRotated = DV3_64(data->accY, -data->accX, data->accZ);
                    dv3_64 gyroData = DV3_64(-data->gyroX, -data->gyroY, -data->gyroZ);

                    dv3_64 accDataCleared = accDataRotated - module->accBias64;
                    dv3_64 gyroDataCleared = gyroData - module->gyroBias64;
                    for(int32 i = 0; i < 3; i++){
                        //NOTE(AK): > and <= because the sd vas trimmed to whole numbers from floats
                        if(accDataCleared.v[i] > -module->accVar64.v[i] && accDataCleared.v[i] <= module->accVar64.v[i]){
                            accDataCleared = {};
                        }
                        if(gyroDataCleared.v[i] > -module->gyroVar64.v[i] && gyroDataCleared.v[i] <= module->gyroVar64.v[i]){
                            gyroDataCleared = {};
                        }
                    }
                    
                    if(module->physicalFrame > memsCalibrationFrame)
                    {
                        
                        mat4_64 rotationMatrix;
                        
                        

                        
                        if(programContext->localisationType == LocalisationType_Mems_Ori || programContext->localisationType == LocalisationType_Mems_Comb){
                            module->gyroSum64 += gyroDataCleared;
                            for(uint8 i = 0; i < 3; i++){
                                module->gyroSum64.v[i] += degModulo;
                                module->gyroSum64.v[i] = module->gyroSum64.v[i] % degModulo;
                            }
                            
                            module->rotationAngles64 = (module->gyroSum64 * degMultiplier)/degDivisor;
                            
                            //rotation difference
                            v4_64 quatX = Quat64(V3_64(1, 0, 0), degToRad64(module->rotationAngles64.x));
                            v4_64 quatY = Quat64(V3_64(0, 1, 0), degToRad64(module->rotationAngles64.y));
                            v4_64 quatZ = Quat64(V3_64(0, 0, 1), degToRad64(module->rotationAngles64.z));
                            
                            rotationMatrix  = quaternionToMatrix64(normalize64(normalize64(quatX * quatY) * quatZ));
                            
                            module->worldOrientation64 = rotationMatrix * module->defaultWorldOrientation64;
                            
                        }
                        if(programContext->localisationType == LocalisationType_Mems_Loco){
                            module->accSum64 += accDataCleared;
                            module->acceleration64 = accDataCleared / accDivisor;
                            module->velocity64 = (module->accSum64/velDivisor)*g;
                            
                            module->velSum64 += module->accSum64;
                            //actual locomotion
                            module->worldPosition64 = module->defaultWorldPosition64 + ((module->velSum64*2 - module->accSum64) / pDivisor)*g;
                        }
                        if(programContext->localisationType == LocalisationType_Mems_Comb){
                            v3_64 realBias = rotationMatrix * (module->accBias64 / accDivisor);
                            module->acceleration64 = rotationMatrix * (accDataRotated / accDivisor) - realBias;
                            module->worldPosition64 += module->velocity64*dt + 0.5*g*dt*dt*module->acceleration64; 
                            module->velocity64 += module->acceleration64*g*dt;
                        }
                        
                        
                    }else if(module->physicalFrame == memsCalibrationFrame){
                        module->accBias64 /= module->calibrationCount;
                        module->gyroBias64 /= module->calibrationCount;
                        
                        dv3_64 gyroVar = {};
                        dv3_64 accVar = {};
                        
                        for(uint32 i = 0; i < module->calibrationCount; i++){
                            dv3_64 gyroMember = module->calibrationGyro64[i] - module->gyroBias64;
                            gyroVar += hadamard64(gyroMember, gyroMember);
                            
                            dv3_64 accMember = module->calibrationAcc64[i] - module->accBias64;
                            accVar +=  hadamard64(accMember, accMember);
                        }
                        v3_64 accVar64r = accVar / (module->calibrationCount - 1);
                        v3_64 gyroVar64r = gyroVar / (module->calibrationCount - 1);
                        for(int32 i = 0; i < 3; i++){
                            module->accVar64.v[i] = (int64)sqrt64((float64)accVar64r.v[i]);
                            module->gyroVar64.v[i] = (int64)sqrt64((float64)gyroVar64r.v[i]);
                        }
                        
                        module->calibrated = true;
                    }else{
                        if(module->physicalFrame > memsWarmedUpFrame){
                           //NOTE(AK) gather data?
                            module->accBias64 += accDataRotated;
                            module->gyroBias64 += gyroData;
                            module->calibrationGyro64[module->calibrationCount] = gyroData;
                            module->calibrationAcc64[module->calibrationCount] = accDataRotated;
                            module->calibrationCount++;
                        }else if(module->physicalFrame == memsWarmedUpFrame){
                            //NOTE(AK) init data gathering
                            module->gyroBias64 = {};
                            module->accBias64 = {};
                            module->calibrationCount = 0;
                        }
                        
                    }
                    if(!programContext->replay) module->memsTailIndex = (module->memsTailIndex + 1) % size;
                    module->physicalFrame++;
                }
                FETCH_AND_ADD(&module->memsStepsAvailable, -stepsAmount);
            }
        }
        
        if(dt == 0){
            programContext->accumulator = 0;
        }else{
            
            programContext->accumulator -= dt*stepsAmount;
        }
        
    }else if(programContext->localisationType == LocalisationType_Xb){
#if METHOD_XBPNG

        for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
            ProgramContext::Module * module = &programContext->modules[i];
            
            if(module->run){
                //wash out the mems data
                FETCH_AND_ADD(&module->memsStepsAvailable, -memsSteps[i]);
                module->memsTailIndex = (module->memsTailIndex + memsSteps[i]) % ARRAYSIZE(module->memsData);          
            }
        }
        //NOTE(AK): this needs to be verified and finished, not done, because XB.algorithms were not used
        /*
        if(programContext->restartReplay || stepsAmount == 0){
            if(programContext->replay){
                for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
                    //reset
                    resetModule(i);
                    programContext->accumulator = 0;
                    programContext->restartReplay = false;
                    Sleep(100);
                }
                return;               
            }else{
                Sleep(500);
            }
        }*/
        
        int32 stepsTaken[2];
        
        for(uint32 moduleIndex = 0; moduleIndex < ARRAYSIZE(programContext->modules); moduleIndex++){
            ProgramContext::Module * module = &programContext->modules[moduleIndex];
            if(module->run){
                //NOTE(AK): Get the latest ping, as we do not care for the sequence, but rather latest info
                //NOTE(AK): Saving not directly as we need to know if the data is from this physical frame
                uint64 lastTicks[4] = {};
                bool doAABB = false;
                stepsTaken[moduleIndex] = module->xbStepsAvailable;
                int32 size = ARRAYSIZE(module->xbData);
                int32 targetTail = (module->xbTailIndex + stepsTaken[moduleIndex]) % size;
                for(; module->xbTailIndex != targetTail; module->xbTailIndex = (module->xbTailIndex + 1) % size){
                    XbData * source = &module->xbData[module->xbTailIndex];          
                    if(module->xbFrames[source->beaconIndex] >= xbWarmedUpFrame && module->xbFrames[source->beaconIndex] < xbCalibrationFrame){
                        //NOTE(AK): save calibration data
                        int32 index = module->xbFrames[source->beaconIndex] - xbWarmedUpFrame;
                        module->calibrationTicks[source->beaconIndex][index] = source->lastTick;                        
                    }else if(module->xbFrames[source->beaconIndex] == xbCalibrationFrame){
                        //NOTE(AK): actually calibrate
                        
                        float64 initialDistance = length64(programContext->beacons[source->beaconIndex].worldPosition64 - module->worldPosition64);
                        float64 supposedTiming = 2*initialDistance / speedOfLight;
                        
                        int32 calibrationSamples = module->xbFrames[source->beaconIndex] - xbWarmedUpFrame;
                        //NOTE(AK):
                        //idea: we could also use the lowest value as no FSPT noise, and pure hw noise
                        float64 lowestNoise = translateTickToTime(module->calibrationTicks[source->beaconIndex][0], programContext->beacons[source->beaconIndex].timeDivisor);
                        for(int32 i = 1; i < calibrationSamples; i++){
                            float64 realTiming = translateTickToTime(module->calibrationTicks[source->beaconIndex][i], programContext->beacons[source->beaconIndex].timeDivisor);
                            ASSERT(realTiming >= supposedTiming);
                            float64 bothNoises = realTiming - supposedTiming;
                            if(bothNoises < lowestNoise){
                                lowestNoise = bothNoises;       
                            }
                        }
                        module->hwBias[source->beaconIndex] = lowestNoise;
                        
                        bool calibrated = true;
                        for(uint8 i = 0; i < ARRAYSIZE(module->xbFrames); i++){
                            calibrated &= module->xbFrames[i] >= xbCalibrationFrame;
                        }
                        module->calibrated = calibrated;
                    }
                    lastTicks[source->beaconIndex] = source->lastTick;
                    module->xbFrames[source->beaconIndex]++;
                    module->lastTicks[source->beaconIndex] = lastTicks[source->beaconIndex];
                }
                
                for(uint8 i = 0; i < ARRAYSIZE(lastTicks); i++){
                    if(lastTicks[i] && module->xbFrames[i] > xbCalibrationFrame){
                        //TODO(AK): induce AABB localisation
                        float64 timing = translateTickToTime(lastTicks[i], programContext->beacons[i].timeDivisor);                        
                        //sub bias
                        if(timing > module->hwBias[i]){
                            timing -= module->hwBias[i];
                        }
                        timing = timing/2;
                        ASSERT(timing > 0);
                        float64 proximity = speedOfLight * timing;
                        programContext->beacons[i].moduleDistance64[moduleIndex] = proximity;
                        doAABB = true;
                    }
                }
                if(module->calibrated && doAABB){
                    //do aabb
                    //NOTE(AK): each with each once, 4 beacons
                    Box_64 metaResults[6];
                    uint8 metaResultCount = 0;
                    for(uint8 i = 0; i < ARRAYSIZE(programContext->beacons); i++){
                        Sphere_64 A;
                        A.origin = programContext->beacons[i].worldPosition64;
                        A.radius = programContext->beacons[i].moduleDistance64[moduleIndex];
                        for(uint8 j = i+1; j < ARRAYSIZE(programContext->beacons); j++){
                            Sphere_64 B;
                            B.origin = programContext->beacons[j].worldPosition64;
                            B.radius = programContext->beacons[j].moduleDistance64[moduleIndex];
                            if(intersectSpheresAABB64(&A, &B, &metaResults[metaResultCount])){
                                metaResultCount++;
                            }
                        }
                    }
                    //NOTE(AK): from the problem specification, if the data was clear, the count would always be 6
                    if(metaResultCount){
                        Box_64 resultAABB = metaResults[0];
                        for(uint8 i = 1; i < metaResultCount; i++){
                            Box_64 targetAABB;
                            if(intersectBoxes64(&resultAABB, &metaResults[i], &targetAABB)){
                                resultAABB = targetAABB;
                            }
                        }
                        module->xbAABB = resultAABB;
                        module->worldPosition64 = V3_64(resultAABB.lowerCorner.x + (resultAABB.upperCorner.x - resultAABB.lowerCorner.x)/2, resultAABB.lowerCorner.y + (resultAABB.upperCorner.y - resultAABB.lowerCorner.y)/2, resultAABB.lowerCorner.z + (resultAABB.upperCorner.z - resultAABB.lowerCorner.z)/2);
                    }
                }
                FETCH_AND_ADD(&module->xbStepsAvailable, -stepsTaken[moduleIndex]);
            }
        }
        
        
#endif
        
#if METHOD_XBSP
        //NOTE(AK): this algorithm was not used at all in the end... 
        //AABB needs to be implemented and replay capability verified
        
        uint16 stepsAmount = 0;
        
        for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
            ProgramContext::Module * module = &programContext->modules[i];
            
            if(module->run){
                //wash out the mems data
                FETCH_AND_ADD(&module->memsStepsAvailable, -memsSteps[i]);
                module->memsTailIndex = (module->memsTailIndex + memsSteps[i]) % ARRAYSIZE(module->memsData); 
                if(stepsAmount == 0){
                    stepsAmount = xbSteps[i];
                }else{
                    stepsAmount = MIN(stepsAmount, xbSteps[i]);
                }
                
            }
        }
        if(programContext->restartReplay || stepsAmount == 0){
            if(programContext->replay){
                for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
                    //reset
                    resetModule(i);
                    programContext->accumulator = 0;
                    programContext->restartReplay = false;
                    Sleep(100);
                }
                return;               
            }else{
                Sleep(500);
            }
        }
        
        int32 stepsToTake = 0;
        float64 totalTime = 0;
        for(;;){
            float64  maxTiming = 0;
            for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
                ProgramContext::Module * module = &programContext->modules[i];
                if(module->run){
                    for(uint16 stepIndex = 0; stepIndex < stepsAmount; stepIndex++){
                        int32 size = ARRAYSIZE(module->xbData);
                        XbData * data;
                        if(programContext->replay){
                            data = &programContext->recordData.data[i].xb[programContext->recordData.data[i].recordDataXbIndex+stepIndex];
                        }else{
                            data = &module->xbData[(module->xbTailIndex+stepIndex)%ARRAYSIZE(module->xbData)];
                        }
                        maxTiming  = MAX(translateTickToTime(data->delay[0], programContext->beacons[0].timeDivisor), maxTiming);
                        for(uint8 ti = 1; ti < ARRAYSIZE(XbData::delay); ti++){
                            float64  maxTimingCandidate = translateTickToTime(data->delay[ti], programContext->beacons[ti].timeDivisor);
                            if(maxTimingCandidate > maxTiming) maxTiming = maxTimingCandidate;
                        }
                    }
                }
            }
            if(totalTime + maxTiming < programContext->accumulator){
                totalTime += maxTiming;
                stepsToTake++;
            }else{
                break;
            }
            
        }
        
        if(stepsToTake > 0){
            for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
                ProgramContext::Module * module = &programContext->modules[i];
                if(module->run){
                    for(uint16 stepIndex = 0; stepIndex < stepsToTake; stepIndex++){
                        XbData * data;
                        if(programContext->replay){
                            data = &programContext->recordData.data[i].xb[programContext->recordData.data[i].recordDataXbIndex++];
                        }else{
                            data = &module->xbData[module->xbTailIndex];
                        }
                        //do stuff, also calibration or some?
                        module->calibrated = true;
                        for(uint8 beaconIndex = 0; beaconIndex < 4; beaconIndex++){
                            float64 timing = translateTickToTime(data->delay[beaconIndex], programContext->beacons[beaconIndex].timeDivisor);
                            float64 timeDelta = timing - module->xbPeriod;
                            
                            float64 difference = speedOfLight*timeDelta;
                            programContext->beacons[beaconIndex].moduleDistance64[i] += difference;
                            //NOTE(AK): superposition principle, the time delta should be as low as possible to consider this a linear step
                            v3_64 direction = module->worldPosition64 - programContext->beacons[beaconIndex].worldPosition64;
                            module->worldPosition64 += difference * normalize64(direction);
                        }
                        
                        if(!programContext->replay) module->xbTailIndex = (module->xbTailIndex + 1) % ARRAYSIZE(module->xbData);
                        module->xbFrame++;
                    }
                    if(!programContext->replay){
                        FETCH_AND_ADD(&module->xbStepsAvailable, -stepsToTake);
                    }
                }
            }
            programContext->accumulator -= totalTime;
        }
#endif
    }
    
    
    programContext->accumulator += getProcessCurrentTime() - start;
}

extern "C" __declspec(dllexport) void renderDomainRoutine(){
    if(!inited || !programContext->inited) return;
    for(uint32 h = 0; h < programContext->renderingTarget->info.height; h++){
        uint32 pitch = h*programContext->renderingTarget->info.width;
        for(uint32 w = 0; w < programContext->renderingTarget->info.width; w++){
            ((uint32 *)programContext->renderingTarget->data)[pitch + w] = 0; 
            
        }
    }
    
    Color white = {255,255, 255};
    Color grey = {200, 200, 200};
    Color blue = {255, 0xCC, 0};
    Color red = {0, 0, 255};
    Color green = {0, 255, 0};
    Color black = {0, 0 , 0};
    
    Color beaconsColors[4] = {
        {53, 73, 0},
        {219, 109, 0},
        {0, 73, 146},
        {182, 109, 255}
    };
    
    
    Color moduleColor = {150, 255, 255};
    
    ProgramContext::Module * activeModule = &programContext->modules[programContext->activeModuleIndex];
    
    int32 size = programContext->renderingTarget->info.width / 6;
    int32 border = (int32)(0.12f * (float32)size);
    
    size = size - border;
    uint8 thickness = 1;
    int32 sizeHalf = size/2;
    float32 rotationY = degToRad(activeModule->rotationAngles64.y);
    float32 rotationZ = degToRad(activeModule->rotationAngles64.z);
    float32 rotationX = degToRad(activeModule->rotationAngles64.x);
    float32 accY = activeModule->acceleration64.y;
    float32 accZ = activeModule->acceleration64.z;
    float32 accX = activeModule->acceleration64.x;
    
    
    
    float32 maximumAcc = powd(2, activeModule->settings.accPrecision + 1) * mpu6050_g;
    
    //dynamic stuff
    int32 humanSize = (int32)(0.6f * size);
    
    //body
    float32 bodyProportion = 0.75f;
    float32 headProportion = 0.2f;
    float32 gap = 1.0f - bodyProportion - headProportion;
    
    int32 humanBodySize = (int32)(bodyProportion * humanSize);
    int32 humanBodyOffset = humanSize - humanBodySize;
    int32 humanBodyWidth = (int32)(0.2f * humanSize);
    int32 humanBodyWidthHalf = humanBodyWidth/2;
    int32 humanBodySizeHalf = humanBodySize/2;
    int32 humanSizeHalf = humanSize/2;
    int32 humanBodyOffsetHalf = humanBodyOffset/2;
    
    
    //head
    int32 humanHeadSize = (int32)(headProportion * humanSize);
    int32 humanHeadSizeHalf = humanHeadSize/2;
    int32 gapSize = (int32)(gap * humanSize);
    
    float32 eyeProportion = 0.2f;
    float32 eyePosition = eyeProportion * 1.5f;
    float32 eyeSize = eyeProportion * humanHeadSize;
    
    
    int32 textSize = 2*size;
    
    dv2 offset = {border, border};
    
    uint8 fontSize = 14;
    
    //position - map frame
    {
        printToBitmap(programContext->renderingTarget, offset.x, offset.y - border, "TOP VIEW", &programContext->font, fontSize, white);
        if(programContext->record){
            printToBitmap(programContext->renderingTarget, offset.x + (strlen("TOP VIEW")+1) * fontSize, offset.y - border, "RECORDING", &programContext->font, fontSize, red);
        }else if(programContext->replay){
            printToBitmap(programContext->renderingTarget, offset.x + (strlen("TOP VIEW")+1) * fontSize, offset.y - border, "REPLAYING", &programContext->font, fontSize, green);
        }else{
            printToBitmap(programContext->renderingTarget, offset.x + (strlen("TOP VIEW")+1) * fontSize, offset.y - border, "(not recording)", &programContext->font, fontSize, grey);
        }
        dv2 bottomRightCorner = {(int32)programContext->renderingTarget->info.width - 3*border - textSize,(int32)programContext->renderingTarget->info.height - size - 3*border};
        drawRectangle(programContext->renderingTarget, &offset, &bottomRightCorner, white, thickness); 
        
        uint8 axisFontSize = fontSize;
        printToBitmap(programContext->renderingTarget, offset.x, bottomRightCorner.y, "X", &programContext->font, axisFontSize, blue);
        printToBitmap(programContext->renderingTarget, offset.x - border, offset.y, " Y", &programContext->font, axisFontSize, blue);
        
        
        float32 maxX = 0;
        float32 maxY = 0;
        
        for(uint8 beaconIndex = 0; beaconIndex < ARRAYSIZE(programContext->beacons); beaconIndex++){
            maxX = MAX(maxX, ABS(programContext->beacons[beaconIndex].worldPosition64.x));
            maxY = MAX(maxY, ABS(programContext->beacons[beaconIndex].worldPosition64.y));
        }
        /*
        for(uint8 moduleIndex = 0; moduleIndex < ARRAYSIZE(programContext->modules); moduleIndex++){
            if(programContext->modules[moduleIndex].run){
                maxX = MAX(maxX, ABS(programContext->modules[moduleIndex].worldPosition64.x));
                maxY = MAX(maxY, ABS(programContext->modules[moduleIndex].worldPosition64.y));
            }
        }
        //hard limit
        maxX = MIN(maxX, 10);
        maxY = MIN(maxY, 10);
        */
        
        float32 scaleX = ((bottomRightCorner.x - offset.x - 2*border)/2)/maxX;
        float32 scaleY = ((bottomRightCorner.y - offset.y - 2*border)/2)/maxY;
        scaleX = scaleY = MIN(scaleY, scaleX);
        
        
        dv2 frameCenter = {(bottomRightCorner.x + offset.x)/2, (bottomRightCorner.y + offset.y)/2};
        
        dv2 a;
        dv2 b;
        a.y = offset.y;
        b.y = bottomRightCorner.y;
        for(int32 meter = (int32)-maxX; meter <= maxX; meter++){
            a.x = b.x = (int32)(meter*scaleX) + frameCenter.x;
            
            drawLine(programContext->renderingTarget, &a, &b, grey, 1);
            char name[4] = {};
            sprintf(name, "%+d", meter);
            printToBitmap(programContext->renderingTarget, a.x, bottomRightCorner.y, name, &programContext->font, axisFontSize, grey);
        }
        
        a.x = offset.x;
        b.x = bottomRightCorner.x;
        for(int32 meter = (int32)-maxY; meter <= maxY; meter++){
            a.y = b.y = (int32)(-meter*scaleY) + frameCenter.y;
            
            drawLine(programContext->renderingTarget, &a, &b, grey, 1);
            char name[4] = {};
            sprintf(name, "%+d", meter);
            printToBitmap(programContext->renderingTarget, 0, a.y - axisFontSize/2, name, &programContext->font, axisFontSize, grey);
        }
        
        
        dv2 pos;
        for(uint8 beaconIndex = 0; beaconIndex < ARRAYSIZE(programContext->beacons); beaconIndex++){
            pos.x = (int32)(scaleX * programContext->beacons[beaconIndex].worldPosition64.x);
            pos.y = (int32)(scaleY * -programContext->beacons[beaconIndex].worldPosition64.y);
            pos = pos + frameCenter;
            drawCircle(programContext->renderingTarget, &pos, 10, beaconsColors[beaconIndex], 1, true);
            if(programContext->localisationType == LocalisationType_Xb){
                drawCircle(programContext->renderingTarget, &pos, (uint32)(scaleX * programContext->beacons[beaconIndex].moduleDistance64[programContext->activeModuleIndex]), beaconsColors[beaconIndex], 1);
                
                
                
            }
            pos = pos + DV2(-10, 10);
            printToBitmap(programContext->renderingTarget, pos.x, pos.y, programContext->beacons[beaconIndex].sidLower + 5, &programContext->font, 12, white);
            
        }
        
        for(uint8 moduleIndex = 0; moduleIndex < ARRAYSIZE(programContext->modules); moduleIndex++){
            if(programContext->modules[moduleIndex].run){
                pos.x = (int32)(scaleX * programContext->modules[moduleIndex].worldPosition64.x);
                pos.y = (int32)(scaleY * -programContext->modules[moduleIndex].worldPosition64.y);
                int32 radius = 20;
                pos = pos + frameCenter;
                drawCircle(programContext->renderingTarget, &pos, radius, moduleColor, 1, true);
                v2 direction;
                direction = V2(-programContext->modules[moduleIndex].worldOrientation64.x, -programContext->modules[moduleIndex].worldOrientation64.y);
                v2 posF = dv2Tov2(pos);
                
                v2 directionOrientation = normalize(direction);
                v2 directionPerpOrientation = {-directionOrientation.y, directionOrientation.x};
                
                //arrow
                v2 trianglePosition = posF + (2 * radius * directionOrientation);
                v2 orientation = 0.14f * size * normalize(trianglePosition - posF);
                v2 orientationPerp = {orientation.y, -orientation.x};
                orientationPerp = 0.5f * orientationPerp;
                
                v2 A = trianglePosition + (0.5f * orientation);
                v2 B = trianglePosition - (0.5f * orientation) + orientationPerp;
                v2 C = trianglePosition - (0.5f * orientation) - orientationPerp;
                dv2 dA = v2Todv2(A);
                dv2 dB = v2Todv2(B);
                dv2 dC = v2Todv2(C);
                
                drawTriangle(programContext->renderingTarget, &dA, &dB, &dC, moduleColor, thickness*2, true);
                
                pos = pos + DV2(-(radius/4), (-radius/2));
                char name[2] = {programContext->modules[moduleIndex].name};
                printToBitmap(programContext->renderingTarget, pos.x, pos.y, name, &programContext->font, radius, black);
            }
        }
        
        offset.x = bottomRightCorner.x + border;
        offset.y = 0;
    }
    
    {
        
        //textual info
        char buffer[122];
        
        if(programContext->localisationType == LocalisationType_Mems_Ori){
            sprintf(buffer, "method: mems orientation");
        }else if(programContext->localisationType == LocalisationType_Mems_Loco){
            sprintf(buffer, "method: mems locomotion");
        }
        else if(programContext->localisationType == LocalisationType_Mems_Comb){
            sprintf(buffer, "method: mems combo");
        }
        else if(programContext->localisationType == LocalisationType_Xb){
            sprintf(buffer, "method: xb");
        }else{
            sprintf(buffer, "method: invalid");
        }
        
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
        offset.y += fontSize*2;
        
        sprintf(buffer, "[%u] module: %1c", programContext->activeModuleIndex, activeModule->name);
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, white);
        int32 oldOffsetX = offset.x;
        offset.x += (strlen(buffer)+1)*fontSize;
        if(activeModule->calibrated){
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, "CALIBRATED", &programContext->font, fontSize, green);
        }else{
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, "NOT CALIBRATED", &programContext->font, fontSize, red);
        }
        offset.x = oldOffsetX;
        offset.y += 2*fontSize;
        
        if(activeModule->run){
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, "Module connected", &programContext->font, fontSize);
        }else{
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, "Module NOT connected", &programContext->font, fontSize);
        }
        offset.y += 2*fontSize;
        
        if(programContext->beaconsRun){
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, "Beacons connected", &programContext->font, fontSize);
        }else{
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, "Beacons NOT connected", &programContext->font, fontSize);
            
        }
        offset.y += 2*fontSize;
        
        if(programContext->localisationType == LocalisationType_Xb){
#if METHOD_XBSP
            sprintf(buffer, "xb frame: %u", activeModule->xbFrame); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
#endif
            
        }
        
        
        if(programContext->localisationType == LocalisationType_Mems_Ori ||
           programContext->localisationType == LocalisationType_Mems_Loco ||
           programContext->localisationType == LocalisationType_Mems_Comb){
            sprintf(buffer, "phys frame: %u", activeModule->physicalFrame); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
        }
        
        {
            sprintf(buffer, "accumulator: %5.2f s", programContext->accumulator); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
        }
        
        
        {
            offset.y += fontSize;
            sprintf(buffer, "world position:"); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x += border;
            sprintf(buffer, "x: %+.3lf", activeModule->worldPosition64.x);
            
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
            
            sprintf(buffer, "y: %+.3lf", activeModule->worldPosition64.y); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
            
            sprintf(buffer, "z: %+.3lf", activeModule->worldPosition64.z); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x -= border;
            offset.y += fontSize;
        }
        
        if(programContext->localisationType == LocalisationType_Xb){
#if METHOD_XBSP            
            sprintf(buffer, "latest time period:"); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x += border;
            
            for(uint8 beaconIndex = 0; beaconIndex < ARRAYSIZE(programContext->beacons); beaconIndex++){
                if(programContext->replay){
                    uint32 latestIndex = programContext->recordData.data[programContext->activeModuleIndex].recordDataXbIndex;      
                    sprintf(buffer, "%9s: %.15f", programContext->beacons[beaconIndex].sidLower, programContext->recordData.data[programContext->activeModuleIndex].xb[latestIndex].delay[beaconIndex]/(float64)programContext->beacons[beaconIndex].timeDivisor);
                    printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
                    offset.y += fontSize;
                }else{
                    sprintf(buffer, "%9s: %.15f", programContext->beacons[beaconIndex].sidLower, activeModule->xbData[(activeModule->xbHeadIndex-1)%ARRAYSIZE(activeModule->xbData)].delay[beaconIndex]/(float64)programContext->beacons[beaconIndex].timeDivisor); 
                    printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
                    offset.y += fontSize;
                }
                
            }
            offset.y += border;
#endif
#if METHOD_XBPNG
            sprintf(buffer, "latest 4 pings:");
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x += border;
            
            
            for(uint8 beaconIndex = 0; beaconIndex < 4; beaconIndex++){
                sprintf(buffer, "%9s: %04u %.9f", programContext->beacons[beaconIndex].sidLower, activeModule->xbFrames[beaconIndex], (activeModule->lastTicks[beaconIndex])/(float64)programContext->beacons[beaconIndex].timeDivisor); 
                printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
                offset.y += fontSize;
            }
#endif
            offset.x -= border;
            sprintf(buffer, "beacon distances:");
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x += border;
            
            for(uint8 beaconIndex = 0; beaconIndex < 4; beaconIndex++){
                sprintf(buffer, "%9s: %.3f", programContext->beacons[beaconIndex].sidLower, programContext->beacons[beaconIndex].moduleDistance64[programContext->activeModuleIndex]); 
                printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
                offset.y += fontSize;
            }
            
#if METHOD_XBPNG
            offset.x -= border;
            sprintf(buffer, "xb AABB:");
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x += border;
            
            sprintf(buffer, "width(x): %.3f", ABS(activeModule->xbAABB.lowerCorner.x - activeModule->xbAABB.upperCorner.x)); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
            sprintf(buffer, "height(y): %.3f", ABS(activeModule->xbAABB.lowerCorner.y - activeModule->xbAABB.upperCorner.y)); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
            sprintf(buffer, "depth(z): %.3f", ABS(activeModule->xbAABB.lowerCorner.z - activeModule->xbAABB.upperCorner.z)); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
#endif
            
        }
        
        
        
        if(programContext->localisationType == LocalisationType_Mems_Ori ||
           programContext->localisationType == LocalisationType_Mems_Loco ||
           programContext->localisationType == LocalisationType_Mems_Comb){
            
            sprintf(buffer, "rotation angles:"); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, blue);
            offset.y += fontSize;
            offset.x += border;
            sprintf(buffer, "x: %.3lf", activeModule->rotationAngles64.x);
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, blue);
            offset.y += fontSize;
            
            
            sprintf(buffer, "y: %.3lf", activeModule->rotationAngles64.y); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, blue);
            offset.y += fontSize;
            
            sprintf(buffer, "z: %.3lf", activeModule->rotationAngles64.z); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, blue);
            offset.y += 2*fontSize;
            offset.x -= border;
            
            sprintf(buffer, "world orientation: %.3lf", length64(activeModule->worldOrientation64)); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, blue);
            offset.y += fontSize;
            offset.x += border;
            sprintf(buffer, "x: %.3lf", activeModule->worldOrientation64.x);
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, blue);
            offset.y += fontSize;
            
            
            sprintf(buffer, "y: %.3lf", activeModule->worldOrientation64.y); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, blue);
            offset.y += fontSize;
            
            sprintf(buffer, "z: %.3lf", activeModule->worldOrientation64.z); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, blue);
            offset.y += 2*fontSize;
            offset.x -= border;
        }
        
        if(programContext->localisationType == LocalisationType_Mems_Ori ||
           programContext->localisationType == LocalisationType_Mems_Loco ||
           programContext->localisationType == LocalisationType_Mems_Comb){
            sprintf(buffer, "acc: %5.2lf m/s2", length64(activeModule->acceleration64));
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, red);
            offset.y += fontSize;
            offset.x += border;
            sprintf(buffer, "x: %+.3lf", activeModule->acceleration64.x); 
            
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, red);
            offset.y += fontSize;
            
            sprintf(buffer, "y: %+.3lf", activeModule->acceleration64.y); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, red);
            offset.y += fontSize;
            
            
            sprintf(buffer, "z: %+.3lf", activeModule->acceleration64.z); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, red);
            offset.y += 2*fontSize;
            offset.x -= border;
            
        }
        
        if(programContext->localisationType == LocalisationType_Mems_Ori ||
           programContext->localisationType == LocalisationType_Mems_Loco ||
           programContext->localisationType == LocalisationType_Mems_Comb){
            sprintf(buffer, "vel: %5.2lf m/s", length64(activeModule->velocity64));
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x += border;
            
            sprintf(buffer, "x: %+.3lf", activeModule->velocity64.x); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
            sprintf(buffer, "y: %+.3lf", activeModule->velocity64.y); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
            sprintf(buffer, "z: %+.3lf", activeModule->velocity64.z); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x -= border;
            
            offset.y += border;
        }
        
        
        
    }
    
    offset = {border, (int32)programContext->renderingTarget->info.height - size - border};
    
    if(programContext->localisationType == LocalisationType_Mems_Ori ||
       programContext->localisationType == LocalisationType_Mems_Loco ||
       programContext->localisationType == LocalisationType_Mems_Comb){
        
        //orientation pitch
        {
            printToBitmap(programContext->renderingTarget, offset.x, offset.y -border, "PITCH", &programContext->font, fontSize, blue);
            
            //static stuff
            dv2 bottomLeftCorner = {offset.x, offset.y + size};
            dv2 bottomRightCorner = {offset.x + size, offset.y + size};
            dv2 center = {offset.x + sizeHalf, offset.y + sizeHalf};
            dv2 centerLeft = {offset.x, center.y};
            dv2 centerRight = {offset.x + size, center.y};
            dv2 centerTop = {center.x, offset.y};
            dv2 centerBot = {center.x, offset.y + size};
            
            drawLine(programContext->renderingTarget, &bottomLeftCorner, &bottomRightCorner, white, thickness);
            drawLine(programContext->renderingTarget, &centerTop, &centerBot, grey, thickness);
            drawLine(programContext->renderingTarget, &centerLeft, &centerRight, grey, thickness);
            
            drawCircle(programContext->renderingTarget, &center, sizeHalf, white, thickness);
            
            
            //body
            
            dv2 humanBodyTopLeftCorner = {center.x - humanBodyWidthHalf, center.y - humanSizeHalf + humanBodyOffsetHalf};dv2 humanBodyBottomRightCorner = {center.x + humanBodyWidthHalf, center.y + humanSizeHalf + humanBodyOffsetHalf};
            
            drawRectangle(programContext->renderingTarget, &humanBodyTopLeftCorner, &humanBodyBottomRightCorner, blue, thickness*2);
            
            //head
            
            dv2 humanHeadTopLeftCorner = {center.x - humanBodyWidthHalf, humanBodyTopLeftCorner.y - humanHeadSize - gapSize};dv2 humanHeadBottomRightCorner = {center.x + humanBodyWidthHalf, humanBodyTopLeftCorner.y - gapSize};
            dv2 humanHeadTopRightCorner = {humanHeadBottomRightCorner.x, humanHeadTopLeftCorner.y};
            dv2 humanHeadBottomLeftCorner = {humanHeadTopLeftCorner.x, humanHeadBottomRightCorner.y};
            
            dv2 humanHeadCenter = {humanHeadTopLeftCorner.x + (humanHeadTopRightCorner.x - humanHeadTopLeftCorner.x)/2, humanHeadTopLeftCorner.y + (humanHeadBottomLeftCorner.y - humanHeadTopLeftCorner.y)/2};
            v2 humanHeadCenterF = dv2Tov2(humanHeadCenter);
            
            v2 humanHeadTopLeftCornerF = rotate(humanHeadTopLeftCorner - humanHeadCenter, -rotationY) + humanHeadCenterF;
            v2 humanHeadTopRightCornerF = rotate(humanHeadTopRightCorner - humanHeadCenter, -rotationY) + humanHeadCenterF;
            v2 humanHeadBottomLeftCornerF = rotate(humanHeadBottomLeftCorner - humanHeadCenter, -rotationY) + humanHeadCenterF;
            v2 humanHeadBottomRightCornerF = rotate(humanHeadBottomRightCorner - humanHeadCenter, -rotationY) + humanHeadCenterF;
            
            
            dv2 humanHeadResultTopLeftCorner = v2Todv2(humanHeadTopLeftCornerF);
            dv2 humanHeadResultTopRightCorner = v2Todv2(humanHeadTopRightCornerF);
            dv2 humanHeadResultBottomLeftCorner = v2Todv2(humanHeadBottomLeftCornerF);
            dv2 humanHeadResultBottomRightCorner = v2Todv2(humanHeadBottomRightCornerF);
            
            
            drawQuad(programContext->renderingTarget, &humanHeadResultTopLeftCorner, &humanHeadResultTopRightCorner, &humanHeadResultBottomLeftCorner, &humanHeadResultBottomRightCorner, blue, thickness*2);
            
            //eyes
            v2 foreheadToChinDirection = normalize(humanHeadBottomLeftCornerF - humanHeadTopLeftCornerF);
            v2 foreheadToBackDirection = normalize(humanHeadTopRightCornerF - humanHeadTopLeftCornerF);
            
            
            v2 humanHeadTopLeftEyeCornerF = humanHeadTopLeftCornerF + eyePosition * humanHeadSize * foreheadToChinDirection;
            v2 humanHeadBottomLeftEyeCornerF = humanHeadTopLeftEyeCornerF + eyeSize * foreheadToChinDirection;
            
            v2 humanHeadTopRightEyeCornerF = humanHeadTopLeftEyeCornerF + eyeSize * foreheadToBackDirection;
            v2 humanHeadBottomRightEyeCornerF = humanHeadBottomLeftEyeCornerF + eyeSize * foreheadToBackDirection;
            
            dv2 humanHeadTopLeftEyeCorner = v2Todv2(humanHeadTopLeftEyeCornerF);
            dv2 humanHeadTopRightEyeCorner = v2Todv2(humanHeadTopRightEyeCornerF);
            dv2 humanHeadBottomLeftEyeCorner = v2Todv2(humanHeadBottomLeftEyeCornerF);
            dv2 humanHeadBottomRightEyeCorner = v2Todv2(humanHeadBottomRightEyeCornerF);
            
            drawQuad(programContext->renderingTarget, &humanHeadTopLeftEyeCorner, &humanHeadTopRightEyeCorner, &humanHeadBottomLeftEyeCorner, &humanHeadBottomRightEyeCorner, blue, thickness*2);
            
            //arrow
            
            v2 centerF = dv2Tov2(center);
            v2 trianglePosition = rotate(centerLeft - center, -rotationY) + centerF;
            
            v2 orientation = 0.14f * size * normalize(trianglePosition - centerF);
            v2 orientationPerp = {orientation.y, -orientation.x};
            orientationPerp = 0.5f * orientationPerp;
            
            v2 A = trianglePosition + (0.5f * orientation);
            v2 B = trianglePosition - (0.5f * orientation) + orientationPerp;
            v2 C = trianglePosition - (0.5f * orientation) - orientationPerp;
            dv2 dA = v2Todv2(A);
            dv2 dB = v2Todv2(B);
            dv2 dC = v2Todv2(C);
            
            drawTriangle(programContext->renderingTarget, &dA, &dB, &dC, blue, thickness*2);
            offset.x += size + border;
        }
        
        
        //orientation roll
        {
            printToBitmap(programContext->renderingTarget, offset.x, offset.y -border, "ROLL", &programContext->font, fontSize, blue);
            //static stuff
            dv2 bottomLeftCorner = {offset.x, offset.y + size};
            dv2 bottomRightCorner = {offset.x + size, offset.y + size};
            dv2 center = {offset.x + sizeHalf, offset.y + sizeHalf};
            dv2 centerLeft = {offset.x, center.y};
            dv2 centerRight = {offset.x + size, center.y};
            dv2 centerTop = {center.x, offset.y};
            dv2 centerBot = {center.x, offset.y + size};
            
            drawLine(programContext->renderingTarget, &bottomLeftCorner, &bottomRightCorner, white, thickness);
            drawLine(programContext->renderingTarget, &centerTop, &centerBot, grey, thickness);
            drawLine(programContext->renderingTarget, &centerLeft, &centerRight, grey, thickness);
            
            drawCircle(programContext->renderingTarget, &center, sizeHalf, white, thickness);
            
            
            v2 centerF = dv2Tov2(center);
            
            //body
            
            dv2 humanBodyTopLeftCorner = {center.x - humanBodyWidthHalf, center.y - humanSizeHalf + humanBodyOffsetHalf};dv2 humanBodyBottomRightCorner = {center.x + humanBodyWidthHalf, center.y + humanSizeHalf + humanBodyOffsetHalf};
            dv2 humanBodyTopRightCorner = {humanBodyBottomRightCorner.x, humanBodyTopLeftCorner.y};
            dv2 humanBodyBottomLeftCorner = {humanBodyTopLeftCorner.x, humanBodyBottomRightCorner.y};
            
            v2 humanBodyTopLeftCornerF = rotate(humanBodyTopLeftCorner - center, rotationX) + centerF;
            v2 humanBodyTopRightCornerF = rotate(humanBodyTopRightCorner - center, rotationX) + centerF;
            v2 humanBodyBottomLeftCornerF = rotate(humanBodyBottomLeftCorner - center, rotationX) + centerF;
            v2 humanBodyBottomRightCornerF = rotate(humanBodyBottomRightCorner - center, rotationX) + centerF;
            
            dv2 humanBodyResultTopLeftCorner = v2Todv2(humanBodyTopLeftCornerF);
            dv2 humanBodyResultTopRightCorner = v2Todv2(humanBodyTopRightCornerF);
            dv2 humanBodyResultBottomLeftCorner = v2Todv2(humanBodyBottomLeftCornerF);
            dv2 humanBodyResultBottomRightCorner = v2Todv2(humanBodyBottomRightCornerF);
            
            drawQuad(programContext->renderingTarget, &humanBodyResultTopLeftCorner, &humanBodyResultTopRightCorner, &humanBodyResultBottomLeftCorner, &humanBodyResultBottomRightCorner, blue, thickness*2);
            
            //head
            
            dv2 humanHeadTopLeftCorner = {center.x - humanBodyWidthHalf, humanBodyTopLeftCorner.y - humanHeadSize - gapSize};dv2 humanHeadBottomRightCorner = {center.x + humanBodyWidthHalf, humanBodyTopLeftCorner.y - gapSize};
            dv2 humanHeadTopRightCorner = {humanHeadBottomRightCorner.x, humanHeadTopLeftCorner.y};
            dv2 humanHeadBottomLeftCorner = {humanHeadTopLeftCorner.x, humanHeadBottomRightCorner.y};
            
            
            v2 humanHeadTopLeftCornerF = rotate(humanHeadTopLeftCorner - center, rotationX) + centerF;
            v2 humanHeadTopRightCornerF = rotate(humanHeadTopRightCorner - center, rotationX) + centerF;
            v2 humanHeadBottomLeftCornerF = rotate(humanHeadBottomLeftCorner - center, rotationX) + centerF;
            v2 humanHeadBottomRightCornerF = rotate(humanHeadBottomRightCorner - center, rotationX) + centerF;
            
            
            dv2 humanHeadResultTopLeftCorner = v2Todv2(humanHeadTopLeftCornerF);
            dv2 humanHeadResultTopRightCorner = v2Todv2(humanHeadTopRightCornerF);
            dv2 humanHeadResultBottomLeftCorner = v2Todv2(humanHeadBottomLeftCornerF);
            dv2 humanHeadResultBottomRightCorner = v2Todv2(humanHeadBottomRightCornerF);
            
            
            drawQuad(programContext->renderingTarget, &humanHeadResultTopLeftCorner, &humanHeadResultTopRightCorner, &humanHeadResultBottomLeftCorner, &humanHeadResultBottomRightCorner, blue, thickness*2);
            
            
            //arrow
            
            
            v2 trianglePosition = rotate(centerLeft - center, rotationX) + centerF;
            
            v2 orientation = 0.14f * size * normalize(trianglePosition - centerF);
            v2 orientationPerp = {orientation.y, -orientation.x};
            orientationPerp = 0.5f * orientationPerp;
            
            v2 A = trianglePosition + (0.5f * orientation);
            v2 B = trianglePosition - (0.5f * orientation) + orientationPerp;
            v2 C = trianglePosition - (0.5f * orientation) - orientationPerp;
            dv2 dA = v2Todv2(A);
            dv2 dB = v2Todv2(B);
            dv2 dC = v2Todv2(C);
            
            drawTriangle(programContext->renderingTarget, &dA, &dB, &dC, blue, thickness*2);
            offset.x += size + border;
        }
        
        //orientation yaw
        {
            printToBitmap(programContext->renderingTarget, offset.x, offset.y -border, "YAW", &programContext->font, fontSize, blue);
            //static stuff
            dv2 topLeftCorner = {offset.x, offset.y};
            dv2 bottomRightCorner = {offset.x + size, offset.y + size};
            dv2 center = {offset.x + sizeHalf, offset.y + sizeHalf};
            dv2 centerLeft = {offset.x, center.y};
            dv2 centerRight = {offset.x + size, center.y};
            dv2 centerTop = {center.x, offset.y};
            dv2 centerBot = {center.x, offset.y + size};
            
            
            
            drawRectangle(programContext->renderingTarget, &topLeftCorner, &bottomRightCorner, white, thickness);
            
            drawLine(programContext->renderingTarget, &centerTop, &centerBot, grey, thickness);
            drawLine(programContext->renderingTarget, &centerLeft, &centerRight, grey, thickness);
            
            drawCircle(programContext->renderingTarget, &center, sizeHalf, white, thickness);
            
            
            v2 centerF = dv2Tov2(center);
            
            
            //head
            
            dv2 humanHeadTopLeftCorner = {center.x - humanHeadSizeHalf, center.y - humanHeadSizeHalf};
            dv2 humanHeadBottomRightCorner = {center.x + humanHeadSizeHalf,center.y + humanHeadSizeHalf};
            dv2 humanHeadTopRightCorner = {humanHeadBottomRightCorner.x, humanHeadTopLeftCorner.y};
            dv2 humanHeadBottomLeftCorner = {humanHeadTopLeftCorner.x, humanHeadBottomRightCorner.y};
            
            
            v2 humanHeadTopLeftCornerF = rotate(humanHeadTopLeftCorner - center, -rotationZ) + centerF;
            v2 humanHeadTopRightCornerF = rotate(humanHeadTopRightCorner - center, -rotationZ) + centerF;
            v2 humanHeadBottomLeftCornerF = rotate(humanHeadBottomLeftCorner - center, -rotationZ) + centerF;
            v2 humanHeadBottomRightCornerF = rotate(humanHeadBottomRightCorner - center, -rotationZ) + centerF;
            
            
            dv2 humanHeadResultTopLeftCorner = v2Todv2(humanHeadTopLeftCornerF);
            dv2 humanHeadResultTopRightCorner = v2Todv2(humanHeadTopRightCornerF);
            dv2 humanHeadResultBottomLeftCorner = v2Todv2(humanHeadBottomLeftCornerF);
            dv2 humanHeadResultBottomRightCorner = v2Todv2(humanHeadBottomRightCornerF);
            
            
            drawQuad(programContext->renderingTarget, &humanHeadResultTopLeftCorner, &humanHeadResultTopRightCorner, &humanHeadResultBottomLeftCorner, &humanHeadResultBottomRightCorner, blue, thickness*2);
            
            
            //arrow
            
            
            v2 trianglePosition = rotate(centerTop - center, -rotationZ) + centerF;
            
            v2 orientation = 0.14f * size * normalize(trianglePosition - centerF);
            
            v2 orientationPerp = {orientation.y, -orientation.x};
            orientationPerp = 0.5f * orientationPerp;
            
            v2 A = trianglePosition + (0.5f * orientation);
            v2 B = trianglePosition - (0.5f * orientation) + orientationPerp;
            v2 C = trianglePosition - (0.5f * orientation) - orientationPerp;
            dv2 dA = v2Todv2(A);
            dv2 dB = v2Todv2(B);
            dv2 dC = v2Todv2(C);
            
            drawTriangle(programContext->renderingTarget, &dA, &dB, &dC, blue, thickness*2);
            offset.x += size + border;
        }
        
        //acc yaw  if not zero, check for it
        v2 topDownAcc = V2(accY, accX);
        {
            printToBitmap(programContext->renderingTarget, offset.x, offset.y -border, "ACC YAW", &programContext->font, fontSize, red);
            //static stuff
            dv2 topLeftCorner = {offset.x, offset.y};
            dv2 bottomRightCorner = {offset.x + size, offset.y + size};
            dv2 center = {offset.x + sizeHalf, offset.y + sizeHalf};
            dv2 centerLeft = {offset.x, center.y};
            dv2 centerRight = {offset.x + size, center.y};
            dv2 centerTop = {center.x, offset.y};
            dv2 centerBot = {center.x, offset.y + size};
            
            
            
            drawRectangle(programContext->renderingTarget, &topLeftCorner, &bottomRightCorner, white, thickness);
            
            drawLine(programContext->renderingTarget, &centerTop, &centerBot, grey, thickness);
            drawLine(programContext->renderingTarget, &centerLeft, &centerRight, grey, thickness);
            
            drawCircle(programContext->renderingTarget, &center, sizeHalf, white, thickness);
            offset.x += size + border;
            
            if(length(topDownAcc) > 0)
            {
                v2 centerF = dv2Tov2(center);
                
                
                v2 accOrientation = normalize(topDownAcc);
                v2 accPerpOrientation = {-accOrientation.y, accOrientation.x};
                //head
                
                dv2 humanHeadTopLeftCorner = {center.x - humanHeadSizeHalf, center.y - humanHeadSizeHalf};
                dv2 humanHeadBottomRightCorner = {center.x + humanHeadSizeHalf, center.y + humanHeadSizeHalf};
                
                drawRectangle(programContext->renderingTarget, &humanHeadTopLeftCorner, &humanHeadBottomRightCorner, red, thickness*2);
                
                
                //arrow
                
                
                v2 trianglePosition = centerF + ((float32)sizeHalf) * accOrientation;
                
                v2 orientation = 0.14f * size * normalize(trianglePosition - centerF);
                v2 orientationPerp = {orientation.y, -orientation.x};
                orientationPerp = 0.5f * orientationPerp;
                
                v2 A = trianglePosition + (0.5f * orientation);
                v2 B = trianglePosition - (0.5f * orientation) + orientationPerp;
                v2 C = trianglePosition - (0.5f * orientation) - orientationPerp;
                dv2 dA = v2Todv2(A);
                dv2 dB = v2Todv2(B);
                dv2 dC = v2Todv2(C);
                
                drawTriangle(programContext->renderingTarget, &dA, &dB, &dC, red, thickness*2);
                
            }
        }
        
        //acc pitch if not zero, check for it
        v2 sideAcc = V2(accX, accZ);
        {
            printToBitmap(programContext->renderingTarget, offset.x, offset.y -border, "ACC PITCH", &programContext->font, fontSize, red);
            
            //static stuff
            dv2 bottomLeftCorner = {offset.x, offset.y + size};
            dv2 bottomRightCorner = {offset.x + size, offset.y + size};
            dv2 center = {offset.x + sizeHalf, offset.y + sizeHalf};
            dv2 centerLeft = {offset.x, center.y};
            dv2 centerRight = {offset.x + size, center.y};
            dv2 centerTop = {center.x, offset.y};
            dv2 centerBot = {center.x, offset.y + size};
            
            
            
            drawLine(programContext->renderingTarget, &bottomLeftCorner, &bottomRightCorner, white, thickness);
            
            drawLine(programContext->renderingTarget, &centerTop, &centerBot, grey, thickness);
            drawLine(programContext->renderingTarget, &centerLeft, &centerRight, grey, thickness);
            
            drawCircle(programContext->renderingTarget, &center, sizeHalf, white, thickness);
            offset.x += size + border;
            
            if(length(sideAcc) > 0){
                
                v2 centerF = dv2Tov2(center);
                
                
                v2 accOrientation = normalize(sideAcc);
                
                //body
                
                dv2 humanBodyTopLeftCorner = {center.x - humanBodyWidthHalf, center.y - humanSizeHalf + humanBodyOffsetHalf};dv2 humanBodyBottomRightCorner = {center.x + humanBodyWidthHalf, center.y + humanSizeHalf + humanBodyOffsetHalf};
                
                drawRectangle(programContext->renderingTarget, &humanBodyTopLeftCorner, &humanBodyBottomRightCorner, red, thickness*2);
                
                //head
                
                dv2 humanHeadTopLeftCorner = {center.x - humanBodyWidthHalf, humanBodyTopLeftCorner.y - humanHeadSize - gapSize};
                dv2 humanHeadBottomRightCorner = {center.x + humanBodyWidthHalf, humanBodyTopLeftCorner.y - gapSize};
                
                drawRectangle(programContext->renderingTarget, &humanHeadTopLeftCorner, &humanHeadBottomRightCorner, red, thickness*2);
                
                //eyes
                
                v2 foreheadToChinDirection = normalize(DV2(humanHeadTopLeftCorner.x, humanHeadTopLeftCorner.y+humanHeadSize) - humanHeadTopLeftCorner);
                v2 foreheadToBackDirection = normalize(DV2(humanHeadTopLeftCorner.x + humanHeadSize, humanHeadTopLeftCorner.y) - humanHeadTopLeftCorner);
                
                
                v2 humanHeadTopLeftEyeCornerF = humanHeadTopLeftCorner + eyePosition * humanHeadSize * foreheadToChinDirection;
                v2 humanHeadBottomLeftEyeCornerF = humanHeadTopLeftEyeCornerF + eyeSize * foreheadToChinDirection;
                
                v2 humanHeadTopRightEyeCornerF = humanHeadTopLeftEyeCornerF + eyeSize * foreheadToBackDirection;
                v2 humanHeadBottomRightEyeCornerF = humanHeadBottomLeftEyeCornerF + eyeSize * foreheadToBackDirection;
                
                dv2 humanHeadTopLeftEyeCorner = v2Todv2(humanHeadTopLeftEyeCornerF);
                dv2 humanHeadTopRightEyeCorner = v2Todv2(humanHeadTopRightEyeCornerF);
                dv2 humanHeadBottomLeftEyeCorner = v2Todv2(humanHeadBottomLeftEyeCornerF);
                dv2 humanHeadBottomRightEyeCorner = v2Todv2(humanHeadBottomRightEyeCornerF);
                
                drawQuad(programContext->renderingTarget, &humanHeadTopLeftEyeCorner, &humanHeadTopRightEyeCorner, &humanHeadBottomLeftEyeCorner, &humanHeadBottomRightEyeCorner, red, thickness*2);
                
                
                
                //arrow
                
                
                v2 trianglePosition = centerF + ((float32)sizeHalf) * accOrientation;
                
                v2 orientation = 0.14f * size * normalize(trianglePosition - centerF);
                v2 orientationPerp = {orientation.y, -orientation.x};
                orientationPerp = 0.5f * orientationPerp;
                
                v2 A = trianglePosition + (0.5f * orientation);
                v2 B = trianglePosition - (0.5f * orientation) + orientationPerp;
                v2 C = trianglePosition - (0.5f * orientation) - orientationPerp;
                dv2 dA = v2Todv2(A);
                dv2 dB = v2Todv2(B);
                dv2 dC = v2Todv2(C);
                
                drawTriangle(programContext->renderingTarget, &dA, &dB, &dC, red, thickness*2);
                
            }
        }
        
        //acc size
        {
            printToBitmap(programContext->renderingTarget, offset.x, offset.y -border, "ACC SIZE", &programContext->font, fontSize, red);
            offset.x += sizeHalf/2;
            
            //static stuff
            dv2 topLeftCorner = {offset.x, offset.y};
            dv2 bottomRightCorner = {offset.x + sizeHalf, offset.y + size};
            
            
            
            
            float32 currentAcc = length(V3(accX, accY, accZ));
            float32 fill = size*currentAcc/maximumAcc;
            int32 levelOffset = size - (int32)fill;
            
            dv2 levelTopLeftCorner = {topLeftCorner.x, topLeftCorner.y + levelOffset};
            drawRectangle(programContext->renderingTarget, &levelTopLeftCorner, &bottomRightCorner, red, thickness, true);
            
            drawRectangle(programContext->renderingTarget, &topLeftCorner, &bottomRightCorner, white, thickness);
        }
    } //if both or mems
    
    
    if(programContext->drawHelp){
        int32 helpWidth = programContext->renderingTarget->info.width / 3;
        int32 helpHeight = programContext->renderingTarget->info.height / 3;
        
        int32 startX = (programContext->renderingTarget->info.width - helpWidth)/2;
        int32 startY = (programContext->renderingTarget->info.height - helpHeight)/2;
        
        int32 endX = startX + helpWidth;
        int32 endY = startY + helpHeight;
        
        //draw black
        for(int32 y = startY; y <= endY; y++){
            int32 pitch = y*programContext->renderingTarget->info.width;
            for(int32 x = startX; x <= endX; x++){
                ((uint32 *)programContext->renderingTarget->data)[pitch + x] = 0; 
            }
        }
        int32 borderX = 10;
        int32 borderY = 10;
        
        dv2 offset = {startX + borderX, startY + borderY}; 
        
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, "HELP [press H to close]", &programContext->font, fontSize*2, white);
        offset.y += fontSize*2;
        
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, "[R] Start/stop/restart recording", &programContext->font, fontSize, white);
        offset.y += fontSize;
        
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, "[P] reposition modules", &programContext->font, fontSize, white);
        offset.y += fontSize;
        
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, "[1] Show module 1 detail", &programContext->font, fontSize, white);
        offset.y += fontSize;
        
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, "[2] Show module 2 detail", &programContext->font, fontSize, white);
        offset.y += fontSize;
        
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, "[F1] Algorithm: Mems orientation", &programContext->font, fontSize, white);
        offset.y += fontSize;
        
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, "[F2] Algorithm: Mems locomotion", &programContext->font, fontSize, white);
        offset.y += fontSize;
        
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, "[F3] Algorithm: Mems combination", &programContext->font, fontSize, white);
        offset.y += fontSize;
        
#if METHOD_XBSP
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, "[F4] Algorithm: XB pump", &programContext->font, fontSize, white);
        offset.y += fontSize;
#endif
        
#if METHOD_XBPNG
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, "[F4] Algorithm: XB ping", &programContext->font, fontSize, white);
        offset.y += fontSize;
#endif
        
        
        
        
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


