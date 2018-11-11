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



#include <Windows.h>
#include "windows_types.h"

#include "common.h"


#define PERSISTENT_MEM MEGABYTE(1)
#define TEMP_MEM MEGABYTE(1)
#define STACK_MEM MEGABYTE(101)


#include "util_mem.h"

#include "windows_time.cpp"
#include "util_rng.cpp"
#include "util_io.cpp"
#include "windows_io.cpp"

#include "mpu6050.cpp"

struct Context{
    HINSTANCE hInstance;
};

Context context;


static inline int main(LPWSTR * argvW, int argc) {
    
    
    LPVOID memoryStart = VirtualAlloc(NULL, TEMP_MEM + PERSISTENT_MEM + STACK_MEM, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    bool memory = memoryStart != NULL;
    
    if(!memory){
        ASSERT(false);
        return 0;
    }
    
    initMemory(memoryStart);
    initTime();
    initRng();
    initIo();
    
    MPU6050Settings settings;
    settings.accPrecision = AccPrecision_2;
    
    v3_64 start = {};
    float64 g = 9.81;
    float64 dt = 0.1;
    
    v3_64 Ra = {};
    v3_64 Rv = {};
    v3_64 Rp = {};
    
    //sum of accel
    dv3_64 Na = {};
    
    //sum of speeds
    dv3_64 Nv = {};
    
    int32 tDivisor = 10;
    int32 velDivisor = tDivisor * mpu6050_getAccDivisor(&settings);
    int32 pDivisor = tDivisor * tDivisor * 2 * mpu6050_getAccDivisor(&settings);
    
    
    
    
    while(true){
        int16 x = (int16) randlcg();
        int16 y = (int16) randlcg();
        int16 z = (int16) randlcg();
        
        v3_64 accReal;
        mpu6050_acc16_float64(settings, x, y, z, &accReal.x, &accReal.y, &accReal.z);
        
        Ra = accReal;
        Rp = Rp + Rv*dt + 0.5*g*dt*dt*accReal;
        Rv = Rv + accReal*dt*g;
        
        
        dv3_64 accRaw = {x, y, z};
        Na += accRaw;
        v3_64 vel = (Na/velDivisor)*g;
        
        Nv += Na;
        dv3_64 currentArg = Nv*2 - Na;
        v3_64 pos = (currentArg / pDivisor)*g;
        
        
        printf("REAL: px: %+017.10lf vx: %+017.10lf \r\n", Rp.x, Rv.x);
        printf("RAW:  px: %+017.10lf vx: %+017.10lf \r\n\r\n", pos.x, vel.x);
        
        
        Sleep(500);
        
        
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



