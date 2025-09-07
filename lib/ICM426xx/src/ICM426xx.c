#include "main.h"
#include "spi.h"
#include <string.h>
#include <math.h>
#include "ICM426xx.h"



static void cs_low(void);
static void cs_high(void);

static void write_single_ICM426xx_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { WRITE | reg, val };
    cs_low();
    HAL_SPI_Transmit(ICM426xx_SPI, tx, 2, HAL_MAX_DELAY);
    cs_high();
}

static uint8_t read_single_ICM426xx_reg(uint8_t reg)
{
    uint8_t tx = READ | reg;
    uint8_t rx = 0;
    cs_low();
    HAL_SPI_Transmit(ICM426xx_SPI, &tx, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(ICM426xx_SPI, &rx, 1, HAL_MAX_DELAY);
    cs_high();
    return rx;
}

static void read_multi(uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t tx = READ | reg;
    cs_low();
    HAL_SPI_Transmit(ICM426xx_SPI, &tx, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(ICM426xx_SPI, buf, len, HAL_MAX_DELAY);
    cs_high();
}

// ---- Bank switching ----
static void set_bank(uint8_t bank)
{
    write_single_ICM426xx_reg(0x76, bank); // REG_BANK_SEL
}

static void cs_low(void)  { HAL_GPIO_WritePin(ICM426xx_SPI_CS_PIN_PORT, ICM426xx_SPI_CS_PIN_NUMBER, GPIO_PIN_RESET); }
static void cs_high(void) { HAL_GPIO_WritePin(ICM426xx_SPI_CS_PIN_PORT, ICM426xx_SPI_CS_PIN_NUMBER, GPIO_PIN_SET); }


// ---- Initialization ----
void ICM426xx_init(void)
{
    // Reset + bank select
    write_single_ICM426xx_reg(B0_DEVICE_CONFIG, 0x01);
    HAL_Delay(10);
    write_single_ICM426xx_reg(B0_REG_BANK_SEL, 0x00);

    // Power accel + gyro
    write_single_ICM426xx_reg(B0_PWR_MGMT0, 0x0A);
    write_single_ICM426xx_reg(B0_ACCEL_CONFIG0, 0x09); // accel ODR=50 Hz, ±2g
    write_single_ICM426xx_reg(B0_GYRO_CONFIG0, 0x09); // gyro ODR=50 Hz, ±250 dps

    // INT1: active high, push-pull, pulsed
    // bit2 = 0 (pulsed)
    // bit1 = 1 (push-pull)
    // bit0 = 1 (active high)
    write_single_ICM426xx_reg(B0_INT_CONFIG, 0x03);

    write_single_ICM426xx_reg(B0_INT_CONFIG0, 0x00);
    write_single_ICM426xx_reg(B0_INT_CONFIG1, 0x00);

    // Enable WOM interrupt routing
    write_single_ICM426xx_reg(B0_INT_SOURCE0, 0x00);
    write_single_ICM426xx_reg(B0_INT_SOURCE1, 0x0F);
    write_single_ICM426xx_reg(B0_INT_SOURCE2, 0x00);
    write_single_ICM426xx_reg(B0_INT_SOURCE3, 0x00);

    // WOM config: differential mode, accel enabled
	write_single_ICM426xx_reg(B0_SMD_CONFIG, 0x0B);  
    HAL_Delay(10);

    // Switch to Bank 4 for WOM thresholds
    write_single_ICM426xx_reg(B0_REG_BANK_SEL, 0x04);
    write_single_ICM426xx_reg(0x4A, 0xFF);
    write_single_ICM426xx_reg(0x4B, 0x30);
    write_single_ICM426xx_reg(0x4C, 0xFF);
    HAL_Delay(10);

    // Back to Bank 0
    write_single_ICM426xx_reg(B0_REG_BANK_SEL, 0x00);

    HAL_Delay(50);
}


float ICM426xx_testReturn(void) {
    int16_t ax_raw = (read_single_ICM426xx_reg(B0_ACCEL_DATA_X1_UI) << 8) |
                     read_single_ICM426xx_reg(B0_ACCEL_DATA_X0_UI);
    return (float)ax_raw / accel_lsb_per_g;
}


static ICM426xx_Sample sample;

uint8_t ICM426xx_interruptStatus(void)
{
    // INT_STATUS (Bank 0, 0x2D) holds WOM / SMD / FIFO / Data Ready flags
    uint8_t status = read_single_ICM426xx_reg(B0_INT_STATUS);

    // Reading INT_STATUS clears the latched interrupt
    return status;
}


// ---- Main loop: read + parse one packet ----
void ICM426xx_loop(void) {
    uint8_t buf[12];

    // Burst read accel + gyro: 0x1F = ACCEL_DATA_X1_UI
    // Register map:
    // 0x1F: ACCEL_X1, 0x20: ACCEL_X0
    // 0x21: ACCEL_Y1, 0x22: ACCEL_Y0
    // 0x23: ACCEL_Z1, 0x24: ACCEL_Z0
    // 0x25: GYRO_X1,  0x26: GYRO_X0
    // 0x27: GYRO_Y1,  0x28: GYRO_Y0
    // 0x29: GYRO_Z1,  0x2A: GYRO_Z0
    read_multi(0x1F, buf, 12);

    int16_t ax = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t ay = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t az = (int16_t)((buf[4] << 8) | buf[5]);

    int16_t gx = (int16_t)((buf[6] << 8) | buf[7]);
    int16_t gy = (int16_t)((buf[8] << 8) | buf[9]);
    int16_t gz = (int16_t)((buf[10] << 8) | buf[11]);

    // Convert to float using sensitivity constants
    sample.ax = (float)ax / accel_lsb_per_g;       // ±2g → 16384 LSB/g
    sample.ay = (float)ay / accel_lsb_per_g;
    sample.az = (float)az / accel_lsb_per_g;

    sample.gx = (float)gx / gyro_lsb_per_dps;      // ±250 dps → 131 LSB/dps
    sample.gy = (float)gy / gyro_lsb_per_dps;
    sample.gz = (float)gz / gyro_lsb_per_dps;

    // No timestamp in this mode, so just use system tick
    sample.ts = HAL_GetTick();
}

// ---- Accessor ----
ICM426xx_Sample ICM426xx_value(void)
{
	
    return sample;
}
