# Rule input $^, rule output $@

PATH := /d/projects/avr/avr8-gnu-toolchain/bin:/d/projects/avr/avrdude:$(PATH)
# Where avr8 is the avr 8-bit toolchain for windows downloaded from:
#   http://www.atmel.com/tools/atmelavrtoolchainforwindows.aspx


target := ledstrip

device := attiny85

efuse  := 0xFE#   0: SELFPRGEN = 0: self programming enabled

hfuse  := 0x97#   7: RSTDISBL  = 1: reset not disabled
              #   6: DWEN      = 0: DebugWIRE enabled
              #   5: SPIEN     = 0: serial programming enabled
              #   4: WDTON     = 1: watchdog timer not enabled
              #   3: EESAVE    = 0: EEPROM preserved on chip erase
              # 2-0: BODLEVEL  = 111: brown-out detect not enabled

lfuse  := 0xE2#   7: CKDIV8    = 1: clock is not divided by 8 => 8MHz instruction cycle
              #   6: CKOUT     = 1: clock output is not enabled
              # 5-4: SUTx      = 10: start-up time suitable for slowly rising power
              # 3-0: CKSELx    = 0010: internal RC oscillator at 8 MHz.


.PHONY: all
.PHONY: program
.PHONY: clean
.PHONY: checkfuses
.PHONY: setfuses
.PHONY: debug       # Starts up dwdebug (was avrice and avr-gdb)
.PHONY: hvprogram


all: $(target).dump debug

program: $(target).bin
	avrdude -c dragon_isp -p t45 -U flash:w:$(target).bin:r

hvprogram: $(target).bin
	avrdude -c dragon_hvsp -p t45 -U flash:w:$(target).bin:r

%.bin: %.elf
	avr-objcopy -O binary $^ $@

%.dump: %.elf
	avr-objdump -D $^ > $@

$(target).elf: %.elf: %.o
	avr-ld -o $@ *.o -M >$*.map

%.o: %.s
	avr-as -agls -gstabs -mmcu=$(device) -o $@ $^ >$*.list

clean:
	rm -f *.axf *.map *.o *.bin *.list *.dump *.map *.elf

checkfuses:
	avrdude -c usbtiny -p $(device) -u -U efuse:v:$(efuse):m -U hfuse:v:$(hfuse):m -U lfuse:v:$(lfuse):m

setfuses:
	avrdude -c usbtiny -p $(device) -u -U efuse:w:$(efuse):m -U hfuse:w:$(hfuse):m -U lfuse:w:$(lfuse):m

run:
#	avarice -B 50kHz -g -w -P $(device) :4242 & sleep 3 ; avr-gdb -tui -ex "layout asm" -ex "display/i $pc" -ex "target remote localhost:4242" $(target).elf
#	avarice -B 50kHz -g -w -P $(device) :4242 & sleep 3 ; avr-gdb -ex "target remote localhost:4242" -ex "ni" -ex "ni" -ex "layout asm" $(target).elf
	../../dwire-debug/dwdebug.exe device u2,l ledstrip.elf, g

debug:
	../../dwire-debug/dwdebug.exe device u2,l ledstrip.elf




