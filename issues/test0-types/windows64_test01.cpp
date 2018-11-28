#include <Windows.h>
#include "windows_types.h"

#include "common.h"


#define PERSISTENT_MEM MEGABYTE(1)
#define TEMP_MEM MEGABYTE(1)
#define STACK_MEM MEGABYTE(101)


#include "util_mem.h"

#include "windows_time.cpp"
#include "util_rng.cpp"
#include "stdio.h"

#include "util_filesystem.cpp"
#include "windows_filesystem.cpp"
#include "util_math.cpp"
#include "mpu6050.cpp"

struct Context{
    HINSTANCE hInstance;
};

Context context;

float64 g = 9.81;
float64 dt = 0.1;

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
    
    
    MPU6050Settings settings;
    settings.accPrecision = AccPrecision_2;
    settings.gyroPrecision = GyroPrecision_250;
    
    
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
    
    int32 degDivisor = tDivisor * mpu6050_getGyroDivisorTimes10(&settings);
    int32 degMultiplier = 10;
    int32 degModulo = 360 * degDivisor / degMultiplier;
    
    v3_64 originalRotation = {0, 0, 1};
    
    v3_64 orientationReal  = originalRotation;
    
    
    v3_64 orientationRaw = originalRotation;
    dv3_64 degreesRaw = {};
    
    v3_64 anglesReal = {};
    v3_64 anglesRaw = {};
    
    char * filename = "test.csv";
    
    
    
    FileContents csv;
    csv.size = 1024;
    csv.contents = &PUSHA(char, csv.size);
    
    strncpy(csv.contents, "iteration;pos.x real;pos.x raw;vel.x real;vel.x raw;angles.x real;angles.x raw;direction.x real; direction.x raw;direction.x realgood\r\n", 1024);
    csv.size = strlen(csv.contents);
    appendFile(filename, &csv);
    
    
    
    for(int i = 1; true; i++){
        
        if(i % 100 == 0) printf("iteration %d reached\r\n", i);
        
        v3_64 pos;
        v3_64 vel;
        v3_64 o;
        //acc
        {
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
            vel = (Na/velDivisor)*g;
            
            Nv += Na;
            dv3_64 currentArg = Nv*2 - Na;
            pos = (currentArg / pDivisor)*g;
            
        }
        
        //gyro
        {
            int16 x = (int16) randlcg();
            int16 y = (int16) randlcg();
            int16 z = (int16) randlcg();
            
            v3_64 gyroReal;
            mpu6050_gyro16_float64(settings, x, y, z, &gyroReal.x, &gyroReal.y, &gyroReal.z);
            
            gyroReal = gyroReal * dt;
            
            //rotation difference
            v4_64 quatX = Quat64(V3_64(1, 0, 0), degToRad64(-gyroReal.x));
            v4_64 quatY = Quat64(V3_64(0, 1, 0), degToRad64(-gyroReal.y));
            v4_64 quatZ = Quat64(V3_64(0, 0, 1), degToRad64(-gyroReal.z));
            
            mat4_64 rotationMatrix  = quaternionToMatrix64(normalize64(normalize64(quatX * quatY) * quatZ));
            
            anglesReal += gyroReal;
            for(uint8 i = 0; i < 3; i++){
                anglesReal.v[i] += 360;
                anglesReal.v[i] = fmodd64(anglesReal.v[i], 360);
                
            }
            
            orientationReal = rotationMatrix * orientationReal;
            
            //rotation difference
            quatX = Quat64(V3_64(1, 0, 0), degToRad64(-anglesReal.x));
            quatY = Quat64(V3_64(0, 1, 0), degToRad64(-anglesReal.y));
            quatZ = Quat64(V3_64(0, 0, 1), degToRad64(-anglesReal.z));
            
            rotationMatrix  = quaternionToMatrix64(normalize64(normalize64(quatX * quatY) * quatZ));
            
            
            o = rotationMatrix * originalRotation; 
            
            dv3_64 gyroRaw = {x, y, z};
            degreesRaw += gyroRaw;
            for(uint8 i = 0; i < 3; i++){
                degreesRaw.v[i] += degModulo;
                degreesRaw.v[i] = degreesRaw.v[i] % degModulo;
            }
            
            v3_64 gyroRawReal = (degreesRaw * degMultiplier)/degDivisor;
            anglesRaw = gyroRawReal;
            
            //rotation difference
            quatX = Quat64(V3_64(1, 0, 0), degToRad64(-gyroRawReal.x));
            quatY = Quat64(V3_64(0, 1, 0), degToRad64(-gyroRawReal.y));
            quatZ = Quat64(V3_64(0, 0, 1), degToRad64(-gyroRawReal.z));
            
            rotationMatrix  = quaternionToMatrix64(normalize64(normalize64(quatX * quatY) * quatZ));
            
            orientationRaw = rotationMatrix * originalRotation;
            
        }
        
        snprintf(csv.contents, 1024, "%d;%60.30lf;%60.30lf;%60.30lf;%60.30lf;%60.30lf;%60.30lf;%60.30lf;%60.30lf;%60.30lf\r\n", i, Rp.x, pos.x, Rv.x, vel.x, anglesReal.x, anglesRaw.x, orientationReal.x, orientationRaw.x, o.x);
        csv.size = strlen(csv.contents);
        appendFile(filename, &csv);
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



