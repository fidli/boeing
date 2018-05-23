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
    MessageType_Data,
    
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
                } boeing;
                struct {
                    uint16 frequency;
                    char channel[3];
                    char pan[5];
                } beacon;
            };
            
        } init;
        struct {
            uint32 length;
        } data;
    };
};
#pragma pack(pop)

#endif