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

MatrixPanel_I2S_DMA *matrix = nullptr;

unsigned long clock_dragging_count = 0;
unsigned long t1, t2;

unsigned int displayed_path_steps = 0;

uint8_t map_coord_onscreen(const int16_t coord) {
  if (coord == -1) {
    return 0;
  }
  if (coord == -2) {
    return PANEL_WIDTH - 1;
  }
  return coord;
}

void write_path(const int16_t (*path)[2], uint16_t length) {
  for (uint16_t i = 0; i < length; i++) {
    if (
        // Incorrect off-screen value
        path[i][0] < -2 || path[i][1] < -2 ||
        //
        path[i][0] >= PANEL_WIDTH || path[i][1] >= PANEL_HEIGHT) {
      continue;
    }

    uint8_t x = map_coord_onscreen(path[i][0]);
    uint8_t y = map_coord_onscreen(path[i][1]);

    if (path[i][0] >= 0 && path[i][1] >= 0)
      matrix->drawPixelRGB888(x, y, 255, 255, 255);
    else if (i == length - 1) // show dimmed indicator of offscreen position
      matrix->drawPixelRGB888(x, y, 96, 96, 96);
  }
}

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
  matrix->fillScreenRGB888(0, 10, 0);
  write_mask(water_mask, water_draw_pixel);
  write_path(run_path, displayed_path_steps);
  if (water_phase % 16 == 0) {
    displayed_path_steps = (displayed_path_steps + 1) % (RUN_PATH_LENGTH + 1);
  }
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

  unsigned long frame_time = 18;
  t2 = millis() - t1;
  if (t2 < frame_time) {
    delay(frame_time - t2);
    // Serial.printf("frame time: %lu ms\n", t2);
  } else {
    clock_dragging_count++;
    if (clock_dragging_count % 100 == 0) {
      Serial.printf(
          "Warning: frames have taken too long: %lu ms (drag=%lu ms)\n", t2,
          t2 - frame_time);
    }
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
