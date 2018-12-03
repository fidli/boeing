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
    int16 values[6][66000];
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
    
    char * filename = "result.csv";
    
    FileContents csv;
    csv.size = 1024;
    csv.contents = &PUSHA(char, csv.size);
    
    dv3_64 accBias = {};
    dv3_64 accVar = {};
    
    //acc expectncy raw
    getNextLine(&contents, line, linelen);
    sscanf(line, "%lld %lld %lld", &accBias.x, &accBias.y, &accBias.z);
    
    //acc std dev raw
    getNextLine(&contents, line, linelen);
    sscanf(line, "%lld %lld %lld", &accVar.x, &accVar.y, &accVar.z);
    
    
    
    ColData * colData = &PUSH(ColData);
    int32 it;
    while(getNextLine(&contents, line, linelen)){
        ASSERT(sscanf(line, "%d;%hd;%hd;%hd;%hd;%hd;%hd", &it, &colData->values[0][colData->count], &colData->values[1][colData->count], &colData->values[2][colData->count], &colData->values[3][colData->count], &colData->values[4][colData->count], &colData->values[5][colData->count]) == 7);
        colData->count++;
    }
    
    
    
    
    int32 windowSize = 1;
    int32 iterationIndex = windowSize;
    while(iterationIndex < colData->count){
        int32 avgCols[6] = {};
        for(int32 coli = 0; coli < 6; coli++){
            for(int32 di = iterationIndex - windowSize; di < iterationIndex; di++){
                avgCols[coli] += colData->values[coli][di];
            }
            avgCols[coli] /= windowSize;
        }
        
        dv3_64 accDataCleared = DV3_64(avgCols[0], avgCols[1], avgCols[2]) - accBias;
        for(int32 i = 0; i < 3; i++){
            //NOTE(AK): > and <= because the sd vas trimmed to whole numbers from floats
            if(accDataCleared.v[i] > -accVar.v[i] && accDataCleared.v[i] <= accVar.v[i]){
                accDataCleared = {};
            }
        }
        
#if 1
        int32 tDivisor = 500/windowSize;
        
        int32 accDivisor = 2048;
        int32 velDivisor = tDivisor * accDivisor;
        int32 pDivisor = tDivisor * tDivisor * 2 * accDivisor;
        
        
        
#endif
#if 1
        snprintf(csv.contents, 1024, "%d;%lld;%lld;%lld;%lld;%lld;%lld\n", iterationIndex, accDataCleared.x, accDataCleared.y, accDataCleared.z, (int64)0, (int64)0, (int64)0);
        csv.size = strlen(csv.contents);
        appendFile(filename, &csv);
#endif
        iterationIndex += windowSize;
        
    }
    
    
    
    
    
    
    if (!VirtualFree(memoryStart, 0, MEM_RELEASE)) {
        //more like log it
        ASSERT(!"Failed to free memory");
    }
    
    return 0;
}



