
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

#include "winsock2.h"
#include "ws2tcpip.h"

#include "windows_types.h"
#include "common.h"

#include "servercode_memory.h"

#include "servercode_common.h"

#define sleep(n) Sleep((n)*1000000);

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


struct MemsData{
    
    int16 accX;
    int16 accY;
    int16 accZ;
    
    int16 gyroX;
    int16 gyroY;
    int16 gyroZ;
    
};

struct XbData{
    float64 delay[4];
};


enum LocalisationType{
    LocalisationType_Invalid,
    
    LocalisationType_Mems,
    LocalisationType_Xb,
    LocalisationType_Both,
    
    LocalisationTypeCount
};

struct ProgramContext : Common{
    bool inited;
    BitmapFont font;
    NetSocket serverSocket;
    NetSocket boeingSocket[2];
    NetSocket beaconsSocket;
    struct Module{
        
        char name;
        MPU6050Settings settings;
        
        v3 orientation;
        v3 acceleration;
        v3 velocity;
        v3 position;
        
        v3 defaultOrientation;
        v3 defaultPosition;
        
        MemsData memsData[86];
        int32 memsTailIndex;
        int32 memsHeadIndex;
        uint32 memsStepsAvailable;
        
        XbData xbData[1000];
        int32 xbTailIndex;
        int32 xbHeadIndex;
        uint32 xbStepsAvailable;
        
        
        v3 gyroBiasLower;
        v3 gyroBias;
        v3 gyroBiasUpper;
        
        v3 accelerationBiasLower;
        v3 accelerationBias;
        v3 accelerationBiasUpper;
        
        uint32 physicalFrame;
        uint32 xbFrame;
        char memsDataBuffer[4096];
        char xbDataBuffer[4096];
        
        Message lastMemsMessage;
        
        bool run;
        uint32 accumulatedSize;
    } modules[2];
    
    struct Beacon{
        uint16 frequency;
        char channel[4];
        char pan[5];
        v3 position;
        char sidLower[9];
    } beacons[4];
    
    bool beaconsRun;
    
    Image * renderingTarget;
    float32 accumulator;
    uint32 beaconsAccumulatedSize;
    Message lastBeaconsMessage;
    uint32 newClientAccumulatedSize;
    
    LocalisationType localisationType;
    
    uint8 activeModuleIndex;
    
    FileWatchHandle configFileWatch;
};

ProgramContext * programContext;
bool inited = false;





void resetBeacons(){
    //programContext->beaconsAccumulatedSize = 0;
}

void resetModule(int index){
    ProgramContext::Module * module = &programContext->modules[index];
    module->memsTailIndex = 0;
    module->memsHeadIndex = 0;
    module->memsStepsAvailable = 0;
    
    module->xbTailIndex = 0;
    module->xbHeadIndex = 0;
    module->xbStepsAvailable = 0;
    
    module->xbFrame = 0;
    module->physicalFrame = 0;
    
    module->orientation = module->defaultOrientation;
    module->position = module->defaultPosition;
    module->velocity = V3(0, 0, 0);
    module->acceleration = V3(0, 0, 0);
    
    module->accelerationBiasLower = {};
    module->accelerationBias = {};
    module->accelerationBiasUpper = {};
    
    module->gyroBiasLower = {};
    module->gyroBias = {};
    module->gyroBiasUpper = {};
    
}

extern "C" __declspec(dllexport) void handleInputDomainRoutine(const ServerInput * input){
    if(input->boeing1){
        programContext->activeModuleIndex = 0;
    }
    if(input->boeing2){
        programContext->activeModuleIndex = 1;
    }
    
    if(input->method1){
        programContext->localisationType = LocalisationType_Mems;
    }
    if(input->method2){
        programContext->localisationType = LocalisationType_Xb;
    }
    if(input->method3){
        programContext->localisationType = LocalisationType_Both;
    }
}

extern "C" __declspec(dllexport) void boeingDomainRoutine(int index){
    if(!inited || !programContext->inited) return;
    NetRecvResult result;
    
    
    ProgramContext::Module * module = &programContext->modules[index];
    
    Message * wrap = &module->lastMemsMessage;
    
    if(programContext->keepRunning && module->run){
        result.bufferLength = sizeof(Message) - module->accumulatedSize;
        result.buffer = ((char *) wrap) + module->accumulatedSize;
        NetResultType resultCode = netRecv(&programContext->boeingSocket[index], &result);
        if(resultCode == NetResultType_Ok){
            module->accumulatedSize += result.resultLength;
            if(module->accumulatedSize == sizeof(Message)){
                
                if(wrap->type == MessageType_Reset){
                    resetModule(index);
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
                    target->accX = ((uint16)(*(module->memsDataBuffer + offset + suboffset)) << 8) + *(module->memsDataBuffer + offset + suboffset + 1);
                    suboffset += 2;
                    target->accY = ((uint16)(*(module->memsDataBuffer + offset + suboffset)) << 8) + *(module->memsDataBuffer + offset + suboffset + 1);
                    suboffset += 2;
                    target->accZ = ((uint16)(*(module->memsDataBuffer + offset + suboffset)) << 8) + *(module->memsDataBuffer + offset + suboffset + 1);
                    
                    suboffset += 2;
                    target->gyroX = ((uint16)(*(module->memsDataBuffer + offset + suboffset)) << 8) + *(module->memsDataBuffer + offset + suboffset + 1);
                    suboffset += 2;
                    target->gyroY = ((uint16)(*(module->memsDataBuffer + offset + suboffset)) << 8) + *(module->memsDataBuffer + offset + suboffset + 1);
                    suboffset += 2;
                    target->gyroZ = ((uint16)(*(module->memsDataBuffer + offset + suboffset)) << 8) + *(module->memsDataBuffer + offset + suboffset + 1);
                    
                    module->memsHeadIndex = (module->memsHeadIndex + 1) % ARRAYSIZE(module->memsData);
                    FETCH_AND_ADD(&module->memsStepsAvailable, 1);
                }
                module->accumulatedSize = 0;
            }
        }else{
            closeSocket(&programContext->boeingSocket[index]);
            module->run = false;
        }
    }
}


extern "C" __declspec(dllexport) void beaconsDomainRoutine(){
    if(!inited || !programContext->inited) return;
    NetRecvResult result;
    result.bufferLength = sizeof(Message);
    Message * message = &programContext->lastBeaconsMessage;
    if(programContext->keepRunning && programContext->beaconsRun){
        result.bufferLength = sizeof(Message) - programContext->beaconsAccumulatedSize;
        result.buffer = ((char *) message) + programContext->beaconsAccumulatedSize;
        NetResultType resultCode = netRecv(&programContext->beaconsSocket, &result);
        if(resultCode == NetResultType_Ok){
            programContext->beaconsAccumulatedSize += result.resultLength;
            if(programContext->beaconsAccumulatedSize == sizeof(Message)){
                if(message->type == MessageType_Reset){
                    resetBeacons();
                    programContext->beaconsAccumulatedSize = 0;
                    return;
                }
                ASSERT(message->data.boeingId <= 1);
                ProgramContext::Module * module = &programContext->modules[message->data.boeingId];
                
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
                
                ASSERT(accumulated == message->data.length);
                ASSERT(accumulated % sizeof(XbData) == 0);
                ASSERT(accumulated / sizeof(XbData) <= ARRAYSIZE(module->xbData) - module->xbStepsAvailable);
                for(uint32 offset = 0; offset < message->data.length; offset += sizeof(XbData)){
                    
                    module->xbData[module->xbHeadIndex] = *((XbData *) (module->xbDataBuffer + offset));
                    module->xbHeadIndex = (module->xbHeadIndex + 1) % ARRAYSIZE(module->xbData);
                    FETCH_AND_ADD(&module->xbStepsAvailable, 1);
                    
                    
                }
                programContext->beaconsAccumulatedSize = 0;
            }
            
        }else{
            closeSocket(&programContext->beaconsSocket);
            programContext->beaconsRun = false;
        }
    }
}



extern "C" __declspec(dllexport) void serverDomainRoutine(){
    if(!inited || !programContext->inited) return;
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
                        if(!programContext->modules[i].run){
                            found = true;
                            break;
                        }
                    }
                    ASSERT(found);
                    
                    
                    ProgramContext::Module * module = &programContext->modules[i];
                    
                    programContext->boeingSocket[i] = socket;
                    
                    
                    module->name = wrap->init.boeing.name;
                    module->settings = wrap->init.boeing.settings;
                    
                    ProgramContext::Beacon * aBeacon = &programContext->beacons[0];
                    
                    Message handshake;
                    handshake.type = MessageType_Init;
                    handshake.init.clientType = ClientType_Beacon;
                    handshake.init.beacon.frequency = aBeacon->frequency;
                    strncpy(handshake.init.beacon.channel, aBeacon->channel, 3);
                    strncpy(handshake.init.beacon.pan, aBeacon->pan, 5);
                    
                    NetSendSource message;
                    message.buffer = (char*)&handshake;
                    message.bufferLength = sizeof(Message);
                    
                    while(netSend(&programContext->boeingSocket[i], &message) != NetResultType_Ok){
                        
                    }
                    //default attributes
                    resetModule(i);
                    module->run = true;
                    
                }else if(clientType == ClientType_Beacon){
                    //reorder beacons according to the beacon client
                    for(uint8 sourceIndex = 0; sourceIndex < ARRAYSIZE(programContext->beacons); sourceIndex++){
                        uint8 targetIndex = 0;
                        for(; targetIndex < ARRAYSIZE(programContext->beacons); targetIndex++){
                            if(!strncmp(wrap->init.beacon.sidLower[sourceIndex], programContext->beacons[targetIndex].sidLower, 8)){
                                if(sourceIndex != targetIndex){
                                    //swap
                                    v3 tempPosition = programContext->beacons[sourceIndex].position;
                                    programContext->beacons[sourceIndex].position = programContext->beacons[targetIndex].position;
                                    programContext->beacons[targetIndex].position = programContext->beacons[sourceIndex].position;
                                    
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
                        beacon->frequency = wrap->init.beacon.frequency;
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
    }
    
}

#include "util_config.cpp"

const char * configPath = "data/server.config";

static bool parseConfig(const char * line){
    if(!strncmp("ip", line, 2)){
        memset(programContext->ip, 0, 16);
        memset(programContext->port, 0, 6);
        return sscanf(line, "ip %16[^ ] %6[^\r\n ]", programContext->ip, programContext->port) == 2;
    }else if(!strncmp("beacons", line, 7)){
        return sscanf(line, "beacons %9[^ \r\n] %9[^ \r\n] %9[^ \r\n] %9[^ \r\n]", programContext->beacons[0].sidLower, programContext->beacons[1].sidLower, programContext->beacons[2].sidLower, programContext->beacons[3].sidLower) == 4;
    }else if(!strncmp("bx", line, 2)){
        return sscanf(line, "bx %f %f %f %f", &programContext->beacons[0].position.x, &programContext->beacons[1].position.x, &programContext->beacons[2].position.x, &programContext->beacons[3].position.x) == 4;
    }else if(!strncmp("by", line, 2)){
        return sscanf(line, "by %f %f %f %f", &programContext->beacons[0].position.y, &programContext->beacons[1].position.y, &programContext->beacons[2].position.y, &programContext->beacons[3].position.y) == 4;
    }else if(!strncmp("bz", line, 2)){
        return sscanf(line, "bz %f %f %f %f", &programContext->beacons[0].position.z, &programContext->beacons[1].position.z, &programContext->beacons[2].position.z, &programContext->beacons[3].position.z) == 4;
    }else if(!strncmp("mpx", line, 3)){
        return sscanf(line, "mpx %f %f", &programContext->modules[0].defaultPosition.x, &programContext->modules[1].defaultPosition.x) == 2;
    }else if(!strncmp("mpy", line, 3)){
        return sscanf(line, "mpy %f %f", &programContext->modules[0].defaultPosition.y, &programContext->modules[1].defaultPosition.y) == 2;
    }else if(!strncmp("mpz", line, 3)){
        return sscanf(line, "mpz %f %f", &programContext->modules[0].defaultPosition.z, &programContext->modules[1].defaultPosition.z) == 2;
    }else if(!strncmp("mox", line, 3)){
        return sscanf(line, "mox %f %f", &programContext->modules[0].defaultOrientation.x, &programContext->modules[1].defaultOrientation.x) == 2;
    }else if(!strncmp("moy", line, 3)){
        return sscanf(line, "moy %f %f", &programContext->modules[0].defaultOrientation.y, &programContext->modules[1].defaultOrientation.y) == 2;
    }else if(!strncmp("moz", line, 3)){
        return sscanf(line, "moz %f %f", &programContext->modules[0].defaultOrientation.z, &programContext->modules[1].defaultOrientation.z) == 2;
    }
    return true;
}


extern "C" __declspec(dllexport) void initDomainRoutine(void * memoryStart, Image * renderingTarget){
    
    initMemory(memoryStart);
    
    programContext = (ProgramContext *)memoryStart;
    
    initTime();
    
    initIo();
    
    if(!programContext->inited){
        programContext->renderingTarget = renderingTarget;
        bool result = true;
        
        result &= watchFile(configPath, &programContext->configFileWatch);
        ASSERT(result);
        if(hasFileChanged(&programContext->configFileWatch)){
            result &= loadConfig(configPath, parseConfig);
        }else{
            ASSERT(false);
        }
        
        
        FileContents fontFile;
        result &= readFile("data\\font.bmp", &fontFile);
        Image source;
        result &= decodeBMP(&fontFile, &source);
        result &= flipY(&source);
        result &= initBitmapFont(&programContext->font, &source, source.info.width / 16);
        
        
        
        programContext->localisationType = LocalisationType_Mems;
        
        NetSocketSettings settings;
        settings.blocking = false;
        result &= initSocket(&programContext->serverSocket, programContext->ip, programContext->port, &settings);
        result &= tcpListen(&programContext->serverSocket, 10);
        
        
        programContext->inited = result;
        
        
        ASSERT(programContext->inited);
    }
    inited = true;
}




extern "C" __declspec(dllexport) void processDomainRoutine(){
    if(!inited || !programContext->inited) return;
    float32 start = getProcessCurrentTime();
    
    if(programContext->localisationType == LocalisationType_Mems){
        
        float32 dt = 0;
        const float32 g = mpu6050_g;
        uint16 stepsAmount;
        
        for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
            ProgramContext::Module * module = &programContext->modules[i];
            
            if(module->run){
                //wash out xb
                module->xbStepsAvailable = 0;
                if(dt == 0){
                    stepsAmount = module->memsStepsAvailable;
                }else{
                    stepsAmount = MIN(stepsAmount, module->memsStepsAvailable);
                }
                dt = mpu6050_getTimeDelta(module->settings.sampleRate);
                
            }
        }
        
        stepsAmount = MIN(stepsAmount, (uint16)(programContext->accumulator / dt));
        
        const uint32 calibrationFrame = 100;
        const uint32 warmedUpFrame = 30;
        
        const float32 accelerationThreshold = 0.45f;
        const float32 orientationThreshold = 1;
        
        for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
            ProgramContext::Module * module = &programContext->modules[i];
            
            if(module->run){
                for(uint16 stepIndex = 0; stepIndex < stepsAmount; stepIndex++){
                    
                    int32 size = ARRAYSIZE(module->memsData);
                    
                    MemsData * data = &module->memsData[module->memsTailIndex];
                    v3 newGyro;
                    mpu6050_gyro2float(module->settings, data->gyroX, data->gyroY, data->gyroZ, &newGyro.x, &newGyro.y, &newGyro.z);
                    v3 newAcceleration;
                    mpu6050_acc2float(module->settings, data->accX, data->accY, data->accZ, &newAcceleration.x, &newAcceleration.y, &newAcceleration.z);
                    
                    if(module->physicalFrame > calibrationFrame)
                    {
                        newGyro -= module->gyroBias;
                        for(uint8 i = 0; i < 3; i++){
                            if(newGyro.v[i] >= module->gyroBiasLower.v[i] && newGyro.v[i] <= module->gyroBiasUpper.v[i]){
                                newGyro.v[i] = 0;
                            }
                        }
                        
                        v3 currentOrientation = newGyro * dt;
                        
                        module->orientation = currentOrientation + module->orientation;
                        
                        for(uint8 i = 0; i < 3; i++){
                            module->orientation.v[i] = fmodd(module->orientation.v[i], 360);
                        }
                        
                        
                        //rotate acceleration vector, as its axes are determined by current rotation
                        v4 quatX = normalize(Quat(V3(1, 0, 0), degToRad(module->orientation.x)));
                        v4 quatY = normalize(Quat(V3(0, 1, 0), degToRad(module->orientation.y)));
                        v4 quatZ = normalize(Quat(V3(0, 0, 1), degToRad(module->orientation.z)));
                        
                        mat4 rotationMatrix  = quaternionToMatrix(normalize(normalize(quatX * quatY) * quatZ));
                        
                        v3 currentAcceleration = rotationMatrix * newAcceleration;
                        
                        
                        currentAcceleration = (currentAcceleration - module->accelerationBias);
                        for(uint8 i = 0; i < 3; i++){
                            if(currentAcceleration.v[i] >= module->accelerationBiasLower.v[i] && currentAcceleration.v[i] <= module->accelerationBiasUpper.v[i]){
                                currentAcceleration.v[i] = 0;
                            }
                        }
                        
                        currentAcceleration = g * currentAcceleration;
                        
                        v3 oldPosition = module->position;
                        
                        module->velocity = currentAcceleration * dt;
                        
                        
                        
                        module->position = oldPosition + module->velocity*dt + 0.5f*module->acceleration*dt*dt;
                        
                        
                        module->acceleration = currentAcceleration;
                        
                        
                        
                        
                        
                    }else if(module->physicalFrame == calibrationFrame){
                        module->gyroBias = 0.5f * (module->gyroBiasLower + module->gyroBiasUpper);
                        module->gyroBiasUpper = module->gyroBiasUpper - module->gyroBias;
                        module->gyroBiasLower = module->gyroBiasLower - module->gyroBias;
                        
                        module->accelerationBias = 0.5f * (module->accelerationBiasLower + module->accelerationBiasUpper);
                        module->accelerationBiasUpper = module->accelerationBiasUpper - module->accelerationBias;
                        module->accelerationBiasLower = module->accelerationBiasLower - module->accelerationBias;
                        
                    }else{
                        newAcceleration = newAcceleration;
                        if(module->physicalFrame > warmedUpFrame){
                            //gather data?
                            for(uint8 i = 0; i < 3; i++){
                                module->gyroBiasLower.v[i] = MIN(module->gyroBiasLower.v[i], newGyro.v[i]);
                                module->gyroBiasUpper.v[i] = MAX(module->gyroBiasUpper.v[i], newGyro.v[i]);
                                
                                module->accelerationBiasLower.v[i] = MIN(module->accelerationBiasLower.v[i], newAcceleration.v[i]);
                                module->accelerationBiasUpper.v[i] = MAX(module->accelerationBiasUpper.v[i], newAcceleration.v[i]);
                            }
                        }else if(module->physicalFrame == warmedUpFrame){
                            module->gyroBiasLower = newGyro;
                            module->gyroBiasUpper = newGyro;
                            
                            module->accelerationBiasLower = newAcceleration;
                            module->accelerationBiasUpper = newAcceleration;
                        }
                        
                    }
                    module->memsTailIndex = (module->memsTailIndex + 1) % size;
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
        
        
        uint16 stepsAmount = 0;
        
        for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
            ProgramContext::Module * module = &programContext->modules[i];
            
            if(module->run){
                //wash out the mems data
                module->memsStepsAvailable = 0;
                
                if(stepsAmount == 0){
                    stepsAmount = module->xbStepsAvailable;
                }else{
                    stepsAmount = MIN(stepsAmount, module->xbStepsAvailable);
                }
            }
        }
        
        
        bool updateAcc = false;
        
        for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
            ProgramContext::Module * module = &programContext->modules[i];
            if(module->run){
                updateAcc = true;
                for(uint16 stepIndex = 0; stepIndex < stepsAmount; stepIndex++){
                    int32 size = ARRAYSIZE(module->xbData);
                    XbData * data = &module->xbData[module->xbTailIndex];
                    float32 maxTiming = data->delay[0];
                    for(uint8 ti = 1; ti < ARRAYSIZE(XbData::delay); ti++){
                        if(data->delay[ti] > maxTiming) maxTiming = data->delay[ti];
                    }
                    if(maxTiming < programContext->accumulator){
                        
                        //do stuff?
                        
                        programContext->accumulator -= maxTiming;
                        module->xbTailIndex = (module->xbTailIndex + 1) % size;
                        FETCH_AND_ADD(&module->xbStepsAvailable, -1);
                    }else if(module->xbFrame < 1){
                        module->xbTailIndex = (module->xbTailIndex + 1) % size;
                        FETCH_AND_ADD(&module->xbStepsAvailable, -1);
                        updateAcc = false;
                    }
                    module->xbFrame++;
                }
                
            }
        }
        if(!updateAcc){
            programContext->accumulator = 0;
        }
        
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
    
    ProgramContext::Module * activeModule = &programContext->modules[programContext->activeModuleIndex];
    
    int32 size = programContext->renderingTarget->info.width / 6;
    int32 border = (int32)(0.1f * (float32)size);
    
    size = size - border;
    uint8 thickness = 1;
    int32 sizeHalf = size/2;
    
    float32 rotationY = degToRad(activeModule->orientation.y);
    float32 rotationZ = degToRad(activeModule->orientation.z);
    float32 rotationX = degToRad(activeModule->orientation.x);
    float32 accY = activeModule->acceleration.y;
    float32 accZ = activeModule->acceleration.z;
    float32 accX = activeModule->acceleration.x;
    
    float32 maximumAcc = powd(2, activeModule->settings.accPrecision + 1) * mpu6050_g;
    
    //dynamic shit
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
    
    //position
    {
        printToBitmap(programContext->renderingTarget, offset.x, offset.y - border, "TOP VIEW", &programContext->font, border, white);
        dv2 bottomRightCorner = {(int32)programContext->renderingTarget->info.width - 3*border - textSize,(int32)programContext->renderingTarget->info.height - size - 3*border};
        drawRectangle(programContext->renderingTarget, &offset, &bottomRightCorner, white, thickness); 
        offset.x = bottomRightCorner.x + border;
        offset.y = 0;
    }
    
    {
        
        //textual info
        char buffer[122];
        uint16 fontSize = border*3/4;
        
        if(programContext->localisationType == LocalisationType_Mems){
            sprintf(buffer, "method: mems");
        }else if(programContext->localisationType == LocalisationType_Xb){
            sprintf(buffer, "method: xb");
        }else if(programContext->localisationType == LocalisationType_Both){
            sprintf(buffer, "method: both");
        }else{
            sprintf(buffer, "method: invalid");
        }
        
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
        offset.y += border;
        
        sprintf(buffer, "module: %1c", activeModule->name); 
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
        
        
        if(programContext->localisationType == LocalisationType_Xb || LocalisationType_Both){
            offset.y += 2*border;
            sprintf(buffer, "xb frame: %u", activeModule->xbFrame); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
        }
        
        
        if(programContext->localisationType == LocalisationType_Mems || LocalisationType_Both){
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
            offset.y += border;
            sprintf(buffer, "position:"); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x += border;
            
            sprintf(buffer, "x: %.3f", activeModule->position.x); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
            
            sprintf(buffer, "y: %.3f", activeModule->position.y); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
            
            sprintf(buffer, "z: %.3f", activeModule->position.z); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x -= border;
            
            offset.y += border;
        }
        
        if(programContext->localisationType == LocalisationType_Xb || LocalisationType_Both){
            offset.y += border;
            sprintf(buffer, "latest time period:"); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x += border;
            
            for(uint8 beaconIndex = 0; beaconIndex < ARRAYSIZE(programContext->beacons); beaconIndex++){
                
                sprintf(buffer, "%9s: %.5f", programContext->beacons[beaconIndex].sidLower, activeModule->xbData[(activeModule->xbHeadIndex-1)%ARRAYSIZE(activeModule->xbData)].delay[beaconIndex]); 
                printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
                offset.y += fontSize;
                
            }
            offset.y += border;
        }
        
        
        
        if(programContext->localisationType == LocalisationType_Mems || programContext->localisationType == LocalisationType_Both){
            sprintf(buffer, "orientation:"); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, blue);
            offset.y += fontSize;
            offset.x += border;
            
            sprintf(buffer, "x: %.3f", activeModule->orientation.x); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, blue);
            offset.y += fontSize;
            
            
            sprintf(buffer, "y: %.3f", activeModule->orientation.y); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, blue);
            offset.y += fontSize;
            
            
            sprintf(buffer, "z: %.3f", activeModule->orientation.z); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, blue);
            offset.y += fontSize;
            offset.x -= border;
            
            offset.y += border;
        }
        
        if(programContext->localisationType == LocalisationType_Mems || programContext->localisationType == LocalisationType_Both){
            sprintf(buffer, "acc: %5.2f m/s2", length(activeModule->acceleration)); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, red);
            offset.y += fontSize;
            offset.x += border;
            
            sprintf(buffer, "x: %.3f", activeModule->acceleration.x); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, red);
            offset.y += fontSize;
            
            
            sprintf(buffer, "y: %.3f", activeModule->acceleration.y); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, red);
            offset.y += fontSize;
            
            
            sprintf(buffer, "z: %.3f", activeModule->acceleration.z); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize, red);
            offset.y += fontSize;
            offset.x -= border;
            
            offset.y += border;
        }
        
        if(programContext->localisationType == LocalisationType_Mems || programContext->localisationType == LocalisationType_Both){
            sprintf(buffer, "vel: %5.2f m/s", length(activeModule->velocity)); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x += border;
            
            sprintf(buffer, "x: %.3f", activeModule->velocity.x); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
            
            sprintf(buffer, "y: %.3f", activeModule->velocity.y); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            
            
            sprintf(buffer, "z: %.3f", activeModule->velocity.z); 
            printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
            offset.y += fontSize;
            offset.x -= border;
            
            offset.y += border;
        }
        
        
        
    }
    
    offset = {border, (int32)programContext->renderingTarget->info.height - size - border};
    
    if(programContext->localisationType == LocalisationType_Mems || programContext->localisationType == LocalisationType_Both){
        
        //orientation pitch
        {
            printToBitmap(programContext->renderingTarget, offset.x, offset.y -border, "PITCH", &programContext->font, border, blue);
            
            //static shit
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
            printToBitmap(programContext->renderingTarget, offset.x, offset.y -border, "ROLL", &programContext->font, border, blue);
            //static shit
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
            printToBitmap(programContext->renderingTarget, offset.x, offset.y -border, "YAW", &programContext->font, border, blue);
            //static shit
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
            printToBitmap(programContext->renderingTarget, offset.x, offset.y -border, "ACC YAW", &programContext->font, border, red);
            //static shit
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
            printToBitmap(programContext->renderingTarget, offset.x, offset.y -border, "ACC PITCH", &programContext->font, border, red);
            
            //static shit
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
            printToBitmap(programContext->renderingTarget, offset.x, offset.y -border, "ACC SIZE", &programContext->font, border, red);
            offset.x += sizeHalf/2;
            
            //static shit
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


