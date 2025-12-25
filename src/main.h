#include <FastLED.h>

#define BAUD_RATE 115200 // serial debug port baud rate

static inline uint8_t tri8(uint8_t t) {
  uint8_t v = (t & 0x7F) << 1;
  return (t & 0x80) ? (uint8_t)(255 - v) : v;
}

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

#define MASK_GROUP_SIZE (sizeof(uint64_t) * 8)
#define MASK_WIDTH PANEL_WIDTH / MASK_GROUP_SIZE

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
