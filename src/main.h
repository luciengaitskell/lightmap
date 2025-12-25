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

const uint64_t water_mask[PANEL_HEIGHT][MASK_WIDTH] = {
    {0x0000000000000000}, {0x0000000000000000}, {0x0000000000000000},
    {0x0000000000000000}, {0x0000000000000000}, {0x0000000000000000},
    {0x0000000000000000}, {0x0000000000000000}, {0x8000000000000000},
    {0xc000000000000000}, {0xc000000000000000}, {0xe000000000000000},
    {0xf000000000000000}, {0xf800000000000000}, {0xf800000000000000},
    {0xfc00000000000000}, {0xfe00000000000000}, {0xfe00200000000000},
    {0xff07c00000000000}, {0xfff0000000000000}, {0xffc0000000000000},
    {0xff00000000000000}, {0xff00000000000000}, {0xff80000000000000},
    {0xfffc000000000000}, {0xffff000000000000}, {0xffffc00000000000},
    {0xfffff00000000000}, {0xfffffe0000000000}, {0xffffff8000000000},
    {0xffffffe000000000}, {0xfffffff800000000}, {0xffffffff00000000},
    {0xffffffffc0000000}, {0xfffffffff0000000}, {0xfffffffffc000000},
    {0xffffffffff000000}, {0xffffffffffe00000}, {0xfffffffffff80000},
    {0xfffffffffffe0000}, {0xffffffffffff8000}, {0x7ffffffffffff000},
    {0x3ffffffffffffc00}, {0x07ffffffffffff00}, {0x40ffffffffffffc0},
    {0x183ffffffffffff0}, {0x03c7fffffffffffe}, {0x00f8ffffffffffff},
    {0x001e3fffffffffff}, {0x000307ffffffffff}, {0x0000f8ffffffffff},
    {0x00007e1fffffffff}, {0x00001fcfffffffff}, {0x000003efffffffff},
    {0x000000dfffffffff}, {0x00000003ffffffff}, {0x000000007fffffff},
    {0x000000000fffffff}, {0x0000000001ffffff}, {0x000000000007ffff},
    {0x0000000000007fff}, {0x0000000000000fff}, {0x00000000000007ff},
    {0x0000000000000000},
};
