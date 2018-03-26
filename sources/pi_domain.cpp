#include <stdint.h>
#include <stdlib.h>
#include "string.h"
#include "stdio.h"
#include <unistd.h>
#include "mpu6050.cpp"


#include "common.h"
#include "util_mem.h"
#include "util_filesystem.h"
#include "util_net.h"

struct DomainState{
    MPU6050Handle memsHandle;
    NetSocket localSocket;
    byte fifoData[1024];
};

DomainState * domainState;

void iterateDomain(bool * keepRunning){
    
    uint16 result = mpu6050_fifoCount(&domainState->memsHandle);
    if(result == 1024){
        printf("overflow, fuk");
    }
    if(result > 120){
        for(uint16 bytesCount = 0; bytesCount < result; bytesCount++){
            domainState->fifoData[bytesCount] = mpu6050_readFifoByte(&domainState->memsHandle);
        }
        NetSendSource source;
        source.bufferLength = result;
        source.buffer = (char*)domainState->fifoData;
        while(netSend(&domainState->localSocket, &source) != NetResultType_Ok){
            printf("send error, fix me and repeat\n");
        }
    }
    
    //sleep(1);
    
}



