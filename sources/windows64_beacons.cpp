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

#include "xbs2.cpp"

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

bool openHandle(const char * addr, HANDLE * result){
    HANDLE serial  = CreateFile(addr, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
    if(serial != INVALID_HANDLE_VALUE){
        *result = serial;
        return true;
    }
    return false;
}

bool setRate(HANDLE * target, int32 rate){
    //DWORD BaudRates[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
    DCB settings;
    if(GetCommState(*target, &settings)){
        if(settings.BaudRate != rate){
            settings.BaudRate = rate;
            bool result = SetCommState(*target, &settings);
            ASSERT(GetCommState(*target, &settings) && settings.BaudRate == rate);
            return result;
        }
        return true;
    }
    return false;
}

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
        XBS2Handle serial;
        if(openHandle("\\\\?\\COM4", &serial.handle)){
            /*NOTE: Failure to enter AT Command Mode is most commonly due to baud rate mismatch. Ensure the
                    Baud’ setting on the “PC Settings” tab matches the interface data rate of the RF module. By default,
                the BD parameter = 3 (9600 bps).
                */
            if(setRate(&serial.handle, 9600)){
                xbs2_setup(&serial);
            }
            
        }
        XBS2Handle serial2;
        if(openHandle("\\\\?\\COM5", &serial2.handle)){
            /*NOTE: Failure to enter AT Command Mode is most commonly due to baud rate mismatch. Ensure the
                    Baud’ setting on the “PC Settings” tab matches the interface data rate of the RF module. By default,
                the BD parameter = 3 (9600 bps).
                */
            if(setRate(&serial2.handle, 9600)){
                xbs2_setup(&serial2);
            }
            
        }
        
        xbs2_transmitMessage(&serial2, serial.sidLower, "ahoj\r");
        
        char buffer[10] = {};
        
        waitForAnyMessage(&serial, buffer);
        
        int i = 0;
        
#if 0
        
        HANDLE serial  = CreateFile("\\\\?\\COM3", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
        if(serial != INVALID_HANDLE_VALUE){
            
            //DWORD BaudRates[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
            
            DCB settings;
            if(GetCommState(serial, &settings)){
                if(settings.BaudRate != 9600){
                    settings.BaudRate = 9600;
                    ASSERT(SetCommState(serial, &settings));
                    ASSERT(GetCommState(serial, &settings) && settings.BaudRate == 9600);
                    int i = 0;
                }
                
                /*NOTE: Failure to enter AT Command Mode is most commonly due to baud rate mismatch. Ensure the
                    Baud’ setting on the “PC Settings” tab matches the interface data rate of the RF module. By default,
                the BD parameter = 3 (9600 bps).
                
                
                    When a command is sent to the module, the module will parse and execute
                    the command. Upon successful execution of a command, the module returns an “OK” message. If
                    execution of a command results in an error, the module returns an “ERROR” message.
                    
                    */
                
                //1 enter command mode
                
                unsigned char result[70] = {};
                OVERLAPPED commStatus = {};
                
                //a 1 second pause [GT (Guard Times) parameter]
                //b "+++"
                //c second pause
                unsigned char buffer[20] = "+++";
                float32 start = getProcessCurrentTime();
                while(getProcessCurrentTime() - start < 1){
                    
                }
                DWORD trash;
                if(WriteFile(serial, buffer, strlen((const char *)buffer), &trash, NULL))
                {
                    
                    
                    start = getProcessCurrentTime();
                    while(getProcessCurrentTime() - start < 1){
                        
                    }
                    DWORD offset  = 0;
                    
                    while(strcmp((const char*)result + offset - 3, "OK\r") != 0 && ReadFile(serial, result + offset, 70 - offset, &trash, NULL)){
                        offset += trash;
                    }
                    
                    
                    //low factory address "ATSH\r"
                    strcpy((char *)buffer, "ATSH\r");
                    ASSERT(WriteFile(serial, buffer, strlen((const char *)buffer), &trash, NULL));
                    
                    
                    //high factory address "ATSL\r"
                    strcpy((char *)buffer, "ATSL\r");
                    ASSERT(WriteFile(serial, buffer, strlen((const char *)buffer), &trash, NULL));
                    
                    
                    //"ATVR\r" firmware version
                    strcpy((char *)buffer, "ATVR\r");
                    ASSERT(WriteFile(serial, buffer, strlen((const char *)buffer), &trash, NULL));
                    
                    //"ATHV\r" hardware version
                    strcpy((char *)buffer, "ATHV\r");
                    ASSERT(WriteFile(serial, buffer, strlen((const char *)buffer), &trash, NULL));
                    
                    //"ATCN\r" exit command mode
                    strcpy((char *)buffer, "ATCN\r");
                    ASSERT(WriteFile(serial, buffer, strlen((const char *)buffer), &trash, NULL));
                    
                    offset = 0;
                    while(strcmp((const char*)result + offset - 3, "OK\r") != 0 && ReadFile(serial, result + offset, 70 - offset, &trash, NULL)){
                        offset += trash;
                    }
                    int i = 0;
                    
                    
                }
                
                
                
            }
            
            
            
        }
#endif
        
#if 0
        HANDLE serial = CreateFile("\\\\?\\COM3", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
        if(serial != INVALID_HANDLE_VALUE){
            DCB settings;
            if(GetCommState(serial, &settings)){
                if(settings.BaudRate != 9600){
                    settings.BaudRate = 9600;
                    ASSERT(SetCommState(serial, &settings));
                    ASSERT(GetCommState(serial, &settings) && settings.BaudRate == 9600);
                    int i = 0;
                }
                
                /*NOTE: Failure to enter AT Command Mode is most commonly due to baud rate mismatch. Ensure the
                Baud’ setting on the “PC Settings” tab matches the interface data rate of the RF module. By default,
                the BD parameter = 3 (9600 bps).
                
                
                When a command is sent to the module, the module will parse and execute
                the command. Upon successful execution of a command, the module returns an “OK” message. If
                execution of a command results in an error, the module returns an “ERROR” message.
                
                */
                
                //1 enter command mode
                
                unsigned char result[70] = {};
                OVERLAPPED commStatus = {};
                
                //a 1 second pause [GT (Guard Times) parameter]
                //b "+++"
                //c second pause
                unsigned char buffer[20] = "+++";
                float32 start = getProcessCurrentTime();
                while(getProcessCurrentTime() - start < 1){
                    
                }
                if(WriteFile(serial, buffer, strlen((const char *)buffer), NULL, &commStatus) == 0 && GetLastError() == ERROR_IO_PENDING)
                {
                    DWORD trash;
                    if(GetOverlappedResult(serial, &commStatus, &trash, true)){
                        start = getProcessCurrentTime();
                        while(getProcessCurrentTime() - start < 1){
                            
                        }
                        if(ReadFile(serial, result, 70, NULL, &commStatus) == 0  && GetLastError() == ERROR_IO_PENDING){
                            
                            if(GetOverlappedResult(serial, &commStatus, &trash, true)){
                                
                                
                                //low factory address "ATSL\r"
                                strcpy((char *)buffer, "ATSL\r");
                                ASSERT(WriteFile(serial, buffer, strlen((const char *)buffer), NULL, &commStatus) == 0 && GetLastError() == ERROR_IO_PENDING);
                                ASSERT(GetOverlappedResult(serial, &commStatus, &trash, true));
                                
                                //high factory address "ATSH\r"
                                strcpy((char *)buffer, "ATSH\r");
                                ASSERT(WriteFile(serial, buffer, strlen((const char *)buffer), NULL, &commStatus) == 0 && GetLastError() == ERROR_IO_PENDING);
                                ASSERT(GetOverlappedResult(serial, &commStatus, &trash, true));
                                
                                //"ATVR\r" firmware version
                                strcpy((char *)buffer, "ATVR\r");
                                ASSERT(WriteFile(serial, buffer, strlen((const char *)buffer), NULL, &commStatus) == 0 && GetLastError() == ERROR_IO_PENDING);
                                ASSERT(GetOverlappedResult(serial, &commStatus, &trash, true));
                                
                                //"ATHV\r" hardware version
                                strcpy((char *)buffer, "ATHV\r");
                                ASSERT(WriteFile(serial, buffer, strlen((const char *)buffer), NULL, &commStatus) == 0 && GetLastError() == ERROR_IO_PENDING);
                                ASSERT(GetOverlappedResult(serial, &commStatus, &trash, true));
                                
                                while(ReadFile(serial, result, 70, NULL, &commStatus) == 0 && GetLastError() == ERROR_IO_PENDING){
                                    if(GetOverlappedResult(serial, &commStatus, &trash, true)){
                                        int i = 0;
                                    }
                                }
                                //"ATCN\r" exit command mode
                                strcpy((char *)buffer, "ATHV\r");
                                ASSERT(WriteFile(serial, buffer, strlen((const char *)buffer), NULL, &commStatus) == 0 && GetLastError() == ERROR_IO_PENDING);
                                ASSERT(GetOverlappedResult(serial, &commStatus, &trash, true));
                                
                                while(ReadFile(serial, result, 70, NULL, &commStatus) == 0 && GetLastError() == ERROR_IO_PENDING){
                                    if(GetOverlappedResult(serial, &commStatus, &trash, true)){
                                        int i = 0;
                                    }
                                }
                                
                            }
                        }
                    }
                    
                }
            }
            
            
            
        }
#endif
#if 0
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
            
            HANDLE beaconSystemHandle = CreateFile(address, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
            
            
            if(INVALID_HANDLE_VALUE != beaconSystemHandle){
                WINUSB_INTERFACE_HANDLE usbHandle;
                
                
                if(WinUsb_Initialize(beaconSystemHandle, &usbHandle)){
                    
                    
                    //https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/using-winusb-api-to-communicate-with-a-usb-device
                    USB_INTERFACE_DESCRIPTOR setting;
                    uint8 inPipe;
                    uint8 outPipe;
                    bool outSet = false;
                    bool inSet = false;
                    if(WinUsb_QueryInterfaceSettings(usbHandle, 0, &setting)){
                        for(uint8 endpointIndex = 0; endpointIndex < setting.bNumEndpoints; endpointIndex++){
                            WINUSB_PIPE_INFORMATION pipeInformation;
                            if(WinUsb_QueryPipe(usbHandle, setting.bInterfaceNumber, endpointIndex, &pipeInformation)){
                                if(USB_ENDPOINT_DIRECTION_IN(pipeInformation.PipeId)){
                                    inPipe = pipeInformation.PipeId;
                                    inSet = true;
                                }else if(USB_ENDPOINT_DIRECTION_OUT(pipeInformation.PipeId)){
                                    outPipe = pipeInformation.PipeId;
                                    outSet = true;
                                }
                            }
                        }
                        
                        if(outSet && inSet){
                            ULONG trash;
                            //pipe policy - raw data?
                            //https://msdn.microsoft.com/en-us/library/windows/hardware/ff540304(v=vs.85).aspx setpolicy
                            //policy options https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/winusb-functions-for-pipe-policy-modification
                            
                            
                            /*
                            NOTE: Failure to enter AT Command Mode is most commonly due to baud rate mismatch. Ensure the
                            Baud’ setting on the “PC Settings” tab matches the interface data rate of the RF module. By default,
                            the BD parameter = 3 (9600 bps).
                            
                            
                            When a command is sent to the module, the module will parse and execute
                            the command. Upon successful execution of a command, the module returns an “OK” message. If
                            execution of a command results in an error, the module returns an “ERROR” message.
                            
                            */
                            
                            //1 enter command mode
                            
                            unsigned char result[70] = {};
                            
                            //a 1 second pause [GT (Guard Times) parameter]
                            //b "+++"
                            //c second pause
                            unsigned char buffer[20] = "+++\r";
                            if(WinUsb_WritePipe(usbHandle, outPipe, buffer, strlen((const char *)buffer), &trash, NULL))
                            {
                                float32 start = getProcessCurrentTime();
                                while(getProcessCurrentTime() - start < 1){
                                    
                                }
                                strcpy((char *)buffer, "ATDL\r");
                                if(WinUsb_WritePipe(usbHandle, outPipe, buffer, strlen((const char *)buffer), &trash, NULL))
                                {
                                    while(WinUsb_ReadPipe(usbHandle, inPipe, result, 70, &trash, NULL)){
                                        int i = 0;
                                    }
                                    int i = 0;
                                    
                                }
                            }
                            
                            
                            
                            
                            int i = 0;
                            
                            
                            //commands
                            //low factory address "ATSL\r"
                            //high factory address "ATSH\r"
                            
                            //"ATVR\r" firmware version
                            //"ATHV\r" hardware version
                            
                            
                            
                            //exit command mode
                            /*
                            Send the ATCN (Exit Command Mode) command (followed by a carriage return).
                            [OR]
                            2. If no valid AT Commands are received within the time specified by CT (Command Mode
                            Timeout) Command, the RF module automatically returns to Idle Mode.
                            */
                            
                        }
                        
                        
                        WinUsb_Free(usbHandle);
                    }
                    
                    
                }
                
                
                
                CloseHandle(beaconSystemHandle);
            }
            
        }
        
        
#endif
        
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



