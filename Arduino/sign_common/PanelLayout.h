#pragma once

#include <stdint.h>

struct MatrixPortalPanelLayoutEntry {
  int16_t sourceX;
  int16_t sourceY;
  int16_t width;
  int16_t height;
  int16_t x;
  int16_t y;
  uint16_t rotation;
};

#define PANEL_LAYOUT_PANEL_COUNT 4
#define PANEL_LAYOUT_PANEL_WIDTH 32
#define PANEL_LAYOUT_PANEL_HEIGHT 32
#define PANEL_LAYOUT_SOURCE_WIDTH 128
#define PANEL_LAYOUT_SOURCE_HEIGHT 32
#define PANEL_LAYOUT_PHYSICAL_WIDTH 128
#define PANEL_LAYOUT_PHYSICAL_HEIGHT 32
#define PANEL_LAYOUT_IDE_LAYOUT_MODE "Row"
#define PANEL_LAYOUT_IDE_ROTATION 0

static const MatrixPortalPanelLayoutEntry PANEL_LAYOUT[] = {
  {0, 0, 32, 32, 0, 0, 0},
  {32, 0, 32, 32, 32, 0, 0},
  {64, 0, 32, 32, 64, 0, 0},
  {96, 0, 32, 32, 96, 0, 0},
};
