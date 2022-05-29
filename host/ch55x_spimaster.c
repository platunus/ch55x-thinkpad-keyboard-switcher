#include <stdint.h>
#include <ch554.h>

#include "ch55x_spimaster.h"

void ch55x_spimaster_init()
{
    SPI0_SETUP = 0;				// Master mode, no interrupt, MSB first, 
    SPI0_CTRL =  bS0_MOSI_OE | bS0_SCK_OE;	// Mode0, MOSI, SCK: output

	P1_MOD_OC &= ~(0b11110000);	// P1.4,5,7(CE0/MOSI/SCK): push-pull output
	P1_DIR_PU |=   0b10110000;
    P1_DIR_PU &= ~(0b01000000); // P1.6(MISO): push-pull input
	P3_MOD_OC &= ~(0b00010010);	// P3.1,4(CE1/CE2): push-pull output
	P3_DIR_PU |=   0b00010010;
/*
	P1_MOD_OC &= 0b01011101;	// P1.0,5,7(LED/MOSI/SCK): push-pull output
	P1_DIR_PU |= 0b10110011;	// P1.6(MISO): push-pull input
    P1_DIR_PU &= 0b10111111;	// P1.0,4(CE1/CE0): bi-directional
	P3_MOD_OC |= 0b00000001;	// P3.0(CE2): bi-directional
	P3_DIR_PU |= 0b00000001;
*/
	CE0 = CE1 = CE2 = 1;
    SPI0_CK_SE = 12; 				// Clock division: 12 (=24MHz/2MHz) -> 2MHz
}

inline void ch55x_spimaster_assert_cs(uint8_t device)
{
	switch(device)
	{
		case 0:
			CE0 = 0;
			break;
		case 1:
			CE1 = 0;
			break;
		case 2:
			CE2 = 0;
			break;
	}
}

inline void ch55x_spimaster_dessert_cs(uint8_t device)
{
	switch(device)
	{
		case 0:
			CE0 = 1;
			break;
		case 1:
			CE1 = 1;
			break;
		case 2:
			CE2 = 1;
			break;
	}
}

inline uint8_t ch55x_spimaster_transfer(uint8_t data)
{
	SPI0_DATA = data;
	while(S0_FREE==0);
	return SPI0_DATA;
}

