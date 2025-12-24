// How to use this library with a FM6126 panel, thanks goes to:
// https://github.com/hzeller/rpi-rgb-led-matrix/issues/746

#ifdef IDF_BUILD
#include "sdkconfig.h"
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#endif

#include "main.h"
#include "xtensa/core-macros.h"
#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// HUB75E pinout
// R1 | G1
// B1 | GND
// R2 | G2
// B2 | E
//  A | B
//  C | D
// CLK| LAT
// OE | GND

/*  Default library pin configuration for the reference
  you can redefine only ones you need later on object creation */
#define R1 42
#define G1 41
#define BL1 40
#define R2 38
#define G2 39
#define BL2 37
#define CH_A 45
#define CH_B 36
#define CH_C 48
#define CH_D 35
#define CH_E 21
#define CLK 2
#define LAT 47
#define OE 14

// Configure for your panel(s) as appropriate!
// #define PIN_E 32
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 64 // Panel height of 64 will required PIN_E to be defined.

#define PANELS_NUMBER 1

#define PANE_WIDTH PANEL_WIDTH *PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT
#define NUM_LEDS PANE_WIDTH *PANE_HEIGHT

MatrixPanel_I2S_DMA *matrix = nullptr;
// patten change delay
#define PATTERN_DELAY 2000

uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

// gradient buffer
CRGB *ledbuff;
//

unsigned long t1, t2, s1 = 0, s2 = 0, s3 = 0;
uint32_t ccount1, ccount2;

uint8_t color1 = 0, color2 = 0, color3 = 0;
uint16_t x, y;

const char *str = "* ESP32 I2S DMA *";

static inline uint8_t tri8(uint8_t t) {
  uint8_t v = (t & 0x7F) << 1;
  return (t & 0x80) ? (uint8_t)(255 - v) : v;
}

#define MASK_GROUP_SIZE (sizeof(uint64_t) * 8)
#define MASK_WIDTH PANEL_WIDTH / MASK_GROUP_SIZE
template <typename F>
void write_mask(const uint64_t (&mask)[PANEL_HEIGHT][MASK_WIDTH], F draw_fn) {
  for (uint16_t y = 0; y < PANEL_HEIGHT; y++) {
    for (uint16_t x_group = 0; x_group < MASK_WIDTH; x_group++) {
      uint64_t row = mask[y][x_group];

      while (row) {
        int next_location = __builtin_ctzll(row);
        uint16_t x = x_group * MASK_GROUP_SIZE + next_location;
        draw_fn(x, y);
        row &= ~(1ULL << next_location);
      }
    }
  }
}

const uint64_t watermask[PANEL_HEIGHT][MASK_WIDTH] = {
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0},
    {0x0000FFFFFFFF0000},
    {0x0000FFFFFFFF0000},
    {0x0000FFFFFFFF0000},
    {0x0000FFFFFFFF0000},
    {0x0000FFFFFFFF0000},
    {0x0000FFFFFFFF0000},
};
uint8_t water_phase = 0;

void water_draw_pixel(uint8_t x, uint8_t y) {
  const uint8_t low = 192;
  const uint8_t high = 255;

  uint8_t idx = 32 * (int8_t)(x - y) + water_phase;
  uint16_t mag = tri8(idx);
  uint16_t mag_scaled = (mag * (uint16_t)(high - low)) >> 8;

  uint8_t mag_shifted = low + mag_scaled;

  matrix->drawPixelRGB888(x, y, 0, 0, mag_shifted);
}

void setup() {

  delay(100);
  Serial.begin(BAUD_RATE);
  Serial.setDebugOutput(true); // Shows ESP32 system debug messages
  delay(500);
  Serial.println("Starting pattern test...");

  // redefine pins if required
  HUB75_I2S_CFG::i2s_pins _pins = {R1,   G1,   BL1,  R2,   G2,  BL2, CH_A,
                                   CH_B, CH_C, CH_D, CH_E, LAT, OE,  CLK};
  HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER, _pins);

  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::SHIFTREG;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
  mxconfig.latch_blanking = 8;
  mxconfig.min_refresh_rate = 100;
  mxconfig.setPixelColorDepthBits(8);

  matrix = new MatrixPanel_I2S_DMA(mxconfig);
  matrix->begin();
  matrix->setBrightness8(255);

  ledbuff =
      (CRGB *)malloc(NUM_LEDS * sizeof(CRGB)); // allocate buffer for some tests
  buffclear(ledbuff);

  // background
  matrix->fillScreenRGB888(0, 0, 63);
  // for checking orientation
  matrix->drawPixelRGB888(0, 0, 1, 1, 1);
  matrix->drawPixelRGB888(0, 31, 1, 1, 1);

  // path example
  matrix->fillRect(48, 32, 1, 12, (1 << 16) - 1);
  matrix->fillRect(48, 44, 10, 1, (1 << 16) - 1);
  matrix->fillRect(58, 44, 1, 10, (1 << 16) - 1);
  matrix->fillRect(38, 54, 20, 1, (1 << 16) - 1);

  Serial.printf("Matrix initialized with calculated refresh rate: %d Hz\n",
                matrix->calculated_refresh_rate);
}

// #define GRAD_TEST
// #define STEP_MODE
uint8_t wheelval = 0;
void loop() {
  t1 = millis();
  // Serial.printf("Cycle: %d\n", ++cycles);
#ifdef GRAD_TEST
  Serial.printf("Water phase: %d, refresh rate: %d\n", water_phase,
                matrix->calculated_refresh_rate);
  matrix->fillScreenRGB888(0, 0, water_phase);
  for (uint8_t x = 0; x < 64; x++) {
    for (uint8_t y = 10; y < 12; y++) {
      matrix->drawPixelRGB888(x, y, 0, 0, x + 100);
    }
  }
#else
  write_mask(watermask, water_draw_pixel);
#endif
  water_phase++;
  t2 = millis() - t1;

  unsigned long wait_time = 10;
  if (t2 < wait_time) {
    delay(wait_time - t2);
    // Serial.printf("frame time: %lu ms\n", t2);
  } else {
    // Serial.printf("took too long: %lu ms\n", t2);
    matrix->drawPixelRGB888(63, 63, 255, 0, 0);
    delay(1);
  }
#ifdef STEP_MODE
  while (Serial.available() == 0) {
    yield();
  }
  while (Serial.available() > 0) {
    Serial.read();
  }
#endif
  return;

  drawText(wheelval++);

  Serial.print("Estimating clearScreen() - ");
  ccount1 = XTHAL_GET_CCOUNT();
  matrix->clearScreen();
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  Serial.printf("%d ticks\n", ccount1);
  delay(PATTERN_DELAY);

  // simple solid colors
  Serial.println("Fill screen: RED");
  matrix->fillScreenRGB888(255, 0, 0);
  delay(PATTERN_DELAY);
  Serial.println("Fill screen: GREEN");
  matrix->fillScreenRGB888(0, 255, 0);
  delay(PATTERN_DELAY);
  Serial.println("Fill screen: BLUE");
  matrix->fillScreenRGB888(0, 0, 255);
  delay(PATTERN_DELAY);

  for (uint8_t i = 5; i; --i) {
    Serial.print("Estimating single drawPixelRGB888(r, g, b) ticks: ");
    color1 = random8();
    ccount1 = XTHAL_GET_CCOUNT();
    matrix->drawPixelRGB888(i, i, color1, color1, color1);
    ccount1 = XTHAL_GET_CCOUNT() - ccount1;
    Serial.printf("%d ticks\n", ccount1);
  }

  // Clearing CRGB ledbuff
  Serial.print("Estimating ledbuff clear time: ");
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  buffclear(ledbuff);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros() - t1;
  Serial.printf("%lu us, %u ticks\n\n", t2, ccount1);

  // Bare fillscreen(r, g, b)
  Serial.print("Estimating fillscreenRGB888(r, g, b) time: ");
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  matrix->fillScreenRGB888(64, 64, 64); // white
  ccount2 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros() - t1;
  s1 += t2;
  Serial.printf("%lu us, avg: %lu, ccnt: %d\n", t2, s1 / cycles, ccount2);
  delay(PATTERN_DELAY);

  Serial.print(
      "Estimating full-screen fillrate with looped drawPixelRGB888(): ");
  y = PANE_HEIGHT;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    --y;
    uint16_t x = PANE_WIDTH;
    do {
      --x;
      matrix->drawPixelRGB888(x, y, 0, 0, 0);
    } while (x);
  } while (y);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros() - t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);

  // created random color gradient in ledbuff
  uint8_t color1 = 0;
  uint8_t color2 = random8();
  uint8_t color3 = 0;

  for (uint16_t i = 0; i < NUM_LEDS; ++i) {
    ledbuff[i].r = color1++;
    ledbuff[i].g = color2;
    if (i % PANE_WIDTH == 0)
      color3 += 255 / PANE_HEIGHT;

    ledbuff[i].b = color3;
  }
  //

  //
  Serial.print(
      "Estimating ledbuff-to-matrix fillrate with drawPixelRGB888(), time: ");
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  mxfill(ledbuff);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros() - t1;
  s2 += t2;
  Serial.printf("%lu us, avg: %lu, %d ticks:\n", t2, s2 / cycles, ccount1);
  delay(PATTERN_DELAY);
  //

  // Fillrate for fillRect() function
  Serial.print("Estimating fullscreen fillrate with fillRect() time: ");
  t1 = micros();
  matrix->fillRect(0, 0, PANE_WIDTH, PANE_HEIGHT, 0, 224, 0);
  t2 = micros() - t1;
  Serial.printf("%lu us\n", t2);
  delay(PATTERN_DELAY);

  Serial.print("Chessboard with fillRect(): "); // шахматка
  matrix->fillScreen(0);
  x = 0, y = 0;
  color1 = random8();
  color2 = random8();
  color3 = random8();
  bool toggle = 0;
  t1 = micros();
  do {
    do {
      matrix->fillRect(x, y, 8, 8, color1, color2, color3);
      x += 16;
    } while (x < PANE_WIDTH);
    y += 8;
    toggle = !toggle;
    x = toggle ? 8 : 0;
  } while (y < PANE_HEIGHT);
  t2 = micros() - t1;
  Serial.printf("%lu us\n", t2);
  delay(PATTERN_DELAY);

  // ======== V-Lines ==========
  Serial.println("Estimating V-lines with drawPixelRGB888(): "); //
  matrix->fillScreen(0);
  color1 = random8();
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    y = 0;
    do {
      matrix->drawPixelRGB888(x, y, color1, color2, color3);
    } while (++y != PANE_HEIGHT);
    x += 2;
  } while (x != PANE_WIDTH);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros() - t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

  Serial.println("Estimating V-lines with vlineDMA(): "); //
  matrix->fillScreen(0);
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->drawFastVLine(x, y, PANE_HEIGHT, color1, color2, color3);
    x += 2;
  } while (x != PANE_WIDTH);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros() - t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

  Serial.println("Estimating V-lines with fillRect(): "); //
  matrix->fillScreen(0);
  color1 = random8();
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->fillRect(x, y, 1, PANE_HEIGHT, color1, color2, color3);
    x += 2;
  } while (x != PANE_WIDTH);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros() - t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

  // ======== H-Lines ==========
  Serial.println("Estimating H-lines with drawPixelRGB888(): "); //
  matrix->fillScreen(0);
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    x = 0;
    do {
      matrix->drawPixelRGB888(x, y, color1, color2, color3);
    } while (++x != PANE_WIDTH);
    y += 2;
  } while (y != PANE_HEIGHT);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros() - t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

  Serial.println("Estimating H-lines with hlineDMA(): ");
  matrix->fillScreen(0);
  color2 = random8();
  color3 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->drawFastHLine(x, y, PANE_WIDTH, color1, color2, color3);
    y += 2;
  } while (y != PANE_HEIGHT);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros() - t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

  Serial.println("Estimating H-lines with fillRect(): "); //
  matrix->fillScreen(0);
  color2 = random8();
  color3 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->fillRect(x, y, PANE_WIDTH, 1, color1, color2, color3);
    y += 2;
  } while (y != PANE_HEIGHT);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros() - t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

  Serial.println("\n====\n");

  // take a rest for a while
  delay(10000);
}

void buffclear(CRGB *buf) {
  memset(buf, 0x00, NUM_LEDS * sizeof(CRGB)); // flush buffer to black
}

void IRAM_ATTR mxfill(CRGB *leds) {
  uint16_t y = PANE_HEIGHT;
  do {
    --y;
    uint16_t x = PANE_WIDTH;
    do {
      --x;
      uint16_t _pixel = y * PANE_WIDTH + x;
      matrix->drawPixelRGB888(x, y, leds[_pixel].r, leds[_pixel].g,
                              leds[_pixel].b);
    } while (x);
  } while (y);
}
//

/**
 *  The one for 256+ matrices
 *  otherwise this:
 *    for (uint8_t i = 0; i < MATRIX_WIDTH; i++) {}
 *  turns into an infinite loop
 */
uint16_t XY16(uint16_t x, uint16_t y) {
  if (x < PANE_WIDTH && y < PANE_HEIGHT) {
    return (y * PANE_WIDTH) + x;
  } else {
    return 0;
  }
}

void drawText(int colorWheelOffset) {
  // draw some text
  matrix->setTextSize(1);     // size 1 == 8 pixels high
  matrix->setTextWrap(false); // Don't wrap at end of line - will do ourselves

  matrix->setCursor(5, 5); // start at top left, with 5,5 pixel of spacing
  uint8_t w = 0;

  for (w = 0; w < strlen(str); w++) {
    matrix->setTextColor(colorWheel((w * 32) + colorWheelOffset));
    matrix->print(str[w]);
  }
}

uint16_t colorWheel(uint8_t pos) {
  if (pos < 85) {
    return matrix->color565(pos * 3, 255 - pos * 3, 0);
  } else if (pos < 170) {
    pos -= 85;
    return matrix->color565(255 - pos * 3, 0, pos * 3);
  } else {
    pos -= 170;
    return matrix->color565(0, pos * 3, 255 - pos * 3);
  }
}
