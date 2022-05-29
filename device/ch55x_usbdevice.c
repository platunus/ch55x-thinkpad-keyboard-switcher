#include <stdint.h>
#include <string.h>

#include <ch554.h>
#include <ch554_usb.h>

#include "ch55x_usbdevice.h"

#define UsbSetupBuf ((USB_SETUP_REQ *)Ep0Buffer)

__xdata __at (FIXED_ADDRESS_EP0_BUFFER) uint8_t Ep0Buffer[DEFAULT_ENDP0_SIZE];	//Endpoint0 OUT&IN
__xdata __at (FIXED_ADDRESS_EP1_BUFFER) uint8_t Ep1Buffer[MAX_PACKET_SIZE];		//Endpoint1 IN
__xdata __at (FIXED_ADDRESS_EP2_BUFFER) uint8_t Ep2Buffer[MAX_PACKET_SIZE];		//Endpoint2 IN

uint8_t SetupReq, SetupLen, UsbConfig;
USB_SETUP_REQ SetupReqBuf;
__code uint8_t *pDescr;

volatile __idata uint8_t RepDescSent;
volatile __idata uint8_t HIDKeyboardDataSent;
volatile __idata uint8_t HIDMouseDataSent;

uint8_t buttonsState = 0;
uint8_t ledsState = 0;

void TxOnlyUart0Write_(uint8_t d)
{
    SBUF = d;
    while (TI == 0);
    TI = 0; 
}

void TxOnlyUart0WriteHex_(uint8_t d)
{
    const uint8_t hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};;
    TxOnlyUart0Write_(hex[d >> 4]); 
    TxOnlyUart0Write_(hex[d & 0x0f]); 
    TxOnlyUart0Write_(' ');
}

// Device Descriptor
__code uint8_t DevDesc[18] = 
{
	0x12,				// bLength
	0x01,				// bDescriptorType: DEVICE
	0x00, 0x02,			// bcdUSB: USB2.0
	0x00,				// bDeviceClass
	0x00,				// bDeviceSubClass
	0x00,				// bDeviceProtocol
	DEFAULT_ENDP0_SIZE,	// bMaxPacketSize0
	VENDOR_ID,			// idVendor
	PRODUCT_ID,			// idProduct
	0x00, 0x03,			// bcdDevice
	0x01,				// iManufacturer
	0x02,				// iProduct
//	0x00,				// iSerialNumber
	0x03,				// iSerialNumber
	0x01				// bNumConfigurations
};

// Configuration Descriptor
__code uint8_t CfgDesc[59] =
{
	// Device
	0x09,		// bLength
	0x02,		// bDescriptorType: CONFIGURATION
	0x3b, 0x00,	// wTotalLength
	0x02,		// bNumInterface
	0x01,		// bConfigurationValue
	0x00,		// iConfiguration
	0xa0,		// bmAttributes: Bus Power/Remote Wakeup
	0x32,		// bMaxPower

	// Interface 0
	0x09,		// bLength
	0x04,		// bDescriptorType: INTERFACE
	0x00,		// bInterfaceNumber
	0x00,		// bAlternateSetting
	0x01,		// bNumEndpoints
	0x03,		// bInterfaceClass: Human Interface Device (HID)
	0x01,		// bInterfaceSubClass
	0x01,		// bInterfaceProtocol: Keyboard
	0x00,		// iInterface

	// HID 0
	0x09,		// bLength
	0x21,		// bDescriptorType: HID
	0x00, 0x01, // bcdHID: 1.00
	0x00,		// bCountryCode
	0x01,		// bNumDescriptors
	0x22,		// bDescriptorType: Report
	sizeof(KeyboardRepDesc) & 0xff, sizeof(KeyboardRepDesc) >> 8,	// wDescriptorLength

	// Endpoint1 IN
	0x07,		// bLength
	0x05,		// bDescriptorType: ENDPOINT
	0x81,		// bEndpointAddress: IN/Endpoint1
	0x03,		// bmAttributes: Interrupt
	MAX_PACKET_SIZE & 0xff, MAX_PACKET_SIZE >> 8, // wMaxPacketSize
	0x0a,		// bInterval

	// Interface 1
	0x09,		// bLength
	0x04,		// bDescriptorType: INTERFACE
	0x01,		// bInterfaceNumber
	0x00,		// bAlternateSetting
	0x01,		// bNumEndpoints
	0x03,		// bInterfaceClass: Human Interface Device (HID)
	0x01,		// bInterfaceSubClass
	0x02,		// bInterfaceProtocol: Mouse
	0x00,		// iInterface

	// HID
	0x09,		// bLength
	0x21,		// bDescriptorType: HID
	0x00, 0x01, // bcdHID: 1.00
	0x00,		// bCountryCode
	0x01,		// bNumDescriptors
	0x22,		// bDescriptorType: Report
	sizeof(MouseRepDesc) & 0xff, sizeof(MouseRepDesc) >> 8,	// wDescriptorLength

	// Endpoint 2 IN
	0x07,		// bLength
	0x05,		// bDescriptorType: ENDPOINT
	0x82,		// bEndpointAddress: IN/Endpoint2
	0x03,		// bmAttributes: Interrupt
	MAX_PACKET_SIZE & 0xff, MAX_PACKET_SIZE >> 8, // wMaxPacketSize
	0x0a,		// bInterval
};

// HID Report Descriptor
__code uint8_t KeyboardRepDesc[81] =
{
	0x05, 0x01,			// USAGE_PAGE (Generic Desktop)
	0x09, 0x06,			// USAGE (Keyboard)
	0xa1, 0x01,			// COLLECTION (Application)
	0x05, 0x07,			//  USAGE_PAGE (Keyboard)
	0x19, 0xe0,			//  USAGE_MINIMUM (Keyboard LeftControl)
	0x29, 0xe7,			//  USAGE_MAXIMUM (Keyboard Right GUI)
	0x15, 0x00,			//  LOGICAL_MINIMUM (0)
	0x25, 0x01,			//  LOGICAL_MAXIMUM (1)
	0x75, 0x01,			//  REPORT_SIZE (1)
	0x95, 0x08,			//  REPORT_COUNT (8)
	0x81, 0x02,			//  INPUT (Data,Var,Abs)

	0x95, 0x01,			//  REPORT_COUNT (1)
	0x75, 0x08,			//  REPORT_SIZE (8)
	0x81, 0x01,			//  INPUT (Cnst,Var,Abs)

	0x95, 0x05,			//  REPORT_COUNT (5)
	0x75, 0x01,			//  REPORT_SIZE (1)
	0x05, 0x08,			//  USAGE_PAGE (LEDs)
	0x19, 0x01,			//  USAGE_MINIMUM (1)
	0x29, 0x05,			//  USAGE_MAXIMUM (5)
	0x91, 0x02,			//  OUTPUT (Data,Var,Abs)

	0x95, 0x01,			//  REPORT_COUNT (1)
	0x75, 0x03,			//  REPORT_SIZE (3)
	0x91, 0x01,			//  OUTPUT (Cnst,Var,Abs)

	0x95, 0x06,			//  REPORT_COUNT (6)
	0x75, 0x08,			//  REPORT_SIZE (8)
	0x16, 0x00, 0x00,	//  LOGICAL_MINIMUM (0)
	0x26, 0xaf, 0x00,	//  LOGICAL_MAXIMUM (175)
	0x05, 0x07,			//  USAGE_PAGE (Keyboard)
	0x1a, 0x00, 0x00,	//  USAGE_MINIMUM (0)
	0x2a, 0xaf, 0x00,	//  USAGE_MAXIMUM (175)
	0x81, 0x00,			//  INPUT (Data,Ary,Abs)

	0x05, 0x0c,			//  USAGE_PAGE (Consumer)
	0x09, 0x00,			//  USAGE (Undefined)
	0x15, 0x80,			//  LOGICAL_MINIMUM (-128)
	0x25, 0x7f,			//  LOGICAL_MAXIMUM (127)
	0x75, 0x08,			//  REPORT_SIZE (8)
	0x95, 0x08,			//  REPORT_COUNT (8)
	0xb1, 0x02,			//  FEATURE (VARIABLE)
	0xc0,				// END_COLLECTION
};

__code uint8_t MouseRepDesc[211] =
{
	0x05, 0x01,			// USAGE_PAGE (Generic Desktop)
	0x09, 0x02,			// USAGE (Mouse)
	0xa1, 0x01,			// COLLECTION (Application)
	0x85, 0x01,			//  REPORT ID (1)
	0x05, 0x09,			//  USAGE_PAGE (Button)
	0x19, 0x01,			//  USAGE_MINIMUM (1)
	0x29, 0x05,			//  USAGE_MAXIMUM (5)
	0x15, 0x00,			//  LOGICAL_MINIMUM (0)
	0x25, 0x01,			//  LOGICAL_MAXIMUM (1)
	0x75, 0x01,			//  REPORT_SIZE (1)
	0x95, 0x05,			//  REPORT_COUNT (5)
	0x81, 0x02,			//  INPUT (Data,Var,Abs)
	0x75, 0x03,			//  REPORT_SIZE (3)
	0x95, 0x01,			//  REPORT_COUNT (1)
	0x81, 0x01,			//  INPUT (Cnst,Var,Abs)
	0x05, 0x01,			//  USAGE_PAGE (Generic Desktop)
	0x09, 0x01,			//  USAGE (Pointer)
	0xa1, 0x00,			//  COLLECTION (Physical)
	0x09, 0x30,			//   USAGE (X)
	0x09, 0x31,			//   USAGE (Y)
	0x15, 0x81,			//   LOGICAL_MINIMUM (-127)
	0x25, 0x7f,			//   LOGICAL_MAXIMUM (127)
	0x75, 0x08,			//   REPORT_SIZE (8)
	0x95, 0x02,			//   REPORT_COUNT (2)
	0x81, 0x06,			//   INPUT (Data,Var,Rel)
	0xc0,				//  END_COLLECTION
	0x09, 0x38,			//  USAGE (Wheel)
	0x95, 0x01,			//  REPORT_COUNT (1)
	0x81, 0x06,			//  INPUT (Data,Var,Rel)
	0x05, 0x0c,			//  USAGE_PAGE (Consumer)
	0x0a, 0x38, 0x02, 	//  USAGE (AC PAN)
	0x81, 0x06, 		//  INPUT (Data,Var,Rel)
	0xc0,				// END_COLLECTION

	0x05, 0x0c,			// USAGE_PAGE (Consumer)
	0x09, 0x01,			// USAGE (Consumer Control)
	0xa1, 0x01,			// COLLECTION (Application)
	0x85, 0x10,			//  REPORT ID (16)
	0x15, 0x00,			//  LOGICAL_MINIMUM (0)
	0x26, 0x3c, 0x02,	//  LOGICAL_MAXIMUM (572)
	0x19, 0x00,			//  USAGE MINIMUM (0)
	0x2a, 0x3c, 0x02,	//  USAGE MAXIMUM (AC FORMAT)
	0x75, 0x10,			//  REPORT_SIZE (16)
	0x95, 0x01,			//  REPORT_COUNT (1)
	0x81, 0x00,			//  INPUT (Cnst,Var,Rel)
	0xc0,				// END_COLLECTION

	0x06, 0xa3, 0xff,	// USAGE PAGE (FFA3H/VENDOR-DEFINE)
	0x09, 0x01,			// USAGE (1)
	0xa1, 0x01,			// COLLECTION (Application)
	0x85, 0x11,			//  REPORT ID (17)
	0x19, 0x00,			//  USAGE MINIMUM (00H)
	0x2a, 0xff, 0x00,	//  USAGE MAXIMUM (FFH)
	0x15, 0x00,			//  LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,	//  LOGICAL_MAXIMUM (255)
	0x75, 0x08,			//  REPORT_SIZE (8)
	0x95, 0x02,			//  REPORT_COUNT (2)
	0x81, 0x02,			//  INPUT (Data,Var,Abs)
	0xc0,				// END COLLECTION

	0x06, 0xa0, 0xff,	// USAGE PAGE (FFA0H/VENDOR-DEFINE)
	0x09, 0x01,			// USAGE (1)
	0xa1, 0x01,			// COLLECTION (Application)
	0x85, 0x15,			//  REPORT ID (21)
	0x1a, 0xf1, 0x00,	//  USAGE MINIMUM (F1H)
	0x2a, 0xfc, 0x00,	//  USAGE MAXIMUM (FCH)
	0x15, 0x00,			//  LOGICAL MINIMUM (0)
	0x25, 0x01,			//  LOGICAL MAXIMUM (1)
	0x75, 0x01,			//  REPORT SIZE (1)
	0x95, 0x0d,			//  REPORT COUNT (13)
	0x81, 0x02,			//  INPUT (Data,Var,Abs)
	0x95, 0x03,			//  REPORT_COUNT (3)
	0x81, 0x01,			//  INPUT (Cnst,Var,Abs)
	0xc0,				// END COLLECTION

	0x06, 0xa1, 0xff,	// USAGE PAGE (FFA1H/VENDOR-DEFINE)
	0x09, 0x01,			// USAGE (1)
	0xa1, 0x01,			// COLLECTION (Application)
	0x85, 0x16,			//  REPORT ID (22)
	0x19, 0x00,			//  USAGE MINIMUM (0)
	0x2a, 0xff, 0x00,	//  USAGE MAXIMUM (255)
	0x15, 0x00,			//  LOGICAL MINIMUM (0)
	0x26, 0xff, 0x00,	//  LOGICAL MAXIMUM (255)
	0x75, 0x08,			//  REPORT SIZE (8)
	0x95, 0x02,			//  REPORT COUNT (2)
	0x81, 0x02,			//  INPUT (Data,Var,Abs)
	0xc0,				// END COLLECTION

	0x06, 0x00, 0xff,	// USAGE PAGE (FF00H/VENDOR-DEFINE)
	0x09, 0x01,			// USAGE (1)
	0xa1, 0x01,			// COLLECTION (Application)
	0x85, 0x13,			//  REPORT ID (19)
	0x09, 0x01,			//  USAGE (1)
	0x15, 0x00,			//  LOGICAL MINIMUM (0)
	0x26, 0xff, 0x00,	//  LOGICAL MAXIMUM (255)
	0x75, 0x08,			//  REPORT_SIZE (8)
	0x95, 0x08,			//  REPORT_COUNT (8)
	0x81, 0x02,			//  INPUT (Data,Var,Abs)
	0x09, 0x02,			//  USAGE (2)
	0x75, 0x08,			//	REPORT_SIZE (8)
	0x95, 0x08,			//  REPORT_COUNT (8)
	0x91, 0x02,			//  OUTPUT (Data,Var,Abs)
	0x09, 0x03,			//  USAGE (Reserved)
	0x75, 0x08,			//  REPORT_SIZE (8)
	0x95, 0x08,			//  REPORT_COUNT (8)
	0xb1, 0x02,			//  FEATURE (VARIABLE)
	0xc0				// END COLLECTION
};

// String Descriptor (Language)
__code unsigned char LangDesc[] = {0x04, 0x03, 0x09, 0x04};

// String Descriptor (Manufacturer)
__code unsigned char ManufDesc[] =
{
	sizeof(ManufDesc), 0x03, MANUFACTURER_DESCRIPTION
};

// String Descritor (Product)
__code unsigned char ProdDesc[] =
{
	sizeof(ProdDesc), 0x03, PRODUCT_DESCRIPTION
};

// String Descritor (SerialNumber)
__code unsigned char SerialNumDesc[] =
{
	sizeof(SerialNumDesc), 0x03, SERIAL_NUMBER_DESCRIPTION
};

void ch55x_usbdevice_interrupt_handler(void) __interrupt (INT_NO_USB)
{
	uint8_t len;
	uint8_t i;

	if(UIF_TRANSFER)
	{
		switch (USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP))
		{
		case UIS_TOKEN_IN | 1:
			UEP1_T_LEN = 0;
			UEP1_CTRL = UEP1_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_NAK;
			HIDKeyboardDataSent = 1;
			break;
		case UIS_TOKEN_IN | 2:
			UEP2_T_LEN = 0;
			UEP2_CTRL = UEP2_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_NAK;
			HIDMouseDataSent = 1;
			break;
		case UIS_TOKEN_SETUP | 0:
			len = USB_RX_LEN;
			if(len == (sizeof(USB_SETUP_REQ)))
			{
TxOnlyUart0Write_('S');
TxOnlyUart0WriteHex_(len);
for (i = 0; i < len; i++) TxOnlyUart0WriteHex_(Ep0Buffer[i]);
TxOnlyUart0Write_(0x0d);
TxOnlyUart0Write_(0x0a);
				SetupLen = UsbSetupBuf->wLengthL;
//				if (UsbSetupBuf->wLengthH || SetupLen > 0x7f )
				if (UsbSetupBuf->wLengthH || SetupLen > sizeof(MouseRepDesc) )
				{
//					SetupLen = 0x7f;
					SetupLen = sizeof(MouseRepDesc);
				}
				len = 0;
				SetupReq = UsbSetupBuf->bRequest;								
				if ( ( UsbSetupBuf->bRequestType & USB_REQ_TYP_MASK ) != USB_REQ_TYP_STANDARD )
				{
					switch( SetupReq ) 
					{
						case 0x01:	//GetReport
							 break;
						case 0x02:	//GetIdle
							 break;	
						case 0x03:	//GetProtocol
							 break;				
						case 0x09:	//SetReport
							 break;
						case 0x0a:	//SetIdle
							 break;	
						case 0x0b:	//SetProtocol
							 break;
						default:
							 len = 0xff;
							 break;
					  }	
				}
				else
				{
					switch(SetupReq)
					{
					case USB_GET_DESCRIPTOR:
						switch(UsbSetupBuf->wValueH)
						{
						case 1:
							pDescr = DevDesc;
							len = sizeof(DevDesc);
							break;
						case 2:
							pDescr = CfgDesc;
							len = sizeof(CfgDesc);
							break;
						case 3:
							if(UsbSetupBuf->wValueL == 0)
							{
								pDescr = LangDesc;
								len = sizeof(LangDesc);
							}
							else if(UsbSetupBuf->wValueL == 1)
							{
								pDescr = ManufDesc;
								len = sizeof(ManufDesc);
							}
							else if(UsbSetupBuf->wValueL == 2)
							{
								pDescr = ProdDesc;
								len = sizeof(ProdDesc);
							}
							else if(UsbSetupBuf->wValueL == 3)
							{
								pDescr = SerialNumDesc;
								len = sizeof(SerialNumDesc);
							}
							else
							{
								len = 0xff;
							}
							break;

						case 0x22:
							if (UsbSetupBuf->wIndexL == 0)
							{
								pDescr = KeyboardRepDesc;
								len = sizeof(KeyboardRepDesc);
							}
							else if (UsbSetupBuf->wIndexL == 1)
							{
								pDescr = MouseRepDesc;
								len = sizeof(MouseRepDesc);
								RepDescSent = 1;
							}
							else
							{
								len = 0xff;
							}
							break;
						default:
							len = 0xff;
							break;
						}
						if ( SetupLen > len )
						{
							SetupLen = len;
						}
						len = SetupLen >= 8 ? 8 : SetupLen;
						memcpy(Ep0Buffer, pDescr, len);
						SetupLen -= len;
						pDescr += len;
						break;
					case USB_SET_ADDRESS:
						SetupLen = UsbSetupBuf->wValueL;
						break;
					case USB_GET_CONFIGURATION:
						Ep0Buffer[0] = UsbConfig;
						if ( SetupLen >= 1 )
						{
							len = 1;
						}
						break;
					case USB_SET_CONFIGURATION:
						UsbConfig = UsbSetupBuf->wValueL;
						break;
					case 0x0a:
						break;
					case USB_CLEAR_FEATURE:
						if ( ( UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_ENDP )
						{
							switch( UsbSetupBuf->wIndexL )
							{
							case 0x81:
								UEP1_CTRL = UEP1_CTRL & ~( bUEP_T_TOG | MASK_UEP_T_RES ) | UEP_T_RES_NAK;
								break;
							case 0x82:
								UEP2_CTRL = UEP2_CTRL & ~( bUEP_T_TOG | MASK_UEP_T_RES ) | UEP_T_RES_NAK;
								break;
							case 0x01:
								UEP1_CTRL = UEP1_CTRL & ~( bUEP_R_TOG | MASK_UEP_R_RES ) | UEP_R_RES_ACK;
								break;
							default:
								len = 0xff;
								break;
							}
						}
						else
						{
							len = 0xff;
						}
						break;
					case USB_SET_FEATURE:
						if( ( UsbSetupBuf->bRequestType & 0x1f ) == 0x00 )
						{
							if( ( ( ( uint16_t )UsbSetupBuf->wValueH << 8 ) | UsbSetupBuf->wValueL ) == 0x01 )
							{
								if( CfgDesc[ 7 ] & 0x20 )
								{
								}
								else
								{
									len = 0xff;
								}
							}
							else
							{
								len = 0xff;
							}
						}
						else if( ( UsbSetupBuf->bRequestType & 0x1f ) == 0x02 )
						{
							if( ( ( ( uint16_t )UsbSetupBuf->wValueH << 8 ) | UsbSetupBuf->wValueL ) == 0x00 )
							{
								switch( ( ( uint16_t )UsbSetupBuf->wIndexH << 8 ) | UsbSetupBuf->wIndexL )
								{
								case 0x02:
									UEP2_CTRL = UEP2_CTRL & (~bUEP_T_TOG) | UEP_R_RES_STALL;
									break;
								case 0x81:
									UEP1_CTRL = UEP1_CTRL & (~bUEP_T_TOG) | UEP_T_RES_STALL;
									break;
								case 0x82:
									UEP2_CTRL = UEP2_CTRL & (~bUEP_T_TOG) | UEP_T_RES_STALL;
									break;
								default:
									len = 0xff;
									break;
								}
							}
							else
							{
								len = 0xff;
							}
						}
						else
						{
							len = 0xff;
						}
						break;
					case USB_GET_STATUS:
						Ep0Buffer[0] = 0x00;
						Ep0Buffer[1] = 0x00;
						if ( SetupLen >= 2 )
						{
							len = 2;
						}
						else
						{
							len = SetupLen;
						}
						break;
					default:
						len = 0xff;
						break;
					}
				}
			}
			else
			{
				len = 0xff;
			}
			if(len == 0xff)
			{
				SetupReq = 0xff;
				UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
			}
			else if(len <= 8)
			{
				UEP0_T_LEN = len;
				UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
			}
			else
			{
				UEP0_T_LEN = 0;
				UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
			}
			break;
		case UIS_TOKEN_IN | 0:	//endpoint0 IN
			switch(SetupReq)
			{
			case USB_GET_DESCRIPTOR:
				len = SetupLen >= 8 ? 8 : SetupLen;
				memcpy( Ep0Buffer, pDescr, len );
				SetupLen -= len;
				pDescr += len;
				UEP0_T_LEN = len;
				UEP0_CTRL ^= bUEP_T_TOG;
				break;
			case USB_SET_ADDRESS:
				USB_DEV_AD = USB_DEV_AD & bUDA_GP_BIT | SetupLen;
				UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
				break;
			default:
				UEP0_T_LEN = 0;
				UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
				break;
			}
			break;
		case UIS_TOKEN_OUT | 0:  // endpoint0 OUT
			len = USB_RX_LEN;
/*
TxOnlyUart0Write_('O');
TxOnlyUart0WriteHex_(len);
for (i = 0; i < len; i++) TxOnlyUart0WriteHex_(Ep0Buffer[i]);
TxOnlyUart0Write_(0x0d);
TxOnlyUart0Write_(0x0a);
*/
			if(SetupReq == 0x09)
			{
				if (len == 1) ledsState = Ep0Buffer[0];	// LEDs
			}
			UEP0_CTRL ^= bUEP_R_TOG;
            break;
		default:
			break;
		}
		UIF_TRANSFER = 0;
	}
	if(UIF_BUS_RST)
	{
		UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
		UEP1_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
		UEP2_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK;
		USB_DEV_AD = 0x00;
		UIF_SUSPEND = 0;
		UIF_TRANSFER = 0;
		UIF_BUS_RST = 0;
	}
	if (UIF_SUSPEND)
	{
		UIF_SUSPEND = 0;
	}
	else {
		USB_INT_FG = 0xff;
	}
}

void HIDKeyboardValueHandle(uint8_t modifier, uint8_t key1, uint8_t key2, uint8_t key3, uint8_t key4, uint8_t key5, uint8_t key6)
{
	HIDKeyboardDataSent = 0;		
	Ep1Buffer[0] = modifier;
	Ep1Buffer[1] = 0;
	Ep1Buffer[2] = key1;
	Ep1Buffer[3] = key2;
	Ep1Buffer[4] = key3;
	Ep1Buffer[5] = key4;
	Ep1Buffer[6] = key5;
	Ep1Buffer[7] = key6;
	UEP1_T_LEN = 8;
	UEP1_CTRL = UEP1_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK;
	while(HIDKeyboardDataSent == 0) {}
}

void HIDMouseValueHandle(uint8_t button, int8_t x, int8_t y, int8_t wheel)
{
	HIDMouseDataSent = 0;		
	Ep2Buffer[0] = 1;
	Ep2Buffer[1] = button;
	Ep2Buffer[2] = x;
	Ep2Buffer[3] = y;
	Ep2Buffer[4] = wheel;
	UEP2_T_LEN = 5;
	UEP2_CTRL = UEP2_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK;
	while(HIDMouseDataSent == 0) {}
}

void ch55x_device_init()
{
	IE_USB = 0;
	USB_CTRL = 0x00;

	UEP0_DMA_L = (uint8_t)Ep0Buffer;
	UEP4_1_MOD &= ~(bUEP4_RX_EN | bUEP4_TX_EN);
	UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;

	UEP1_DMA = (uint16_t)Ep1Buffer;
	UEP4_1_MOD = UEP4_1_MOD & ~bUEP1_BUF_MOD | bUEP1_TX_EN;
	UEP1_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK;
	UEP1_T_LEN = 0;

	UEP2_DMA_L = (uint8_t)Ep2Buffer;
	UEP2_3_MOD = UEP2_3_MOD & ~bUEP2_BUF_MOD | bUEP2_TX_EN;
	UEP2_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK;
	UEP2_T_LEN = 0;

	USB_DEV_AD = 0x00;
	UDEV_CTRL = bUD_PD_DIS;
	USB_CTRL = bUC_DEV_PU_EN | bUC_INT_BUSY | bUC_DMA_EN;

	UDEV_CTRL |= bUD_PORT_EN;
	USB_INT_FG = 0xff;
	USB_INT_EN = bUIE_SUSPEND | bUIE_TRANSFER | bUIE_BUS_RST;
	IE_USB = 1;

	RepDescSent = 0;
	EA = 1;
	while(RepDescSent == 0) {};
}

void ch55x_mouse_move(int8_t delta_x, int8_t delta_y)
{
	HIDMouseValueHandle(0, delta_x, delta_y, 0);
}

void ch55x_mouse_scroll(int8_t delta)
{
	HIDMouseValueHandle(0, 0, 0, delta);
}

void ch55x_mouse_press(uint8_t buttons)
{
	buttonsState |= buttons & MOUSE_BUTTONS_MASK;
	HIDMouseValueHandle(buttonsState, 0, 0, 0);
}

void ch55x_mouse_release(uint8_t buttons)
{
	buttonsState &= ~(buttons & MOUSE_BUTTONS_MASK);
	HIDMouseValueHandle(buttonsState, 0, 0, 0);
}

void ch55x_mouse_release_all()
{
	buttonsState = 0;
	HIDMouseValueHandle(buttonsState, 0, 0, 0);
}

void ch55x_keyboard_press(uint8_t modifier, uint8_t key)
{
	HIDKeyboardValueHandle(modifier, key, 0, 0, 0, 0, 0);
}

void ch55x_keyboard_release(uint8_t modifier)
{
	HIDKeyboardValueHandle(modifier, 0, 0, 0, 0, 0, 0);
}

void ch55x_mouse_send(uint8_t *buf, uint8_t len)
{
	if (len < MAX_PACKET_SIZE)
	{
		HIDMouseDataSent = 0;		
		memcpy(Ep2Buffer, buf, len);
		UEP2_T_LEN = len;
		UEP2_CTRL = UEP2_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK;
		while(HIDMouseDataSent == 0) {}
	}
}

void ch55x_keyboard_send(uint8_t *buf, uint8_t len)
{
	if (len < MAX_PACKET_SIZE)
	{
		HIDKeyboardDataSent = 0;		
		memcpy(Ep1Buffer, buf, len);
		UEP1_T_LEN = len;
		UEP1_CTRL = UEP1_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK;
		while(HIDKeyboardDataSent == 0) {}
	}
}

uint8_t ch55x_keyboard_get_leds_state()
{
	return ledsState;
}

