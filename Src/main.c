/*
 * Name: Bare_Metal_LEDMatrixTest
 * Purpose: To use the SPI Driver I wrote to interface an LED Matrix Display
 * Author: Tom Nguyen
 * Date: 5/3/2025
 */

#include <stdint.h>

#define RCC 			0x40023800
#define RCC_CR			(RCC + 0x00)
#define RCC_CFGR		(RCC + 0x08)
#define RCC_AHB1ENR		(RCC + 0x30)
#define RCC_APB1		(RCC + 0x40)
#define RCC_APB2ENR		(RCC + 0x44)
#define FLASH			0x40023C00
#define FLASH_ACR		(FLASH + 0x00)
#define TIM3		0x40000400
#define TIM3_CR1	(TIM3 + 0x00)
#define TIM3_DIER	(TIM3 + 0x0C)
#define TIM3_SR		(TIM3 + 0x10)
#define TIM3_CNT	(TIM3 + 0x24)
#define TIM3_PSC	(TIM3 + 0x28)
#define TIM3_ARR	(TIM3 + 0x2C)
#define SPI1			0x40013000
#define SPI1_CR1		(SPI1 + 0x00)
#define SPI1_CR2		(SPI1 + 0x04)
#define SPI1_SR			(SPI1 + 0x08)
#define SPI1_DR			(SPI1 + 0x0C)
#define GPIOA			0x40020000
#define GPIOA_MODER		(GPIOA + 0x00)
#define GPIOA_AFRL		(GPIOA + 0x20)
#define CS_Port			GPIOA
#define CS_Pin			4
#define CS_BSRR			(CS_Port + 0x18)

static void SetSystemClockto16MHz(void);
static void ConfigureTimer3(void);
static void Delay(uint32_t ms);
static void SPI1ClockEnable(void);
static void GPIOAClockEnable(void);
static void SPI1Init(void);
static void SPI1WriteToDR(uint16_t data);
static void WaitForTransmissionEnd(void);
static void EnableSlave(void);
static void DisableSlave(void);
static void SPI1_Transmit(uint16_t data);
static void SPI1PinsInit(void);
static void max7219_write(uint8_t addr, uint8_t data);
static void matrixClear(void);
static void matrixInit(void);
static uint8_t intToHexPosition(uint8_t val);
static void positionToMatrixPos(uint8_t x_pos[], uint8_t y_pos[], int numberOfCords, uint8_t outputArray[]);
static void LEDMatrixWrite(uint8_t outputArray[]);
static void LEDMatrixRowWrite(uint8_t outputArray[], uint8_t row);
static void LEDMatrixColumnWrite(uint8_t outputArray[], uint8_t col);

int main(void)
{
	SetSystemClockto16MHz();
	ConfigureTimer3();
	SPI1ClockEnable();
	GPIOAClockEnable();

	SPI1PinsInit();
	SPI1Init();

	matrixInit();

	// Write data here
	// Here is a drawing of a heart
	int numberOfCords = 16;
	uint8_t x_pos[] =
	{
	1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8
	};
	uint8_t y_pos[] =
	{
	5, 6, 4, 7, 3, 7, 2, 6, 2, 6, 3, 7, 4, 7, 5, 6
	};
	uint8_t outputArray[9] = {0}; // Make sure outputArray initialize to zeroes

	positionToMatrixPos(x_pos, y_pos, numberOfCords, outputArray); // Makes it easy for user

	while(1)	// Now, let's do a small show! This is the row, bottom to top version
	{
		for (volatile int i = 1; i <= 8; i++)
		{
			for (volatile int j = 1; j <= i; j++)
			{
				LEDMatrixRowWrite(outputArray, j);
			}
			Delay(500); // Half a second
		}
		matrixClear();
		Delay(500);
		LEDMatrixWrite(outputArray);
		Delay(500);
		matrixClear();
		Delay(1000);
	}

//	while(1)	// Now, this is the column, left to right version
//	{
//		for (volatile int i = 1; i <= 8; i++)
//		{
//			LEDMatrixColumnWrite(outputArray, i);
//			Delay(500);
//		}
//		Delay(500);
//		matrixClear();
//		Delay(500);
//		LEDMatrixWrite(outputArray);
//		Delay(500);
//		matrixClear();
//		Delay(1000);
//	}
}

void SetSystemClockto16MHz(void)
{
	// Initialize System Clock
	uint32_t *RCC_CR_Ptr = (uint32_t*)RCC_CR;
	// Turn on HSI
	*RCC_CR_Ptr |= (uint32_t)0x1;
	// Wait for HSI Clock to be ready
	while ((*RCC_CR_Ptr & 0x2) == 0);

	// Configure Prescalers
	uint32_t *RCC_CFGR_Ptr = (uint32_t*)RCC_CFGR;
	// HPRE
	*RCC_CFGR_Ptr &= ~(uint32_t)(0b1111 << 4);
	// PPRE1
	*RCC_CFGR_Ptr &= ~(uint32_t)(0b111 << 10);
	// PPRE2
	*RCC_CFGR_Ptr &= ~(uint32_t)(0b111 << 13);

	// Set HSI as Clock Source
	*RCC_CFGR_Ptr &= ~(uint32_t)(0b11);

	// Configure Flash
	uint32_t *FLASH_ACR_Ptr = (uint32_t*)FLASH_ACR;
	// Latency
	*FLASH_ACR_Ptr |= (uint32_t)(0b0000 << 0);
	// ICEN
	*FLASH_ACR_Ptr |= (uint32_t)(0b1 << 9);
	// DCEN
	*FLASH_ACR_Ptr |= (uint32_t)(0b1 << 10);

	// Turn off HSE
	*RCC_CR_Ptr &= ~((uint32_t)0x1 << 16);
}

void ConfigureTimer3(void)
{
	// Enable TIM3 Clock
	uint32_t *RCC_APB1_Ptr = (uint32_t*)RCC_APB1;
	*RCC_APB1_Ptr |= (uint32_t)0x2;

	// Set Prescaler
	uint32_t *TIM3_PSC_Ptr = (uint32_t*)TIM3_PSC;
	*TIM3_PSC_Ptr |= (uint32_t)0xF;

	// Set Auto-Reload
	uint32_t *TIM3_ARR_Ptr = (uint32_t*)TIM3_ARR;
	*TIM3_ARR_Ptr = (uint32_t)0x3E7;

//	// Enable Interrupt
//	uint32_t *TIM3_DIER_Ptr = (uint32_t*)TIM3_DIER;
//	*TIM3_DIER_Ptr |= (uint32_t)0x1;

	// Clear UIF Bit
	uint32_t *TIM3_SR_Ptr = (uint32_t*)TIM3_SR;
	*TIM3_SR_Ptr &= (uint32_t)0xFFFE;

//	// Enable NVIC Interrupt for Timer 3
//	NVIC_EnableIRQ(TIM3_IRQn);

	// Enable TIM3
	uint32_t *TIM3_CR1_Ptr = (uint32_t*)TIM3_CR1;
	*TIM3_CR1_Ptr = (uint32_t)0b1 << 0;
}

void Delay(uint32_t ms)
{
	volatile uint32_t i;
	uint32_t *TIM3_CNT_Ptr = (uint32_t*)TIM3_CNT;
	uint32_t *TIM3_SR_Ptr = (uint32_t*)TIM3_SR;
	for (i = 0; i <= ms; i++)
	{
		// Clear TIM3 Count
		*TIM3_CNT_Ptr = 0;

		// Wait for UIF (1 cycle of 1kHz clocking)
		while((*TIM3_SR_Ptr & 0x1) == 0);	// This will make a 1ms delay

		// Reset UIF
		*TIM3_SR_Ptr &= (uint32_t)0xFFFE;
	}
}

void SPI1ClockEnable(void)
{
	// First, SPI clock through APB2 Bus
	uint32_t *RCC_APB2ENR_Ptr = (uint32_t*)RCC_APB2ENR;
	*RCC_APB2ENR_Ptr |= (uint32_t)(0x1 << 12);
}

void GPIOAClockEnable(void)
{
	// Now, Enable GPIOA Clock through AHB1 Bus
	uint32_t *RCC_AHB1ENR_Ptr = (uint32_t*)RCC_AHB1ENR;
	*RCC_AHB1ENR_Ptr |= (uint32_t)0x1;
}

void SPI1Init(void)
{
	// Set Up SPI Init
	uint32_t *SPI1_CR1_Ptr = (uint32_t*)SPI1_CR1;

	// NOTE: Simplex is basically just full duplex but we don't use MISO

	// BIDIMODE off
	*SPI1_CR1_Ptr &= ~(uint32_t)(0x1 << 15);
	// CRC Calculations off
	*SPI1_CR1_Ptr &= ~(uint32_t)(0x1 << 13);
	// DFF to 16 bits
	*SPI1_CR1_Ptr |= (uint32_t)(0x1 << 11);
	// RXOnly off since we are transferring from master to slave
	*SPI1_CR1_Ptr &= ~(uint32_t)(0x1 << 10);
	// SSM Disabled
	// MSB Selected
	*SPI1_CR1_Ptr &= ~(uint32_t)(0x1 << 7);
	// Baud Rate of 2 MBits/S
	*SPI1_CR1_Ptr &= ~(uint32_t)(0b111 << 3);
	*SPI1_CR1_Ptr |= (uint32_t)(0b010 << 3);
	// Put into Master Mode
	*SPI1_CR1_Ptr |= (uint32_t)(0x1 << 2);
	// Set CPOL and CPHA
	*SPI1_CR1_Ptr &= ~(uint32_t)(0x3);

	// SSOE enabled
	uint32_t *SPI1_CR2_Ptr = (uint32_t*)SPI1_CR2;
	*SPI1_CR2_Ptr |= 0x4;

	// Finally, enable SPI
	*SPI1_CR1_Ptr |= (uint32_t)(0x1 << 6);
}

void SPI1WriteToDR(uint16_t data)
{
	// Load data into SPI1 data register
	uint32_t *SPI1_DR_Ptr = (uint32_t*)SPI1_DR;
	*SPI1_DR_Ptr = (uint32_t)data;
}

void WaitForTransmissionEnd(void)
{
	// Wait for transmission to end by checking BSY and TXE
	uint32_t *SPI1_SR_Ptr = (uint32_t*)SPI1_SR;
	while ((*SPI1_SR_Ptr & (0b1 << 7)) != 0);
	while ((*SPI1_SR_Ptr & (0b1 << 1)) == 0);
}

void EnableSlave(void)
{
	// Enable Slave
	uint32_t *CS_BSRR_Ptr = (uint32_t*)CS_BSRR;
	*CS_BSRR_Ptr |= (uint32_t)(0b1 << (CS_Pin + 16));
}

void DisableSlave(void)
{
	// Disable Slave
	uint32_t *CS_BSRR_Ptr = (uint32_t*)CS_BSRR;
	*CS_BSRR_Ptr |= (uint32_t)(0b1 << CS_Pin);
}

void SPI1_Transmit(uint16_t data)
{
	EnableSlave();
	SPI1WriteToDR(data);
	WaitForTransmissionEnd();
	DisableSlave();
}

void SPI1PinsInit(void)
{
	// Initialize SPI GPIO Pins
	// First, PinA5 for SCLK
	uint32_t *GPIOA_MODER_Ptr = (uint32_t*)GPIOA_MODER;
	// Set to Alternate Function
	*GPIOA_MODER_Ptr &= ~(uint32_t)(0b11 << 10);
	*GPIOA_MODER_Ptr |= (uint32_t)(0b10 << 10);
	// Next, PinA7 for MOSI
	*GPIOA_MODER_Ptr &= ~(uint32_t)(0b11 << 14);
	*GPIOA_MODER_Ptr |= (uint32_t)(0b10 << 14);

	// Set a GPIO Pin for CS Pin
	uint32_t *CS_Port_Ptr = (uint32_t*)CS_Port;
	// Set Pin 4 to output
	*CS_Port_Ptr &= ~(uint32_t)(0b11 << 2 * CS_Pin);
	*CS_Port_Ptr |= (uint32_t)(0b01 << 2 * CS_Pin);

	// Set up alternate function by selecting AF5 (According to datasheet)
	uint32_t *GPIOA_AFRL_Ptr = (uint32_t*)GPIOA_AFRL;
	*GPIOA_AFRL_Ptr |= (uint32_t)(0b0101 << 16);
	*GPIOA_AFRL_Ptr |= (uint32_t)(0b0101 << 20);
	*GPIOA_AFRL_Ptr |= (uint32_t)(0b0101 << 28);
	// Initialize to High
	DisableSlave();
}

void max7219_write(uint8_t addr, uint8_t data)
{
	uint16_t writeData = (addr << 8) | data;
	SPI1_Transmit(writeData);
}

void matrixClear(void)
{
	for (int i = 1; i <= 8; i++)
	{
		max7219_write(i, 0x00);	// Clear Screen
	}
}

void matrixInit(void)
{
	max7219_write(0x09, 0);		// No Decoding
	max7219_write(0x0A, 0x02);	// 5/32 Light Intensity
	max7219_write(0x0B, 0x07);	// Scan all columns (Turn them all on)
	max7219_write(0x0C, 0x01);	// Normal Operation (No shutdown mode)
	max7219_write(0x0F, 0x00);	// No Display Test

	matrixClear();
}

uint8_t intToHexPosition(uint8_t val)
{
	switch (val)
	{
		case 1:
			return 0x01;
			break;
		case 2:
			return 0x02;
			break;
		case 3:
			return 0x04;
			break;
		case 4:
			return 0x08;
			break;
		case 5:
			return 0x10;
			break;
		case 6:
			return 0x20;
			break;
		case 7:
			return 0x40;
			break;
		case 8:
			return 0x80;
			break;
		default:
			return -1;		// Should never get this, only enter values between 1 and 8
	}
}

void positionToMatrixPos(uint8_t x_pos[], uint8_t y_pos[], int numberOfCords, uint8_t outputArray[])
{
	for (int i = 0; i < numberOfCords; i++)
	{
		uint8_t x = x_pos[i];
		uint8_t y = y_pos[i];
		x = intToHexPosition(x);
		outputArray[y] |= x;
	}
}

void LEDMatrixWrite(uint8_t outputArray[])
{
	uint16_t writePos;
	for (int i = 1; i <= 8; i++)
	{
		writePos = (i << 8) | outputArray[i];
		SPI1_Transmit(writePos);
	}
}

void LEDMatrixRowWrite(uint8_t outputArray[], uint8_t row)
{
	uint16_t writePos;
	writePos = (row << 8) | outputArray[row];
	SPI1_Transmit(writePos);
}

void LEDMatrixColumnWrite(uint8_t outputArray[], uint8_t col)
{
	// We want to write from col 1 to this col variable (inclusive)
	for (int i = 1; i <= 8; i++)
	{
		uint8_t row_val = outputArray[i];
		for (int j = 8; j > col; j--)
		{
			row_val &= ~intToHexPosition(j);
		}
		uint16_t writeRow = (i << 8) | row_val;
		SPI1_Transmit(writeRow);
	}
}
