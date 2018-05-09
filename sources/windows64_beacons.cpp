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



#define PERSISTENT_MEM MEGABYTE(1+100+100+1)
#define TEMP_MEM MEGABYTE(1)
#define STACK_MEM MEGABYTE(1)


#include "util_mem.h"

#include "windows_net.cpp"
#include "windows_io.cpp"
#include "windows_time.cpp"
#include "util_conv.cpp"

//{4d36e97e-e325-11ce-bfc1-08002be10318}
DEFINE_GUID(GUID_OTHER, 0x4d36e97e, 0xe325, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18);
//88bae032-5a81-49f0-bc3d-a4ff138216d6
DEFINE_GUID(GUID_USB_DEVICE, 0x88bae032, 0x5a81, 0x49f0, 0xbc, 0x3d, 0xa4, 0xff, 0x13, 0x82, 0x16, 0xd6);

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



static inline int main(LPWSTR * argvW, int argc) {
    
    LPVOID memoryStart = VirtualAlloc(NULL, TEMP_MEM + PERSISTENT_MEM + STACK_MEM, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    bool memory = memoryStart != NULL;
    
    if(!memory){
        ASSERT(false);
        return 0;
    }
    initMemory(memoryStart);
    
    
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
        
        bool foundDevice = false;
        char address[1024];
        
        HDEVINFO deviceInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        
        if (deviceInfo != INVALID_HANDLE_VALUE) {
            
            DWORD index = 0;
            SP_DEVINFO_DATA devinfo;
            devinfo.cbSize = sizeof(SP_DEVINFO_DATA);
            while(!foundDevice && SetupDiEnumDeviceInfo(deviceInfo, index, &devinfo)){
                
                char VID[5];
                char PID[5];
                
                DWORD ifIndex = 0;
                SP_DEVICE_INTERFACE_DATA ifData;
                ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
                while(!foundDevice && SetupDiEnumDeviceInterfaces(deviceInfo, &devinfo, &GUID_DEVINTERFACE_USB_DEVICE, ifIndex, &ifData)){
                    
                    DWORD requiredSize;
                    if(SetupDiGetDeviceInterfaceDetail(deviceInfo, &ifData, NULL, 0, &requiredSize, NULL) == FALSE && GetLastError() == ERROR_INSUFFICIENT_BUFFER){
                        char * buffer = &PUSHA(char, requiredSize);
                        SP_DEVICE_INTERFACE_DETAIL_DATA * detailData = (SP_DEVICE_INTERFACE_DETAIL_DATA*)buffer;
                        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
                        if(SetupDiGetDeviceInterfaceDetail(deviceInfo, &ifData, detailData, requiredSize, NULL, NULL)){
                            if(sscanf(detailData->DevicePath, "\\\\?\\usb#vid_%4s&pid_%4s", VID, PID) == 2){
                                if(!strcmp(VID, "0403") && !strcmp(PID, "ee18")){
                                    strcpy_n(address, detailData->DevicePath, ARRAYSIZE(address));
                                    foundDevice = true;
                                }
                            }
                        }
                        POP;
                    }
                    ifIndex++;
                }
                
                
                
                index++;
            }
            
            SetupDiDestroyDeviceInfoList(deviceInfo);
        }
        
        if(foundDevice){
            
            HANDLE usbROOTfile = CreateFile(address, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
            
            if(INVALID_HANDLE_VALUE != usbROOTfile){
                WINUSB_INTERFACE_HANDLE usbROOTHandle;
                
                if(WinUsb_Initialize(usbROOTfile, &usbROOTHandle)){
                    WINUSB_INTERFACE_HANDLE deviceHandle;
                    if(WinUsb_GetAssociatedInterface(&usbROOTHandle, 0, &deviceHandle)){
                        //readPipe
                        /*WinUsb_ReadPipe(
                            _In_      WINUSB_INTERFACE_HANDLE InterfaceHandle,
                            _In_      UCHAR                   PipeID,
                            _Out_     PUCHAR                  Buffer,
                            _In_      ULONG                   BufferLength,
                            _Out_opt_ PULONG                  LengthTransferred,
                            _In_opt_  LPOVERLAPPED            Overlapped
                            );
                            */
                        WinUsb_Free(deviceHandle);
                        
                    }
                    WinUsb_Free(usbROOTHandle);
                }
                
                
                
                
                CloseHandle(usbROOTfile);
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



