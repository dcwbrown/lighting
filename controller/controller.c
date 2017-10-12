// Specifically for an ILI9481B



// Wiring from below


//      o                    +--------v--------+                   o
//  Rst o--------------------|28 PC5     dW   1|                   o
//   CS o--------------------|27 PC4     PD0  2|--------+   +------o D2
//   RS o--------------------|26 PC3     PD1  3|-------+|  / +-----o D3
//   WR o--------------------|25 PC2     PD2  4|-------||-+ / +----o D4
//   RD o--------------------|24 PC1     PD3  5|-------||--+ / +---o D5
//                           |23 PC0     PD4  6|-------||---+ / +--o D6
//      o              +-----|22 Gnd-+  +Vcc  7|       ||    / / +-o D7
//  Gnd o-------+------+     |21 ARef+-/-Gnd  8|-----+ ||   / / /
//      o       |   +--------|20 AVcc-+  PB6  9|     | |+--/-/-/---o D0
//   5v o-------|---+  +-----|19 Sck     PB7 10|---+ | +--/-/-/----o D1
//  3v3 o-------|-+ |  |+----|18 Miso    PD5 11|---|-|---+ / /     o SS
//      o       | | |  ||+---|17 Mosi    PD6 12|---|-|----+ /      o DI
//              | | |  |||+--|16 Ss      PD7 13|---|-|-----+       o DO
//              | | |  ||||+-|15 PB1     PB0 14|-+ | |             o SCK
//              | | |  ||||| +-----------------+ | | |             o
//  +------+    | | |  |||||                     | | |             o
//  |  5v 3|----|-|-+  ||||+-------------------+ | | |
//  |      |    | |    ||||   +--------        | | | |
//  | Out 2|----|-+----||||-+-|1 +3.3v       +---------+
//  | Gnd 1|----+-|----||||-|-|2 Gnd         | knob &  |
//  +------+    | |    |||| +-|3 Ce          | switch  |
//             /  |    |||+---|4 Csn         +---------+
//            /   |    +||----|5 Sck
//      0.1u +-||-+     |+----|6 Mosi
//           |    |     +-----|7 Miso
//       10u +-||-+           |8 Irq
//                            +--------



#define printf(...)  // printf's in ui.h unused in actual device

#include <stdint.h>
#include <util/delay_basic.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define countof(a) (sizeof(a)/sizeof(0[a]))

void delay(int ms) {while (ms) {_delay_loop_2(4000); ms--;}}

typedef uint8_t   u8;   typedef int8_t    s8;
typedef uint16_t  u16;  typedef int16_t   s16;
typedef uint32_t  u32;

typedef uint16_t FlashAddr;


// LCD control signals (active low)

#define Rs  0b11011111  // Reset
#define Cs  0b11101111  // Chip select
#define Cd  0b11110111  // Command mode
#define Wr  0b11111011  // Write strobe
#define Rd  0b11111101  // Read strobe

#define LcdIdle 0b00111110  // Reset, Cs, Cd Ww and Rd inactive

#define CsCdWr    (LcdIdle & Cs & Cd & Wr)  // CS, CD and WR active
#define CsCdNwr   (LcdIdle & Cs & Cd     )  // CS and CD active, WR inactive
#define CsNcdNwr  (LcdIdle & Cs          )  // CS active, CD and WR inactive
#define CsNcdWr   (LcdIdle & Cs      & Wr)  // CS active, CD inactive, WR active

//#define Bytes(...) (u8[]){__VA_ARGS__}
//template <int len> void CommandLcd(u8 const(&buf)[len]) {

#define Bytes(...) (u8[]){__VA_ARGS__}, sizeof((u8[]){__VA_ARGS__})

void SendCommand(u8 cmd) {
  PORTC = CsCdWr;  // CS, CD and WR active
  PORTD = cmd;
  PORTC = CsCdNwr;  // CS, CD active, WR inactive
  PORTC = CsNcdNwr;  // CS remains active, CD goes high to return to data mode
}

void CommandLcd(const u8 *buf, u8 len) {
  SendCommand(buf[0]);
  buf++; len--;
  while (len--) {
    PORTC = CsNcdWr;   // CS and WR active, CD inactive
    PORTD = *(buf++);
    PORTC = CsNcdNwr;  // CS active, WR and CD inactive
  }
  PORTC = LcdIdle;  // CS and CD both go inactive
}

void SendDataWord(u16 w) {
  PORTC = CsNcdWr; PORTD = w / 256; PORTC = CsNcdNwr;
  PORTC = CsNcdWr; PORTD = w % 256; PORTC = CsNcdNwr;
}

void RepeatDataWord(u16 w, u8 len) { // len 0 => 256 times.
  if (w/256 == w%256) { // Optimise for common case of all black or all white and some others
    PORTD = w / 256;
    do {PORTC=CsNcdWr; PORTC=CsNcdNwr; PORTC=CsNcdWr; PORTC=CsNcdNwr; len--;} while (len);
  } else {
    do {
      PORTC = CsNcdWr; PORTD = w / 256; PORTC = CsNcdNwr;
      PORTC = CsNcdWr; PORTD = w % 256; PORTC = CsNcdNwr;
      len--;
    } while (len);
  }
}

u8 u6sqrt(u16 n) {  // from 12 bit (0..4095) to 6 bit (0 .. 63)
  u8 result;
  // n is passed in rB:rA
  // uses
  //   r23 - mask
  //   r22 - sqrt
  //   r21 - check
  asm(
    "        ldi   r23,0x20       ; mask (sufficient for 0 <= n <= 4095) \n"
    "        eor   r22,r22        ; sqrt                                 \n"
    "                                                                    \n"
    "isqr2:  mov   r21,r22        ; check = sqrt                         \n"
    "        add   r21,r23        ; check += mask                        \n"
    "        mul   r21,r21        ; r1:r0 = check*check                  \n"
    "        cp    r0,%A1         ; compare check*check with parameter n \n"
    "        cpc   r1,%B1                                                \n"
    "        brcc  isqr4          ; if check*check > n                   \n"
    "                                                                    \n"
    "        mov   r22,r21        ; sqrt = check                         \n"
    "                                                                    \n"
    "isqr4:  lsr   r23            ; mask >>= 1                           \n"
    "        brne  isqr2          ; loop if mask nonzero                 \n"
    "                                                                    \n"
    "        eor   r1,r1          ; restore r1==0 invariant              \n"
    "        mov   %0,r22         ; return sqrt                          \n"
  : "=r" (result)             // Result should be assigned to register %0
  : "r"  (n)                  // Parameter will be found in registers %A1 and %B1
  : "r21", "r22", "r23");
  return result;
}

void WriteRegion(u16 x0, u16 y0, u16 x1, u16 y1) {
  SendCommand(0x2A); SendDataWord(x0); SendDataWord(x1);
  SendCommand(0x2B); SendDataWord(y0); SendDataWord(y1);
  SendCommand(0x2C);
}

void InitLCD() {
  // Port B: Set all pins as inputs with pull-ups activated.
  DDRB  = 0b00000000;
  PORTB = 0b11111111;

  // Enable pin change interrupts for combined knob/pushbutton connections
  PCMSK0 = 0x83;  // PORTB pins 7, 1 and 0.
  PCICR  = 1;     // Enable interrupt on PCINT pins 0 through 7 (where enabled in PCMSK0)

  // PORTD - LCD byte data and command io
  DDRD  = 0b11111111;    // Port D is output
  PORTD = 0b00000000;

  //          R
  //          sCRWR
  //          tSSRD
  PORTC = 0b00111111;    // Set RD, WR, CD, CS and Reset outputs high, set pull up on inputs.
  DDRC  = 0b00111111;    // Set RD, WR, CD, CS and Reset pins as outputs.

  delay(50);
  PORTC = 0b00011111;    // Hold reset low for 2ms
  delay(2);
  PORTC = 0b00111111;    // Reset high and wait 50ms
  delay(50);

  CommandLcd(Bytes(0x01));                               // Soft Reset and wait 20 ms (more than 10 frame times)
  delay(20);

  PORTC = 0b00111110; // Debug - take PORTC.0 low to trigger oscilloscope.

  CommandLcd(Bytes(0x28));                               // Display Off
  CommandLcd(Bytes(0x3A, 0x55));                         // Pixel read=565, write=565.
  CommandLcd(Bytes(0xB0, 0x00));                         // unlocks E0, F0
  CommandLcd(Bytes(0xB3, 0x02, 0x00, 0x00, 0x00));       // Frame Memory, interface [02 00 00 00] (default on reset)
  CommandLcd(Bytes(0xB4, 0x00));                         // Frame mode [00] (default on reset)
  CommandLcd(Bytes(0xD0, 0x07, 0x42, 0x18));             // Set Power [00 43 18] x1.00, x6, x3
  CommandLcd(Bytes(0xD1, 0x00, 0x07, 0x10));             // Set VCOM  [00 00 00] x0.72, x1.02
  CommandLcd(Bytes(0xD2, 0x01, 0x02));                   // Set Power for Normal Mode [01 22]
  CommandLcd(Bytes(0xD3, 0x01, 0x02));                   // Set Power for Partial Mode [01 22]
  CommandLcd(Bytes(0xD4, 0x01, 0x02));                   // Set Power for Idle Mode [01 22]
//CommandLcd(Bytes(0xC0, 0x12, 0x3B, 0x00, 0x02, 0x11)); // Panel Driving BGR for 1581 [10 3B 00 02 11]
  CommandLcd(Bytes(0xC0, 0x10, 0x3B, 0x00, 0x02, 0x11)); // Panel Driving BGR for 1581 [10 3B 00 02 11]
  CommandLcd(Bytes(0xC1, 0x10, 0x10, 0x88));             // Display Timing Normal [10 10 88]
  CommandLcd(Bytes(0xC5, 0x03));                         // Frame Rate [03]
  CommandLcd(Bytes(0xC6, 0x02));                         // Interface Control [02]

//CommandLcd(Bytes(0xC8, 0x00, 0x32, 0x36, 0x45,         // Gamma settings
//                 0x06, 0x16, 0x37, 0x75, 0x77,
//                 0x54, 0x0C, 0x00));
//
  CommandLcd(Bytes(0x11));                               // Exit sleep mode.
  delay(150);
  CommandLcd(Bytes(0x29));                               // Display on.
  CommandLcd(Bytes(0x20));                               // Exit invert mode.
  CommandLcd(Bytes(0x36, 0x0A));                         // Default address mode. Sets BGR order and horiz flip.
}


#include "ui.h"
#include "wireless.h"


ISR(PCINT0_vect)     {PinChangeInterrupt();}
ISR(BADISR_vect)     {}
ISR(TIMER0_OVF_vect) {Timer0Interrupt();}

u8 update[4]  = {0}; // Strips updated
u8 colours[4][4] = {  // colours[ledstrip][colourindex]
  {0, 0, 0, 20}, // Strip 0 '15925'
  {0, 0, 0, 20}, // Strip 1 '25925'
  {0, 0, 0, 20}, // Strip 2 '35925'
  {0, 0, 0, 20}  // Strip 3 '45925'
};
s8 destination = -1;  // ledstrip for which transmission is underway, -1 otherwise


void CheckSendStatus() {
  u8 status = 0;
  if (destination >= 0) if ((status = RfStatus()) & 0x30) {
    if (status & 0x10) WriteRfCmd(FLUSH_TX); // Flush FIFO if not already written
    WriteRfReg(STATUS, 0x70);     // Clear all three interrupt flags
    destination = -1;
  }
}

void CheckUpdate() {
  for (u8 i=0; i<countof(update); i++) {
    if (update[i]) {
      destination = i;
      RfWrite(i, colours[i]);
      update[i] = 0;
      break;
    }
  }
}

void SetColour(u8 knob) {
  for (int strip=0; strip<4; strip++) {
    colours[strip][knob] = knobs[knob].nextstep;
    update[strip] = 1;
  }
}

void Cycle() {
  if (destination < 0) CheckUpdate(); else CheckSendStatus();
  for (int knob=0; knob<4; knob++) {
    if (knobs[knob].curstep != knobs[knob].nextstep) {
      SetColour(knob);
      UpdatePointer(&knobs[knob]);
    }
  }
}


int main() {

  // Prepare timer counter 0 for use as 32ms debounce timer
  TCCR0A = 0x00;  // Normal operation, count up, overflow at 0xFF.
  TCCR0B = 0x05;  // No output compare, divide processor clock by 1024.
  TIMSK0 = 0x00;  // Initially do not generate timer interrupt.

  InitLCD();

  Initscreen();

  InitWireless();

  //sendLed(0x4, 0x4, 0x0, 0x20);

  //wirelessTest();

  sei();

  while (1) {Cycle();}

  return 0;
}


// 87ceeb  1000 0111   1100 1110   1110 1011    1000 0110 0111 1101 867D
// FFDEAD  11111111 11011110 10101101   1111 1110 1111 0101  FEF5
// Wheat2  EED8AE   11101110 11011000 10101110   1110 1110 1101 0101   EED5
// Wheat3  CDBA96   11001101 10111010 01010110   1100 1101 1100 1010   CDCA

