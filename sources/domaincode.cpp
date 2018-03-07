#include <stdlib.h>
#include "string.h"
#include "stdio.h"

bool * gassert;
char * gassertMessage;

#define ASSERT(expression) if(!(expression)) { sprintf(gassertMessage, "ASSERT FAILED on line %d in file %s\n", __LINE__, __FILE__); *gassert = true; printf(gassertMessage);}


#define PERSISTENT_MEM 0
#define TEMP_MEM 0
#define STACK_MEM 0

#include "common.h"
#include "util_mem.h"
#include "util_filesystem.h"
#include "util_font.cpp"
#include "util_image.cpp"
#include "util_conv.cpp"

#include "domaincode.h"

struct DomainState : DomainInterface{
    BitmapFont font;
    bool valid;
};


DomainState * domainState;
extern DomainInterface * domainInterface;



extern "C" void initDomain(bool * assert, char * assertMessage, void * memstart){
    gassertMessage = assertMessage;
    gassert = assert;
    
    domainState = (DomainState *) memstart;
    domainInterface = (DomainInterface *) memstart;
    
    ASSERT((byte*)memstart + sizeof(DomainState) < (byte*)mem.stack);
    
    FileContents fontContents;
    Image fontImage;
    bool font = readFile("data/font.bmp", &fontContents) && decodeBMP(&fontContents, &fontImage) && flipY(&fontImage) && initBitmapFont(&domainState->font, &fontImage, fontImage.info.width / 16);
    domainState->valid = font;
    
    if(!font){
        printf("Font init err\n");
    }
    if(domainState->valid){
        printf("Domain init all good\n");
    }else{
        printf("Domain init BAD");
    }
}


extern "C" void iterateDomain(bool * keepRunning){
    if(domainState->valid){
        
        //render
        uint32 height = domainState->renderTarget.info.height;
        uint32 width = domainState->renderTarget.info.width;
        
        for(uint32 h = 0; h < height; h++){
            uint32 pitch = h * width;
            for(uint32 w = 0; w < width; w++){
                ((uint32*)domainState->renderTarget.data)[pitch + w] = 0xFF000000;
            }
        }
        
        printToBitmap(&domainState->renderTarget, 0, 0, "UP and running", &domainState->font, 16);
        
        uint32 offset = 0;
        char buf[50];
        //fps
        sprintf(buf, "FPS:%u", domainState->lastFps);
        ASSERT(printToBitmap(&domainState->renderTarget, offset*domainState->font.current.gridSize, height - 48, buf, &domainState->font, 16));
        offset += strlen(buf);
        
        
    }
}

extern "C" void closeDomain(){
    
}


