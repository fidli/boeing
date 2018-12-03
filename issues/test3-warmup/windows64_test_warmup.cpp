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

struct MemsData{
    struct {
        int16 value;
        int32 frequency;
    } frequencies[6][200000];
    int32 totalFrequencies[6];
    int32 counts[6];
};

struct ColData{
    int16 values[6][200000];
    int32 count;
};

struct Entropies{
    float32 col[6][200000];
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
    
#if 0
    
    MemsData * data = &PUSH(MemsData);
    Entropies * entropies = &PUSH(Entropies);
    int32 entropyIndex = 0;
    while(getNextLine(&contents, line, linelen)){
        int16 cols[6];
        ASSERT(sscanf(line, "%hd %hd %hd %hd %hd %hd", &cols[0], &cols[1], &cols[2], &cols[3], &cols[4], &cols[5]) == 6);
        for(int32 coli = 0; coli < 6; coli++){
            bool found = false;
            for(int32 si = 0; si < data->counts[coli]; si++){
                if(cols[coli] == data->frequencies[coli][si].value){
                    found = true;
                    data->frequencies[coli][si].frequency++;
                    break;
                }
            }
            if(!found){
                data->frequencies[coli][data->counts[coli]].value = cols[coli];
                data->frequencies[coli][data->counts[coli]].frequency++;
                data->counts[coli]++;
            }
            data->totalFrequencies[coli]++;
            
        }
        for(int32 coli = 0; coli < 6; coli++){
            float32 entropy = 0;
            for(int32 si = 0; si < data->counts[coli]; si++){
                float32 prob = (float32)data->frequencies[coli][si].frequency / data->totalFrequencies[coli];
                entropy -= prob * log(prob)/log(2);
            }
            entropies->col[coli][entropyIndex] = entropy;
        }
        
        snprintf(csv.contents, 1024, "%d %f %f %f %f %f %f\n", entropyIndex, entropies->col[0][entropyIndex], entropies->col[1][entropyIndex], entropies->col[2][entropyIndex], entropies->col[3][entropyIndex], entropies->col[4][entropyIndex], entropies->col[5][entropyIndex]);
        csv.size = strlen(csv.contents);
        appendFile(filename, &csv);
        
        
        entropyIndex++;
        
    }
    
    
#endif
    
#if 1
    int32 windowSize;
    sscanf(argv[2], "%d", &windowSize);
    
    printf("window size: %d\r\n", windowSize);
    
    MemsData * data = &PUSH(MemsData);
    Entropies * entropies = &PUSH(Entropies);
    int32 entropyIndex = 0;
    ColData * colData = &PUSH(ColData);
    while(getNextLine(&contents, line, linelen)){
        ASSERT(sscanf(line, "%hd %hd %hd %hd %hd %hd", &colData->values[0][colData->count], &colData->values[1][colData->count], &colData->values[2][colData->count], &colData->values[3][colData->count], &colData->values[4][colData->count], &colData->values[5][colData->count]) == 6);
        colData->count++;
    }
    
    
    while(entropyIndex < colData->count - windowSize){
        if(entropyIndex == colData->count / 4){
            printf("25%\r\n");
        }
        
        if(entropyIndex == colData->count / 2){
            printf("50%\r\n");
        }
        
        if(entropyIndex == 3 * colData->count / 4){
            printf("75%\r\n");
        }
        for(int32 coli = 0; coli < 6; coli++){
            data->counts[coli] = 0;
            data->totalFrequencies[coli] = 0;
        }
        
        for(int32 i = entropyIndex; i < entropyIndex + windowSize; i++){
            
            for(int32 coli = 0; coli < 6; coli++){
                bool found = false;
                for(int32 si = 0; si < data->counts[coli]; si++){
                    if(colData->values[coli][i] == data->frequencies[coli][si].value){
                        found = true;
                        data->frequencies[coli][si].frequency++;
                        break;
                    }
                }
                if(!found){
                    data->frequencies[coli][data->counts[coli]].value = colData->values[coli][i];
                    data->frequencies[coli][data->counts[coli]].frequency = 1;
                    data->counts[coli]++;
                }
                data->totalFrequencies[coli]++;
                
            }
        }
        
        
        for(int32 coli = 0; coli < 6; coli++){
            float32 entropy = 0;
            for(int32 si = 0; si < data->counts[coli]; si++){
                float32 prob = (float32)data->frequencies[coli][si].frequency / data->totalFrequencies[coli];
                entropy -= prob * log(prob)/log(2);
            }
            entropies->col[coli][entropyIndex] = entropy;
        }
        
        snprintf(csv.contents, 1024, "%d %f %f %f %f %f %f\n", entropyIndex, entropies->col[0][entropyIndex], entropies->col[1][entropyIndex], entropies->col[2][entropyIndex], entropies->col[3][entropyIndex], entropies->col[4][entropyIndex], entropies->col[5][entropyIndex]);
        csv.size = strlen(csv.contents);
        appendFile(filename, &csv);
        
        
        
        entropyIndex++;
    }
    
    
    
    
    
#endif
    
    
    
    if (!VirtualFree(memoryStart, 0, MEM_RELEASE)) {
        //more like log it
        ASSERT(!"Failed to free memory");
    }
    
    return 0;
}



