/*
* ICM426xx.h
*
*  Created on: Dec 26, 2020
*      Author: mokhwasomssi
*/


#ifndef __ICM426xx_H__
#define	__ICM426xx_H__


#include "spi.h"			// header from stm32cubemx code generate
#include <stdbool.h>

/* User Configuration */
#define ICM426xx_SPI					(&hspi1)

#define ICM426xx_SPI_CS_PIN_PORT		GPIOA
#define ICM426xx_SPI_CS_PIN_NUMBER		GPIO_PIN_4

// ---- Struct to hold parsed values ----
typedef struct {
    float ax, ay, az;   // accel in g
    float gx, gy, gz;   // gyro in dps
    uint32_t ts;        // timestamp (20-bit, in sample ticks)
} ICM426xx_Sample;

ICM426xx_Sample ICM426xx_value(void);
void ICM426xx_loop(void);
// ---- Scaling factors (depends on FSR) ----
static const  float accel_lsb_per_g = 16384.0f; // for ±2g
static const  float gyro_lsb_per_dps = 131.0f;  // for ±250 dps

// ---- SPI helpers ----
#define READ  0x80
#define WRITE 0x00

/* Typedefs */
typedef enum
{
	ub_0 = 0 << 4,
	ub_1 = 1 << 4,
	ub_2 = 2 << 4,
	ub_3 = 3 << 4
} userbank;

typedef enum
{
	_250dps,
	_500dps,
	_1000dps,
	_2000dps
} gyro_full_scale;

typedef enum
{
	_2g,
	_4g,
	_8g,
	_16g
} accel_full_scale;

typedef struct
{
	float x;
	float y;
	float z;
} axises;

typedef enum
{
	power_down_mode = 0,
	single_measurement_mode = 1,
	continuous_measurement_10hz = 2,
	continuous_measurement_20hz = 4,
	continuous_measurement_50hz = 6,
	continuous_measurement_100hz = 8
} operation_mode;


/* Main Functions */

// sensor init function.
// if sensor id is wrong, it is stuck in while.
void ICM426xx_init();
void ICM426xx_hard_reset();

// 16 bits ADC value. raw data.
void ICM426xx_gyro_read(axises* data);	
void ICM426xx_accel_read(axises* data);

// Convert 16 bits ADC value to their unit.
void ICM426xx_gyro_read_dps(axises* data); 
void ICM426xx_accel_read_g(axises* data);
void ICM426xx_setInterrupt();
uint8_t ICM426xx_interruptStatus(void);

/* Sub Functions */
bool ICM426xx_who_am_i();
uint8_t ICM426xx_get_device_id();





#define ICM426xx_ID 0x5C

/* -------------------- User Bank 0 -------------------- */
#define B0_DEVICE_CONFIG        0x11
#define B0_DRIVE_CONFIG         0x13
#define B0_INT_CONFIG           0x14
#define B0_FIFO_CONFIG          0x16
#define B0_TEMP_DATA1_UI        0x1D
#define B0_TEMP_DATA0_UI        0x1E
#define B0_ACCEL_DATA_X1_UI     0x1F
#define B0_ACCEL_DATA_X0_UI     0x20
#define B0_ACCEL_DATA_Y1_UI     0x21
#define B0_ACCEL_DATA_Y0_UI     0x22
#define B0_ACCEL_DATA_Z1_UI     0x23
#define B0_ACCEL_DATA_Z0_UI     0x24
#define B0_GYRO_DATA_X1_UI      0x25
#define B0_GYRO_DATA_X0_UI      0x26
#define B0_GYRO_DATA_Y1_UI      0x27
#define B0_GYRO_DATA_Y0_UI      0x28
#define B0_GYRO_DATA_Z1_UI      0x29
#define B0_GYRO_DATA_Z0_UI      0x2A
#define B0_TMST_FSYNCH          0x2B
#define B0_TMST_FSYNCL          0x2C
#define B0_INT_STATUS           0x2D
#define B0_FIFO_COUNTH          0x2E
#define B0_FIFO_COUNTL          0x2F
#define B0_FIFO_DATA            0x30
#define B0_APEX_DATA0           0x31
#define B0_APEX_DATA1           0x32
#define B0_APEX_DATA2           0x33
#define B0_APEX_DATA3           0x34
#define B0_APEX_DATA4           0x35
#define B0_APEX_DATA5           0x36
#define B0_INT_STATUS2          0x37
#define B0_INT_STATUS3          0x38
#define B0_SIGNAL_PATH_RESET    0x4B
#define B0_INTF_CONFIG0         0x4C
#define B0_INTF_CONFIG1         0x4D
#define B0_PWR_MGMT0            0x4E
#define B0_GYRO_CONFIG0         0x4F
#define B0_ACCEL_CONFIG0        0x50
#define B0_GYRO_CONFIG1         0x51
#define B0_GYRO_ACCEL_CONFIG0   0x52
#define B0_ACCEL_CONFIG1        0x53
#define B0_TMST_CONFIG          0x54
#define B0_APEX_CONFIG0         0x56
#define B0_SMD_CONFIG           0x57
#define B0_FIFO_CONFIG1         0x5F
#define B0_FIFO_CONFIG2         0x60
#define B0_FIFO_CONFIG3         0x61
#define B0_FSYNC_CONFIG         0x62
#define B0_INT_CONFIG0          0x63
#define B0_INT_CONFIG1          0x64
#define B0_INT_SOURCE0          0x65
#define B0_INT_SOURCE1          0x66
#define B0_INT_SOURCE2          0x67
#define B0_INT_SOURCE3          0x68
#define B0_INT_SOURCE4          0x69
#define B0_INT_SOURCE5          0x6A
#define B0_FIFO_LOST_PKT0       0x6C
#define B0_FIFO_LOST_PKT1       0x6D
#define B0_SELF_TEST_CONFIG     0x70
#define B0_WHO_AM_I             0x75
#define B0_REG_BANK_SEL         0x76

/* -------------------- User Bank 1 -------------------- */
#define B1_SENSOR_CONFIG0       0x03
#define B1_SENSOR_CONFIG1       0x04
#define B1_GYRO_CONFIG_STATIC2  0x0B
#define B1_GYRO_CONFIG_STATIC3  0x0C
#define B1_GYRO_CONFIG_STATIC4  0x0D
#define B1_GYRO_CONFIG_STATIC5  0x0E
#define B1_GYRO_CONFIG_STATIC6  0x0F
#define B1_GYRO_CONFIG_STATIC7  0x10
#define B1_GYRO_CONFIG_STATIC8  0x11
#define B1_GYRO_CONFIG_STATIC9  0x12
#define B1_GYRO_CONFIG_STATIC10 0x13
#define B1_XG_ST_DATA           0x5F
#define B1_YG_ST_DATA           0x60
#define B1_ZG_ST_DATA           0x61
#define B1_TMSTVAL0             0x62
#define B1_TMSTVAL1             0x63
#define B1_TMSTVAL2             0x64
#define B1_INTF_CONFIG3         0x79
#define B1_INTF_CONFIG4         0x7A
#define B1_INTF_CONFIG5         0x7B
#define B1_INTF_CONFIG6         0x7C

/* -------------------- User Bank 2 -------------------- */
#define B2_ACCEL_CONFIG_STATIC2 0x03
#define B2_ACCEL_CONFIG_STATIC3 0x04
#define B2_ACCEL_CONFIG_STATIC4 0x05

#define B2_XA_ST_DATA           0x0E
#define B2_YA_ST_DATA           0x0F
#define B2_ZA_ST_DATA           0x10

#define B2_AUX1_CONFIG1         0x11
#define B2_AUX1_CONFIG2         0x12
#define B2_AUX1_CONFIG3         0x13

#define B2_TEMP_DATA1_AUX1      0x18
#define B2_TEMP_DATA0_AUX1      0x19
#define B2_ACCEL_DATA_X1_AUX1   0x1A
#define B2_ACCEL_DATA_X0_AUX1   0x1B
#define B2_ACCEL_DATA_Y1_AUX1   0x1C
#define B2_ACCEL_DATA_Y0_AUX1   0x1D
#define B2_ACCEL_DATA_Z1_AUX1   0x1E
#define B2_ACCEL_DATA_Z0_AUX1   0x1F
#define B2_GYRO_DATA_X1_AUX1    0x20
#define B2_GYRO_DATA_X0_AUX1    0x21
#define B2_GYRO_DATA_Y1_AUX1    0x22
#define B2_GYRO_DATA_Y0_AUX1    0x23
#define B2_GYRO_DATA_Z1_AUX1    0x24
#define B2_GYRO_DATA_Z0_AUX1    0x25
#define B2_TMSTVAL0_AUX1        0x26
#define B2_TMSTVAL1_AUX1        0x27
#define B2_INT_STATUS_AUX1      0x28

#define B2_AUX2_CONFIG1         0x29
#define B2_AUX2_CONFIG2         0x2A
#define B2_AUX2_CONFIG3         0x2B

#define B2_TEMP_DATA1_AUX2      0x30
#define B2_TEMP_DATA0_AUX2      0x31
#define B2_ACCEL_DATA_X1_AUX2   0x32
#define B2_ACCEL_DATA_X0_AUX2   0x33
#define B2_ACCEL_DATA_Y1_AUX2   0x34
#define B2_ACCEL_DATA_Y0_AUX2   0x35
#define B2_ACCEL_DATA_Z1_AUX2   0x36
#define B2_ACCEL_DATA_Z0_AUX2   0x37
#define B2_GYRO_DATA_X1_AUX2    0x38
#define B2_GYRO_DATA_X0_AUX2    0x39
#define B2_GYRO_DATA_Y1_AUX2    0x3A
#define B2_GYRO_DATA_Y0_AUX2    0x3B
#define B2_GYRO_DATA_Z1_AUX2    0x3C
#define B2_GYRO_DATA_Z0_AUX2    0x3D
#define B2_TMSTVAL0_AUX2        0x3E
#define B2_TMSTVAL1_AUX2        0x3F
#define B2_INT_STATUS_AUX2      0x40

#define B2_TMD4                 0x41
#define B2_TMD5                 0x42
#define B2_TMD6                 0x43
#define B2_TMD7                 0x44

/* -------------------- User Bank 3 -------------------- */
#define B3_PU_PD_CONFIG1        0x06
#define B3_PU_PD_CONFIG2        0x0E

/* -------------------- User Bank 4 -------------------- */
#define B4_FDR_CONFIG           0x00
#define B4_APEX_CONFIG1         0x40
#define B4_APEX_CONFIG2         0x41
#define B4_APEX_CONFIG3         0x42
#define B4_APEX_CONFIG4         0x43
#define B4_APEX_CONFIG5         0x44
#define B4_APEX_CONFIG6         0x45
#define B4_APEX_CONFIG7         0x46
#define B4_APEX_CONFIG8         0x47
#define B4_APEX_CONFIG9         0x48
#define B4_APEX_CONFIG10        0x49
#define B4_ACCEL_WOM_X_THR      0x4A
#define B4_ACCEL_WOM_Y_THR      0x4B
#define B4_ACCEL_WOM_Z_THR      0x4C
#define B4_INT_SOURCE6          0x4D
#define B4_INT_SOURCE7          0x4E
#define B4_INT_SOURCE8          0x4F
#define B4_INT_SOURCE9          0x50
#define B4_INT_SOURCE10         0x51
#define B4_OFFSET_USER0         0x77
#define B4_OFFSET_USER1         0x78
#define B4_OFFSET_USER2         0x79
#define B4_OFFSET_USER3         0x7A
#define B4_OFFSET_USER4         0x7B
#define B4_OFFSET_USER5         0x7C
#define B4_OFFSET_USER6         0x7D
#define B4_OFFSET_USER7         0x7E
#define B4_OFFSET_USER8         0x7F

#endif	/* __ICM426xx_H__ */