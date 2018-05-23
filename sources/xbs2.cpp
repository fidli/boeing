#ifndef XBS2
#define XBS2

#include "util_time.h"

#include "util_serial.h"

struct XBS2Handle : SerialHandle{
    char sidLower[9];
    float32 guardTime;
    uint32 baudrate;
    uint16 frequency;
    char channel[3];
    char pan[5];
};


void wait(float32 seconds){
    float32 start = getProcessCurrentTime();
    while(aseqr(getProcessCurrentTime() - start, seconds, 0.00005f)){}
}



static int32 xbs2_sendMessage(XBS2Handle * module, const char * buffer){
    return writeSerial(module, buffer, strlen(buffer));
}

static void waitForMessage(XBS2Handle * module, char * responseBuffer, const char * message, float32 timeout = -1){
    int32 offset = 0;
    uint32 msglen = strlen(message);
    while(offset < msglen || strcmp_n(responseBuffer + offset - 3, message, msglen)){
        offset += readSerial(module, responseBuffer + offset, 70 - offset, timeout);
    }
    
}

int32 waitForAnyMessage(XBS2Handle * module, char * responseBuffer, float32 timeout = -1){
    int32 offset = 0;
    if(timeout == -1){
        while(offset == 0 || responseBuffer[offset-1] != '\r'){
            offset += readSerial(module, responseBuffer + offset, 1, timeout);
        }
    }else{
        while(offset == 0 || responseBuffer[offset-1] != '\r'){
            int32 res = readSerial(module, responseBuffer + offset, 1, timeout);
            if(res <= 0) return offset;
            offset += res;
        }
        
    }
    return offset;
}


static bool xbs2_enterCommandMode(XBS2Handle * module){
    //enter command mode
    //a 1 second pause [GT (Guard Times) parameter]
    //b "+++"
    //c second pause
    
    char result[7] = {};
    wait(module->guardTime);
    if(!xbs2_sendMessage(module, "+++")) return false;
    wait(module->guardTime);
    waitForAnyMessage(module, result, 1.1f);
    if(!strcmp_n("OK\r", result, 7)){
        return true;
    }
    return false;
}


static bool xbs2_exitCommandMode(XBS2Handle * module){
    char result[7] = {};
    if(!xbs2_sendMessage(module, "ATCN\r")) return false;
    waitForAnyMessage(module, result, 1.1f);
    if(!strcmp_n("OK\r", result, 7)){
        return true;
    }
    return false;
}


bool xbs2_detectAndSetStandardBaudRate(XBS2Handle * module){
    module->guardTime = 1.1f;
    //sorted by default/popularity?
    uint32 rates[] = {9600, 115200, 19200, 38400, 57600, 4800, 2400, 1200};
    for(uint8 rateIndex = 0; rateIndex < ARRAYSIZE(rates); rateIndex++){
        if(setBaudRate(module, rates[rateIndex])){
            if(clearSerialPort(module)){
                if(xbs2_enterCommandMode(module)){
                    module->baudrate = rates[rateIndex];
                    if(!xbs2_exitCommandMode(module)){
                        return false;
                    }
                    return true;
                }
                //command mode timeout
                wait(10);
                clearSerialPort(module);
            }
        }
    }
    return false;
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

bool xbs2_initNetwork(XBS2Handle * module, char * channelMask = "1FFE"){
    if(xbs2_enterCommandMode(module)){
        //reset nework defaults
        char result[70] = {};
        bool success = true;
        
        char command[10];
        success = success && sprintf(command, "ATSC%4s\r", channelMask) == 1 && xbs2_sendMessage(module, command)  && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 3);
        
        success = success && xbs2_sendMessage(module, "ATNR0\r")  && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 3);
        
        //wait for network restart
        wait(1);
        
        if(success && xbs2_enterCommandMode(module)){
            
            success = success && xbs2_sendMessage(module, "ATAI\r")  && waitForAnyMessage(module, result) > 0 && !strcmp_n("0\r", result, 2); 
            
            // operating channel
            success = success && xbs2_sendMessage(module, "ATCH\r") && waitForAnyMessage(module, result) > 0 && sscanf(result, "%5[^\r]", module->channel) == 1;
            if(success){
                /**
            bit flag (channel)
            0 (0x0B) 4 (0x0F) 8 (0x13) 12 (0x17)
                1 (0x0C) 5 (0x10) 9 (0x14) 13 (0x18)
                2 (0x0D) 6 (0x11) 10 (0x15) 14 (0x19)
                3 (0x0E) 7 (0x12) 11 (0x16) 15 (0x1A)
                
                */
                if(!strcmp_n(module->channel, "B", 2)){
                    module->frequency = 2405;
                }else if(!strcmp_n(module->channel, "C", 2)){
                    module->frequency = 2410;
                }else if(!strcmp_n(module->channel, "D", 2)){
                    module->frequency = 2415;
                }else if(!strcmp_n(module->channel, "E", 2)){
                    module->frequency = 2420;
                }else if(!strcmp_n(module->channel, "F", 2)){
                    module->frequency = 2425;
                }else if(!strcmp_n(module->channel, "10", 2)){
                    module->frequency = 2430;
                }else if(!strcmp_n(module->channel, "11", 2)){
                    module->frequency = 2435;
                }else if(!strcmp_n(module->channel, "12", 2)){
                    module->frequency = 2440;
                }else if(!strcmp_n(module->channel, "13", 2)){
                    module->frequency = 2445;
                }else if(!strcmp_n(module->channel, "14", 2)){
                    module->frequency = 2450;
                }else if(!strcmp_n(module->channel, "15", 2)){
                    module->frequency = 2455;
                }else if(!strcmp_n(module->channel, "16", 2)){
                    module->frequency = 2460;
                }else if(!strcmp_n(module->channel, "17", 2)){
                    module->frequency = 2465;
                }else if(!strcmp_n(module->channel, "18", 2)){
                    module->frequency = 2470;
                }else if(!strcmp_n(module->channel, "19", 2)){
                    module->frequency = 2475;
                }else if(!strcmp_n(module->channel, "1A", 2)){
                    module->frequency = 2480;
                }
            }
            
            // pan id
            success = success && xbs2_sendMessage(module, "ATID\r") && waitForAnyMessage(module, result) > 0 && sscanf(result, "%7[^\r]", module->pan) == 1;
            
            
            xbs2_exitCommandMode(module);
        }else{
            if(!success) xbs2_exitCommandMode(module);
            return false;
        }
        
        return success;
    }else{
        return false;
    }
}


bool xbs2_readValues(XBS2Handle * module){
    //reading values
    
    char result[70] = {};
    
    bool success = true;
    
    if(xbs2_enterCommandMode(module)){
        
        //low factory address "ATSL\r"
        success = success && xbs2_sendMessage(module, "ATSL\r") && waitForAnyMessage(module, result) > 0 && sscanf(result, "%9[^\r]", module->sidLower) == 1;
        
        xbs2_exitCommandMode(module);
        return success;
        
    }else{
        return false;
    }
}

bool xbs2_initModule(XBS2Handle * module){
    
    module->guardTime = 1.1f;
    char result[70] = {};
    
    bool success = true;
    
    if(xbs2_enterCommandMode(module)){
        
        //reset factory defaults
        success = success && xbs2_sendMessage(module, "ATRE\r") && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 3);
        result[0] = 0;
        
        //reset power
        success = success && xbs2_sendMessage(module, "ATFR\r") && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 3);
        result[0] = 0;
        
        if(success) wait(4); //after 2 seconds is reset, 2 seconds reserve
        
        if(success && xbs2_enterCommandMode(module)){
            //other settings are default and it seems fine
            
            //send bytes as they arrive
            success = success && xbs2_sendMessage(module, "ATRO0\r") && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 3);
            
            //disable rssi
            success = success && xbs2_sendMessage(module, "ATP00\r") && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 3);
            
            //disable network indicator diod
            success = success && xbs2_sendMessage(module, "ATD50\r") && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 3);
            
            //disable flow control flags
            success = success && xbs2_sendMessage(module, "ATD70\r") && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 3);
            
            //broadcast hops - max 1
            success = success && xbs2_sendMessage(module, "ATBH0\r") && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 3);
            
            //broadcast destination address
            success = success && xbs2_sendMessage(module, "ATDLFFFF\r") && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 3);
            
            //all modules have same high addressw set as destination address
            success = success && xbs2_sendMessage(module, "ATSH\r") && waitForAnyMessage(module, result) > 0;
            if(success){
                char add[9];
                char buff[14];
                sscanf(result, "%9[^\r]", add);
                sprintf(buff, "ATDH%8s\r", add);
                success = success && xbs2_sendMessage(module, buff) && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 3);
            }
            
            /*
            //3ms guard time lesser is hardly achievable, sometimes it does not work
            success = success && xbs2_sendMessage(module, "ATGT003\r") && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 1);
            if(success) module->guardTime = 0.0031f;
            */
            
            //100ms
            success = success && xbs2_sendMessage(module, "ATGT064\r") && waitForAnyMessage(module, result) > 0 && !strcmp_n("OK\r", result, 1);
            if(success) module->guardTime = 0.110f;
            
            
        }
        xbs2_exitCommandMode(module);
        return success;
    }else{
        return false;
    }
}

#endif