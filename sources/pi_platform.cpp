#include "stdio.h"
#include "string.h"
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
#include "util_filesystem.cpp"
#include "linux_filesystem.cpp"
#include "linux_thread.cpp"



#include "linux_net.cpp"
#include "linux_time.cpp"
#include "linux_dll.cpp"

#include "boeing_common.h"

struct Context{
    bool pumpMemsRunning;
    bool pumpXbRunning;
    Thread memsThread;
    Thread xbThread;
    bool freeze;
    Common common;
};

Context * context;

DEFINEDLLFUNC(void, initDomainRoutine, void *);
DEFINEDLLFUNC(void, processDomainRoutine, void);
DEFINEDLLFUNC(void, pumpMemsDomainRoutine, void);
DEFINEDLLFUNC(void, softResetBoeing, void);
DEFINEDLLFUNC(void, pumpXbDomainRoutine, void);




static void pumpMemsPlatform(void *){
    while(context->common.keepRunning){
        if(!context->freeze && pumpMemsDomainRoutine != NULL){
            context->pumpMemsRunning = true;
            pumpMemsDomainRoutine();
            context->pumpMemsRunning = false;
        }else{
            sleep(1);
        }
    }
}

static void pumpXbPlatform(void *){
    while(context->common.keepRunning){
        if(!context->freeze && pumpXbDomainRoutine != NULL){
            context->pumpXbRunning = true;
            pumpXbDomainRoutine();
            context->pumpXbRunning = false;
        }else{
            sleep(1);
        }
    }
}

void customWait(){
    printf("froze\n");
    context->freeze = true;
    while(context->pumpXbRunning);
    while(context->pumpMemsRunning);
    printf("wait over\n");
}


int main(int argc, char ** argv) {
    
    void * memoryStart = valloc(TEMP_MEM + PERSISTENT_MEM + STACK_MEM);
    if (memoryStart)
    {
        initMemory(memoryStart);
        
        context = (Context *) memoryStart;
        context->common.keepRunning = true;
        
        PPUSHA(char, MEGABYTE(8));
        void * domainMemory = (void*)&context->common;
        
        if(!initNet()){
            return false;
        }
        initTime();
        
        
        bool threadResult = true;
        
        threadResult &= createThread(&context->memsThread, pumpMemsPlatform, NULL);
        threadResult &= createThread(&context->xbThread, pumpXbPlatform, NULL);
        
        void * domainLibrary = NULL;
        
        FileWatchHandle domaincode;
        
        bool watchSuccess = watchFile("./domain.so", &domaincode);
        
        printf("startup, thread result %hhu, watch success %hhu\n", threadResult, watchSuccess);
        
        if(threadResult && watchSuccess){
            
            while (context->common.keepRunning) {
                
                if(hasDllChangedAndReloaded(&domaincode, &domainLibrary, customWait)){
                    pumpXbDomainRoutine = NULL;
                    pumpMemsDomainRoutine = NULL;
                    processDomainRoutine = NULL;
                    initDomainRoutine = NULL;
                    softResetBoeing = NULL;
                    OBTAINDLLFUNC(domainLibrary, pumpXbDomainRoutine);
                    OBTAINDLLFUNC(domainLibrary, pumpMemsDomainRoutine);
                    OBTAINDLLFUNC(domainLibrary, initDomainRoutine);
                    OBTAINDLLFUNC(domainLibrary, processDomainRoutine);
                    OBTAINDLLFUNC(domainLibrary, softResetBoeing);
                    printf("dll changed ptrs %hhu, %hhu, %hhu, %hhu %hhu\n", pumpXbDomainRoutine != 0, pumpMemsDomainRoutine != 0, initDomainRoutine != 0, processDomainRoutine != 0, softResetBoeing != 0);
                    
                    
                    if(domainLibrary == NULL){
                        pumpXbDomainRoutine = NULL;
                        pumpMemsDomainRoutine = NULL;
                        processDomainRoutine = NULL;
                        initDomainRoutine = NULL;
                        softResetBoeing = NULL;
                        context->freeze = false;
                    }else{
                        context->freeze = false;
                        if(initDomainRoutine){
                            initDomainRoutine(domainMemory);
                        }
                        if(softResetBoeing){
                            softResetBoeing();
                        }
                    }
                    
                }
                
                if(processDomainRoutine){
                    processDomainRoutine();
                }
                
            }
            
            
            closeSocket(&context->common.boeingSocket);
            joinThread(&context->memsThread);
            joinThread(&context->xbThread);
            
            free(memoryStart);
        }
        
    }
    return 0;
}