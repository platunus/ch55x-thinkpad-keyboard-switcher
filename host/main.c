#include <ch554.h>
#include "debug.h"
#include "usbhost.h"
#include <ch554_usb.h>
#include <stdio.h>
#include <string.h>
#include "ch55x_spimaster.h"

__code uint8_t  SetupGetDevDescr[] = { USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_DEVICE, 0x00, 0x00, sizeof( USB_DEV_DESCR ), 0x00 };
__code uint8_t  SetupGetCfgDescr[] = { USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_CONFIG, 0x00, 0x00, 0x04, 0x00 };
__code uint8_t  SetupSetUsbAddr[] = { USB_REQ_TYP_OUT, USB_SET_ADDRESS, USB_DEVICE_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00 };
__code uint8_t  SetupSetUsbConfig[] = { USB_REQ_TYP_OUT, USB_SET_CONFIGURATION, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
__code uint8_t  SetupSetUsbInterface[] = { USB_REQ_RECIP_INTERF, USB_SET_INTERFACE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
__code uint8_t  SetupClrEndpStall[] = { USB_REQ_TYP_OUT | USB_REQ_RECIP_ENDP, USB_CLEAR_FEATURE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
__code uint8_t  SetupGetHubDescr[] = { HUB_GET_HUB_DESCRIPTOR, HUB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_HUB, 0x00, 0x00, sizeof( USB_HUB_DESCR ), 0x00 };
__code uint8_t  SetupSetHIDIdle[]= { 0x21,HID_SET_IDLE,0x00,0x00,0x00,0x00,0x00,0x00 };
__code uint8_t  SetupGetHIDDevReport[] = { 0x81, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_REPORT, 0x00, 0x00, 0xFF, 0x00 };

__code uint8_t SetReportInitSetup[] = {0x21, 0x09, 0x13, 0x03, 0x01, 0x00, 0x03, 0x00};
__code uint8_t SetReportInitOut[4][3] = {{0x13, 0x01, 0x03}, {0x13, 0x09, 0x01},
										{0x13, 0x05, 0x01}, {0x13, 0x02, 0x05}};
__code uint8_t SetReportSetup[] = {0x21, 0x09, 0x00, 0x02, 0x00, 0x00, 0x01, 0x00};

__xdata uint8_t  UsbDevEndp0Size;
__xdata __at (0x0000) uint8_t RxBuffer[MAX_PACKET_SIZE];
__xdata __at (0x0040) uint8_t TxBuffer[MAX_PACKET_SIZE];

uint8_t Set_Port = 0;

__xdata _RootHubDev ThisUsbDev;
__xdata _DevOnHubPort DevOnHubPort[HUB_MAX_PORTS];

__bit RootHubId;
__bit FoundNewDev;

volatile __idata uint8_t sw, count_sw;
volatile __idata uint8_t polling;


#define TIMER0_INTERVAL 10000	// us -> 10ms interval
#define SW   RXD_				//P1.2
void Timer0Init() {
	P1_MOD_OC |=  (0b00000100);	// Bi-directinal with pullup
	P1_DIR_PU |=  (0b00000100);
	sw = 1;
	count_sw = 0;
	polling = 0;

	T2MOD = (T2MOD | bTMR_CLK) & ~bT0_CLK;				// Fsys/12 = 2MHz
	TMOD = TMOD | bT0_M0;								// Mode1: 16bit timer
	PT0 = 0;											// Low priorty 
	ET0 = 1;											// Interrupt enable
	TH0 = (0 - FREQ_SYS / 12 / 1000000 * TIMER0_INTERVAL ) >> 8; 
	TL0 = (0 - FREQ_SYS / 12 / 1000000 * TIMER0_INTERVAL ) & 0xff; 
	TR0 = 1;											// Timer0 start

	EA = 1; 
}

void Timer0InterruptHandler(void) __interrupt (INT_NO_TMR0) {
	TR0 = 0;											// Timer0 stop
	TH0 = (0 - FREQ_SYS / 12 / 1000000 * TIMER0_INTERVAL ) >> 8;
	TL0 = (0 - FREQ_SYS / 12 / 1000000 * TIMER0_INTERVAL ) & 0xff;
	TR0 = 1;											// Timer0 start
	count_sw++;
	if (count_sw == 3) {
		sw = SW;
		count_sw = 0;
	}
	polling = 1;
}

#define ACT0 INT0	//P3.2
#define ACT1 RXD	//P3.0
#define ACT2 T1		//P3.5
#define LED0 TIN0	//P1.0
#define LED1 TIN1	//P1.1
#define LED2 INT1	//P3.3
void PinInit() {
	// P1.0,1, P3.3:	Push-pull output (0:1)
	// P3.0,2,5:		Push-pull input (0:0)
	P1_MOD_OC &= ~(0b00000011);
	P1_DIR_PU |=  (0b00000011);
	P3_MOD_OC &= ~(0b00101101);
	P3_DIR_PU &= ~(0b00100101);
	P3_DIR_PU |=  (0b00001000);

	LED0 = 0;
	LED1 = 1;
	LED2 = 1;
}

void setLeds(uint8_t leds) {
	uint8_t len;
	__xdata uint8_t setReportBuf[1];
	memcpy(TxBuffer, SetReportSetup, sizeof(SetReportSetup));
	setReportBuf[0] = leds;
	HostCtrlTransfer(setReportBuf, &len);
}

void setPilotLeds(uint8_t select) {
	switch(select) {
		case 0:
			LED0 = 0;
			LED1 = 1;
			LED2 = 1;
			break;
		case 1:
			LED0 = 1;
			LED1 = 0;
			LED2 = 1;
			break;
		case 2:
			LED0 = 1;
			LED1 = 1;
			LED2 = 0;
			break;
	}
}

void main() {
    uint8_t   i, s,k, len, endp;
    uint16_t  loc;
	__xdata uint8_t setReportBuf[3];
	uint8_t leds, leds_pre[3], select;
	uint8_t sw_pre = 1;
	uint8_t act[3];

	uint8_t modifier_pre = 0;
	int8_t shift2 = 0;

	CfgFsys();	
	mDelaymS(50);
	mInitSTDIO();
	CH554UART0Alter();
    printf("Start @ChipID=%02X\n", (uint16_t)CHIP_ID );
	ch55x_spimaster_init();
	Timer0Init();
    InitUSB_Host( );
    FoundNewDev = 0;
	select = 0;
	leds_pre[0] = leds_pre[1] = leds_pre[2] = 0;

    printf("Wait Device In\n");
    while (1) {
		if (sw != sw_pre) {
			if (sw == 0) select = (select + 1) % 3;
			sw_pre = sw;
		}
		act[0] = ACT0;
		act[1] = ACT1;
		act[2] = ACT2;
		if (act[select]) {
			setPilotLeds(select);
		} else {
			select = (select + 1) % 3;
			if (act[select]) {
				setPilotLeds(select);
			} else {
				select = (select + 1) % 3;
				if (act[select]) {
					setPilotLeds(select);
				}
			}
		}

        s = ERR_SUCCESS;
        if (UIF_DETECT) {
            UIF_DETECT = 0;
            s = AnalyzeRootHub();
            if (s == ERR_USB_CONNECT) FoundNewDev = 1;						
        }
        if (FoundNewDev) {
            FoundNewDev = 0;
//          mDelaymS(200);
            s = EnumAllRootDevice();
            if (s != ERR_SUCCESS) {						
                printf( "EnumAllRootDev err = %02X\n", (uint16_t)s );					
            }
			setLeds(leds_pre[select]);

			memcpy(TxBuffer, SetReportInitSetup, sizeof(SetReportInitSetup));
			memcpy(setReportBuf, SetReportInitOut[0], 3);
			HostCtrlTransfer(setReportBuf, &len);
			memcpy(TxBuffer, SetReportInitSetup, sizeof(SetReportInitSetup));
			memcpy(setReportBuf, SetReportInitOut[1], 3);
			HostCtrlTransfer(setReportBuf, &len);
			memcpy(TxBuffer, SetReportInitSetup, sizeof(SetReportInitSetup));
			memcpy(setReportBuf, SetReportInitOut[2], 3);
			HostCtrlTransfer(setReportBuf, &len);
			memcpy(TxBuffer, SetReportInitSetup, sizeof(SetReportInitSetup));
			memcpy(setReportBuf, SetReportInitOut[3], 3);
			HostCtrlTransfer(setReportBuf, &len);
        }

		s = EnumAllHubPort();
		if (s != ERR_SUCCESS) {
			printf("EnumAllHubPort err = %02X\n", (uint16_t)s );
		}

		loc = SearchTypeDevice(USB_DEV_CLASS_HID);
		if (loc != 0xFFFF && polling){
			polling = 0;
			loc = (uint8_t)loc;

			for (k=0; k!=4; k++)
			{	
				endp = loc ? DevOnHubPort[loc-1].GpVar[k] : ThisUsbDev.GpVar[k];
				if ((endp & USB_ENDP_ADDR_MASK) == 0) break;

//				printf("endp: %02X\n",(uint16_t)endp);
				SelectHubPort(loc);
				s = USBHostTransact( USB_PID_IN << 4 | endp & 0x7F, endp & 0x80 ? bUH_R_TOG | bUH_T_TOG : 0, 0 );
				if ( s == ERR_SUCCESS ){
					if ( USB_RX_LEN ){
						len = USB_RX_LEN;
						if ((endp & 0x7f) == 1) {
							if (RxBuffer[0] == 0x22) {
								if (modifier_pre == 0x20) shift2 = -1;
								else if (modifier_pre == 0x02) shift2 = 1;
							}
							modifier_pre = RxBuffer[0];
						}
//						putchar('.');
//						printf("%d%d%d", act[0], act[1], act[2]);
						ch55x_spimaster_assert_cs(select);
						leds = ch55x_spimaster_transfer(len + 2);
						ch55x_spimaster_transfer(endp & 0x7f);
						for (i = 0; i < len; i++) {
							ch55x_spimaster_transfer(RxBuffer[i]);
						}
						ch55x_spimaster_dessert_cs(select);
//
//						printf("%02X: ", endp & 0x7f);
//						for (i = 0; i < len; i++) {
//							printf("%02X ", RxBuffer[i]);
//						}
//						printf("\r\n");
//
						if (leds != leds_pre[select]) {
							setLeds(leds);
							leds_pre[select] = leds;
							printf("%02x", leds);
						}

						if (shift2 == 1) {
							if (select == 0) {
								if (act[1]) select = 1;
								else if (act[2]) select = 2;
							} else if (select == 1) {
								if (act[2]) select = 2;
							}
						} else if (shift2 == -1) {
							if (select == 2) {
								if (act[1]) select = 1;
								else if (act[0]) select = 0;
							} else if (select == 1) {
								if (act[0]) select = 0;
							}
						}
						if (shift2) shift2 = 0;
					}
					endp ^= 0x80;
					if (loc) DevOnHubPort[loc-1].GpVar[k] = endp;
					else ThisUsbDev.GpVar[k] = endp;
				} else if (s != (USB_PID_NAK | ERR_USB_TRANSFER)) {
					printf("keyboard error %02x\n", (uint16_t)s);
				}

			}								
			SetUsbSpeed(1);
		}            					
    }
}
