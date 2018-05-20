#ifndef XBS2
#define XBS2

#include "util_time.h"

#include "util_serial.h"

struct XBS2Handle : SerialHandle{
    char sidLower[9];
};


/** THESE 3 CAN BE IMPLEMENTED ANYHOW*/
int32 writeSerial(XBS2Handle * module, const char * buffer, uint32 length){
    DWORD written;
    if(WriteFile(module->handle, buffer, length, &written, NULL)){
        return (int32)written;
    }
    return -1;
    
}

int32 readSerial(XBS2Handle * module, char * buffer, uint32 maxRead){
    DWORD read;
    if(ReadFile(module->handle, buffer, maxRead, &read, NULL)){
        return (int32)read;
    }
    return -1;
}

void wait(float32 seconds){
    float32 start = getProcessCurrentTime();
    while(getProcessCurrentTime() - start < seconds){}
}

/** END OF CUSTOM IMPLEMENTATION */




static int32 xbs2_sendMessage(XBS2Handle * module, const char * buffer){
    return writeSerial(module, buffer, strlen(buffer));
}

static void waitForMessage(XBS2Handle * module, char * responseBuffer, const char * message){
    int32 offset = 0;
    uint32 msglen = strlen(message);
    while(offset < msglen || strcmp_n(responseBuffer + offset - 3, message, msglen)){
        offset += readSerial(module, responseBuffer + offset, 70 - offset);
    }
    
}

void waitForAnyMessage(XBS2Handle * module, char * responseBuffer){
    int32 offset = 0;
    while(offset == 0 || responseBuffer[offset-1] != '\r'){
        offset += readSerial(module, responseBuffer + offset, 1);
    }
    
}


static void xbs2_enterCommandMode(XBS2Handle * module){
    char result[4];
    wait(1);
    ASSERT(xbs2_sendMessage(module, "+++"));
    wait(1);
    waitForMessage(module, result, "OK\r");
}


static void xbs2_exitCommandMode(XBS2Handle * module){
    char result[4];
    ASSERT(xbs2_sendMessage(module, "ATCN\r"));
    waitForMessage(module, result, "OK\r");
}



void xbs2_transmitMessage(XBS2Handle * source, const char * lowerAddress, const char * message){
    char res[20];
    
    xbs2_enterCommandMode(source);
    char buff[14];
    sprintf(buff, "ATDL%8s\r", lowerAddress);
    ASSERT(xbs2_sendMessage(source, buff));
    
    waitForAnyMessage(source, res);
    xbs2_exitCommandMode(source);
    
    
    ASSERT(xbs2_sendMessage(source, message));
}




void xbs2_setup(XBS2Handle * module){
    
    //memcpy(module->fifo, "\0", ARRAYSIZE(module->fifo));
    //module->tail = module->head = 0;
    
    //enter command mode
    //a 1 second pause [GT (Guard Times) parameter]
    //b "+++"
    //c second pause
    
    char result[70] = {};
    
    xbs2_enterCommandMode(module);
    
    //reset nework defaults
    ASSERT(xbs2_sendMessage(module, "ATNR0\r"));
    waitForMessage(module, result, "OK\r");
    
    wait(1);
    ASSERT(xbs2_sendMessage(module, "+++"));
    wait(1);
    waitForMessage(module, result, "OK\r");
    
    //reset factory defaults
    ASSERT(xbs2_sendMessage(module, "ATRE\r"));
    waitForMessage(module, result, "OK\r");
    
    //reset power
    ASSERT(xbs2_sendMessage(module, "ATFR\r"));
    waitForMessage(module, result, "OK\r");
    
    
    wait(5); //after 2 seconds is reset, then 1 second  silence, 2 sec reserve
    ASSERT(xbs2_sendMessage(module, "+++"));
    wait(1);
    waitForMessage(module, result, "OK\r");
    
    
    //other settings are default and it seems fine
    
    //result status "ATAI\r"
    ASSERT(xbs2_sendMessage(module, "ATAI\r"));
    waitForAnyMessage(module, result);
    ASSERT(!strcmp_n("0\r", result, 2));
    
    //low factory address "ATSL\r"
    ASSERT(xbs2_sendMessage(module, "ATSL\r"));
    waitForAnyMessage(module, result);
    sscanf(result, "%9[^\r]", module->sidLower);
    
    ASSERT(xbs2_sendMessage(module, "ATSH\r"));
    waitForAnyMessage(module, result);
    
    char add[9];
    char buff[14];
    sscanf(result, "%9[^\r]", add);
    sprintf(buff, "ATDH%8s\r", add);
    ASSERT(xbs2_sendMessage(module, buff));
    waitForMessage(module, result, "OK\r");
    
    //send bytes as they arrive
    ASSERT(xbs2_sendMessage(module, "ATRO0\r"));
    waitForMessage(module, result, "OK\r");
    
    xbs2_exitCommandMode(module);
    
}

#endif