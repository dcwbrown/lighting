#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF


void FillColour(u16 x, u16 y, u16 w, u16 h, u16 rgb) {
  u8 l;
  WriteRegion(x, y, x+w-1, y+h-1);

  while(h) {
    l = w>>8;  while (l) {RepeatDataWord(rgb, 0); l--;}
    l = w&255; while (l) {SendDataWord(rgb); l--;}
    h--;
  }

  PORTC = LcdIdle;  // CS and CD both go inactive
}



u16 AlphaMultiplyChannel(u8 p, u8 a) { // reduce gamma encoded 6 bit pixel p by 6 bit linear gamma a.
  return ((p*p)/4) * a;
}


//u16 AlphaMultiplyChannel(u8 level, u8 alpha) { // level in sqrt space, alpha and result in linear space
//  u16 n1 = (alpha * level) / 2;     // 11 bit
//  u16 n2 = (n1/2) * level;          // 16 bit
//  return n1 + n2;                   // 16 bit
//}

u16 AlphaMultiplyPixel(u16 pixel, u8 alpha) { // 565 rgb pixel * 6 bit alpha
  if (alpha >= 63) return pixel;
  return
    ((u6sqrt(AlphaMultiplyChannel((pixel >> 10) & 0x3E, alpha) >> 4) & 0x3E) << 10)
  | ( u6sqrt(AlphaMultiplyChannel((pixel >>  5) & 0x3F, alpha) >> 4)         << 5)
  | ( u6sqrt(AlphaMultiplyChannel((pixel <<  1) & 0x3E, alpha) >> 4)         >> 1);
}


u8 BlendChannel(u8 fg, u8 bg, u8 alpha) {
  u8 blend = u6sqrt((  AlphaMultiplyChannel(fg, alpha)
                    + AlphaMultiplyChannel(bg, 63-alpha)) >> 4);
  //printf("  BlendChannel(fg %02x, bg %02x, alpha %02x) -> %02x.\n", fg, bg, alpha, blend);
  return blend;
}

u16 BlendPixel(u16 fg, u16 bg, u8 alpha) {
  if (bg == 0) return AlphaMultiplyPixel(fg, alpha);
  if (alpha >= 63) return fg;
  if (alpha == 0)  return bg;

  //printf("BlendPixel(fg %04x, bg %04x, alpha %02x)\n", fg, bg, alpha);

  u16 pixel =
    ((BlendChannel((fg>>10) & 0x3e,  (bg>>10) & 0x3e,  alpha) &0x3e)  << 10)
  | ( BlendChannel((fg>> 5) & 0x3f,  (bg>> 5) & 0x3f,  alpha       )  << 5)
  | ( BlendChannel((fg<< 1) & 0x3e,  (bg<< 1) & 0x3e,  alpha       )  >> 1);

  //printf("-> %04x.\n", pixel);

  return pixel;
}



u16 paint = YELLOW;

void RenderAlphaMap(u16 x, u16 y, const u8 *map) {

  u16 w = __LPM_word((FlashAddr)(map)); map += 2;
  u16 h = __LPM_word((FlashAddr)(map)); map += 2;

  printf("RenderAlphaMap w %d, h %d.\n", w, h);

  WriteRegion(x, y, x+w-1, y+h-1);

  u8 alpha = 0;
  u8 len   = __LPM((FlashAddr)(map++));
  u8 code  = len & 0xC0;

  while (code <= 0x80) {
    switch (code) {
      case 0x00: alpha = len;   len = 1;     break;
      case 0x40: alpha = 0;     len &= 0x3F; break;
      case 0x80: alpha = 0x3F;  len &= 0x3F; break;
    }

    u16 rgb = AlphaMultiplyPixel(paint, alpha);
    //printf("AlphaMultiplyPixel(paint %04x x alpha %02x) = %04x.\n", paint, alpha, rgb);
    RepeatDataWord(rgb, len);

    len = __LPM((FlashAddr)(map++)); // *(map++);
    code = len & 0xC0;
  }

  PORTC = LcdIdle;  // CS remains active, CD goes high to return to data mode
}



const u8 PROGMEM /*__attribute__((__progmem__))*/ am1[] = {
  // a
  0x08, 0x00,  0x08, 0x00,  // 8x8
  0x00, 0x0E, 0x33, 0x3F, 0x3E, 0x2E, 0x06, 0x42, 0x36, 0x25, 0x02, 0x05, 0x30, 0x2B, 0x46, 0x20,
  0x38, 0x42, 0x16, 0x33, 0x3D, 0x3F, 0x3C, 0x38, 0x00, 0x0B, 0x3F, 0x1C, 0x01, 0x00, 0x20, 0x38,
  0x00, 0x18, 0x3F, 0x43, 0x2A, 0x38, 0x00, 0x11, 0x3F, 0x10, 0x01, 0x1A, 0x34, 0x3B, 0x01, 0x00,
  0x26, 0x3E, 0x3D, 0x24, 0x0A, 0x3A, 0x38, 0xFF,
};



#define HORZ 0
#define VERT 1
#define FULL 63


void PixelRunAlpha(
  u8  orientation,   // 0 - horizontal, 1 - vertical
  u16 major,         // Offset of row from parallel edge - y coord for HORZ, x xoord for VERT
  u16 first,         // Offset of first pixel of run     - x coord for HORZ, y xoord for VERT
  u16 last,          // Offset of last pixel of run      - x coord for HORZ, y xoord for VERT
  u16 paint,         // Paint colour
  u8  alpha1,        // Alpha blend for first pixel
  u8  alpha2,        // Alpha blend for middle pixels
  u8  alpha3         // Alpha blend for last pixel
) {
  u16 paint1;
  u16 paint2;
  u16 paint3;
  u16 i;
  //u8 hi, lo;

  paint1 = AlphaMultiplyPixel(paint, alpha1);
  paint2 = AlphaMultiplyPixel(paint, alpha2);
  if (alpha3 == alpha1) paint3 = paint1;
  else paint3 = AlphaMultiplyPixel(paint, alpha3);

  // Preset write memory command bounds

  if (orientation == HORZ) WriteRegion(first, major, last, major);
  else                     WriteRegion(major, first, major, last);
  SendDataWord(paint1);
  i = first+1; while (i < last) {SendDataWord(paint2); i++;}
  if (i == last) {SendDataWord(paint3);}

  PORTC = LcdIdle;  // CS and CD both go inactive
}


//void FilledCircle(u16 cx, u16 cy, u16 r) {
//  u16 x,  y;
//  s16 dx, dy;
//  s16 er;
//
//  x  = r;  dx = 1 - 2*r;
//  y  = 0;  dy = 1;
//  er = 0;
//
//  while (x >= y) {
//
//    u8 lum = (63 * (2*er + dx)) / (2*dx);
//
//    PixelRunAlpha(HORZ, cy-y, cx-x,   cx+x,   paint, lum, 63, lum);
//    PixelRunAlpha(VERT, cx-y, cy-x,   cy-r/2, paint, lum, 63, 63);
//    PixelRunAlpha(VERT, cx-y, cy+r/2, cy+x,   paint, 63, 63, lum);
//    if (y>0) {
//      PixelRunAlpha(HORZ, cy+y, cx-x,   cx+x,   paint, lum, 63, lum);
//      PixelRunAlpha(VERT, cx+y, cy-x,   cy-r/2, paint, lum, 63, 63);
//      PixelRunAlpha(VERT, cx+y, cy+r/2, cy+x,   paint, 63, 63, lum);
//    }
//
//    y++;  er += dy;  dy += 2;
//
//    if (2*er + dx > 0) {x--;  er += dx;  dx += 2;}
//  }
//}




void PlotHollowCircle(u16 cx, u16 cy, u16 r, u16 t) {
  u16 x1,    x2,  y;
  s16 dx1,  dx2,  dy;
  s16 er1,  er2;

  x1  = r-t;   dx1 = 1 - 2*(r-t);
  x2  = r+t;   dx2 = 1 - 2*(r+t);
  er1 = 0;     er2 = 0;
  y = 0;       dy = 1;

  while (x2 >= y) {
    u8  lum1 = (x1 >= y) ? (63 - (63 * (2*er1 + dx1)) / (2*dx1)) : 63;
    u8  lum2 = (63 * (2*er2 + dx2)) / (2*dx2);
    u16 x1c  = (x1 >= y) ? x1 : y;

    PixelRunAlpha(HORZ, cy+y, cx+x1c, cx+x2, paint, lum1, 63, lum2);
    PixelRunAlpha(HORZ, cy+y, cx-x2, cx-x1c, paint, lum2, 63, lum1);
    PixelRunAlpha(VERT, cx+y, cy+x1c, cy+x2, paint, lum1, 63, lum2);
    PixelRunAlpha(VERT, cx+y, cy-x2, cy-x1c, paint, lum2, 63, lum1);

    if (y>0) {
      PixelRunAlpha(HORZ, cy-y, cx+x1c, cx+x2, paint, lum1, 63, lum2);
      PixelRunAlpha(HORZ, cy-y, cx-x2, cx-x1c, paint, lum2, 63, lum1);
      PixelRunAlpha(VERT, cx-y, cy+x1c, cy+x2, paint, lum1, 63, lum2);
      PixelRunAlpha(VERT, cx-y, cy-x2, cy-x1c, paint, lum2, 63, lum1);
    }

    er1+=dy; if (2*er1+dx1 > 0) {x1--;  er1+=dx1; dx1+=2;}
    er2+=dy; if (2*er2+dx2 > 0) {x2--;  er2+=dx2; dx2+=2;}
    y++; dy += 2;
  }
}


// PlotLine

u16 foreground, background;

void PaintPair(u8 orientation, u16 x, u16 y, u8 alpha) {
  WriteRegion(x, y, x + (orientation==HORZ), y + (orientation==VERT));
  //SendDataWord(AlphaMultiplyPixel(paint, alpha));
  //SendDataWord(AlphaMultiplyPixel(paint, FULL-alpha));
  SendDataWord(BlendPixel(foreground, background, alpha));
  SendDataWord(BlendPixel(foreground, background, FULL-alpha));
  PORTC = LcdIdle;     // CS and CD both go inactive
}


//  void PlotLine(u16 x0, u16 y0,  s16 dx, s16 dy) {
//    s8 sx = 1, sy = 1;
//    if (dx < 0) {dx = -dx; sx = -1;}
//    if (dy < 0) {dy = -dy; sy = -1;}
//    u8 swap = dx<dy; if (swap) {s16 t=dx; dx=dy; dy=t;}
//    u16 D = 0;
//    u16 x = 0;
//    u16 y = 0;
//    u16 xp, yp;
//    s8 xi, yi;
//
//    while (x <= (u16)dx) {
//      int next = (FULL*D)/dx;
//
//      if (!swap) {xp=x; yp=y; xi=0;  yi=sy;}
//      else       {xp=y; yp=x; xi=sx; yi=0;}
//
//      xp = (sx < 0) ? x0-xp : x0+xp;
//      yp = (sy < 0) ? y0-yp : y0+yp;
//
//      if (xi == 0) {
//        if (yi < 0) PaintPair(VERT, xp, yp-1, next);
//        else        PaintPair(VERT, xp, yp,   FULL-next);
//      } else {
//        if (xi < 0) PaintPair(HORZ, xp-1, yp, next);
//        else        PaintPair(HORZ, xp, yp,   FULL-next);
//      }
//
//      D += dy;
//
//      if (D >= (u16)dx) {
//        y++;
//        D -= dx;
//      }
//
//      x++;
//    }
//  }

void PlotPartLine(u16 x0, u16 y0,  s16 dx, s16 dy, s16 minx, s16 miny, s16 maxx, s16 maxy) {

  //printf("x0 %d, y0 %d, dx %d, dy %d, minx %d, miny %d, maxx %d, maxy %d",
  //        x0,    y0,    dx,    dy,    minx,    miny,    maxx,    maxy
  //);

  s8 sx = 1, sy = 1;
  if (dx < 0) {dx=-dx; minx=-minx; maxx=-maxx; sx=-1;}
  if (dy < 0) {dy=-dy; miny=-miny; maxy=-maxy; sy=-1;}
  u8 swap = dx<dy; if (swap) {s16 t=dx; dx=dy; dy=t; minx=miny; maxx=maxy;}
  u16 D = 0;
  u16 x = 0;
  u16 y = 0;
  u16 xp, yp;
  s8 xi, yi;

  //printf(" --> minx %d, maxx %d.\n", minx, maxx);

  while ((x <= (u16)dx)  &&  (x <= (u16)maxx)) {

    if (x > (u16)minx) {
      if (!swap) {xp=x; yp=y; xi=0;  yi=sy;}
      else       {xp=y; yp=x; xi=sx; yi=0;}

      xp = (sx < 0) ? x0-xp : x0+xp;
      yp = (sy < 0) ? y0-yp : y0+yp;

      int alpha = (FULL*D)/dx;
      if (xi == 0) {
        if (yi < 0) PaintPair(VERT, xp, yp-1, alpha);
        else        PaintPair(VERT, xp, yp,   FULL-alpha);
      } else {
        if (xi < 0) PaintPair(HORZ, xp-1, yp, alpha);
        else        PaintPair(HORZ, xp, yp,   FULL-alpha);
      }
    }

    D += dy;

    if (D >= (u16)dx) {
      y++;
      D -= dx;
    }

    x++;
  }
}



const u8 PROGMEM sines[70] = { // Provides sines at steps of 1.25 degrees.
                               // note: sins of 70,71 and 72 are all 256 and
                               // not provided here (as this is a table of
                               // byte values and can encode no more than 255).
  0x00, 0x06, 0x0b, 0x11, 0x16, 0x1c, 0x21, 0x27, 0x2c, 0x32, 0x37, 0x3d,
  0x42, 0x48, 0x4d, 0x52, 0x58, 0x5d, 0x62, 0x67, 0x6c, 0x71, 0x76, 0x7b,
  0x80, 0x85, 0x8a, 0x8e, 0x93, 0x97, 0x9c, 0xa0, 0xa5, 0xa9, 0xad, 0xb1,
  0xb5, 0xb9, 0xbd, 0xc0, 0xc4, 0xc8, 0xcb, 0xce, 0xd2, 0xd5, 0xd8, 0xdb,
  0xde, 0xe0, 0xe3, 0xe6, 0xe8, 0xea, 0xed, 0xef, 0xf1, 0xf2, 0xf4, 0xf6,
  0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xfe, 0xff, 0xff
};

s16 Sine(u16 step) { // step in 288th of a circle - each step corresponds to 1.25 degrees.
  if (step > 144) return -Sine(step-144);
  if (step > 72)  return  Sine(144-step);
  if (step > 69)  return  256;

  return __LPM((FlashAddr)(sines+step));
}

void GetVec(u16 step, s16 *dx, s16 *dy) { // step in 0..256, 1.25 degrees per step
  step = 272 - step;   // Run clockwiase starting 20 degrees (16 steps) after 6pm.
  *dx = Sine(step);
  *dy = Sine((step + 72) % 288); // (Cosine)
}


void Reticulate(u16 x, u16 y) {
  s16 dx, dy;
  int step;
  for(step=0; step<=256; step+=8) {
    GetVec(step, &dx, &dy);
    printf("  step %3d, dx %3d, dy %3d.\n", step, dx, dy);

    //PlotLine(160, 320, dx/4, dy/4);

    //PlotPartLine(
    //  x,           y,            // Origin
    //  dx,          dy,           // Slope
    //  (dx*10)/256, (dy*10)/256,  // Start about 10 pixels from origin
    //  (dx*37)/256, (dy*37)/256   // End about 37 pixels from origin
    //);

    if (step%16 == 0) {
      PlotPartLine(
        x,           y,            // Origin
        dx,          dy,           // Slope
        (dx*39)/256, (dy*39)/256,  // Start about 10 pixels from origin
        (dx*53)/256, (dy*53)/256   // End about 37 pixels from origin
      );
    }

    PlotPartLine(
      x,           y,            // Origin
      dx,          dy,           // Slope
      (dx*42)/256, (dy*42)/256,  // Start about 10 pixels from origin
      (dx*51)/256, (dy*51)/256  // Start about 10 pixels from origin
      //(dx*53)/256, (dy*53)/256   // End about 37 pixels from origin
    );
  }
}


void DrawPointer(u16 x, u16 y, u16 step, u16 colour) {
  s16 dx, dy;
  GetVec(step, &dx, &dy);
  background = BLACK; foreground = colour;
  PlotPartLine(
    x, y,
    dx, dy,
    (dx*5)/256, (dy*5)/256,  // Start about 10 pixels from origin
    (dx*35)/256, (dy*35)/256   // End about 37 pixels from origin
  );
}


void scale(u16 x, u16 y, u16 p) {
  paint = p;  background = p;  foreground = WHITE;
  PlotHollowCircle(x, y, 46, 8);
  Reticulate(x, y);
}


struct knob {
  u16 x, y;
  u16 curstep, nextstep;
  u8 reading;
} knobs[4];

struct knob *rknob  = &knobs[0];
struct knob *gknob  = &knobs[1];
struct knob *bknob  = &knobs[2];
struct knob *wwknob = &knobs[3];

u8 currknob = 3;
u8 knobdown = 0;

void UpdatePointer(struct knob *k) {
  DrawPointer(k->x, k->y, k->curstep, BLACK);
  k->curstep = k->nextstep;
  DrawPointer(k->x, k->y, k->curstep, WHITE);
}

void InitKnob(u16 x, u16 y, u16 colour, struct knob *k) {
  k->x        = x;
  k->y        = y;
  k->curstep  = 128;
  k->nextstep = 128;
  k->reading  = 0;
  scale(x, y, colour);
  DrawPointer(k->x, k->y, k->curstep, WHITE);
}

void TurnKnob(struct knob *k, u8 backward) {
  if (backward) {if (k->nextstep >   0) k->nextstep--;}
  else          {if (k->nextstep < 255) k->nextstep++;}
}


//----------------------------------------------------------------------------//

void Initscreen() {
  printf("Clear to black.\n");
  FillColour(0,0, 320,480, 0);       // Clear screen to black

  printf("RenderAlphaMap letter a.\n");
  RenderAlphaMap(10,10, am1);

  printf("Plot the colour knob scales.\n");
  InitKnob(260, 60, 0xFA20, rknob);
  InitKnob(260,180, 0x8400, gknob);
  InitKnob(260,300, 0x49F1, bknob);
  InitKnob(260,420, 0xCDCA, wwknob);

  for (int i=0; i<4; i++) {
    knobs[i].nextstep=0; UpdatePointer(&knobs[i]);
  }
}

u8 turning = 0;

void PinChangeInterrupt() {
  u8 port = PINB;

  u8 phase = port & 3;
  if (phase == 0) turning = 1;
  else if (turning) {
    if (phase == 3) {
      TurnKnob(&knobs[currknob], turning&1); turning = 0;
    }
    else turning = phase;
  }

  u8 pressed = (port & 0x80) == 0;
  if (pressed) { // Knob is pressed
    if (knobdown == 0) currknob = (currknob+1) % 4; // Advance colour at first suggestion of press
    knobdown = 1;
    TIFR0    = 7;  // Clear any pending timer 0 interrupts
    TCNT0    = 0;  // Reset timer 0 to count = 0
    TIMSK0   = 1;  // Enable interrupt on timer 0 overflow
  }
}

void Timer0Interrupt() { // knob has been released for 32ms
  knobdown = 0;
  TIMSK0 = 0;  // Leave timer 0 running but disable its interrupts
}
