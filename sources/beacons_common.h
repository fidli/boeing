#ifndef BEACONS_COMMON
#define BEACONS_COMMON

struct Common{
    char ip[16];
    char port[6];
    NetSocket beaconsSocket;
    bool keepRunning;
};

#endif
