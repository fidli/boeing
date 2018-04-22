#ifndef MPU6050
#define MPU6050


#define DEBUG 1

float mpu6050_g = 9.8f;

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

#pragma pack(push, 1)
struct MPU6050Settings{
    GyroPrecision gyroPrecision;
    AccPrecision accPrecision;
    uint16 sampleRate;
};
#pragma pack(pop)


struct MPU6050Handle{
    //platform, lib specific handles
    int fd;
    //mpu6050 specific stuff
    MPU6050Settings settings;
};


#ifndef SERVER

#include "wiringPi.h"
#include "wiringPiI2C.h"

//replace these two with whatever I2C implementation that you fancy
void write8Reg(MPU6050Handle * handle, int reg, uint8 data){
    wiringPiI2CWriteReg8(handle->fd, reg, (int)data);
}
uint8 read8Reg(MPU6050Handle * handle, int reg){
    return (uint8) wiringPiI2CReadReg8(handle->fd, reg);
}

#else
void write8Reg(MPU6050Handle * handle, int reg, uint8 data){
    ASSERT(!"SUKC MY DICK");
}
uint8 read8Reg(MPU6050Handle * handle, int reg){
    ASSERT(!"SUKC MY DICK");
    return 0;
}

#endif

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

#define MPU6050_REGISTER_ACCEL_XOUT_H 0x3B
#define MPU6050_REGISTER_ACCEL_XOUT_L 0x3C
#define MPU6050_REGISTER_ACCEL_YOUT_H 0x3D
#define MPU6050_REGISTER_ACCEL_YOUT_L 0x3E
#define MPU6050_REGISTER_ACCEL_ZOUT_H 0x3F
#define MPU6050_REGISTER_ACCEL_ZOUT_L 0x40


//resets all the flags to default state
void mpu6050_reset(MPU6050Handle * handle){
    write8Reg(handle, MPU6050_REGISTER_PWR_MGMT_1, 128);
    //wait untill device resets
    //64 is default value of this register
    uint8 reg;
    while((reg = read8Reg(handle, MPU6050_REGISTER_PWR_MGMT_1)) != 64){
#if DEBUG
        printf("reseting device  %hhu\n", reg);
        sleep(1);
#endif
    }
}

void mpu6050_setup(MPU6050Handle * handle, const MPU6050Settings settings){
    
    //turn on power cycling for fifo settings
    write8Reg(handle, MPU6050_REGISTER_PWR_MGMT_1, 32);
    
    //use fifo buffer, no slaves, reset FIFO
    write8Reg(handle, MPU6050_REGISTER_USER_CONTROL, 64);
    
    uint8 reg;
    while((reg = read8Reg(handle, MPU6050_REGISTER_USER_CONTROL)) != 64){
#if DEBUG
        printf("reseting fifo %hhu\n", reg);
#endif
    }
    
    //clean the reg
    write8Reg(handle, MPU6050_REGISTER_PWR_MGMT_2, 0);
    //no sleep, no temperature X axis gyroscope as clock
    write8Reg(handle, MPU6050_REGISTER_PWR_MGMT_1, 8+1);
    
    
    //fifo mask, what to put in fifo, gyro x y z, acc x y z in order of Hbyte, Lbyte, 12 bytes per sample, acc values first then gyro ones
    write8Reg(handle, MPU6050_REGISTER_FIFO_EN, 64+32+16+8);
    //no FSYNC, NO lfp
    write8Reg(handle, MPU6050_REGISTER_CONFIG, 0);
    //sample rate = (8khz / (settings->sampleRate+1))
    uint8 param = (8000 / settings.sampleRate) - 1;
    write8Reg(handle, MPU6050_REGISTER_SAMPLE_RATE, param);
    
    //sensitivity
    write8Reg(handle, MPU6050_REGISTER_ACCEL_CONFIG, settings.accPrecision << 3);
    write8Reg(handle, MPU6050_REGISTER_GYRO_CONFIG, settings.gyroPrecision << 3);
    
    
    
}

uint16 mpu6050_fifoCount(MPU6050Handle * handle){
    uint8 high = read8Reg(handle, MPU6050_REGISTER_FIFO_COUNT_H);
    uint8 low = read8Reg(handle, MPU6050_REGISTER_FIFO_COUNT_L);
    return (((uint16) high) << 8) + low;
}

uint8 mpu6050_readFifoByte(MPU6050Handle * handle){
    return read8Reg(handle, MPU6050_REGISTER_FIFO_R_W);
}


void mpu6050_acc2float(const MPU6050Settings setting, const int16 x, const int16 y, const int16 z, float32 * result_x, float32 * result_y,float32 * result_z){
    float32 attun;
    switch(setting.accPrecision){
        case AccPrecision_2:{
            attun = 1.0f / 16384; 
        }break;
        case AccPrecision_4:{
            attun = 1.0f / 8192;
        }break;
        case AccPrecision_8:{
            attun = 1.0f / 4096;
        }break;
        case AccPrecision_16:{
            attun = 1.0f / 2048;
        }break;
    }
    *result_x = attun * x;
    *result_y = attun * y;
    *result_z = attun * z;
}


void mpu6050_gyro2float(const MPU6050Settings setting, const int16 x, const int16 y, const int16 z, float32 * result_x, float32 * result_y, float32 * result_z){
    float32 attun;
    switch(setting.gyroPrecision){
        case GyroPrecision_250:{
            attun = 1.0f / 131; 
        }break;
        case GyroPrecision_500:{
            attun = 1.0f / 65.5f;
        }break;
        case GyroPrecision_1000:{
            attun = 1.0f / 32.8f;
        }break;
        case GyroPrecision_2000:{
            attun = 1.0f / 16.4f;
        }break;
    }
    *result_x = attun * x;
    *result_y = attun * y;
    *result_z = attun * z;
}

float32 mpu6050_getTimeDelta(const uint16 sampleRate){
    if(sampleRate == 250){
        return 0.04f;
    }else{
        return 1.0f / sampleRate;
    }
}

#endif