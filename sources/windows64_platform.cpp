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


#include "common.h"
#include "windows_time.cpp"


#define PERSISTENT_MEM MEGABYTE(1)
#define TEMP_MEM MEGABYTE(1)
#define STACK_MEM MEGABYTE(1+700+1)

#include "windows_time.cpp"

#include "util_mem.h"
#include "util_string.cpp"
#include "windows_filesystem.cpp"
#include "util_image.cpp"
#include "util_graphics.cpp"
#include "util_conv.cpp"
#include "util_io.cpp"
#include "windows_io.cpp"

#include "windows_net.cpp"

#include "windows_dll.cpp"

#include "windows_thread.cpp"


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

struct DrawContext{
    uint32 * drawbuffer;
    Image drawBitmapData;
    Image originalBitmapData;
    BITMAPINFO drawinfo;
    HDC  backbufferDC;
    HBITMAP DIBSection;
    uint32 width;
    uint32 height;
};


#include "servercode_common.h"

struct Context{
    HINSTANCE hInstance;
    HWND window;
    DrawContext renderer;
    Image renderingTarget;
    Thread serverThread;
    Thread beaconsThread;
    Thread boeingThread[2];
    Thread processThread;
    bool freeze;
    bool serverRunning;
    bool processRunning;
    bool beaconsRunning;
    bool boeingRunning[2];
    Common common;
};


Context * context;




#include "util_config.cpp"

void resizeCanvas(HWND window, LONG width, LONG height){
    if(context->renderer.DIBSection){
        DeleteObject(context->renderer.DIBSection);
    }else{
        
        context->renderer.backbufferDC = CreateCompatibleDC(NULL);
    }
    
    context->renderer.drawinfo = {{
            sizeof(BITMAPINFOHEADER),
            width,
            -height,
            1,
            32,
            BI_RGB,
            0,
            0,
            0,
            0,
            0},
        0
    };
    context->renderer.DIBSection = CreateDIBSection(context->renderer.backbufferDC, &context->renderer.drawinfo, DIB_RGB_COLORS, (void**) &context->renderer.drawbuffer, NULL, NULL);
    
    context->renderer.height = height;
    context->renderer.width = width;
    
}

void updateCanvas(HDC dc, int x, int y, int width, int height){
    StretchDIBits(dc, x, y, width, height, 0, 0, width, height, context->renderer.drawbuffer, &context->renderer.drawinfo, DIB_RGB_COLORS, SRCCOPY);
}

#include "server_input.h"

ServerInput input;

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam){
    switch(message)
    {
        case WM_SIZE:{
            resizeCanvas(context->window, (WORD)lParam, (WORD) (lParam >> sizeof(WORD) * 8));
        }
        break;
        case WM_PAINT:{
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(window, &paint);
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;
            
            updateCanvas(dc, x, y, width, height);
            
            EndPaint(context->window, &paint);
        }
        case WM_KEYUP:{
            switch (wParam){
                case 0x31:{ //'1'
                    input.boeing1 = true;
                }break;
                case 0x32:{ //'2'
                    input.boeing2 = true;
                }break;
                case VK_F1:{
                    input.method1 = true;
                }break;
                case VK_F2:{
                    input.method2 = true;
                }break;
                case VK_F3:{
                    input.method3 = true;
                }break;
                case VK_F4:{
                    input.method4 = true;
                }break;
                case 0x52:{ //'r'
                    input.record = true;
                }break;
                case 0x48:{ //'h'
                    input.help = true;
                }break;
                case 0x50:{ //'p'
                    input.reposition = true;
                }break;
                
            };
        }break;
        break;
        case WM_CLOSE:
        case WM_DESTROY:
        {
            context->common.keepRunning = false;
            return 0;
        } break;
    }
    
    return DefWindowProc (window, message, wParam, lParam);
}

DEFINEDLLFUNC(void, beaconsDomainRoutine, void);
DEFINEDLLFUNC(void, serverDomainRoutine,  void);
DEFINEDLLFUNC(void, boeingDomainRoutine,  int);
DEFINEDLLFUNC(void, initDomainRoutine, void *, Image *, char*);
DEFINEDLLFUNC(void, processDomainRoutine, void);
DEFINEDLLFUNC(void, renderDomainRoutine, void);
DEFINEDLLFUNC(void, handleInputDomainRoutine, ServerInput *);



static void beaconsPlatform(void *){
    while(context->common.keepRunning){
        if(!context->freeze && beaconsDomainRoutine != NULL){
            context->beaconsRunning = true;
            beaconsDomainRoutine();
            context->beaconsRunning = false;
        }else{
            Sleep(1000);
        }
    }
}



static void serverPlatform(void *){
    while(context->common.keepRunning){
        if(!context->freeze && serverDomainRoutine != NULL){
            context->serverRunning = true;
            serverDomainRoutine();
            context->serverRunning = false;
        }else{
            Sleep(1000);
        }
    }
}

static void boeingPlatform(int index){
    while(context->common.keepRunning){
        if(!context->freeze && boeingDomainRoutine != NULL){
            context->boeingRunning[index] = true;
            boeingDomainRoutine(index);
            context->boeingRunning[index] = false;
        }else{
            Sleep(1000);
        }
    }
}

static void processPlatform(void *){
    while(context->common.keepRunning){
        if(!context->freeze && processDomainRoutine != NULL){
            context->processRunning = true;
            processDomainRoutine();
            context->processRunning = false;
        }else{
            Sleep(1000);
        }
    }
}


void customWait(){
    context->freeze = true;
    print("froze \n");
    while(context->processRunning);
    while(context->boeingRunning[0]);
    while(context->boeingRunning[1]);
    while(context->serverRunning);
    while(context->beaconsRunning);
    
    print("wait ready\n");
    
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
    context->hInstance = GetModuleHandle(NULL);
    context->common.keepRunning = true;
    context->renderer.drawBitmapData = {};
    
    PPUSHA(char, MEGABYTE(700));
    void * domainMemory = (void*)&context->common;
    
    
    bool privileges = jettisonAllPrivileges() == ERROR_SUCCESS;
    
    char ** argv = &PUSHA(char *, argc);
    char ** argvUTF8 = &PUSHA(char *, argc);
    bool success = true;
    for(int i = 0; i < argc && success; i++){
        int toAlloc = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
        success &= toAlloc != 0;
        argvUTF8[i] = &PUSHA(char, toAlloc);
        int res = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, argvUTF8[i], toAlloc, NULL, NULL);
        success &= res != 0;
        uint32 finalLen;
        //this is more or equal to real needed
        argv[i] = &PUSHA(char, toAlloc);
        if(convUTF8toAscii((byte *)argvUTF8[i], toAlloc, &argv[i], &finalLen) != 0){
            printf("Error: argument is not fully ascii compatible - those characters were replaced by '_'. Please use simple ASCII parameter values\n");
        }
    }
    bool argvSuccess = success;
    
    
    
    bool socketResult = initNet();
    ASSERT(socketResult);
    
    bool threadResult = true;
    
    threadResult &= createThread(&context->beaconsThread, beaconsPlatform, NULL);
    threadResult &= createThread(&context->serverThread, serverPlatform, NULL);
    threadResult &= createThread(&context->boeingThread[0], (void (*)(void *))boeingPlatform, (void *)0);
    threadResult &= createThread(&context->boeingThread[1], (void (*)(void *))boeingPlatform, (void *)1);
    threadResult &= createThread(&context->processThread, processPlatform, NULL);
    
    
    
    
    context->renderingTarget.info.bitsPerSample = 8;
    context->renderingTarget.info.samplesPerPixel = 4;
    context->renderingTarget.info.interpretation = BitmapInterpretationType_ARGB;
    context->renderingTarget.info.origin = BitmapOriginType_TopLeft;
    
    WNDCLASSEX style = {};
    style.cbSize = sizeof(WNDCLASSEX);
    style.style = CS_OWNDC;
    style.lpfnWndProc = WindowProc;
    style.hInstance = context->hInstance;
    style.lpszClassName = "MainClass";
    bool registerWindow = RegisterClassEx(&style) != 0;
    
    context->window = CreateWindowEx(NULL,
                                     "MainClass", "Boeing", WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
                                     CW_USEDEFAULT, CW_USEDEFAULT,CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, context->hInstance, NULL);
    
    bool windowCreated = context->window != NULL;
    
    bool initSuccess = initIo() && initTime() && initNet();
    
    char * file = NULL;
    if(argc >= 2){
        file = argv[1];
    }
    
    HMODULE serverLibrary = 0;
    
    FileWatchHandle servercode;
    
    bool watchSuccess = watchFile("servercode.dll", &servercode);
    
    if(initSuccess && watchSuccess  && socketResult && threadResult && windowCreated && registerWindow && argvSuccess && privileges && memory){
        
        
        
        ShowWindow(context->window, SW_SHOWMAXIMIZED);
        
        
        while (context->common.keepRunning) {
            
            float32 frameStartTime = getProcessCurrentTime();
            
            if(hasDllChangedAndReloaded(&servercode, &serverLibrary, customWait)){
                OBTAINDLLFUNC(serverLibrary, beaconsDomainRoutine);
                OBTAINDLLFUNC(serverLibrary, serverDomainRoutine);
                OBTAINDLLFUNC(serverLibrary, boeingDomainRoutine);
                OBTAINDLLFUNC(serverLibrary, initDomainRoutine);
                OBTAINDLLFUNC(serverLibrary, processDomainRoutine);
                OBTAINDLLFUNC(serverLibrary, renderDomainRoutine);
                OBTAINDLLFUNC(serverLibrary, handleInputDomainRoutine);
                
                
                if(serverLibrary == NULL){
                    beaconsDomainRoutine = NULL;
                    serverDomainRoutine = NULL;
                    boeingDomainRoutine = NULL;
                    initDomainRoutine = NULL;
                    processDomainRoutine = NULL;
                    renderDomainRoutine = NULL;
                    handleInputDomainRoutine = NULL;
                }else{
                    if(initDomainRoutine){
                        initDomainRoutine(domainMemory, &context->renderingTarget, file);
                    }
                }
                context->freeze = false;
            }
            
            input = {};
            
            MSG msg;
            while(PeekMessage(&msg, context->window, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            
            if(handleInputDomainRoutine){
                handleInputDomainRoutine(&input);
            }
            
            context->renderingTarget.info.width = context->renderer.width;
            context->renderingTarget.info.height = context->renderer.height;
            context->renderingTarget.data = (byte *)context->renderer.drawbuffer;
            
            
            
            if(renderDomainRoutine != NULL){
                renderDomainRoutine();
            }
            float32 endFrameTime = getProcessCurrentTime();
            float32 timeTaken = endFrameTime-frameStartTime;
            //30 fps
            if(timeTaken < 0.033f){
                Sleep(1000*(DWORD)(0.033f - timeTaken));
            }
            InvalidateRect(context->window, NULL, TRUE);
            
            
        }
        
        
    }else{
        ASSERT(false);
    }
    
    joinThread(&context->serverThread);
    joinThread(&context->beaconsThread);
    joinThread(&context->boeingThread[0]);
    joinThread(&context->boeingThread[1]);
    joinThread(&context->processThread);
    
    closeSocket(&context->common.serverSocket);
    closeSocket(&context->common.beaconsSocket);
    closeSocket(&context->common.boeingSocket[0]);
    closeSocket(&context->common.boeingSocket[1]);
    
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

