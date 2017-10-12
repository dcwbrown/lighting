# Controller with LCD

target := controller

PATH := /d/projects/avr/avr8-gnu-toolchain/bin:$(PATH)

# PATH := /home/dave/projects/avr8/bin:$(PATH)
# Where avr8 is the avr 8-bit toolchain for windows downloaded from:
#   http://www.atmel.com/tools/atmelavrtoolchainforwindows.aspx


EFUSE := 0xff#   0: SELFPRGEN = 1: self programming disabled

HFUSE := 0x9f#   7: RSTDISBL  = 1: reset not disabled
             #   6: DWEN      = 0: DebugWIRE enabled
             #   5: SPIEN     = 0: serial programming enabled
             #   4: WDTON     = 1: watchdog timer not enabled
             #   3: EESAVE    = 1: EEPROM not preserved on chip erase
             # 2-0: BODLEVEL  = 7: brown-out detect not enabled

LFUSE := 0xE2#   7: CKDIV8    = 1: clock is not divided by 8
             #   6: CKOUT     = 1: clock output is not enabled
             # 5-4: SUTx      = 2: start-up time suitable for slowly rising power
             # 3-0: CKSELx    = 2: internal RS oscillator at 8 MHz.


.PHONY: all
.PHONY: program
.PHONY: clean
.PHONY: checkfuses
.PHONY: setfuses
.PHONY: debug       # Starts up dwdebug (was avrice and avr-gdb)
.PHONY: hvprogram


all: $(target).dump debug

program: $(target).bin
	dwdebug l $(target).elf,qs

%.bin: %.elf
	avr-objcopy -O binary $^ $@

%.dump: %.elf
	avr-objdump -D $^ > $@

$(target).elf: %.elf: %.o *.h
	avr-ld -o $@ *.o -M >$*.map

%.o: %.s
	avr-as -agls -gstabs -mmcu=atmega328 -o $@ $^ >$*.list

%.o: %.c *.h
	avr-gcc -Wall -Wextra -Os --std=gnu99 -gstabs -mmcu=atmega328 -o $@ $< >$*.list

clean:
	rm -f *.axf *.map *.o *.bin *.list *.dump *.map *.elf

run:
#	avarice -B 50kHz -g -w -P attiny45 :4242 & sleep 3 ; avr-gdb -tui -ex "layout asm" -ex "display/i $pc" -ex "target remote localhost:4242" $(target).elf
#	avarice -B 50kHz -g -w -P attiny45 :4242 & sleep 3 ; avr-gdb -ex "target remote localhost:4242" -ex "ni" -ex "ni" -ex "layout asm" $(target).elf
	../../dwire-debug/dwdebug.exe l controller.elf, g

debug:
	../../dwire-debug/dwdebug.exe l controller.elf


