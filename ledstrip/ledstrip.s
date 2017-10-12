;;;       Ledstrip - Receive commands from nRF24L01+ and drive led strip
;;
;;
;;
;;
;;
;;

;;        Wiring from above
;
;                           +------+
;   5v <--------------------|  5v 3|<---+-----------------------------------------------< 5v
;       0.1u  /--||--\      |      |    |         +-----------------
;        10u +---||---+-----| Out 2|----|------+--|1 +3.3v
;  Gnd <-----+--------------| Gnd 1|----|------|--|2 Gnd
;            |              +------+    |      +--|3 Ce
;            |    +---------------------|---+
;            |    |   +------v-------+  |   |     |
;            |    |   |1 dW     Vcc 8|--+   +-----|4 Csn
;            |    +---|2 Pb3    Scl 7|------------|5 Sck
;  LDO <-----|--------|3 Pb4   Miso 6|------------|6 Mosi
;            +--------|4 Gnd   Mosi 5|------------|7 Miso
;            |        +--------------+            |8 Irq
;            |                                    +-----------------
;            +--------------------------------------------------------------------------< Gnd





;         ATTiny25/45/85 registers used in this program

          .equ   USICR, 0x0D ; Universal serial interface control register
          .equ   USISR, 0x0E ; Universal serial interface status register
          .equ   USIDR, 0x0F ; Universal serial interface data register
          .equ   PCMSK, 0x15 ; Pin change mask
          .equ   DDRB,  0x17 ; Data direction
          .equ   PORTB, 0x18 ; Data port
          .equ   EECR,  0x1C ; EEProm control register
          .equ   EEDR,  0x1D ; EEProm data register
          .equ   EEARL, 0x1E ; EEProm address register low
          .equ   EEARH, 0x1F ; EEProm address register high
          .equ   CLKPR, 0x26 ; Clock prescale
          .equ   OCR0A, 0x29 ; Output compare register A
          .equ   TCCR0A,0x2A ; Timer/counter control register A
          .equ   TCNT0, 0x32 ; Timer/counter 0 current count
          .equ   TCCR0B,0x33 ; Timer/counter control register B
          .equ   MCUCR, 0x35 ; MCU control register
          .equ   TIMSK, 0x39 ; Timer/counter interrupt mask register
          .equ   GIFR,  0x3A ; General interrupt flag register
          .equ   GIMSK, 0x3B ; General interrupt mask register
          .equ   SPL,   0x3D ; Stack pointer (low byte)
          .equ   SPH,   0x3E ; Stack pointer (high byte)
          .equ   SREG,  0x3F ; Status register

          .equ   CSN,3       ; PORTB bit connected to nRF24L01+ SPI chip select not
          .equ   LDO,4       ; PORTB bit used for output to LED strip




;         Global registers
;
;         r12 - Red
;         r13 - Green
;         r14 - Blue
;         r15 - Warm white




;         Flash memory content

          .org   0
interrupt_vectors:
          rjmp   reset       ; 0x0000  RESET
          reti               ; 0x0001  INT0         - external interrupt request 0
          reti               ; 0x0002  PCINT0       - pin change interrupt request 0
          reti               ; 0x0003  TIMER1_COMPA - timer/counter 1 compare match A
          reti               ; 0x0004  TIMER1_OVF   - timer/counter 1 overflow
          reti               ; 0x0005  TIMER0_OVF   - timer/counter 0 overflow
          reti               ; 0x0006  EE_RDY       - EEPROM ready
          reti               ; 0x0007  ANA_COMP     - analog comparator
          reti               ; 0x0008  ADC          - ADC conversion complete
          reti               ; 0x0009  TIMER1_COMPB - timer/counter 1 compare match B
          reti               ; 0x000A  TIMER0_COMPA - timer/counter 0 compare match A
          reti               ; 0x000B  TIMER0_COMPB - timer/counter 0 compare match B
          reti               ; 0x000C  WDT          - watchdog time-out
          reti               ; 0x000D  USI_START    - universal serial interface start
          reti               ; 0x000E  USI_OVF      - universal serial interface overflow






;;;;      Timing

;;        msdelay
;
;         entry r26/27 - Number of milliseconds to delay

msdelay:  push  r24          ; 2
          push  r25          ; 2

ms2:      ldi   r24,lo8(1999); 1
          ldi   r25,hi8(1999); 1

ms4:      sbiw  r24,1        ; 2
          brne  ms4          ; 2/1

          sbiw  r26,1
          brne  ms2

          pop   r25
          pop   r24
          ret


          .macro MSEC delay
          push  r26
          push  r27
          ldi   r26,lo8(\delay)
          ldi   r27,hi8(\delay)
          rcall msdelay
          pop   r27
          pop   r26
          .endm






;;;;      SK6812 LED driver





;;;       LedByte - Send byte to LED string
;;
;;        entry  r16 - byte to send
;;
;;        Device rquirements:
;;        0 sent as 300ns(150-450ns) high followed by 900ns(750-1050ns) low.
;;        1 sent as 600ns(450-750ns) high followed by 600ns(450-750ns) low.

LedByte:

;         Pixel send loop.
;         0 sent as 10 cycles: 3 cycles high, 7 cycles low = 375ns + 975ns = 1250ns
;         1 sent as 10 cycles: 5 cycles high, 5 cycles low = 625ns + 625ns = 1250ns

          ldi    r17,8

seb2:     lsl    r16         ; (1) Get next bit to C
          sbi    PORTB,LDO   ; (2) Go high
          brcs   seb4        ; (1/2) if sending one

          cbi    PORTB,LDO   ; (2) Sending 0. Go low after 3 cycles = 375ns
          dec    r17         ; (1)
          nop                ; (1)
          brne   seb2        ; (2) advance to next bit after low for 7 cycles - 975ns

          rjmp   seb6

seb4:     dec    r17         ; (1)
          cbi    PORTB,LDO   ; (2) Sending 1. Go low after 5 cycles = 625ns
          brne   seb2        ; (2) advance to next bit after low for 5 cycles - 625ns

seb6:     ret






;;;       Set LED colour
;;
;;        entry  r12 - Red
;;               r13 - Green
;;               r14 - Blue
;;               r15 - Warm white

SetColour:

;         Send reset

          cbi    PORTB,LDO   ; B4 := 0

          ldi    r31,16000>>8 ; Set countdown for 8 MSEC         ldi    r30,16000&0xFF

col2:     sbiw   30,1        ; (2)
          brne   col2        ; (2)

;         Send the RGBWW value all 144 pixels

          ldi    r29,(144)>>8
          ldi    r28,(144)&0xFF

col4:     mov    r16,r13     ; Send green
          rcall  LedByte

          mov    r16,r12     ; Send red
          rcall  LedByte

          mov    r16,r14     ; Send blue
          rcall  LedByte

          mov    r16,r15     ; Send warm white
          rcall  LedByte

          sbiw   r28,1
          brne   col4

          ret






;;;;      NRFL24L01 wireless driver






;;;       spi - Transfer byte to/from NRF24L01
;;
;;        entry  r16 - command
;;
;;        exit   r16 - data returned by NRF24L01

spi:
          out    USIDR,r16   ; Command to serial interface data register

          ldi    r16,0x40    ; Clear any pending counter overflow flag
          out    USISR,r16

;         USI control register bits set (0x1B):
;         7    USISIE  0   No start condition detector interrupt
;         6    USIOIE  0   No counter overflow interrupt
;         5,4  USIWM   01  Three wire mode
;         3,2  USICS   10  External clock (but see USICLK)
;         1    USICLK  1   Make USITC the clock source for the 4 bit counter
;         0    USITC   1   Start by setting clock high

          ldi    r16,0x1B    ; Set 3 wire mode, s/w clock strobe and toggle sck

spi2:     out    USICR,r16
          in     r17,USISR   ; Check for counter overflow, i.e. 8 bits complete
          sbrs   r17,6       ; Skip if all 8 bits transferred
          rjmp   spi2

          in     r16,USIDR
          ret






;;        nRF24L01+ Registers

          .equ  CONFIG,       0x00
          .equ  EN_AA,        0x01
          .equ  EN_RXADDR,    0x02
          .equ  SETUP_RETR,   0x04
          .equ  RF_CH,        0x05
          .equ  RF_SETUP,     0x06
          .equ  STATUS,       0x07
          .equ  RX_ADDR_P0,   0x0A
          .equ  RX_ADDR_P1,   0x0B
          .equ  TX_ADDR,      0x10
          .equ  RX_PW_P0,     0x11
          .equ  RX_PW_P1,     0x12
          .equ  FIFO_STATUS,  0x17
          .equ  FEATURE,      0x1D
          .equ  DYNPD,        0x1C


;;        nRF24L01+ command macros

          .equ  W_REGISTER,   0x20
          .equ  FLUSH_TX,     0xE1
          .equ  FLUSH_RX,     0xE2
          .equ  W_TX_PAYLOAD, 0xA0

          .macro WriteRfCmd cmd
          cbi    PORTB,CSN   ; Activate nRF24L01+ chip select
          ldi    r16,\cmd
          rcall  spi
          sbi    PORTB,CSN   ; Activate nRF24L01+ chip select
          .endm

          .macro WriteRfReg reg, val
          cbi    PORTB,CSN   ; Activate nRF24L01+ chip select
          ldi    r16,0x20|\reg
          rcall  spi
          ldi    r16,\val
          rcall  spi
          sbi    PORTB,CSN   ; Activate nRF24L01+ chip select
          .endm

          .macro ReadRfReg reg
          cbi    PORTB,CSN   ; Activate nRF24L01+ chip select
          ldi    r16,\reg
          rcall  spi
          ldi    r16,0xff
          rcall  spi
          sbi    PORTB,CSN   ; Activate nRF24L01+ chip select
          .endm




;;;       Get nRF24L01+ status
;
;         exit  r16 - device status register

Status:   cbi    PORTB,CSN   ; Activate nRF24L01+ chip select
          ldi   r16,0xff
          rcall spi
          sbi    PORTB,CSN   ; Activate nRF24L01+ chip select
          ret





;;;       ReadEEProm - load one byte from eeprom
;
;         entry r16 - address
;
;         exit  r16 - loaded value

ReadEEProm:
          out   EEARL,r16              ; Address 7-0
          ldi   r16,0                  ; Address 15-8 is zero
          out   EEARH,r16
          sbi   EECR,0                 ; Start eeprom read by writing EERE
          in    r16,EEDR
          ret






;;;       WirelessInit
;
;

WirelessInit:
          sbi    PORTB,CSN   ; Activate nRF24L01+ chip select
          MSEC  100
          WriteRfCmd FLUSH_TX
          WriteRfReg CONFIG,     0x05  ; Power down (in RX mode) with 2 byte CRCs
          MSEC  5
          WriteRfReg SETUP_RETR, 0x34  ; 1ms per retry, 4 retries
          WriteRfReg RF_SETUP,   0x04  ; 1Mbps data rate, -6dBm power
          WriteRfReg FEATURE,    0x00  ; Disable EN_DPL, EN_ACK_PAY and EN_DYN_ACK
          WriteRfReg DYNPD,      0x00  ; Disable dynamic payload on all pipes
          WriteRfReg STATUS,     0x70  ; Clear all three interrupt flags
          WriteRfReg RF_CH,        76  ; This channel should be universally safe and not bleed over into adjacent spectrum.
          WriteRfCmd FLUSH_TX
          WriteRfCmd FLUSH_RX

;         Set our wireless address

          cbi    PORTB,CSN   ; Activate nRF24L01+ chip select
          ldi    r16,0x20|RX_ADDR_P1
          rcall  spi

          ldi   r16,1                  ; Our address is stored in eeprom location 1
          rcall ReadEEProm
          subi  r16,-'1'
          rcall  spi

          ldi    r16,'5'
          rcall  spi

          ldi    r16,'9'
          rcall  spi

          ldi    r16,'2'
          rcall  spi

          ldi    r16,'5'
          rcall  spi
          sbi    PORTB,CSN   ; Activate nRF24L01+ chip select

          WriteRfReg RX_PW_P1,   4     ; Payload length
          WriteRfReg EN_RXADDR,  2     ; Enable Rx on pipe 1

          WriteRfReg CONFIG,     0x0F  ; Power up in RX mode with 2 byte CRCs
          MSEC  5
          ret






;;;;      Initialisation






;;;       reset - initialise chip and start main program
;;
;;

reset:

;         Set stack pointer to allow 32 bytes above IO space for stack

          ldi    r16,0x7f
          out    SPL,r16

          ldi    r16,0
          out    SPH,r16


;         Set clock to 8MHz

          ldi    r16,0x80    ; Clock prescale change enable
          ldi    r17,0       ; Prescale division factor 1
          out    CLKPR,r16   ; Enable clock change
          out    CLKPR,r17   ; Switch CPU from 1MHz to 8MHz



;         Initialise PORTB:
;
;         ATtiny85      YL-105    NRF24L01    SK6812
;         ----------    ------    --------    ------
;         1 dW          n/c
;         2 PB3         2 CSN     4 CSN
;         3 PB4                               DIN
;         4 Gnd
;         5 PB0/DI      5 MISO    7 MISO
;         6 PB1/DO      4 MOSI    6 MOSI
;         7 PB2/SCK     3 SCK     5 SCK
;         8 Vcc
;
;                            PB5     PB4     PB3     PB2     PB1     PB0
;         +-------+-------+-------+-------+-------+-------+-------+-------+
;         |       |       |       |  LED  |  CSN  |  SCK  |  DO   |  DI   |
;         |       |       |       |  DIN  |Output |Output |Output | Input |
;         +-------+-------+-------+-------+-------+-------+-------+-------+
; Initial values:
; PORTB   |   0   |   0   |   1   |   0   |   1   |   0   |   0   |   0   | $28
; DDR     |   0   |   0   |   0   |   1   |   1   |   1   |   1   |   0   | $1e
;
; Note    Unused inputs are initialised high to enable the pullup resistors
;         Output CSN is active low and therefore is initialised high
;         Input DI is initialised high to activate its pullup resistor


          ldi    r16,0x28    ; Enable input pullups so floating inputs do not cause problems
          out    PORTB,r16

          ldi    r16,0x1E    ; CSN, SCK and DO LED DIN as output, remainder as input
          out    DDRB,r16


;         Set initial LED colour

          ldi    r16,2       ; Red stored value
          rcall  ReadEEProm
          mov    r12,r16

          ldi    r16,3       ; Green stored value
          rcall  ReadEEProm
          mov    r13,r16

          ldi    r16,4       ; Blue stored value
          rcall  ReadEEProm
          mov    r14,r16

          ldi    r16,5       ; Warm white stored value
          rcall  ReadEEProm
          mov    r15,r16

          rcall  SetColour

;         Initialise the nRF24L01+

          rcall  WirelessInit

;         Wait for incoming led colour settings

led2:     rcall Status       ; Wait for completion status
          sbrs  r16,6        ; Skip if receive data ready (RX_DR)
          rjmp  led2

;         Read updated led settings

led4:     cbi    PORTB,CSN   ; Activate nRF24L01+ chip select
          ldi   r16,0x61     ; Read RX payload
          rcall spi
          ldi   r16,0xFF
          rcall spi
          mov   r12,r16      ; Red
          ldi   r16,0xFF
          rcall spi
          mov   r13,r16      ; Green
          ldi   r16,0xFF
          rcall spi
          mov   r14,r16      ; Blue
          ldi   r16,0xFF
          rcall spi
          mov   r15,r16      ; Warm white
          sbi    PORTB,CSN   ; Activate nRF24L01+ chip select

;         Send the updated colour setting to all the leds in the strip

          rcall SetColour
          WriteRfReg STATUS,0x40 ; Clear RX_DR data ready interrupt flag
          ReadRfReg FIFO_STATUS
          sbrs  r16,0        ; Skip if all pending payloads have been read
          rjmp  led4         ; Immediately load next payload

;         Go back and wait for another packet

          rjmp led2          ; Wait for another setting
