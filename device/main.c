#include <stdint.h>

#include <ch554.h>
#include <debug.h>

#include "ch55x_usbdevice.h"

#define JIGGLER_INTERVAL 50000		// ms
#define JIGGLER_MOVING_DISTANCE 10	// pixels
#define LED_ON_PERIOD 100			// ms

#define TIMER0_INTERVAL 1000	// us -> 1ms interval

// Indicator LED: P1.2
#define LED_PIN_PORT_MOD 	P1_MOD_OC
#define LED_PIN_NO		   	2
#define LED 				RXD_

#define MAX_BUF	30

volatile __idata uint32_t mills;
volatile __idata uint8_t buf[2][MAX_BUF];
volatile __idata uint8_t offset = 0;
volatile __idata uint8_t idx = 0;

void TxOnlyUart0Init()
{
	volatile uint32_t x;
	volatile uint8_t x2;

	P3_MOD_OC = 0xfd; 

	SM0 = 0;
	SM1 = 1;
	SM2 = 0;
//	RCLK = 0;		// for RXD
	TCLK = 0;
	PCON |= SMOD;
	x = (uint32_t)FREQ_SYS / 115200 / 16;
	x2 = ((uint32_t)FREQ_SYS * 10 / 115200 / 16) % 10;
	if ( x2 >= 5 ) x++;

	TMOD = TMOD & ~bT1_GATE & ~bT1_CT & ~MASK_T1_MOD | bT1_M1;	// Mode2 : 8bit reload timer
	T2MOD = T2MOD | bTMR_CLK | bT1_CLK;
	TH1 = 0 - (uint8_t)x;
	TR1 = 1;
	TI = 0;
//	REN = 1;		/ for RXD
	ES = 1;
	PS = 1;
}

void TxOnlyUart0Write(uint8_t d)
{
	SBUF = d;
	while (TI == 0);
	TI = 0;
}

void TxOnlyUart0WriteHex(uint8_t d)
{
	const uint8_t hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};;
	TxOnlyUart0Write(hex[d >> 4]);
	TxOnlyUart0Write(hex[d & 0x0f]);
	TxOnlyUart0Write(' ');
}

void Timer0Init()
{
	T2MOD = (T2MOD | bTMR_CLK) & ~bT0_CLK;				// Fsys/12 = 2MHz
	TMOD = TMOD | bT0_M0;								// Mode1: 16bit timer
	PT0 = 0;											// Low priorty 
	ET0 = 1;											// Interrupt enable
	TH0 = (0 - FREQ_SYS / 12 / 1000000 * TIMER0_INTERVAL ) >> 8; 
	TL0 = (0 - FREQ_SYS / 12 / 1000000 * TIMER0_INTERVAL ) & 0xff; 
	TR0 = 1;											// Timer0 start

	EA = 1;
}

void Timer0InterruptHandler(void) __interrupt (INT_NO_TMR0) 
{
	TH0 = (0 - FREQ_SYS / 12 / 1000000 * TIMER0_INTERVAL ) >> 8; 
	TL0 = (0 - FREQ_SYS / 12 / 1000000 * TIMER0_INTERVAL ) & 0xff; 
	mills++;
}

void LEDInit()
{
	LED_PIN_PORT_MOD = LED_PIN_PORT_MOD & ~(1 << LED_PIN_NO);		// push-pull
	LED = 0;
}

void SPI0InterruptHandler(void) __interrupt (INT_NO_SPI0)
{
	if (S0_IF_FIRST)
	{
		offset = 0;
		S0_IF_FIRST = 0;
	}
	if (S0_IF_BYTE)
	{
		if (offset < MAX_BUF) buf[idx][offset++] = SPI0_DATA;
		S0_IF_BYTE = 0;
	}
}

void SPI0SlaveInit()
{
/*
	P1_MOD_OC &= 0x0F;		// P1.4-6(SCS/MOSI/MISO/SCK): input
	P1_DIR_PU &= 0x0F;
*/
	P1_MOD_OC &= 0x1F;		// P1.5-6(MOSI/MISO/SCK): push-pull input
	P1_DIR_PU &= 0x1F;		// P1.4(SCS): bi-directinal with pullup
	SPI0_SETUP = bS0_MODE_SLV | bS0_IE_FIRST | bS0_IE_BYTE;
	SPI0_CTRL = bS0_MISO_OE;
	IP_EX |= bIP_SPI0;
	IE_SPI0 = 1;
}

main()
{
	uint8_t i;
	uint8_t len = 0;
	uint8_t scs_pre;
	uint8_t index;

	CfgFsys();

	mDelaymS(5);

	ch55x_device_init();
	Timer0Init();

	LEDInit();

	SPI0SlaveInit();
	
	TxOnlyUart0Init();

	ch55x_keyboard_release(0);

	scs_pre = 1;

	while(1)
	{
		if (SCS == 1 && scs_pre == 0)
		{
			len = offset;
			index = idx;
			idx = idx ? 0 : 1;
			if (len == buf[index][0] && len > 3) {
				LED = 1;
				if (buf[index][1] == 1)
				{
					ch55x_keyboard_send(buf[index] + 2, len - 2);
				}
				else if (buf[index][1] == 2)
				{
					ch55x_mouse_send(buf[index] + 2, len - 2);
				}
				LED = 0;
			}
			scs_pre = 1;
			mills = 0;

/*
			TxOnlyUart0Write('.');
			TxOnlyUart0WriteHex(len);
			TxOnlyUart0Write(':');
			for (i = 0; i < len; i++)
			{
				TxOnlyUart0WriteHex(buf[index][i]);
			}
			TxOnlyUart0Write(0x0d);
			TxOnlyUart0Write(0x0a);
*/
		}
		else if ( SCS == 1 && mills > JIGGLER_INTERVAL)
		{
			LED = 1;
			ch55x_keyboard_press(0, 0x47);	// Enable Scroll Lock
			ch55x_keyboard_release(0);
			ch55x_keyboard_press(0, 0x47);	// Disable Scroll Lock
			ch55x_keyboard_release(0);
			LED = 0;
			mills = 0;
		}
		else if (SCS == 0 && scs_pre == 1)
		{
			SPI0_S_PRE = ch55x_keyboard_get_leds_state();
			scs_pre = 0;
		}
	}
}
