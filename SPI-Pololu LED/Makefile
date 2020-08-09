COMPILE = avr-gcc -std=gnu99 -Wall -pedantic -Os -Iusbdrv -I. -mmcu=atmega8 -DF_CPU=8000000UL

OBJECTS = main.o

AVRDUDE = avrdude -p atmega8 -P usb -c usbasp -U flash:w:main.hex -U hfuse:w:0xD9:m -U lfuse:w:0xC4:m
#AVRDUDE = avrdude -p atmega88 -P usb -c usbasp -U flash:w:main.hex -U hfuse:w:0xDF:m -U lfuse:w:0xE2:m

# symbolic targets:
all:	main.hex

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@

flash:	all
	$(AVRDUDE)

# ATMEGA88 fuse bytes (remember that they are active low)

# Fuse high byte:
# 0xDF = 1 1 0 1   1 1 1 1 <-- BODLEVEL0
#        ^ ^ ^ ^   ^ ^ ^------ BODLEVEL1
#        | | | |   | +-------- BODLEVEL2
#        | | | |   + --------- EESAVE
#        | | | +-------------- WDTON
#        | | +---------------- SPIEN
#        | +------------------ DWEN
#        +-------------------- RSTDISBL
# Fuse low byte:
# 0xEF = 1 1 1 0   1 1 1 1
#        ^ ^ \ /   \--+--/
#        | |  |       +------- CKSEL 3..0
#        | |  +--------------- SUT 1..0
#        | +------------------ CKOUT
#        +-------------------- CKDIV8

clean:
	rm -f main.hex .lst main.obj main.cof main.list main.map main.eep.hex main.bin *.o usbdrv/*.o main.s usbdrv/*.s

# file targets:
main.bin:	$(OBJECTS)
	$(COMPILE) -o main.bin $(OBJECTS)

main.hex:	main.bin
	rm -f main.hex main.eep.hex
	avr-objcopy -j .text -j .data -O ihex main.bin main.hex
#	./checksize main.bin
# do the checksize script as our last action to allow successful compilation
# on Windows with WinAVR where the Unix commands will fail.

disasm:	main.bin
	avr-objdump -d main.bin

#cpp:
#	$(COMPILE) -E main.c
