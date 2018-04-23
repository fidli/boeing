
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


#define PERSISTENT_MEM MEGABYTE(1+100+100+1)
#define TEMP_MEM MEGABYTE(1)
#define STACK_MEM MEGABYTE(1)

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



struct Context{
    HINSTANCE hInstance;
    HWND window;
    
};

bool quit;

Context context;
DrawContext renderer;
Image renderingTarget;


#include "windows_thread.cpp"




void resizeCanvas(HWND window, LONG width, LONG height){
    if(renderer.DIBSection){
        DeleteObject(renderer.DIBSection);
    }else{
        
        renderer.backbufferDC = CreateCompatibleDC(NULL);
    }
    
    renderer.drawinfo = {{
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
    renderer.DIBSection = CreateDIBSection(renderer.backbufferDC, &renderer.drawinfo, DIB_RGB_COLORS, (void**) &renderer.drawbuffer, NULL, NULL);
    
    renderer.height = height;
    renderer.width = width;
    
}

void updateCanvas(HDC dc, int x, int y, int width, int height){
    StretchDIBits(dc, x, y, width, height, 0, 0, width, height, renderer.drawbuffer, &renderer.drawinfo, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam){
    switch(message)
    {
        case WM_SIZE:{
            resizeCanvas(context.window, (WORD)lParam, (WORD) (lParam >> sizeof(WORD) * 8));
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
            
            EndPaint(context.window, &paint);
        }
        break;
        case WM_CLOSE:
        case WM_DESTROY:
        {
            quit = true;
            return 0;
        } break;
    }
    
    return DefWindowProc (window, message, wParam, lParam);
}


void (*render)(void);
void (*process)(float32 *);
bool (*init)(Image *, bool *, void *, bool);
void (*mockMemsData)(float32 *);
void (*close)(void);

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
    bool argvSuccess = success;
    
    
    renderingTarget.info.bitsPerSample = 8;
    renderingTarget.info.samplesPerPixel = 4;
    renderingTarget.info.interpretation = BitmapInterpretationType_ARGB;
    renderingTarget.info.origin = BitmapOriginType_TopLeft;
    
    WNDCLASSEX style = {};
    style.cbSize = sizeof(WNDCLASSEX);
    style.style = CS_OWNDC;
    style.lpfnWndProc = WindowProc;
    style.hInstance = context.hInstance;
    style.lpszClassName = "MainClass";
    bool registerWindow = RegisterClassEx(&style) != 0;
    
    context.window = CreateWindowEx(NULL,
                                    "MainClass", "Boeing", WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
                                    CW_USEDEFAULT, CW_USEDEFAULT,CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, context.hInstance, NULL);
    
    bool windowCreated = context.window != NULL;
    
    bool initSuccess = initIo() && initTime() && initNet();
    
    
    float32 accumulator = 0;
    
    float32 frameStartTime = 0;
    
    char * servercode = "servercode.dll";
    char * servercodeCopy = "servercodeCopy.dll";
    
    LocalTime oldChangeTime = {};
    
    HMODULE serverLibrary = 0;
    
    
    if(initSuccess && windowCreated && registerWindow && argvSuccess && privileges && memory){
        
        bool wasInit = false;
        
        ShowWindow(context.window, SW_SHOWMAXIMIZED);
        
        LocalTime newChangeTime;
        
        while (!quit) {
            
            if(getFileChangeTime(servercode, &newChangeTime)){
                if(newChangeTime != oldChangeTime){
                    
                    if(close){
                        close();
                    }
                    Sleep(100);
                    FreeLibrary(serverLibrary);
                    render = NULL;
                    process = NULL;
                    init = NULL;
                    mockMemsData = NULL;
                    
                    if(CopyFile(servercode, servercodeCopy, false) > 0){
                        
                        serverLibrary = LoadLibrary(servercodeCopy);
                        if(serverLibrary != NULL){
                            process = (void (*)(float32*))GetProcAddress(serverLibrary, "process");
                            render = (void (*)(void))GetProcAddress(serverLibrary, "render");
                            mockMemsData = (void (*)(float32*))GetProcAddress(serverLibrary, "mockMemsData");
                            init = (bool (*)(Image*,bool*,void*, bool))GetProcAddress(serverLibrary, "init");
                            close = (void (*)(void))GetProcAddress(serverLibrary, "close");
                            ASSERT(process && render && mockMemsData && init && close);
                        }
                        
                        if(init){
                            init(&renderingTarget, &quit, (void *)((byte*)mem.persistent + MEGABYTE(1)), wasInit);
                            wasInit = true;
                        }
                        
                        
                        oldChangeTime = newChangeTime;
                    }
                }
            }
            
            frameStartTime = getProcessCurrentTime();
            
            MSG msg;
            while(PeekMessage(&msg, context.window, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            
            
            renderingTarget.info.width = renderer.width;
            renderingTarget.info.height = renderer.height;
            renderingTarget.data = (byte *)renderer.drawbuffer;
            
            
            
            if(process != NULL && render != NULL){
                if(mockMemsData != NULL){
                    mockMemsData(&accumulator);
                }
                process(&accumulator);
                render();
            }
            
            InvalidateRect(context.window, NULL, TRUE);
            
            accumulator += getProcessCurrentTime() - frameStartTime;
        }
        
        if(close != NULL){
            close();
        }
        
    }else{
        ASSERT(false);
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
    renderer.drawBitmapData = {};
    
    int result = main(argv,argc);
    LocalFree(argv);
    ExitProcess(result);
}

