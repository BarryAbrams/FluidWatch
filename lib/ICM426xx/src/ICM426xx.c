#include "ICM426xx.h"

/* Static Functions */
static void     cs_high();
static void     cs_low();

static void     select_user_bank(userbank ub);

static uint8_t  read_single_ICM426xx_reg(userbank ub, uint8_t reg);
static void     write_single_ICM426xx_reg(userbank ub, uint8_t reg, uint8_t val);
static uint8_t* read_multiple_ICM426xx_reg(userbank ub, uint8_t reg, uint8_t len);
static void     write_multiple_ICM426xx_reg(userbank ub, uint8_t reg, uint8_t* val, uint8_t len);


#define ICM426XX_WHOAMI_EXPECTED   0x5C
#define ICM426XX_RESET_DELAY_MS    100
#define ICM426XX_RESET_RETRIES     10

void ICM426xx_hard_reset(void)
{
    // Reset via DEVICE_CONFIG (safer soft reset)
    write_single_ICM426xx_reg(0, B0_DEVICE_CONFIG, 0x01); // SOFT_RESET
    HAL_Delay(10);

    // Ensure Bank 0
    write_single_ICM426xx_reg(0, B0_REG_BANK_SEL, 0x00);

    // Configure interface clock + wakeup
    write_single_ICM426xx_reg(0, B0_INTF_CONFIG1, 0x01);
    write_single_ICM426xx_reg(0, B0_PWR_MGMT0, 0x06);

    HAL_Delay(100);

    // Verify identity
    uint8_t whoami = read_single_ICM426xx_reg(0, B0_WHO_AM_I);
    if (whoami != ICM426XX_WHOAMI_EXPECTED) {
        // Handle error
    }
}

/* Main Functions */
void ICM426xx_init(void)
{
    // Reset + bank select
    write_single_ICM426xx_reg(0, B0_DEVICE_CONFIG, 0x01);
    HAL_Delay(10);
    write_single_ICM426xx_reg(0, B0_REG_BANK_SEL, 0x00);

    // Power accel + gyro
    write_single_ICM426xx_reg(0, B0_PWR_MGMT0, 0x06);
    write_single_ICM426xx_reg(0, B0_ACCEL_CONFIG0, 0x09); // accel ODR=50 Hz, ±2g
	write_single_ICM426xx_reg(0, B0_INT_CONFIG, 0x00); 
	write_single_ICM426xx_reg(0, B0_INT_CONFIG0, 0x00);
	write_single_ICM426xx_reg(0, B0_INT_CONFIG1, 0x00);

	write_single_ICM426xx_reg(0, B0_INT_SOURCE0, 0x00);
	write_single_ICM426xx_reg(0, B0_INT_SOURCE1, 0x0F);
	write_single_ICM426xx_reg(0, B0_INT_SOURCE2, 0x00);
	write_single_ICM426xx_reg(0, B0_INT_SOURCE3, 0x00);

	write_single_ICM426xx_reg(0, B0_SMD_CONFIG, 0x0A);
    HAL_Delay(10);

	write_single_ICM426xx_reg(0, B0_REG_BANK_SEL, 0x04);
	write_single_ICM426xx_reg(4, 0x4A, 0x30);
	write_single_ICM426xx_reg(4, 0x4B, 0x30);
	write_single_ICM426xx_reg(4, 0x4C, 0x30);
    HAL_Delay(10);

	// Back to Bank 0
	write_single_ICM426xx_reg(0, B0_REG_BANK_SEL, 0x00);

    HAL_Delay(50);
}

void ICM426xx_setInterrupt(void) {
	// Interrupt functionality is configured via the Interrupt Configuration register. Items that are configurable include the interrupt pins
	// configuration, the interrupt latching and clearing method, and triggers for the interrupt. Items that can trigger an interrupt are (1)
	// Clock generator locked to new reference oscillator (used when switching clock sources); (2) new data is available to be read (from
	// the FIFO and Data registers); (3) accelerometer event interrupts; (4) FIFO watermark; (5) FIFO overflow. The interrupt status can be
	// read from the Interrupt Status register. */



}

uint8_t ICM426xx_interruptStatus(void)
{
	// get the interrupt status
	uint8_t status = read_single_ICM426xx_reg(ub_0, B0_INT_STATUS);
	return status;	
}

/* Sub Functions */
bool ICM426xx_who_am_i()
{
	uint8_t ICM426xx_id = read_single_ICM426xx_reg(ub_0, B0_WHO_AM_I);

	if(ICM426xx_id == ICM426xx_ID)
		return true;
	else
		return false;
}

uint8_t ICM426xx_get_device_id()
{
	return read_single_ICM426xx_reg(ub_0, B0_WHO_AM_I);
}

void ICM426xx_device_reset()
{
	write_single_ICM426xx_reg(ub_0, B0_DEVICE_CONFIG, 0x80 | 0x01);
	HAL_Delay(100);
}

void ICM426xx_wakeup()
{
	uint8_t new_val = read_single_ICM426xx_reg(ub_0, B0_PWR_MGMT0);
	new_val &= 0xBF;

	write_single_ICM426xx_reg(ub_0, B0_PWR_MGMT0, new_val);
	HAL_Delay(100);
}

void ICM426xx_sleep()
{
	uint8_t new_val = read_single_ICM426xx_reg(ub_0, B0_PWR_MGMT0);
	new_val |= 0x40;

	write_single_ICM426xx_reg(ub_0, B0_PWR_MGMT0, new_val);
	HAL_Delay(100);
}

void ICM426xx_spi_slave_enable()
{
	uint8_t new_val = read_single_ICM426xx_reg(ub_0, B0_INTF_CONFIG0);
	new_val |= 0x10;

	write_single_ICM426xx_reg(ub_0, B0_INTF_CONFIG0, new_val);
}

void ICM426xx_clock_source(uint8_t source)
{
	uint8_t new_val = read_single_ICM426xx_reg(ub_0, B0_PWR_MGMT0);
	new_val |= source;

	write_single_ICM426xx_reg(ub_0, B0_PWR_MGMT0, new_val);
}

uint8_t ICM426xx_read_status(void)
{
    return read_single_ICM426xx_reg(ub_0, B0_INT_STATUS3);
}

/* Static Functions */
static void cs_high()
{
	HAL_GPIO_WritePin(ICM426xx_SPI_CS_PIN_PORT, ICM426xx_SPI_CS_PIN_NUMBER, SET);	
}

static void cs_low()
{
	HAL_GPIO_WritePin(ICM426xx_SPI_CS_PIN_PORT, ICM426xx_SPI_CS_PIN_NUMBER, RESET);
}

static void select_user_bank(userbank ub)
{
	uint8_t write_reg[2];
	write_reg[0] = WRITE | B0_REG_BANK_SEL;
	write_reg[1] = ub;

	cs_low();
	HAL_SPI_Transmit(ICM426xx_SPI, write_reg, 2, 10);
	cs_high();
}

static uint8_t read_single_ICM426xx_reg(userbank ub, uint8_t reg)
{
	uint8_t read_reg = READ | reg;
	uint8_t reg_val;
	select_user_bank(ub);

	cs_low();
	HAL_SPI_Transmit(ICM426xx_SPI, &read_reg, 1, 1000);
	HAL_SPI_Receive(ICM426xx_SPI, &reg_val, 1, 1000);
	cs_high();

	return reg_val;
}

static void write_single_ICM426xx_reg(userbank ub, uint8_t reg, uint8_t val)
{
	uint8_t write_reg[2];
	write_reg[0] = WRITE | reg;
	write_reg[1] = val;

	select_user_bank(ub);

	cs_low();
	HAL_SPI_Transmit(ICM426xx_SPI, write_reg, 2, 1000);
	cs_high();
}

static uint8_t* read_multiple_ICM426xx_reg(userbank ub, uint8_t reg, uint8_t len)
{
	uint8_t read_reg = READ | reg;
	static uint8_t reg_val[6];
	select_user_bank(ub);

	cs_low();
	HAL_SPI_Transmit(ICM426xx_SPI, &read_reg, 1, 1000);
	HAL_SPI_Receive(ICM426xx_SPI, reg_val, len, 1000);
	cs_high();

	return reg_val;
}

static void write_multiple_ICM426xx_reg(userbank ub, uint8_t reg, uint8_t* val, uint8_t len)
{
	uint8_t write_reg = WRITE | reg;
	select_user_bank(ub);

	cs_low();
	HAL_SPI_Transmit(ICM426xx_SPI, &write_reg, 1, 1000);
	HAL_SPI_Transmit(ICM426xx_SPI, val, len, 1000);
	cs_high();
}

