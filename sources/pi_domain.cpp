#include <stdint.h>
#include <stdlib.h>
#include "string.h"
#include "stdio.h"
#include <unistd.h>

#include "wiringPi.h"
#include "wiringPiI2C.h"

#include "linux_serial.cpp"

#include "mpu6050.cpp"
#include "messages.h"


#include "common.h"

#undef ASSERT
#define ASSERT(expression)  if(!(expression)) {printf("ASSERT failed on line %d file %s\n", __LINE__, __FILE__); *(int *)0 = 0;}



#include "util_mem.h"
#include "util_math.cpp"
#include "util_filesystem.cpp"
#include "util_net.h"
#include "util_thread.h"
#include "xbs2.cpp"

#include "boeing_common.h"


#include "algorithms.h"

struct DomainState : Common{
    MPU6050Handle memsHandle;
    byte fifoData[4092];
    int16 head;
    int16 tail;
    uint16 fifoCount;
    
    bool haltMemsPolling;
    bool memsPollingHalted;
    bool xbPumpHalted;
    
    char memsSendBuffer[4096];
    
    char beaconSids[4][9];
    
    char id;
    
    XBS2Handle xb;
    uint16 xbFreq;
    char channel[3];
    char pan[5];
    bool inited;
    
    float32 xbPeriod;
    
    FileWatchHandle configFileWatch;
};

bool inited = false;

DomainState * domainState;

#include "util_config.cpp"

const char * configPath = "data/boeing.config";


extern "C" void pumpXbDomainRoutine(){
    if(!inited || !domainState->inited) return;
    
    
#if METHOD_XBPNG
    char xbRecvBuffer[64];
    //awaiting 9 chars (address + '\r')
    int32 res = xbs2_waitForAnyMessage(&domainState->xb, xbRecvBuffer, ARRAYSIZE(xbRecvBuffer), 3);
    xbRecvBuffer[8] = 0;
    if(res == 9){
#if LOG_PING
        printf("Got ping announcement from %s\n", xbRecvBuffer);
        printf("Flushing pipe\n");
#endif
        while(!clearSerialPort(&domainState->xb));
        while(!xbs2_changeAddress(&domainState->xb, xbRecvBuffer)){
            printf("Failed to change XB address\n");
            sleep(1);
        }
#if LOG_PING
        printf("Sending ACK, then entering logging silence and waiting for ping\n");
#endif
        while(!xbs2_transmitByte(&domainState->xb, domainState->id)){
            printf("Failed to transmit ACK\n");
            sleep(1);
        }
        char ping;
        //awaiting 1 char
        if(xbs2_waitForAnyByte(&domainState->xb, &ping, 3)){
            while(!xbs2_transmitByte(&domainState->xb, ping));
#if LOG_PING
            printf("Got ping message '%c', replied ping '%c'\n", ping, ping);
#endif
        }else{
#if LOG_PING
            printf("Failed to optain ping message. timeout\n");
#endif
        }
        
    }else if (res != 0){
#if LOG_PING
        printf("Got only %d characters: %s\n", res, xbRecvBuffer);
#endif
    }
    //else is ok, there is no ping request
#endif
#if METHOD_XPSP
    
    if(domainState->haltMemsPolling){
        domainState->xbPumpHalted = true;
        printf("xb pump halted\n");
        while(domainState->haltMemsPolling){};
        printf("xb pump unhalted\n");
        domainState->xbPumpHalted = false;
    }
    //address is set to broadcast, just pump with constant pace
    
    //wait(0.1f);
    /*
    //multicast
    bool result = xbs2_transmitByte(&domainState->xb, domainState->id);
    ASSERT(result);
    */
    
    wait(domainState->xbPeriod);
    xbs2_transmitByteQuick(&domainState->xb, domainState->id);
    
    /*
    //carousel 
    for(uint8 i = 0; i < ARRAYSIZE(domainState->beaconSids); i++){
    //float32 start = getProcessCurrentTime();
    xbs2_changeAddressQuick(&domainState->xb, domainState->beaconSids[i]);
    //printf("[%d] change address time taken: %f\n", i, getProcessCurrentTime() - start);
    xbs2_transmitByteQuick(&domainState->xb, domainState->id);
    }*/
#endif
    
}

static bool connectToServer(){
    
    closeSocket(&domainState->boeingSocket);
    
    NetSocketSettings settings;
    settings.blocking = true;
    settings.reuseAddr = true;
    
    printf("opening socket\n");
    if(!openSocket(&domainState->boeingSocket, &settings)){
        printf("failed to open socket\n");
        return false;
    }
    
    
    printf("connecting to server: %16s:%6s\n", domainState->ip, domainState->port);
    if(tcpConnect(&domainState->boeingSocket, domainState->ip, domainState->port) == false){
        printf("failed to connect to server\n");
        return false;
    }
    
    
    printf("sending namaste to the server\n");
    //send settings and other info
    NetSendSource namaste;
    Message namasteMessage;
    namasteMessage.type = MessageType_Init;
    namasteMessage.init.clientType = ClientType_Boeing;
    namasteMessage.init.boeing.xbPeriod = domainState->xbPeriod;
    strncpy(namasteMessage.init.boeing.sidLower, domainState->xb.sidLower, 9);
    //WTF, why not sizeof(Message)
    namaste.bufferLength = sizeof(namasteMessage.reserved) + sizeof(namasteMessage.init);
    namasteMessage.init.boeing.name = domainState->id;
    namasteMessage.init.boeing.settings = domainState->memsHandle.settings;
    namaste.buffer = (char*) &namasteMessage;
    while(true){
        NetResultType subRes = netSend(&domainState->boeingSocket, &namaste);
        if(subRes == NetResultType_Ok || subRes == NetResultType_Closed){
            
            printf("init message sent\n");
            
            printf("getting xbs2 settings from server\n");
            //accept christ blood and body in form of module settings
            Message xbs2settings;
            NetRecvResult result;
            result.bufferLength = sizeof(Message);
            result.buffer = (char *)&xbs2settings;
            
            
            
            NetResultType resultCode = netRecv(&domainState->boeingSocket, &result);
            while(resultCode != NetResultType_Ok){
                printf("fk\n");
                sleep(1);
            }
            
            domainState->xbFreq = xbs2settings.init.beacon.frequencyKhz;
            strncpy(domainState->channel, xbs2settings.init.beacon.channel, 3);
            strncpy(domainState->pan, xbs2settings.init.beacon.pan, 5);
            
            
            
            //set the xbs
            printf("got the settings, channel: %3s, pan: %5s\n", domainState->channel, domainState->pan);
            printf("connecting to the xbs network\n");
            
            char channelMask[5];
            if(!xbs2_getChannelMask(domainState->channel, channelMask)){
                printf("ERROR, failed to determino channel mask\n");
                return false;
            }
            while(!xbs2_initNetwork(&domainState->xb, channelMask));
            
            
            
            printf("success, channel %s, pan %s\n", domainState->xb.channel, domainState->xb.pan);
            
            NetSendSource ready;
            Message readyMessage;
            readyMessage.type = MessageType_Ready;
            readyMessage.ready.id = domainState->id;
            ready.bufferLength = sizeof(Message);
            ready.buffer = (char*) &readyMessage;
            NetResultType subSubRes = netSend(&domainState->boeingSocket, &ready);
            if(subSubRes == NetResultType_Ok || subSubRes == NetResultType_Closed){
                break;
            }
        }
    }
    
    return true;
}

static void sendAndReconnect(const NetSendSource * source){
    NetResultType result;
    
    result = netSend(&domainState->boeingSocket, source);
    if(result == NetResultType_Closed || result == NetResultType_Timeout){
        printf("send error, trying reconnect\n");
        connectToServer();
        printf("errno: %d\n", errno);
        sleep(1);
    }
    
}



extern "C" void softResetBoeing(){
    if(!inited || !domainState->inited) return;
    
    printf("soft reset inited\n");
    
    domainState->haltMemsPolling = true;
    while(!domainState->memsPollingHalted){}
#if METHOD_XPSP
    while(!domainState->xbPumpHalted){}
#endif
    
    NetSendSource reset;
    Message resetMessage;
    resetMessage.type = MessageType_Reset;
    reset.bufferLength = sizeof(Message);
    reset.buffer = (char*) &resetMessage;
    sendAndReconnect(&reset);
    
    
    
    domainState->head = 0;
    domainState->tail = 0;
    domainState->fifoCount = 0;
    
    
    
    mpu6050_resetFifo(&domainState->memsHandle);
    
    printf("soft reset finished\n");
    
    
    domainState->haltMemsPolling = false;
    
};

extern "C" void pumpMemsDomainRoutine(){
    if(!inited || !domainState->inited) return;
    
    if(domainState->haltMemsPolling){
        domainState->memsPollingHalted = true;
        printf("mems pump halted\n");
        while(domainState->haltMemsPolling){};
        printf("mems pump unhalted\n");
        domainState->memsPollingHalted = false;
    }
    
    uint16 result = mpu6050_fifoCount(&domainState->memsHandle);
    
    if(result >= 1024){
        printf("overflow, this is not good: result: %hu\n", result);
        result = 1024;
    }
    
    uint16 size = ARRAYSIZE(domainState->fifoData);
    uint16 tail = (domainState->head + result) % size;
    
    for(uint16 i = domainState->head; i != tail; i = (i+1) % size)
        //for(uint16 i = 0; i < result; i++)
    {
        uint8 data = mpu6050_readFifoByte(&domainState->memsHandle);
        domainState->fifoData[i] = data;
#if 0
        int16 toPrint;
        if(i&1){
            toPrint = ((domainState->fifoData[i-1]) << 8) | data;
        }
        if(i%12 == 1){
            printf("muzzletop\n");
            printf("ax: %hd", toPrint);
        }
        if(i%12 == 3){
            printf(" ay: %hd", toPrint);
        }
        if(i%12 == 5){
            printf(" az: %hd\n", toPrint);
        }
        if(i%12 == 7){
            printf("gx: %hd", toPrint);
        }
        if(i%12 == 9){
            printf(" gy: %hd", toPrint);
        }
        if(i%12 == 11){
            printf(" gz: %hd i:(%hu)\n", toPrint, i);
        }
#endif
    }
    domainState->head = tail;
    FETCH_AND_ADD(&domainState->fifoCount, result);
    
    
}


static bool parseConfig(const char * line){
    if(!strncmp("ip", line, 2)){
        memset(domainState->ip, 0, 16);
        memset(domainState->port, 0, 6);
        return sscanf(line, "ip %16[^ ] %6[^ \r\n]", domainState->ip, domainState->port) == 2;
    }else if(!strncmp("id", line, 2)){
        return sscanf(line, "id %c[^ \r\n]", &domainState->id) == 1;
    }else if(!strncmp("gyro", line, 4)){
        return sscanf(line, "gyro %d[^\r\n ]", &domainState->memsHandle.settings.gyroPrecision) == 1;
    }else if(!strncmp("acc", line, 3)){
        return sscanf(line, "acc %d[^\r\n ]", &domainState->memsHandle.settings.accPrecision) == 1;
    }else if(!strncmp("memsrate", line, 8)){
        return sscanf(line, "memsrate %hu[^\r\n ]", &domainState->memsHandle.settings.sampleRate) == 1;
    }else if(!strncmp("xbrate", line, 6)){
        float32 rate;
        if(sscanf(line, "xbrate %f[^\r\n ]", &rate) == 1){
            domainState->xbPeriod = 1.0f/rate;
            return true;
        }
        return false;
    }else if(!strncmp("beacons", line, 6)){
        return sscanf(line, "beacons %8[^\r\n ] %8[^\r\n ] %8[^\r\n ] %8[^\r\n ]", domainState->beaconSids[0], domainState->beaconSids[1], domainState->beaconSids[2], domainState->beaconSids[3]) == 4;
    }
    return true;
}


#define IMMEDIATE 1

extern "C" void initDomainRoutine(void * memoryStart){
    
    initMemory(memoryStart);
    initNet();
    initTime();
    
    domainState = (DomainState *) memoryStart;
    wiringPiSetup();
    
    inited = true;
    if(!domainState->inited){
        bool result = true;
        result &= watchFile(configPath, &domainState->configFileWatch);
        ASSERT(result);
        if(hasFileChanged(&domainState->configFileWatch)){
            result &= loadConfig(configPath, parseConfig);
        }else{
            printf("failed to hotload config\n");
            return;
        }
        printf("beacon sids: ");
        for(uint8 i = 0; i < ARRAYSIZE(domainState->beaconSids); i++){
            printf("%s ", domainState->beaconSids[i]);
        }
        printf("\n");
        domainState->haltMemsPolling = false;
        domainState->memsPollingHalted = false;
        domainState->xbPumpHalted = false;
        
        domainState->memsHandle.fd = wiringPiI2CSetup(0x68);
        if(domainState->memsHandle.fd < 0){
            printf("falied to open i2c handle\n");
            return;
        }
        
        printf("setting mems module, gyro: %d, acc %d, sample rate: %hu\n", domainState->memsHandle.settings.gyroPrecision, domainState->memsHandle.settings.accPrecision, domainState->memsHandle.settings.sampleRate);
        printf("xb time period %f\n", domainState->xbPeriod);
        
        
        mpu6050_reset(&domainState->memsHandle);
        mpu6050_setup(&domainState->memsHandle, domainState->memsHandle.settings);
        printf("mpu6050 fifo size: %hhu\n", mpu6050_fifoCount(&domainState->memsHandle));
        printf("mpu6050 pwr_mngmt1 %hhu\n", read8Reg(&domainState->memsHandle, MPU6050_REGISTER_PWR_MGMT_1));
        domainState->head = 0;
        domainState->tail = 0;
        domainState->fifoCount = 0;
        
        
        printf("setting up xb2 module\n");
        
        printf("opening handle\n");
        if(!openHandle("/dev/ttyAMA0", &domainState->xb)){
            printf("failed to open serial handle\n");
            return;
        }
        printf("handle opened\n");
        
        domainState->xb.guardTime = 1.1f;
        
        printf("detecting baud rate\n");
        if(!xbs2_detectAndSetStandardBaudRate(&domainState->xb)){
            printf("failed to detect baud rate on xb\n");
            closeHandle(&domainState->xb);
            return;
        }
        printf("baud rate detected: %d\n", domainState->xb.baudrate);
        printf("initing module\n");
        XBS2InitSettings settings;
        settings.prepareForBroadcast = true;
#if METHOD_XBPNG
        settings.prepareForBroadcast = false;
#endif
        if(!xbs2_initModule(&domainState->xb, &settings)){
            printf("falied to init xb module\n");
            closeHandle(&domainState->xb);
            return;
        }
        printf("module inited, reading values\n");
        
        if (!xbs2_readValues(&domainState->xb)){
            printf("failed to read module values");
            closeHandle(&domainState->xb);
            return;
        }
        
        printf("values read, sid %9s\n", domainState->xb.sidLower);
        
        
        
        
#if IMMEDIATE
#else
        
        if(!connectToServer()){
            return;
        }
#endif
        
        
        domainState->inited = true;
        softResetBoeing();
    }
    
}






extern "C" void processDomainRoutine(){
    if(!inited || !domainState->inited) return;
    
    
    
#if IMMEDIATE
    int8 xH = read8Reg(&domainState->memsHandle, MPU6050_REGISTER_ACCEL_XOUT_H);
    int8 xL = read8Reg(&domainState->memsHandle, MPU6050_REGISTER_ACCEL_XOUT_L);
    int16 resultX = ((uint16)xH) << 8 | xL;
    
    int8 yH = read8Reg(&domainState->memsHandle, MPU6050_REGISTER_ACCEL_YOUT_H);
    int8 yL = read8Reg(&domainState->memsHandle, MPU6050_REGISTER_ACCEL_YOUT_L);
    int16 resultY = ((uint16)yH) << 8 | yL;
    
    int8 zH = read8Reg(&domainState->memsHandle, MPU6050_REGISTER_ACCEL_ZOUT_H);
    int8 zL = read8Reg(&domainState->memsHandle, MPU6050_REGISTER_ACCEL_ZOUT_L);
    int16 resultZ = ((uint16)zH) << 8 | zL;
    
    printf("%hd %hd %hd\n", resultX, resultY, resultZ);
    sleep(1);
    
#else
    
    uint16 count = domainState->fifoCount;
    
    ASSERT(count < ARRAYSIZE(domainState->fifoData));
    
    uint16 toRead = (count / 12) * 12;
    
    if(toRead > 12*10){
        
        uint16 size = ARRAYSIZE(domainState->fifoData);
        uint16 tail = (domainState->tail + toRead) % size;
        
        
        
        
        for(uint16 i = domainState->tail, j = 0; i != tail; i = (i+1) % size, j++)
        {
            domainState->memsSendBuffer[j] = domainState->fifoData[i];
        }
        
        
        Message header;
        header.type = MessageType_Data;
        header.data.length = toRead;
        
        NetSendSource source;
        source.bufferLength = sizeof(Message);
        source.buffer = (char*)&header;
        
        
        //printf("%hd %hd %hd\n", ((int16*)(domainState->fifoData+domainState->tail))[0], ((int16*)(domainState->fifoData+domainState->tail))[1], ((int16*)(domainState->fifoData+domainState->tail))[2]);
        
        sendAndReconnect(&source);
        
        source.bufferLength = toRead;
        source.buffer = domainState->memsSendBuffer;
        
        sendAndReconnect(&source);
        
        domainState->tail += toRead;
        domainState->tail %= ARRAYSIZE(domainState->fifoData);
        FETCH_AND_ADD(&domainState->fifoCount, -toRead);
        
    }
    
    
    
    
    
#endif
    
    
    
    
    
    
}




