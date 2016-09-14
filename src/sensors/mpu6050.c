/*
	This file is part of martink project. 

	martink firmware project is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	martink firmware is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with martink firmware.  If not, see <http://www.gnu.org/licenses/>.

	Github: https://github.com/mkschreder

	Contributors:
	* Davide Gironi - developing original driver
	* Martin K. Schröder - maintenance since Oct 2014
*/

#include <stdlib.h>
#include <string.h>
#include <arch/soc.h>
#include "mpu6050.h"
#include "mpu6050registers.h"

#if MPU6050_GETATTITUDE == 1 || MPU6050_GETATTITUDE == 2
#include <math.h>  //include libm
#endif

//static uint8_t buffer[14];

//enable the getattitude functions
//because we do not have a magnetometer, we have to start the chip always in the same position
//then to obtain your object attitude you have to apply the aerospace sequence
//0 disabled
//1 mahony filter
//2 dmp chip processor
#define MPU6050_GETATTITUDE 0

//definitions for raw data
//gyro and acc scale
#define MPU6050_GYRO_FS MPU6050_GYRO_FS_2000
#define MPU6050_ACCEL_FS MPU6050_ACCEL_FS_2

#define MPU6050_GYRO_LSB_250 131.0
#define MPU6050_GYRO_LSB_500 65.5
#define MPU6050_GYRO_LSB_1000 32.8
#define MPU6050_GYRO_LSB_2000 16.4
#if MPU6050_GYRO_FS == MPU6050_GYRO_FS_250
#define MPU6050_GGAIN MPU6050_GYRO_LSB_250
#elif MPU6050_GYRO_FS == MPU6050_GYRO_FS_500
#define MPU6050_GGAIN MPU6050_GYRO_LSB_500
#elif MPU6050_GYRO_FS == MPU6050_GYRO_FS_1000
#define MPU6050_GGAIN MPU6050_GYRO_LSB_1000
#elif MPU6050_GYRO_FS == MPU6050_GYRO_FS_2000
#define MPU6050_GGAIN MPU6050_GYRO_LSB_2000
#endif

#define MPU6050_ACCEL_LSB_2 16384.0
#define MPU6050_ACCEL_LSB_4 8192.0
#define MPU6050_ACCEL_LSB_8 4096.0
#define MPU6050_ACCEL_LSB_16 2048.0
#if MPU6050_ACCEL_FS == MPU6050_ACCEL_FS_2
#define MPU6050_AGAIN MPU6050_ACCEL_LSB_2
#elif MPU6050_ACCEL_FS == MPU6050_ACCEL_FS_4
#define MPU6050_AGAIN MPU6050_ACCEL_LSB_4
#elif MPU6050_ACCEL_FS == MPU6050_ACCEL_FS_8
#define MPU6050_AGAIN MPU6050_ACCEL_LSB_8
#elif MPU6050_ACCEL_FS == MPU6050_ACCEL_FS_16
#define MPU6050_AGAIN MPU6050_ACCEL_LSB_16
#endif

#define MPU6050_CALIBRATEDACCGYRO 1//set to 1 if is calibrated
#if MPU6050_CALIBRATEDACCGYRO == 1
#define MPU6050_AXOFFSET 0
#define MPU6050_AYOFFSET 0
#define MPU6050_AZOFFSET 0
#define MPU6050_AXGAIN 16384.0
#define MPU6050_AYGAIN 16384.0
#define MPU6050_AZGAIN 16384.0
#define MPU6050_GXOFFSET 0
#define MPU6050_GYOFFSET 0
#define MPU6050_GZOFFSET 0
// gain is maxint16 (32768) /range - range degrees of precision
#define MPU6050_GXGAIN 16.4
#define MPU6050_GYGAIN 16.4
#define MPU6050_GZGAIN 16.4
#endif

#define MPU6050_TIME_DELAY  0x0100

#define MPU6050_STATE_INITIALIZED (1 << 0)

#include <thread/pt.h>

#define STORE_VEC3_16(target) target[0] = READ_INT16(self->buf); target[1] = READ_INT16(self->buf + 2); target[2] = READ_INT16(self->buf + 4);

#define MPU6050_IO_TIMEOUT 500000

typedef struct mpu6050 mpu6050_t; 

ASYNC_PROCESS(mpu6050_task){
	struct mpu6050 *self = container_of(__self, struct mpu6050, process); 
	static const struct _init {
		uint8_t reg; 
		uint16_t value; 
	} init_sequence[] = {
		{ 0xff, 50000 },
		{ MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_CLOCK_PLL_XGYRO },
		{ 0xff, 10000 },
		{ MPU6050_RA_CONFIG, MPU6050_CFG_DLPF_BW_42 },
		{ MPU6050_RA_SMPLRT_DIV, 4 },
		{ MPU6050_RA_GYRO_CONFIG, MPU6050_GYRO_CONFIG_FS_2000 }, 
		{ MPU6050_RA_ACCEL_CONFIG, MPU6050_ACCEL_CONFIG_FS_2 }, 
		{ MPU6050_RA_INT_PIN_CFG, MPU6050_INTCFG_I2C_BYPASS_EN }, 
		{ MPU6050_RA_USER_CTRL, 0 }, 
	}; 
	
	ASYNC_BEGIN(); 
	
	IO_OPEN(self->dev); 
	
	for(self->count = 0; self->count < (sizeof(init_sequence) / sizeof(init_sequence[0])); self->count++){
		if(init_sequence[self->count].reg == 0xff) {
			AWAIT_DELAY(self->time, init_sequence[self->count].value); 
		} else {
			self->buf[0] = init_sequence[self->count].value; 
			IO_SEEK(self->dev, init_sequence[self->count].reg, SEEK_SET); 
			IO_WRITE(self->dev, self->buf, 1); 
		}
	}
	IO_CLOSE(self->dev); 
	
	self->state |= MPU6050_STATE_INITIALIZED; 
	
	// get the data from sensor at regular intervals
	
	while(1){
		static timestamp_t time = 0; 
		time = timestamp_now(); 
		//self->time = timestamp_now(); 
		IO_OPEN(self->dev); 
		
		static const uint8_t regs[] = { MPU6050_RA_ACCEL_XOUT_H, MPU6050_RA_GYRO_XOUT_H}; 
		for(self->count = 0; self->count < 2; self->count ++){
			IO_SEEK(self->dev, regs[self->count], SEEK_SET); 
			IO_READ(self->dev, self->buf, 6); 
			STORE_VEC3_16((self->raw + (self->count * 3))); 
		}
		
		IO_CLOSE(self->dev); 
		
		time = timestamp_ticks_to_us(timestamp_now() - time); 
		//printf("MPU read: %luus\n", (uint32_t)time); 
		
		static timestamp_t tfps = 0; 
		static int fps = 0; 
		if(timestamp_expired(tfps)){
			printf("MPU FPS: %d\n", fps); 
			fps = 0; 
			tfps = timestamp_from_now_us(1000000); 
		} fps++; 
		
		//TIMEOUT(10000); 
		
		//printf("MPU: %d %d %d %d %d %d\n", self->raw[0], self->raw[1], self->raw[2], 
		//	self->raw[3], self->raw[4], self->raw[5]); 
		
	}
	
	ASYNC_END(0); 
}

void mpu6050_init(struct mpu6050 *self, io_dev_t dev) {
	self->dev = dev; 
	self->state = 0; 
	
	ASYNC_PROCESS_INIT(&self->process, mpu6050_task); 
	ASYNC_QUEUE_WORK(&ASYNC_GLOBAL_QUEUE, &self->process); 
}

void mpu6050_deinit(struct mpu6050 *self){
	(void)self; 
	//libk_unlink_thread(&self->kthread);
}

/*
void mpu6050_update(struct mpu6050 *self){
	if(!(self->state & MPU6050_STATE_INITIALIZED))
		_mpu6050_init_thread(self); 
	else 
		_mpu6050_update_thread(self); 
}*/

//********************
// IMMEDIATE METHODS
//********************


void mpu6050_readRawAcc(struct mpu6050 *self, int16_t* ax, int16_t* ay, int16_t* az){
	*ax = self->raw[0]; 
	*ay = self->raw[1]; 
	*az = self->raw[2]; 
}

void mpu6050_readRawGyr(struct mpu6050 *self, int16_t* gx, int16_t* gy, int16_t* gz){
	*gx = self->raw[3]; 
	*gy = self->raw[4]; 
	*gz = self->raw[5]; 
}

void mpu6050_convertAcc(struct mpu6050 *self, int16_t ax, int16_t ay, int16_t az, float *axg, float *ayg, float *azg){
	(void)(self); 
	*axg = (float)(ax-MPU6050_AXOFFSET)/MPU6050_AXGAIN;
	*ayg = (float)(ay-MPU6050_AYOFFSET)/MPU6050_AYGAIN;
	*azg = (float)(az-MPU6050_AZOFFSET)/MPU6050_AZGAIN;
}

void mpu6050_convertGyr(struct mpu6050 *self, int16_t gx, int16_t gy, int16_t gz, float *gxd, float *gyd, float *gzd){
	(void)(self); 
	*gxd = (float)(gx-MPU6050_GXOFFSET)/MPU6050_GXGAIN;
	*gyd = (float)(gy-MPU6050_GYOFFSET)/MPU6050_GYGAIN;
	*gzd = (float)(gz-MPU6050_GZOFFSET)/MPU6050_GZGAIN;
}


/*
static int8_t mpu6050_readBytes(struct mpu6050 *self, uint8_t regAddr, uint8_t length, uint8_t *data) {
	i2c_start_write(self->i2c, self->addr, &regAddr, 1);
	delay_us(10);
	i2c_start_read(self->i2c, self->addr, data, length);
	i2c_stop(self->i2c); 
	return length; 
}

static int8_t mpu6050_readByte(struct mpu6050 *self, uint8_t regAddr, uint8_t *data) {
    return mpu6050_readBytes(self, regAddr, 1, data);
}

static void mpu6050_writeBytes(struct mpu6050 *self, uint8_t regAddr, uint8_t length, uint8_t* data) {
	uint8_t wr[16] = {regAddr};
	length &= 0x0f; 
	memcpy(&wr[1], data, length); 
	
	if(length > 0) {
		i2c_start_write(self->i2c, self->addr, wr, length + 1);
		i2c_stop(self->i2c); 
	}
}

static void mpu6050_writeByte(struct mpu6050 *self, uint8_t regAddr, uint8_t data) {
  return mpu6050_writeBytes(self, regAddr, 1, &data);
}

static int8_t mpu6050_readBits(struct mpu6050 *self, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t *data) {
    // 01101001 read byte
    // 76543210 bit numbers
    //    xxx   args: bitStart=4, length=3
    //    010   masked
    //   -> 010 shifted
    int8_t count = 0;
    if(length > 0) {
		uint8_t b;
		if ((count = mpu6050_readByte(self, regAddr, &b)) != 0) {
			uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
			b &= mask;
			b >>= (bitStart - length + 1);
			*data = b;
		}
    }
    return count;
}

static void mpu6050_writeBits(struct mpu6050 *self, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t data) {
    //      010 value to write
    // 76543210 bit numbers
    //    xxx   args: bitStart=4, length=3
    // 00011100 mask byte
    // 10101111 original value (sample)
    // 10100011 original & ~mask
    // 10101011 masked | value
	if(length > 0) {
		uint8_t b = 0;
		if (mpu6050_readByte(self, regAddr, &b) != 0) { //get current data
			uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
			data <<= (bitStart - length + 1); // shift data into correct position
			data &= mask; // zero all non-important bits in data
			b &= ~(mask); // zero all important bits in existing byte
			b |= data; // combine data with existing byte
			mpu6050_writeByte(self, regAddr, b);
		}
	}
}

static void mpu6050_writeBit(struct mpu6050 *self, uint8_t regAddr, uint8_t bitNum, uint8_t data) {
	uint8_t b;
	mpu6050_readByte(self, regAddr, &b);
	b = (data != 0) ? (b | (1 << bitNum)) : (b & ~(1 << bitNum));
	mpu6050_writeByte(self, regAddr, b);
}

static PT_THREAD(mpu6050_writeBit(struct mpu6050 *self, uint8_t regAddr, uint8_t bitNum, uint8_t data)) {
	uint8_t b;
	mpu6050_readByte(self, regAddr, &b);
	b = (data != 0) ? (b | (1 << bitNum)) : (b & ~(1 << bitNum));
	mpu6050_writeByte(self, regAddr, b);
}*/
/*
int8_t mpu6050_getTCXGyroOffset(struct mpu6050 *self) {
	mpu6050_readBits(self, MPU6050_RA_XG_OFFS_TC, MPU6050_TC_OFFSET_BIT, MPU6050_TC_OFFSET_LENGTH, buffer);
    return buffer[0];
}

void mpu6050_setTCXGyroOffset(struct mpu6050 *self, int8_t offset) {
	mpu6050_writeBits(self, MPU6050_RA_XG_OFFS_TC, MPU6050_TC_OFFSET_BIT, MPU6050_TC_OFFSET_LENGTH, offset);
}

int8_t mpu6050_getTCYGyroOffset(struct mpu6050 *self) {
	mpu6050_readBits(self, MPU6050_RA_YG_OFFS_TC, MPU6050_TC_OFFSET_BIT, MPU6050_TC_OFFSET_LENGTH, (uint8_t *)buffer);
    return buffer[0];
}

void mpu6050_setTCYGyroOffset(struct mpu6050 *self, int8_t offset) {
	mpu6050_writeBits(self, MPU6050_RA_YG_OFFS_TC, MPU6050_TC_OFFSET_BIT, MPU6050_TC_OFFSET_LENGTH, offset);
}

int8_t mpu6050_getTCZGyroOffset(struct mpu6050 *self) {
	mpu6050_readBits(self, MPU6050_RA_ZG_OFFS_TC, MPU6050_TC_OFFSET_BIT, MPU6050_TC_OFFSET_LENGTH, (uint8_t *)buffer);
    return buffer[0];
}

void mpu6050_setTCZGyroOffset(struct mpu6050 *self, int8_t offset) {
	mpu6050_writeBits(self, MPU6050_RA_ZG_OFFS_TC, MPU6050_TC_OFFSET_BIT, MPU6050_TC_OFFSET_LENGTH, offset);
}

int16_t mpu6050_getXGyroOffset(struct mpu6050 *self) {
	uint8_t buf[2]; 
	mpu6050_readByte(self, MPU6050_RA_XG_OFFS_USRH, &buf[0]);
	mpu6050_readByte(self, MPU6050_RA_XG_OFFS_USRL, &buf[1]);
  return (((int16_t)buf[0]) << 8) | buf[1]; 
}

void mpu6050_setXGyroOffset(struct mpu6050 *self, int16_t offset) {
	uint8_t buf[2] = {offset >> 8, offset}; 
	mpu6050_writeByte(self, MPU6050_RA_XG_OFFS_USRH, buf[0]);
	mpu6050_writeByte(self, MPU6050_RA_XG_OFFS_USRL, buf[1]);
}

int16_t mpu6050_getYGyroOffset(struct mpu6050 *self) {
	uint8_t buf[2]; 
	mpu6050_readByte(self, MPU6050_RA_YG_OFFS_USRH, &buf[0]);
	mpu6050_readByte(self, MPU6050_RA_YG_OFFS_USRL, &buf[1]);
  return (((int16_t)buf[0]) << 8) | buf[1]; 
}

void mpu6050_setYGyroOffset(struct mpu6050 *self, int16_t offset) {
	uint8_t buf[2] = {offset >> 8, offset}; 
	mpu6050_writeByte(self, MPU6050_RA_YG_OFFS_USRH, buf[0]);
	mpu6050_writeByte(self, MPU6050_RA_YG_OFFS_USRL, buf[1]);
}

int16_t mpu6050_getZGyroOffset(struct mpu6050 *self) {
	uint8_t buf[2]; 
	mpu6050_readByte(self, MPU6050_RA_ZG_OFFS_USRH, &buf[0]);
	mpu6050_readByte(self, MPU6050_RA_ZG_OFFS_USRL, &buf[1]);
  return (((int16_t)buf[0]) << 8) | buf[1]; 
}

void mpu6050_setZGyroOffset(struct mpu6050 *self, int16_t offset) {
	uint8_t buf[2] = {offset >> 8, offset}; 
	mpu6050_writeByte(self, MPU6050_RA_ZG_OFFS_USRH, buf[0]);
	mpu6050_writeByte(self, MPU6050_RA_ZG_OFFS_USRL, buf[1]);
}
*/
/*** ACCELEROMETER OFFSETS ***/
/*
int16_t mpu6050_getXAccOffset(struct mpu6050 *self) {
	uint8_t buf[2]; 
	mpu6050_readByte(self, MPU6050_RA_XA_OFFS_H, &buf[0]);
	mpu6050_readByte(self, MPU6050_RA_XA_OFFS_L_TC, &buf[1]);
  return (((int16_t)buf[0]) << 8) | buf[1]; 
}

void mpu6050_setXAccOffset(struct mpu6050 *self, int16_t offset) {
	uint8_t buf[2] = {offset >> 8, offset}; 
	mpu6050_writeByte(self, MPU6050_RA_XA_OFFS_H, buf[0]);
	mpu6050_writeByte(self, MPU6050_RA_XA_OFFS_L_TC, buf[1]);
}

int16_t mpu6050_getYAccOffset(struct mpu6050 *self) {
	uint8_t buf[2]; 
	mpu6050_readByte(self, MPU6050_RA_YA_OFFS_H, &buf[0]);
	mpu6050_readByte(self, MPU6050_RA_YA_OFFS_L_TC, &buf[1]);
  return (((int16_t)buf[0]) << 8) | buf[1]; 
}

void mpu6050_setYAccOffset(struct mpu6050 *self, int16_t offset) {
	uint8_t buf[2] = {offset >> 8, offset}; 
	mpu6050_writeByte(self, MPU6050_RA_YA_OFFS_H, buf[0]);
	mpu6050_writeByte(self, MPU6050_RA_YA_OFFS_L_TC, buf[1]);
}

int16_t mpu6050_getZAccOffset(struct mpu6050 *self) {
	uint8_t buf[2]; 
	mpu6050_readByte(self, MPU6050_RA_ZA_OFFS_H, &buf[0]);
	mpu6050_readByte(self, MPU6050_RA_ZA_OFFS_L_TC, &buf[1]);
  return (((int16_t)buf[0]) << 8) | buf[1]; 
}

void mpu6050_setZAccOffset(struct mpu6050 *self, int16_t offset) {
	uint8_t buf[2] = {offset >> 8, offset}; 
	mpu6050_writeByte(self, MPU6050_RA_ZA_OFFS_H, buf[0]);
	mpu6050_writeByte(self, MPU6050_RA_ZA_OFFS_L_TC, buf[1]);
}

static void mpu6050_setSleepDisabled(struct mpu6050 *self) {
	mpu6050_writeBit(self, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_SLEEP_BIT, 0);
}

uint8_t mpu6050_probe(struct mpu6050 *self) {
	mpu6050_readBits(self, MPU6050_RA_WHO_AM_I, MPU6050_WHO_AM_I_BIT, MPU6050_WHO_AM_I_LENGTH, (uint8_t *)buffer);
	if(buffer[0] == 0x34)
		return 1;
	else
		return 0;
}
*/

/*
 * initialize the accel and gyro
 */
 /*
void mpu6050_init(struct mpu6050 *self, i2c_dev_t i2c, uint8_t addr) {
	self->i2c = i2c; 
	self->addr = addr;
	self->flags = 0; 
	
	PT_INIT(&self->uthread); // update
	PT_INIT(&self->rthread); // read
	PT_INIT(&self->wthread); // write
	PT_INIT(&self->ithread); // init
	
	//allow mpu6050 chip clocks to start up
	delay_us(100000L);

	//set sleep disabled
	mpu6050_writeBit(self, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_SLEEP_BIT, 0);
	
	//wake up delay needed sleep disabled
	delay_us(10000L);
	
	//set clock source
	//  it is highly recommended that the device be configured to use one of the gyroscopes 
	// (or an external clock source) as the clock reference for improved stability
	mpu6050_writeBits(self, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_CLKSEL_BIT, MPU6050_PWR1_CLKSEL_LENGTH, MPU6050_CLOCK_PLL_XGYRO);
	//set DLPF bandwidth to 42Hz
	mpu6050_writeBits(self, MPU6050_RA_CONFIG, MPU6050_CFG_DLPF_CFG_BIT, MPU6050_CFG_DLPF_CFG_LENGTH, MPU6050_DLPF_BW_42);
    //set sampe rate
	mpu6050_writeByte(self, MPU6050_RA_SMPLRT_DIV, 4); //1khz / (1 + 4) = 200Hz
	//set gyro range
	mpu6050_writeBits(self, MPU6050_RA_GYRO_CONFIG, MPU6050_GCONFIG_FS_SEL_BIT, MPU6050_GCONFIG_FS_SEL_LENGTH, MPU6050_GYRO_FS);
	//set accel range
	mpu6050_writeBits(self, MPU6050_RA_ACCEL_CONFIG, MPU6050_ACONFIG_AFS_SEL_BIT, MPU6050_ACONFIG_AFS_SEL_LENGTH, MPU6050_ACCEL_FS);
	
	//enable i2c bypass by default for sensors connected to aux i2c bus
	mpu6050_writeBit(self, MPU6050_RA_INT_PIN_CFG, MPU6050_INTCFG_I2C_BYPASS_EN_BIT, (1 << MPU6050_INTCFG_I2C_BYPASS_EN_BIT)); 
	// disable master mode on the i2c aux bus
	mpu6050_writeBit(self, MPU6050_RA_USER_CTRL, 5, 0); 
	
	#if MPU6050_GETATTITUDE == 1
	#error "Do not enable timer 0 it is in use elsewhere!"
	//MPU6050_TIMER0INIT
	#endif
}*/

