#define VENDOR_ID	0xef, 0x17		// 17ef
#define PRODUCT_ID	0x47, 0x60		// 6047

#define MANUFACTURER_DESCRIPTION	\
	'L', 0x00, 'e', 0x00, 'n', 0x00, 'o', 0x00, \
	'v', 0x00, 'o', 0x00
#define PRODUCT_DESCRIPTION	\
	'T', 0x00, 'h', 0x00, 'i', 0x00, 'n', 0x00, \
	'k', 0x00, 'P', 0x00, 'a', 0x00, 'd', 0x00, \
	' ', 0x00, 'C', 0x00, 'o', 0x00, 'm', 0x00, \
	'p', 0x00, 'a', 0x00, 'c', 0x00, 't', 0x00, \
	' ', 0x00, 'U', 0x00, 'S', 0x00, 'B', 0x00, \
	' ', 0x00, 'K', 0x00, 'e', 0x00, 'y', 0x00, \
	'b', 0x00, 'o', 0x00, 'a', 0x00, 'r', 0x00, \
	'd', 0x00, ' ', 0x00, 'w', 0x00, 'i', 0x00, \
	't', 0x00, 'h', 0x00, ' ', 0x00, 'T', 0x00, \
	'r', 0x00, 'a', 0x00, 'c', 0x00, 'k', 0x00, \
	'P', 0x00, 'o', 0x00, 'i', 0x00, 'n', 0x00, \
	't', 0x00
#define SERIAL_NUMBER '0'
#define SERIAL_NUMBER_DESCRIPTION	\
	'2', 0x00, '0', 0x00, '2', 0x00, '2', 0x00, \
	'0', 0x00, '4', 0x00, '1', 0x00, '1', 0x00, \
	'-', 0x00, SERIAL_NUMBER, 0x00

#define MOUSE_LEFT_BUTTON	1
#define MOUSE_RIGHT_BUTTON	2
#define MOUSE_MIDDLE_BUTTON	4
#define MOUSE_EXTEND1_BUTTON	8
#define MOUSE_EXTEND2_BUTTON	16
#define MOUSE_BUTTONS_MASK	0b00011111

/*
Memory map:
EP0 Buf     00 - 07 
EP1 Buf     10 - 4f 
EP2 Buf     50 - 8f 
*/
#define FIXED_ADDRESS_EP0_BUFFER    0x0000  
#define FIXED_ADDRESS_EP1_BUFFER    0x0010 
#define FIXED_ADDRESS_EP2_BUFFER    0x0050 

void ch55x_device_init();
void ch55x_mouse_move(int8_t delta_x, int8_t delta_y);
void ch55x_mouse_scroll(int8_t delta);
void ch55x_mouse_press(uint8_t buttons);
void ch55x_mouse_release(uint8_t buttons);
void ch55x_mouse_release_all();
void ch55x_keyboard_press(uint8_t modifier, uint8_t key);
void ch55x_keyboard_release(uint8_t modifier);

void ch55x_mouse_send(uint8_t *buf, uint8_t len);
void ch55x_keyboard_send(uint8_t *buf, uint8_t len);
uint8_t ch55x_keyboard_get_leds_state();

void ch55x_usbdevice_interrupt_handler(void) __interrupt (INT_NO_USB);

