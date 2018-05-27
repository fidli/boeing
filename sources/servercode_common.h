#ifndef SERVERCODE_COMMON
#define SERVERCODE_COMMON

struct Common{
    char ip[16];
    char port[6];
    NetSocket serverSocket;
    NetSocket boeingSocket[2];
    NetSocket beaconsSocket;
    bool keepRunning;
};

#endif
