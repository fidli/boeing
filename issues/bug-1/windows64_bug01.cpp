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

#include "windows_time.cpp"

#include "util_mem.h"
#include "windows_serial.cpp"
#include "util_math.cpp"
#include "util_string.cpp"
#include "xbs2.cpp"
#include "util_serial.h"

struct Context{
    HINSTANCE hInstance;
};

Context context;


static inline int main(LPWSTR * argvW, int argc) {
    
    //RESOLVED
    //incorrect writeSerialQuick implementation on windows side
    
    //TO replicate this
    //1) run server
    //2) run classic beacons
    //3) run pi module
    //4) quit beacons
    //4) run this - and feel free to debug
    
    LPVOID memoryStart = VirtualAlloc(NULL, TEMP_MEM + PERSISTENT_MEM + STACK_MEM, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    bool memory = memoryStart != NULL;
    
    if(!memory){
        ASSERT(false);
        return 0;
    }
    
    initMemory(memoryStart);
    initTime();
    
    //this expects modules to be already set up
    char coms[4][6];
    strcpy(coms[0], "COM12");
    strcpy(coms[1], "COM14");
    strcpy(coms[2], "COM6");
    strcpy(coms[3], "COM9");
    
    
    
    
    XBS2Handle serials[4];
    
    char path[255];
    for(uint8 serialIndex = 0; serialIndex < ARRAYSIZE(coms); serialIndex++){
        XBS2Handle * beacon = &serials[serialIndex];
        beacon->guardTime = 1.1f;
        sprintf(path, "\\\\?\\%5s", coms[serialIndex]);
        if(!openHandle(path, beacon)){
            ASSERT(false);
        }
        while(!xbs2_readValues(beacon)){
            Sleep(10000);
        }
    }
    
    for(uint32 sampleIndex = 0; sampleIndex < 10; sampleIndex++){
        for(uint8 beaconIndex = 0; beaconIndex < ARRAYSIZE(coms); beaconIndex++){
            XBS2Handle * serial = &serials[beaconIndex];
            
            while(!xbs2_changeAddress(serial, "400A3F4E")){
                Sleep(1000);
            }
            while(!clearSerialPort(serial));
            char msg[10];
            sprintf(msg, "%s\r", serial->sidLower);
            while(!xbs2_transmitMessage(serial, msg)){
                Sleep(1000);
            }
            char ack[2] = {'-', 0};
            
            int32 acklength = xbs2_waitForAnyByte(serial, ack, 3);
            if(ack[0] == '1'){
                
                char response[2] = {'-', 0};
                xbs2_transmitByteQuick(serial, '1');
                int32 length = xbs2_waitForAnyByte(serial, response, 3);
                if(response[0] == '1'){
                    //GOOD
                    int i = 9;
                }else{
                    if(length == 0){
                        //NOT GOOD
                        int i = 9;
                    }else{
                        //NOT GOOD
                        int i = 9;
                    }
                    
                    
                }
                
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
    int result = main(argv,argc);
    LocalFree(argv);
    ExitProcess(result);
}



