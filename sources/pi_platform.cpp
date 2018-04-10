#include "stdio.h"
#include "linux_types.h"
#include <errno.h>
#include <stdlib.h>
#include <malloc.h>
#include "string.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dlfcn.h>

#include "util_time.h"
#include "linux_time.cpp"

#include "common.h"

#define PERSISTENT_MEM MEGABYTE(10)
#define TEMP_MEM MEGABYTE(10)
#define STACK_MEM MEGABYTE(10)


#include "util_mem.h"
#include "util_filesystem.h"
#include "linux_filesystem.cpp"
#include "linux_thread.cpp"


#include "wiringPi.h"
#include "wiringPiI2C.h"

#include "mpu6050.cpp"
#include "linux_net.cpp"

#include "pi_domain.cpp"



bool initPlatform()
{
    
    void * memoryStart = valloc(TEMP_MEM + PERSISTENT_MEM + STACK_MEM);
    if (memoryStart)
    {
        initMemory(memoryStart);
        
        if(!initNet()){
            return false;
        }
        
        
        domainState = (DomainState *) mem.persistent;
        ASSERT(PERSISTENT_MEM >= sizeof(DomainState));
        
        wiringPiSetup();
        
        domainState->memsHandle.fd = wiringPiI2CSetup(0x68);
        
        if(domainState->memsHandle.fd < 0){
            return false;
        }
        
        
        
        return true;
    }
    
    return false;
}



int main(int argc, char ** argv) {
    bool keepRunning = initPlatform();
    if(keepRunning){
        domainRun();
    }
    
}