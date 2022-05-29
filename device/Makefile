TARGET = mouse_switcher_device

# Adjust the XRAM location and size to leave space for the USB DMA buffers
# Buffer layout in XRAM:
# 0x0000 Ep0Buffer[8]
# 0x0010 Ep1Buffer[64]
# 0x0050 EP2Buffer[64]
#
# This takes a total of 256bytes, so there are 768 bytes left.
XRAM_SIZE = 0x0300
XRAM_LOC = 0x0100

CODE_SIZE = 0x3800

FREQ_SYS = 24000000

EXTRA_FLAGS = --opt-code-speed

C_FILES = \
	main.c \
	ch55x_usbdevice.c \
	../ch554_sdcc/include/debug.c

include ../ch554_sdcc/examples/Makefile.include

program:
	chflasher.py -w -f mouse_switcher_device.bin 
