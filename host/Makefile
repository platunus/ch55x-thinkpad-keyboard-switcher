TARGET = mouse_swithcer_host

FREQ_SYS=24000000
EXTRA_FLAGS = -DMAX_PACKET_SIZE=64

XRAM_SIZE = 0x380
XRAM_LOC = 0x080

C_FILES = \
	main.c \
	usbhost.c \
	ch55x_spimaster.c \
	debug.c

include ../ch554_sdcc/examples/Makefile.include

program:
	chflasher.py -w -p /dev/ttyACM1 -f $(TARGET).bin

program2:
	chflasher.py -w -p /dev/ttyUSB0 -f $(TARGET).bin

