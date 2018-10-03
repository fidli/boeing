#ifndef MESSAGES
#define MESSAGES

#include "mpu6050.cpp"

enum ClientType{
    ClientType_Invalid,
    
    ClientType_Beacon,
    ClientType_Boeing,
    
    ClientTypeCount
};

enum MessageType{
    MessageType_Invalid,
    MessageType_Init,
    MessageType_Reset,
    MessageType_Data,
    MessageType_Calibrate,
    MessageType_Ready,
    
    MessageTypeCount
};

#pragma pack(push, 1)
struct Message{
    union{
        uint32 reserved;
        MessageType type;
    };
    union{
        struct {
            ClientType clientType;
            union{
                struct {
                    char name;
                    MPU6050Settings settings;
                    float32 xbPeriod;
                    char sidLower[9];
                } boeing;
                struct {
                    uint16 frequencyKhz;
                    char channel[3];
                    char pan[5];
                    char sidLower[4][9];
                    uint64 timeDivisor;
                } beacon;
                
            };
            
        } init;
        struct {
            char id;
        } ready;
        struct {
            uint32 sampleCount;
            char sidLower[9];
            char id;
        } calibrate;
        struct {
            uint32 length;
            uint8 boeingId;
        } data;
    };
};
#pragma pack(pop)

#endif