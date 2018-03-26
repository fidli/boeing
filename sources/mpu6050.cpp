#ifndef MPU6050
#define MPU6050

#include "wiringPi.h"
#include "wiringPiI2C.h"

enum GyroPrecision{
    GyroPrecision_250,
    GyroPrecision_500,
    GyroPrecision_1000,
    GyroPrecision_2000
};

enum AccPrecision{
    AccPrecision_2,
    AccPrecision_4,
    AccPrecision_8,
    AccPrecision_16
};


struct MPU6050Settings{
    GyroPrecision gyroPrecision;
    AccPrecision accPrecision;
};


struct MPU6050Handle{
    //platform, lib specific handles
    int fd;
    //mpu6050 specific stuff
    GyroPrecision gyroPrecision;
    AccPrecision accPrecision;
    
};


//replace these two with whatever I2C implementation that you fancy
void write8Reg(MPU6050Handle * handle, int reg, uint8 data){
    wiringPiI2CWriteReg8(handle->fd, reg, (int)data);
}
uint8 read8Reg(MPU6050Handle * handle, int reg){
    return (uint8) wiringPiI2CReadReg8(handle->fd, reg);
}


#define MPU6050_REGISTER_SAMPLE_RATE 0x19
#define MPU6050_REGISTER_CONFIG 0x1A
#define MPU6050_REGISTER_GYRO_CONFIG 0x1B
#define MPU6050_REGISTER_ACCEL_CONFIG 0x1C
#define MPU6050_REGISTER_FIFO_EN 0x23
#define MPU6050_REGISTER_USER_CONTROL 0x6A
#define MPU6050_REGISTER_PWR_MGMT_1 0x6B
#define MPU6050_REGISTER_PWR_MGMT_2 0x6C

#define MPU6050_REGISTER_FIFO_COUNT_H 0x72
#define MPU6050_REGISTER_FIFO_COUNT_L 0x73
#define MPU6050_REGISTER_FIFO_R_W 0x74


//resets all the flags to default state
void mpu6050_reset(MPU6050Handle * handle){
    write8Reg(handle, MPU6050_REGISTER_PWR_MGMT_1, 128);
    //wait untill device resets
    //64 is default value of this register
    while(read8Reg(handle, MPU6050_REGISTER_PWR_MGMT_1) != 64){
        printf("reseting \n");
    }
}

void mpu6050_setup(MPU6050Handle * handle, const MPU6050Settings settings){
    
    //no sleep, no temperature X axis gyroscope as clock
    write8Reg(handle, MPU6050_REGISTER_PWR_MGMT_1, 8+1);
    
    //use fifo buffer, no slaves
    write8Reg(handle, MPU6050_REGISTER_USER_CONTROL, 64);
    
    //write8Reg(handle, MPU6050_REGISTER_PWR_MGMT_2, 0);
    
    //fifo mask, what to put in fifo, gyro x y z, acc x y z in order of Hbyte, Lbyte, 12 bytes per sample
    write8Reg(handle, MPU6050_REGISTER_FIFO_EN, 64+32+16+8);
    //no FSYNC, NO lfp
    write8Reg(handle, MPU6050_REGISTER_CONFIG, 7);
    //sample rate 8khz / (7+1)
    write8Reg(handle, MPU6050_REGISTER_SAMPLE_RATE, 7);
    
    //sensitivity
    write8Reg(handle, MPU6050_REGISTER_ACCEL_CONFIG, settings.accPrecision << 3);
    write8Reg(handle, MPU6050_REGISTER_GYRO_CONFIG, settings.gyroPrecision << 3);
    
    
    
}

uint16 mpu6050_fifoCount(MPU6050Handle * handle){
    uint8 high = read8Reg(handle, MPU6050_REGISTER_FIFO_COUNT_H);
    uint8 low = read8Reg(handle, MPU6050_REGISTER_FIFO_COUNT_L);
    return (((uint16) high) << 8) | low;
}

uint8 mpu6050_readFifoByte(MPU6050Handle * handle){
    return read8Reg(handle, MPU6050_REGISTER_FIFO_R_W);
}
#endif