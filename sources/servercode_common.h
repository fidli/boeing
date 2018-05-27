#ifndef SERVERCODE_COMMON
#define SERVERCODE_COMMON

struct Common{
    NetSocket serverSocket;
    NetSocket boeingSocket[2];
    NetSocket beaconsSocket;
    bool keepRunning;
};

#endif
