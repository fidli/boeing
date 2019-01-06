#include <Windows.h>
#include "windows_types.h"

#include "common.h"


#define PERSISTENT_MEM MEGABYTE(0)
#define TEMP_MEM MEGABYTE(100)
#define STACK_MEM MEGABYTE(0)


#include "util_mem.h"

#include "windows_time.cpp"
#include "util_rng.cpp"
#include "stdio.h"

#include "util_filesystem.cpp"
#include "windows_filesystem.cpp"
#include "util_math.cpp"


struct Context{
    HINSTANCE hInstance;
};

Context context;

struct ColData{
    int16 values[6][300000];
    int32 count;
};


static inline int main(int argc, char ** argv) {
    
    
    LPVOID memoryStart = VirtualAlloc(NULL, TEMP_MEM + PERSISTENT_MEM + STACK_MEM, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    bool memory = memoryStart != NULL;
    
    if(!memory){
        ASSERT(false);
        return 0;
    }
    
    initMemory(memoryStart);
    initTime();
    initRng();
    
    int linelen = 500;
    char * line = &PUSHA(char, linelen);
    FileContents contents;
    ASSERT(getFileSize((const char *)argv[1], &contents.size));
    contents.contents = &PUSHA(char, contents.size);
    
    ASSERT(readFile((const char *)argv[1], &contents));
    
    char * filename = "data.rec";
    char * filename2 = "pos.rec";
    
    FileContents csv;
    csv.size = 1024;
    csv.contents = &PUSHA(char, csv.size);
    
    FileContents pos;
    pos.size = 1024;
    pos.contents = &PUSHA(char, pos.size);
    
    
    dv3_64 accBias = {};
    dv3_64 accVar = {};
    
    dv3_64 gyroBias = {};
    dv3_64 gyroVar = {};
    
    dv3_64 accSum64 = {};
    dv3_64 velSum64 = {};
    dv3_64 gyroSum64 = {};
    
    v3_64 worldPosition64 = {};
    v3_64 rotationAngles64 = {};
    
    v3_64 defaultWorldPosition64 = {};
    
    //default worldPosition
    getNextLine(&contents, line, linelen);
    sscanf(line, "%lf %lf %lf", &defaultWorldPosition64.x, &defaultWorldPosition64.y, &defaultWorldPosition64.z);
    
    //default acc sum
    getNextLine(&contents, line, linelen);
    sscanf(line, "%lld %lld %lld", &accSum64.x, &accSum64.y, &accSum64.z);
    
    //default vel sum
    getNextLine(&contents, line, linelen);
    sscanf(line, "%lld %lld %lld", &velSum64.x, &velSum64.y, &velSum64.z);
    
    //default gyro sum
    getNextLine(&contents, line, linelen);
    sscanf(line, "%lld %lld %lld", &gyroSum64.x, &gyroSum64.y, &gyroSum64.z);
    
    
    //acc expectncy raw
    getNextLine(&contents, line, linelen);
    sscanf(line, "%lld %lld %lld", &accBias.x, &accBias.y, &accBias.z);
    
    //acc std dev raw
    getNextLine(&contents, line, linelen);
    sscanf(line, "%lld %lld %lld", &accVar.x, &accVar.y, &accVar.z);
    
    //gyro expectncy raw
    getNextLine(&contents, line, linelen);
    sscanf(line, "%lld %lld %lld", &gyroBias.x, &gyroBias.y, &gyroBias.z);
    
    //gyro std dev raw
    getNextLine(&contents, line, linelen);
    sscanf(line, "%lld %lld %lld", &gyroVar.x, &gyroVar.y, &gyroVar.z);
    
    
    
    ColData * colData = &PUSH(ColData);
    while(getNextLine(&contents, line, linelen)){
        ASSERT(sscanf(line, "%hd %hd %hd %hd %hd %hd", &colData->values[0][colData->count], &colData->values[1][colData->count], &colData->values[2][colData->count], &colData->values[3][colData->count], &colData->values[4][colData->count], &colData->values[5][colData->count]) == 6);
        colData->count++;
    }
    
    
    
    int32 windowSize;
    sscanf(argv[2], "%d", &windowSize);
    
    printf("window size: %d\r\n", windowSize);
    
    int32 iterationIndex = windowSize;
    while(iterationIndex < colData->count){
        int32 avgCols[6] = {};
        for(int32 coli = 0; coli < 6; coli++){
            for(int32 di = iterationIndex - windowSize; di < iterationIndex; di++){
                avgCols[coli] += colData->values[coli][di];
            }
            avgCols[coli] /= windowSize;
        }
        
        dv3_64 accDataCleared = DV3_64(avgCols[1], -avgCols[0], avgCols[2]) - accBias;
        
        /*for(int32 i = 0; i < 3; i++){
            //NOTE(AK): > and <= because the sd vas trimmed to whole numbers from floats
            if(accDataCleared.v[i] > -accVar.v[i] && accDataCleared.v[i] <= accVar.v[i]){
                accDataCleared = {};
            }
        }*/
        
        
        dv3_64 gyroDataCleared = DV3_64(-avgCols[3], -avgCols[4], -avgCols[5]) - gyroBias;
        /*
        for(int32 i = 0; i < 3; i++){
            //NOTE(AK): > and <= because the sd vas trimmed to whole numbers from floats
            if(gyroDataCleared.v[i] > -gyroVar.v[i] && gyroDataCleared.v[i] <= gyroVar.v[i]){
                gyroDataCleared = {};
            }
        }
        */
#if 1
        int32 tDivisor = 500/windowSize;
        float64 g = 9.8196f;
        int32 accDivisor = 2048;
        int32 velDivisor = tDivisor * accDivisor;
        int32 pDivisor = tDivisor * tDivisor * 2 * accDivisor;
        
        int32 degDivisor = tDivisor * 655;
        int32 degMultiplier = 10;
        int32 degModulo = 360 * degDivisor / degMultiplier;
        
        accSum64 += accDataCleared;
        
        //actual locomotion
        worldPosition64 = defaultWorldPosition64 + ((velSum64*2 + accSum64) / pDivisor)*g;
        velSum64 += accSum64;
        
        gyroSum64 += gyroDataCleared;
        for(uint8 i = 0; i < 3; i++){
            gyroSum64.v[i] += degModulo;
            gyroSum64.v[i] = gyroSum64.v[i] % degModulo;
        }
        
        rotationAngles64 = (gyroSum64 * degMultiplier)/degDivisor;
        
#endif
#if 1
        snprintf(csv.contents, 1024, "%d;%lld;%lld;%lld;%lld;%lld;%lld\n", iterationIndex, accDataCleared.x, accDataCleared.y, accDataCleared.z, gyroDataCleared.x, gyroDataCleared.y, gyroDataCleared.z);
        csv.size = strlen(csv.contents);
        appendFile(filename, &csv);
        snprintf(pos.contents, 1024, "%d;%lf;%lf;%lf;%lf;%lf;%lf\n", iterationIndex, worldPosition64.x, worldPosition64.y, worldPosition64.z, rotationAngles64.x, rotationAngles64.y, rotationAngles64.z);
        pos.size = strlen(pos.contents);
        appendFile(filename2, &pos);
#endif
        iterationIndex += windowSize;
        
    }
    
    
    
    
    
    
    if (!VirtualFree(memoryStart, 0, MEM_RELEASE)) {
        //more like log it
        ASSERT(!"Failed to free memory");
    }
    
    return 0;
}



