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

#define PANEL_WIDTH 64
#define PANEL_HEIGHT 64

#define PANELS_NUMBER 1

#define PANE_WIDTH PANEL_WIDTH *PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT
#define NUM_LEDS PANE_WIDTH *PANE_HEIGHT

MatrixPanel_I2S_DMA *matrix = nullptr;

unsigned long t1, t2;

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
    {0x0000FFFFFFFFFF00},
    {0x0000FFFFFFFFFFF0},
    {0x0000FFFFFFFFFFF8},
    {0x0000FFFFFFFFFFFC},
    {0x0000FFFFFFFFFFFE},
    {0x0000FFFFFFFFFFFF},
    {0x00000FFFFFFFFFFF},
    {0x00000FFFFFFFFFFF},
    {0x000000FFFFFFFFFF},
    {0x000000FFFFFFFFFF},
    {0x000000FFFFFFFFFF},
};
uint16_t water_phase = 0;

void water_draw_pixel(uint8_t x, uint8_t y) {
  const uint8_t low = 128;
  const uint8_t high = 255;

  uint8_t idx = 16 * (int8_t)(x - y) + ((water_phase >> 1) & 0xFF);
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
  mxconfig.min_refresh_rate = 0;
  mxconfig.setPixelColorDepthBits(8);
  mxconfig.double_buff = true;

  matrix = new MatrixPanel_I2S_DMA(mxconfig);
  matrix->begin();
  matrix->setBrightness8(255);

  for (uint8_t i = 0; i < 2; i++) {
    matrix->flipDMABuffer();

    delay(1000 / matrix->calculated_refresh_rate);

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
  }
  Serial.printf(
      "Matrix initialized, refresh rate: %u Hz, color depth: %u bits\n",
      matrix->calculated_refresh_rate, mxconfig.getPixelColorDepthBits());
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

  matrix->flipDMABuffer();
  unsigned long t_flip = millis();
  // now, wait for frame flip to actually happen
  // ... can do other, non frame buffer related work here
  unsigned long dma_flip_time = 1000 / matrix->calculated_refresh_rate;
  t2 = millis() - t_flip;
  if (t2 < dma_flip_time) {
    delay(dma_flip_time - t2);
  }

  unsigned long frame_time = 15;
  t2 = millis() - t1;
  if (t2 < frame_time) {
    delay(frame_time - t2);
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
}
