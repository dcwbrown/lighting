// Transfer byte to and from nRF24L01+
// SPI ports:
//
//    SCK  = PB5  0x20
//    MISO = PB4  0x10
//    MOSI = PB3  0x08
//    nSS  = PB2  0x04
//
// Note, the wireless SPI implementation shares port B with the knob,
// which uses PB0 and PB1 as inputs.

u8 spi(u8 cmd) {SPDR = cmd; while (!(SPSR & (1<<SPIF))); return SPDR;}

#define CSN0 PORTB &= ~0x04;  // Bring nRF24L01+ slave select low (active)
#define CSN1 PORTB |=  0x04;  // Bring nRF24L01+ slave select high (inactive)

// Registers
#define CONFIG       0x00
#define EN_AA        0x01
#define EN_RXADDR    0x02
#define SETUP_RETR   0x04
#define RF_CH        0x05
#define RF_SETUP     0x06
#define STATUS       0x07
#define RX_ADDR_P0   0x0A
#define RX_ADDR_P1   0x0B
#define TX_ADDR      0x10
#define RX_PW_P0     0x11
#define RX_PW_P1     0x12
#define FEATURE      0x1D
#define DYNPD        0x1C

// Commands
#define W_REGISTER   0x20
//#define ACTIVATE   0x50
#define FLUSH_TX     0xE1
#define FLUSH_RX     0xE2
#define W_TX_PAYLOAD 0xA0

void WriteRfCmd(u8 cmd)         {CSN0; spi(cmd); CSN1;}
void WriteRfReg(u8 reg, u8 val) {CSN0; spi(W_REGISTER|reg); spi(val); CSN1;}
void WriteRfAdr(u8 reg, u8 *adr) {
  CSN0; spi(W_REGISTER|reg); for (int i=0; i<5; i++) spi(adr[i]); CSN1;
}

void ReadRfReg(u8 reg) {CSN0; spi(reg); spi(0xFF); CSN1;}

void InitWireless() {
  DDRB  = 0x2C;  // nSS, SCK and MOSI are outputs
  PORTB = 0xC7;  // Lower SCK and MOSI, nSS remains high
  SPCR  = (1<<SPE)|(1<<MSTR)|(1<<SPR0);
  CSN1; delay(100);
  WriteRfCmd(FLUSH_TX);
  WriteRfReg(CONFIG,     0x0E);     // Power up in TX mode with 2 byte CRCs
  delay(5);
  WriteRfReg(SETUP_RETR, 0x34);     // 1ms per retry, 4 retries
  WriteRfReg(RF_SETUP,   0x04);     // 1Mbps data rate, -6dBm power
  WriteRfReg(FEATURE,    0x00);     // Disable EN_DPL, EN_ACK_PAY and EN_DYN_ACK
  WriteRfReg(DYNPD,      0x00);     // Disable dynamic payload on all pipes
  WriteRfReg(STATUS,     0x70);     // Clear all three interrupt flags
  WriteRfReg(RF_CH,        76);     // This channel should be universally safe and not bleed over into adjacent spectrum.
  WriteRfCmd(FLUSH_TX);
  WriteRfCmd(FLUSH_RX);
  WriteRfReg(CONFIG,     0x0E);     // Power up in TX mode with 2 byte CRCs
  delay(5);

  WriteRfReg(EN_AA, 0x03);     // Enable auto acknowledge on pipes 0 and 1

  //  // Receive on P1 at address "25925"
  //  WriteRfAdr(RX_ADDR_P1, (u8*)"25925");
  //  WriteRfReg(RX_PW_P1, 4);     // Payload length

  WriteRfReg(EN_RXADDR, 3);    // Enable Rx on pipes 0 (for tx ack) and 1
}

u8 writeAddr[5] = {"x5925"};

u8 RfStatus() {CSN0; u8 status = spi(0xFF); CSN1; return status;}

void WriteTxPayload(u8 *buf, u8 len) {
  CSN0; spi(W_TX_PAYLOAD); for (u8 i=0; i<len; i++) spi(buf[i]); CSN1;
}

void RfWrite(u8 strip, u8 *payload) { // Payload length hardcoded at 4 bytes
  // Transmit to "x5925", receiving acknowledgements on P0
  writeAddr[0] = strip+'1';
  WriteRfAdr(RX_ADDR_P0, writeAddr);
  WriteRfAdr(TX_ADDR,    writeAddr);
  WriteRfReg(RX_PW_P0, 4);     // Payload length
  WriteTxPayload(payload, 4);
}


void sendLed(u8 r, u8 g, u8 b, u8 ww) {
  u8 payload[4]; payload[0] = r; payload[1] = g; payload[2] = b; payload[3] = ww;
  RfWrite(0, payload);
}

