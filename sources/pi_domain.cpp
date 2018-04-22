#include <stdint.h>
#include <stdlib.h>
#include "string.h"
#include "stdio.h"
#include <unistd.h>
#include "mpu6050.cpp"
#include "messages.h"

#include "common.h"
#include "util_mem.h"
#include "util_filesystem.h"
#include "util_net.h"
#include "util_thread.h"


struct DomainState{
    MPU6050Handle memsHandle;
    NetSocket localSocket;
    byte fifoData[4092];
    int16 head;
    int16 tail;
    Thread poller;
    uint16 fifoCount;
    bool haltPolling;
    bool pollHalted;
    
};

DomainState * domainState;


void pollModule(bool * go){
    while(*go){
        if(domainState->haltPolling){
            domainState->pollHalted = true;
            domainState->haltPolling = false;
            while(domainState->pollHalted){}
        }
        
        //usleep(100000);
        uint16 result = mpu6050_fifoCount(&domainState->memsHandle);
        
        if(result >= 1024){
            printf("overflow, fuk: result: %hu\n", result);
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
}

#define IMMEDIATE 0

void connectToServer(volatile bool * go){
    domainState->haltPolling = true;
#if IMMEDIATE
#else
    
    while(tcpConnect(&domainState->localSocket, "10.0.0.10", "25555") == false){
        printf("failed to connect to server\n");
        sleep(1);
    }
    //send settings and other info
    NetSendSource namaste;
    Message namasteMessage;
    namasteMessage.type = MessageType_Init;
    namaste.bufferLength = sizeof(namasteMessage.reserved) + sizeof(namasteMessage.init);
    sprintf(namasteMessage.init.name, "61.68");
    namasteMessage.init.settings = domainState->memsHandle.settings;
    namaste.buffer = (char*) &namasteMessage;
    while(true){
        NetResultType subRes = netSend(&domainState->localSocket, &namaste);
        if(subRes == NetResultType_Ok || subRes == NetResultType_Closed){
            printf("init message sent\n");
            break;
        }
    }
#endif
    
    
    
    while(domainState->haltPolling){}
    
    
    mpu6050_reset(&domainState->memsHandle);
    mpu6050_setup(&domainState->memsHandle, domainState->memsHandle.settings);
    printf("mpu6050 fifo size: %hhu\n", mpu6050_fifoCount(&domainState->memsHandle));
    printf("mpu6050 pwr_mngmt1 %hhu\n", read8Reg(&domainState->memsHandle, MPU6050_REGISTER_PWR_MGMT_1));
    domainState->head = 0;
    domainState->tail = 0;
    domainState->fifoCount = 0;
    domainState->pollHalted = false;
}

void domainRun(){
    volatile bool go = true;
    
    
    
    NetSocketSettings settings;
    settings.blocking = true;
    settings.reuseAddr = true;
    
    
    if(!openSocket(&domainState->localSocket, &settings)){
        printf("falied to open local socket\n");
        go &= false;
        return;
    }
    
    domainState->memsHandle.settings = {GyroPrecision_500, AccPrecision_4, 250};
    domainState->haltPolling =  false;
    domainState->pollHalted = false;
    
    
    
#if IMMEDIATE
#else
    if(!createThread(&domainState->poller, (void (*)(void*)) pollModule, (void * ) &go)){
        printf("failed to create thread\n");
        go &= false;
    }
#endif
    
    
    Message header;
    header.type = MessageType_Data;
    
    char * sendData = &PUSHA(char, ARRAYSIZE(domainState->fifoData));
    
    connectToServer(&go);
    
    
    while(go){
        
        
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
                sendData[j] = domainState->fifoData[i];
            }
            
            
            header.data.length = toRead;
            NetSendSource source;
            source.bufferLength = sizeof(Message);
            source.buffer = (char*)&header;
            
            
            
            //printf("%hd %hd %hd\n", ((int16*)(domainState->fifoData+domainState->tail))[0], ((int16*)(domainState->fifoData+domainState->tail))[1], ((int16*)(domainState->fifoData+domainState->tail))[2]);
            
            NetResultType result;
            bool cont = false;
            while((result = netSend(&domainState->localSocket, &source)) != NetResultType_Ok){
                printf("send error\n");
                if(result == NetResultType_Closed){
                    closeSocket(&domainState->localSocket);
                    if(!openSocket(&domainState->localSocket, &settings)){
                        printf("falied to open local socket\n");
                        go &= false;
                        return;
                    }
                    printf("socket closed, reconnecting \n");
                    connectToServer(&go);
                    cont = true;
                    break;
                }else{
                    sleep(1);
                }
            }
            
            if(!cont){
                
                source.bufferLength = toRead;
                source.buffer = sendData;
                
                while((result = netSend(&domainState->localSocket, &source)) != NetResultType_Ok){
                    printf("send error\n");
                    if(result == NetResultType_Closed){
                        closeSocket(&domainState->localSocket);
                        if(!openSocket(&domainState->localSocket, &settings)){
                            printf("falied to open local socket\n");
                            go &= false;
                            return;
                        }
                        printf("socket closed, reconnecting \n");
                        connectToServer(&go);
                        cont = true;
                        break;
                    }else{
                        sleep(1);
                    }
                }
                
                if(!cont){
                    domainState->tail += toRead;
                    domainState->tail %= ARRAYSIZE(domainState->fifoData);
                    FETCH_AND_ADD(&domainState->fifoCount, -toRead);
                }
            }
        }
        
        
        
        
#endif
        
        
    }
    
    joinThread(&domainState->poller);
    
}




