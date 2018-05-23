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


struct Context{
    HINSTANCE hInstance;
    
};

bool quit;

Context context;


bool (*initDomain)(void *);
void (*iterateDomain)();

static inline int main(LPWSTR * argvW, int argc) {
    
    
    LPVOID memoryStart = VirtualAlloc(NULL, TEMP_MEM + PERSISTENT_MEM + STACK_MEM, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    bool memory = memoryStart != NULL;
    
    if(!memory){
        ASSERT(false);
        return 0;
    }
    initMemory(memoryStart);
    
    void * domainMemory = (void*)&PPUSHA(char, MEGABYTE(100));
    
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
    
    
    if(initSuccess && argvSuccess && privileges){
        char codeFilename[256] = "beacons_domain.dll";
        char codeFilenameCopy[256] = "beacons_domain_temp.dll";
        FileWatchHandle dll;
        HMODULE domainCode = NULL;
        if(watchFile(codeFilename, &dll)){
            
            bool wasInit = false;
            
            while(!quit){
                
                if((domainCode == NULL || hasFileChanged(&dll))){
                    
                    while(!CopyFile(codeFilename, codeFilenameCopy, FALSE));
                    if(domainCode != NULL){
                        FreeLibrary(domainCode);
                    }
                    domainCode = NULL;
                    HMODULE temp;
                    temp =  LoadLibrary(codeFilenameCopy);
                    if(temp){
                        
                        initDomain = (bool(*)(void *))GetProcAddress(temp, "initDomain");
                        iterateDomain = (void(*)(void))GetProcAddress(temp, "iterateDomain");
                        if(initDomain && iterateDomain){
                            domainCode = temp;
                            print("Domain code reloaded\r\n");
                        }
                    }
                    
                }
                
                if(!wasInit){
                    if(domainCode){
                        wasInit = initDomain(domainMemory);
                    }
                }
                
                iterateDomain();
                
            }
        }
        
        
        
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
    context.hInstance = GetModuleHandle(NULL);
    quit = false;
    int result = main(argv,argc);
    LocalFree(argv);
    ExitProcess(result);
}



