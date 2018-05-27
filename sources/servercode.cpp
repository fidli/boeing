
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




struct MemsData{
    
    int16 accX;
    int16 accY;
    int16 accZ;
    
    int16 gyroX;
    int16 gyroY;
    int16 gyroZ;
    
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
        
        MemsData data[86];
        int32 tailIndex;
        int32 headIndex;
        uint32 stepsAvailable;
        
        
        v3 gyroBiasLower;
        v3 gyroBias;
        v3 gyroBiasUpper;
        
        v3 accelerationBiasLower;
        v3 accelerationBias;
        v3 accelerationBiasUpper;
        
        uint32 physicalFrame;
        char * dataBuffer;
        bool run;
    } modules[2];
    
    struct Beacon{
        uint16 frequency;
        char channel[4];
        char pan[5];
        v3 position;
    } beacons[4];
    
    bool beaconsRun;
    
    Image * renderingTarget;
    float32 accumulator;
    
    FileWatchHandle configFileWatch;
};

ProgramContext * programContext;
bool inited = false;

extern "C" __declspec(dllexport) void boeingDomainRoutine(int index){
    if(!inited || !programContext->inited) return;
    NetRecvResult result;
    
    
    ProgramContext::Module * module = &programContext->modules[index];
    Message wrap;
    
    if(programContext->keepRunning && module->run){
        result.bufferLength = sizeof(Message);
        result.buffer = (char *) &wrap;
        NetResultType resultCode = netRecv(&programContext->boeingSocket[index], &result);
        if(resultCode == NetResultType_Ok){
            
            ASSERT(wrap.data.length % sizeof(MemsData) == 0 && sizeof(MemsData) == 12);
            
            result.bufferLength = wrap.data.length; 
            result.buffer = module->dataBuffer;
            result.resultLength = 0;
            uint16 accumulated = 0;
            while(accumulated != wrap.data.length){
                
                
                result.buffer = module->dataBuffer + accumulated;
                result.bufferLength = wrap.data.length - accumulated;
                result.resultLength = 0;
                
                resultCode = netRecv(&programContext->boeingSocket[index], &result);
                accumulated += result.resultLength;
                
                if(resultCode != NetResultType_Ok){
                    break;
                }
            }
            
            ASSERT(accumulated == wrap.data.length);
            
            for(uint32 offset = 0; offset < wrap.data.length; offset += sizeof(MemsData)){
                
                MemsData * target = &module->data[module->headIndex];
                
                uint32 suboffset = 0;
                target->accX = ((uint16)(*(module->dataBuffer + offset + suboffset)) << 8) + *(module->dataBuffer + offset + suboffset + 1);
                suboffset += 2;
                target->accY = ((uint16)(*(module->dataBuffer + offset + suboffset)) << 8) + *(module->dataBuffer + offset + suboffset + 1);
                suboffset += 2;
                target->accZ = ((uint16)(*(module->dataBuffer + offset + suboffset)) << 8) + *(module->dataBuffer + offset + suboffset + 1);
                
                suboffset += 2;
                target->gyroX = ((uint16)(*(module->dataBuffer + offset + suboffset)) << 8) + *(module->dataBuffer + offset + suboffset + 1);
                suboffset += 2;
                target->gyroY = ((uint16)(*(module->dataBuffer + offset + suboffset)) << 8) + *(module->dataBuffer + offset + suboffset + 1);
                suboffset += 2;
                target->gyroZ = ((uint16)(*(module->dataBuffer + offset + suboffset)) << 8) + *(module->dataBuffer + offset + suboffset + 1);
                
                module->headIndex = (module->headIndex + 1) % ARRAYSIZE(module->data);
                FETCH_AND_ADD(&module->stepsAvailable, 1);
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
    Message message;
    result.buffer = (char *) &message;
    if(programContext->beaconsRun){
        NetResultType resultCode = netRecv(&programContext->beaconsSocket, &result);
        if(resultCode == NetResultType_Ok){
            ASSERT(message.type == MessageType_Data);
            //todo beacons processing
            
        }else{
            closeSocket(&programContext->beaconsSocket);
            programContext->beaconsRun = false;
        }
    }
}

void resetModule(int index){
    ProgramContext::Module * module = &programContext->modules[index];
    module->tailIndex = 0;
    module->headIndex = 0;
    module->stepsAvailable = 0;
    module->physicalFrame = 0;
    
    module->orientation = V3(0, 0, 0);
    module->position = V3(0, 0, 0);
    module->velocity = V3(0, 0, 0);
    module->acceleration = V3(0, 0, 0);
    
    module->accelerationBiasLower = {};
    module->accelerationBias = {};
    module->accelerationBiasUpper = {};
    
    module->gyroBiasLower = {};
    module->gyroBias = {};
    module->gyroBiasUpper = {};
    
}


extern "C" __declspec(dllexport) void serverDomainRoutine(){
    if(!inited || !programContext->inited) return;
    NetSocketSettings settings;
    settings.blocking = true;
    NetSocket socket;
    if(tcpAccept(&programContext->serverSocket, &socket, &settings)){
        NetRecvResult result;
        result.bufferLength = sizeof(Message);
        result.buffer = &PUSHA(char, result.bufferLength);
        
        //is this boeing or beacons client ?
        NetResultType resultCode = netRecv(&socket, &result);
        if(resultCode == NetResultType_Ok){
            
            Message * wrap = (Message *) result.buffer;
            ASSERT(wrap->type == MessageType_Init);
            ClientType clientType = wrap->init.clientType;
            if(clientType == ClientType_Boeing){
                ASSERT(programContext->beaconsRun);
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
                ASSERT(wrap->init.boeing.settings.sampleRate == 250);
                
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
                for(uint8 i = 0; i < ARRAYSIZE(programContext->beacons); i++){
                    ProgramContext::Beacon * beacon = &programContext->beacons[i];
                    
                    strncpy(beacon->channel, wrap->init.beacon.channel, 3);
                    strncpy(beacon->pan, wrap->init.beacon.pan, 5);
                    beacon->frequency = wrap->init.beacon.frequency;
                }
                programContext->beaconsSocket = socket;
                programContext->beaconsRun = true;
            }
        }else{
            closeSocket(&socket);
        }
    }
    
}

#include "util_config.cpp"

const char * configPath = "data/server.config";

static bool parseConfig(const char * line){
    if(!strncmp("ip", line, 2)){
        memset(programContext->ip, 0, 16);
        memset(programContext->port, 0, 6);
        return sscanf(line, "ip %16[^ ] %6[^\r\n ]", programContext->ip, programContext->port) == 2;
    }
    return true;
}


extern "C" __declspec(dllexport) void initDomainRoutine(void * memoryStart, Image * renderingTarget){
    
    initMemory(memoryStart);
    
    programContext = (ProgramContext *)memoryStart;
    
    initTime();
    
    
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
        
        
        
        
        programContext->modules[0].dataBuffer = &PUSHA(char, 4092);
        programContext->modules[1].dataBuffer = &PUSHA(char, 4092);
        
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
    
    if(hasFileChanged(&programContext->configFileWatch)){
        loadConfig(configPath, parseConfig);
    }
    
    
    float32 dt = 0;
    const float32 g = mpu6050_g;
    uint16 stepsAmount;
    
    for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
        ProgramContext::Module * module = &programContext->modules[i];
        
        if(module->run){
            if(dt == 0){
                stepsAmount = module->stepsAvailable;
            }else{
                stepsAmount = MIN(stepsAmount, module->stepsAvailable);
            }
            dt = mpu6050_getTimeDelta(module->settings.sampleRate);
            
        }
    }
    
    stepsAmount = MIN(stepsAmount, (uint16)(programContext->accumulator / dt));
    
    const uint32 calibrationFrame = 100;
    const uint32 warmedUpFrame = 30;
    
    const float32 accelerationThreshold = 0.45f;
    const float32 orientationThreshold = 1;
    
    
    for(uint16 stepIndex = 0; stepIndex < stepsAmount; stepIndex++){
        for(uint32 i = 0; i < ARRAYSIZE(programContext->modules); i++){
            ProgramContext::Module * module = &programContext->modules[i];
            
            if(module->run){
                int32 size = ARRAYSIZE(module->data);
                
                MemsData * data = &module->data[module->tailIndex];
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
                module->tailIndex = (module->tailIndex + 1) % size;
                module->physicalFrame++;
            }
        }
    }
    
    
    if(dt == 0){
        programContext->accumulator = 0;
    }else{
        programContext->accumulator -= dt*stepsAmount;
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
    
    ProgramContext::Module * activeModule = &programContext->modules[0];
    
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
        uint16 fontSize = border;
        
        sprintf(buffer, "module: %1c", activeModule->name); 
        printToBitmap(programContext->renderingTarget, offset.x, offset.y, buffer, &programContext->font, fontSize);
        
        
        
        {
            offset.y += 2*border;
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
        
        {
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
        
        {
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
        
        {
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


