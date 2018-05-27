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
#include <winsock2.h>
#include <ws2tcpip.h>

#include "windows_types.h"


#include <Windows.h>
#include <Winusb.h>
#include <Setupapi.h>
#include <Usbiodef.h>

#include "common.h"



#define PERSISTENT_MEM MEGABYTE(1)
#define TEMP_MEM MEGABYTE(1)
#define STACK_MEM MEGABYTE(101)


#include "util_mem.h"

#include "windows_net.cpp"
#include "windows_io.cpp"
#include "windows_time.cpp"
#include "util_conv.cpp"

#include "windows_dll.cpp"

#include "util_config.cpp"




static inline DWORD jettisonAllPrivileges() {
    DWORD result = ERROR_SUCCESS;
    HANDLE processToken  = NULL;
    if(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &processToken)){
        DWORD privsize = 0;
        GetTokenInformation(processToken, TokenPrivileges, NULL, 0, &privsize);
        TOKEN_PRIVILEGES * priv = (TOKEN_PRIVILEGES *) &PUSHS(byte, privsize);
        if(GetTokenInformation(processToken, TokenPrivileges, priv , privsize, &privsize)){
            
            for(DWORD i = 0; i < priv->PrivilegeCount; ++i ){ 
                priv->Privileges[i].Attributes = SE_PRIVILEGE_REMOVED;
            }
            if(AdjustTokenPrivileges(processToken, TRUE, priv, NULL, NULL, NULL) == 0){
                result = GetLastError();
            }
        }else{
            result = GetLastError();
        }
        POP;
    }else{
        result = GetLastError();
    }
    CloseHandle(processToken);
    return result;
}

#include "windows_thread.cpp"

#include "beacons_common.h"

#include "windows_dll.cpp"

struct Context{
    HINSTANCE hInstance;
    Thread beaconThread[4];
    bool freeze;
    bool beaconRunning[4];
    Common common;
};

Context * context;

DEFINEDLLFUNC(void, beaconDomainRoutine, int);
DEFINEDLLFUNC(void, initDomainRoutine, void *);
DEFINEDLLFUNC(void, iterateDomainRoutine, void);


static void beaconPlatform(int index){
    while(context->common.keepRunning){
        if(!context->freeze && beaconDomainRoutine != NULL){
            context->beaconRunning[index] = true;
            beaconDomainRoutine(index);
            context->beaconRunning[index] = false;
        }else{
            Sleep(1000);
        }
    }
}

void customWait(){
    context->freeze = true;
    for(uint8 i = 0; i < ARRAYSIZE(context->beaconThread); i++){
        while(context->beaconRunning[i]);
    }
}


static inline int main(LPWSTR * argvW, int argc) {
    
    
    LPVOID memoryStart = VirtualAlloc(NULL, TEMP_MEM + PERSISTENT_MEM + STACK_MEM, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    bool memory = memoryStart != NULL;
    
    if(!memory){
        ASSERT(false);
        return 0;
    }
    initMemory(memoryStart);
    context = (Context *) memoryStart;
    context->common.keepRunning = true;
    
    PPUSHA(char, MEGABYTE(100));
    void * domainMemory = (void*)&context->common;
    
    bool privileges = jettisonAllPrivileges() == ERROR_SUCCESS;
    
    char ** argv = &PUSHA(char *, argc);
    char ** argvUTF8 = &PUSHA(char *, argc);
    bool success = true;
    if(!initTime()){
        
        
        for(int i = 0; i < argc && success; i++){
            int toAlloc = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
            success &= toAlloc != 0;
            argvUTF8[i] = &PUSHA(char, toAlloc);
            int res = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, argvUTF8[i], toAlloc, NULL, NULL);
            success &= res != 0;
            uint32 finalLen;
            //this is more or equal to real needed
            argv[i] = &PPUSHA(char, toAlloc);
            if(convUTF8toAscii((byte *)argvUTF8[i], toAlloc, &argv[i], &finalLen) != 0){
                printf("Error: argument is not fully ascii compatible - those characters were replaced by '_'. Please use simple ASCII parameter values\n");
            }
        }
    }
    bool argvSuccess = success;
    
    bool initSuccess = initNet() && initIo();
    
    bool threadResult = true;
    
    for(uint8 i = 0; i < ARRAYSIZE(context->beaconThread); i++){
        threadResult &= createThread(&context->beaconThread[i], (void (*)(void *))beaconPlatform, (void *) i);
    }
    
    HMODULE beaconsLibrary = 0;
    
    FileWatchHandle beaconsCode;
    
    bool watchSuccess = watchFile("beacons_domain.dll", &beaconsCode);
    
    
    if(initSuccess && argvSuccess && threadResult && privileges && watchSuccess){
        
        while(context->common.keepRunning){
            
            if(hasDllChangedAndReloaded(&beaconsCode, &beaconsLibrary, customWait)){
                OBTAINDLLFUNC(beaconsLibrary, beaconDomainRoutine);
                OBTAINDLLFUNC(beaconsLibrary, initDomainRoutine);
                OBTAINDLLFUNC(beaconsLibrary, iterateDomainRoutine);
                
            }
            
            if(beaconsLibrary == NULL){
                initDomainRoutine = NULL;
                iterateDomainRoutine = NULL;
                beaconDomainRoutine = NULL;
            }else{
                context->freeze = false;
                if(initDomainRoutine){
                    initDomainRoutine(domainMemory);
                }
            }
            
            
            if(iterateDomainRoutine){
                iterateDomainRoutine();
            }
            
        }
        
        
    }
    
    closeSocket(&context->common.beaconsSocket);
    
    for(uint8 i = 0; i < ARRAYSIZE(context->beaconThread); i++){
        joinThread(&context->beaconThread[i]);
    }
    
    if (!VirtualFree(memoryStart, 0, MEM_RELEASE)) {
        //more like log it
        ASSERT(!"Failed to free memory");
    }
    
    return 0;
}



int mainCRTStartup(){
    int argc = 0;
    LPWSTR * argv =  CommandLineToArgvW(GetCommandLineW(), &argc);
    int result = main(argv,argc);
    LocalFree(argv);
    ExitProcess(result);
}



