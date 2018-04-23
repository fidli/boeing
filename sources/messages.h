#ifndef MESSAGES
#define MESSAGES

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
            char name[10];
            MPU6050Settings settings;
        } init;
        struct {
            uint32 length;
        } data;
    };
};
#pragma pack(pop)

#endif