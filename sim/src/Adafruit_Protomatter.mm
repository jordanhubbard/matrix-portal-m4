#include "Adafruit_Protomatter.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

@interface MatrixPreviewView : NSView
@end

@interface PreviewWindowDelegate : NSObject <NSWindowDelegate>
@end

@interface PreviewActionTarget : NSObject
@end

@interface PreviewToolbarDelegate : NSObject <NSToolbarDelegate>
@end

namespace {
struct Panel {
  int sourceX = 0;
  int sourceY = 0;
  int width = 64;
  int height = 64;
  int x = 0;
  int y = 0;
  int rotation = 0;
};

struct Rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

NSWindow *previewWindow = nil;
MatrixPreviewView *previewView = nil;
PreviewWindowDelegate *windowDelegate = nil;
PreviewActionTarget *actionTarget = nil;
PreviewToolbarDelegate *toolbarDelegate = nil;
const Adafruit_Protomatter *activeMatrix = nullptr;
bool keepRunning = true;
bool showGrid = true;
bool dragging = false;
bool layoutDirty = false;
bool suppressPreviewWindow = false;
bool layoutInitialized = false;
int scale = 8;
int defaultPanelWidth = 32;
int defaultPanelHeight = 32;
int logicalWidth = 64;
int logicalHeight = 64;
int selectedPanel = 0;
int dragOffsetX = 0;
int dragOffsetY = 0;
int viewMinX = 0;
int viewMinY = 0;
int viewOriginX = 16;
int viewOriginY = 16;
uint32_t maxFrames = 0;
uint32_t presentedFrames = 0;
std::string baseWindowTitle = "Matrix Portal M4 Preview";
std::string layoutPath = "sim-panel-layout.txt";
std::string controlPath;
std::string framePath;
std::string currentSketchPath =
#ifdef SKETCH_NAME
  SKETCH_NAME;
#else
  "Arduino/rainbow/rainbow.ino";
#endif
std::vector<Panel> panels;
MatrixPortalSim::HardwareState hardwareState;
const auto startTime = std::chrono::steady_clock::now();

int rotatedWidth(const Panel &panel);
int rotatedHeight(const Panel &panel);
void addPanel();
void deleteSelectedPanel();
void rotateSelectedPanel();
void fitPanelsInRow();
bool saveLayout();
bool loadLayout();
void markLayoutChanged();
void updateWindowTitle();
void requestPreviewDisplay();
void handleMouseDownPoint(NSPoint point);
void handleMouseDraggedPoint(NSPoint point);
void handleKeyEvent(NSEvent *event, bool down);
void drawPreview(CGContextRef context, NSRect bounds);
void ensurePreviewLayout(const Adafruit_Protomatter &matrix);
void writeFrameFile(const Adafruit_Protomatter &matrix);

std::string sketchDisplayName() {
  size_t slash = currentSketchPath.find_last_of("/\\");
  std::string name = slash == std::string::npos
                       ? currentSketchPath
                       : currentSketchPath.substr(slash + 1);
  size_t dot = name.find_last_of('.');
  if (dot != std::string::npos) {
    name = name.substr(0, dot);
  }
  return name.empty() ? currentSketchPath : name;
}

int rotatedWidth(const Panel &panel) {
  return panel.rotation == 90 || panel.rotation == 270 ? panel.height : panel.width;
}

int rotatedHeight(const Panel &panel) {
  return panel.rotation == 90 || panel.rotation == 270 ? panel.width : panel.height;
}

int snapCoord(int value) {
  const int snap = 8;
  return static_cast<int>(std::lround(static_cast<double>(value) / snap)) * snap;
}

void colorToRgb(uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b) {
  r = static_cast<uint8_t>(((color >> 11) & 0x1f) * 255 / 31);
  g = static_cast<uint8_t>(((color >> 5) & 0x3f) * 255 / 63);
  b = static_cast<uint8_t>((color & 0x1f) * 255 / 31);
}

void parseSize(const std::string &value, int &width, int &height) {
  const size_t marker = value.find('x');
  if (marker == std::string::npos) {
    return;
  }
  int parsedWidth = std::atoi(value.substr(0, marker).c_str());
  int parsedHeight = std::atoi(value.substr(marker + 1).c_str());
  if (parsedWidth > 0 && parsedHeight > 0) {
    width = parsedWidth;
    height = parsedHeight;
  }
}

bool parseBoolValue(const std::string &value) {
  return value == "1" || value == "true" || value == "yes" || value == "on";
}

std::vector<int> parseIntegers(const std::string &line) {
  std::vector<int> values;
  for (size_t i = 0; i < line.size();) {
    bool negative = false;
    if (line[i] == '-') {
      negative = true;
      ++i;
    }
    if (i >= line.size() || !std::isdigit(static_cast<unsigned char>(line[i]))) {
      ++i;
      continue;
    }

    int value = 0;
    while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) {
      value = value * 10 + (line[i] - '0');
      ++i;
    }
    values.push_back(negative ? -value : value);
  }
  return values;
}

void applyControlFile() {
  if (controlPath.empty()) {
    return;
  }
  std::ifstream in(controlPath);
  if (!in) {
    return;
  }

  std::string line;
  while (std::getline(in, line)) {
    size_t equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    std::string key = line.substr(0, equals);
    std::string value = line.substr(equals + 1);
    if (key == "auto_tilt") {
      hardwareState.autoTilt = parseBoolValue(value);
    } else if (key == "button_up") {
      hardwareState.buttonUpPressed = parseBoolValue(value);
    } else if (key == "button_down") {
      hardwareState.buttonDownPressed = parseBoolValue(value);
    } else if (key == "accel_x") {
      hardwareState.accelX = static_cast<float>(std::atof(value.c_str()));
    } else if (key == "accel_y") {
      hardwareState.accelY = static_cast<float>(std::atof(value.c_str()));
    } else if (key == "analog0") {
      hardwareState.analogValues[0] = std::max(0, std::min(1023, std::atoi(value.c_str())));
    }
  }
}

void updateWindowTitle() {
  if (!previewWindow) {
    return;
  }
  std::ostringstream title;
  title << sketchDisplayName() << " - " << baseWindowTitle;
  if (!panels.empty() && selectedPanel >= 0 && selectedPanel < static_cast<int>(panels.size())) {
    const Panel &panel = panels[static_cast<size_t>(selectedPanel)];
    title << " - Panel " << (selectedPanel + 1)
          << " at " << panel.x << "," << panel.y
          << " rotated " << panel.rotation;
  }
  [previewWindow setTitle:[NSString stringWithUTF8String:title.str().c_str()]];
}

void calculateBounds() {
  if (panels.empty()) {
    viewMinX = 0;
    viewMinY = 0;
    return;
  }

  viewMinX = panels[0].x;
  viewMinY = panels[0].y;
  for (const Panel &panel : panels) {
    viewMinX = std::min(viewMinX, panel.x);
    viewMinY = std::min(viewMinY, panel.y);
  }
}

Rect panelRect(const Panel &panel) {
  return Rect{
    viewOriginX + (panel.x - viewMinX) * scale,
    viewOriginY + (panel.y - viewMinY) * scale,
    rotatedWidth(panel) * scale,
    rotatedHeight(panel) * scale
  };
}

bool mapPanelPixel(const Panel &panel, int localX, int localY, int &sourceX, int &sourceY) {
  switch (panel.rotation) {
  case 90:
    sourceX = panel.sourceX + localY;
    sourceY = panel.sourceY + panel.height - 1 - localX;
    break;
  case 180:
    sourceX = panel.sourceX + panel.width - 1 - localX;
    sourceY = panel.sourceY + panel.height - 1 - localY;
    break;
  case 270:
    sourceX = panel.sourceX + panel.width - 1 - localY;
    sourceY = panel.sourceY + localX;
    break;
  default:
    sourceX = panel.sourceX + localX;
    sourceY = panel.sourceY + localY;
    break;
  }

  return sourceX >= 0 && sourceY >= 0 && sourceX < logicalWidth && sourceY < logicalHeight;
}

void requestPreviewDisplay() {
  [previewView setNeedsDisplay:YES];
  updateWindowTitle();
}

void markLayoutChanged() {
  saveLayout();
  requestPreviewDisplay();
}

void addPanel() {
  Panel panel;
  panel.width = std::min(defaultPanelWidth, logicalWidth);
  panel.height = std::min(defaultPanelHeight, logicalHeight);
  if (panel.width <= 0) {
    panel.width = defaultPanelWidth;
  }
  if (panel.height <= 0) {
    panel.height = defaultPanelHeight;
  }

  int index = static_cast<int>(panels.size());
  int logicalColumns = std::max(1, logicalWidth / panel.width);
  panel.sourceX = (index % logicalColumns) * panel.width;
  panel.sourceY = ((index / logicalColumns) * panel.height) % std::max(panel.height, logicalHeight);
  panel.x = index * panel.width;
  panel.y = 0;

  panels.push_back(panel);
  selectedPanel = static_cast<int>(panels.size()) - 1;
  markLayoutChanged();
}

void ensureDefaultPanels() {
  if (!panels.empty()) {
    return;
  }

  int panelWidth = std::min(defaultPanelWidth, logicalWidth);
  int panelHeight = std::min(defaultPanelHeight, logicalHeight);
  for (int sy = 0; sy < logicalHeight; sy += panelHeight) {
    for (int sx = 0; sx < logicalWidth; sx += panelWidth) {
      Panel panel;
      panel.sourceX = sx;
      panel.sourceY = sy;
      panel.width = std::min(panelWidth, logicalWidth - sx);
      panel.height = std::min(panelHeight, logicalHeight - sy);
      panel.x = sx;
      panel.y = sy;
      panels.push_back(panel);
    }
  }
  selectedPanel = panels.empty() ? -1 : 0;
}

void deleteSelectedPanel() {
  if (panels.empty() || selectedPanel < 0 || selectedPanel >= static_cast<int>(panels.size())) {
    return;
  }
  panels.erase(panels.begin() + selectedPanel);
  if (selectedPanel >= static_cast<int>(panels.size())) {
    selectedPanel = static_cast<int>(panels.size()) - 1;
  }
  markLayoutChanged();
}

void rotateSelectedPanel() {
  if (panels.empty() || selectedPanel < 0 || selectedPanel >= static_cast<int>(panels.size())) {
    return;
  }
  Panel &panel = panels[static_cast<size_t>(selectedPanel)];
  panel.rotation = (panel.rotation + 90) % 360;
  markLayoutChanged();
}

void fitPanelsInRow() {
  int x = 0;
  for (Panel &panel : panels) {
    panel.x = x;
    panel.y = 0;
    x += rotatedWidth(panel);
  }
  markLayoutChanged();
}

bool saveLayout() {
  std::ofstream out(layoutPath);
  if (!out) {
    std::cerr << "Could not write layout: " << layoutPath << "\n";
    return false;
  }

  int minX = 0;
  int minY = 0;
  int maxX = 0;
  int maxY = 0;
  if (!panels.empty()) {
    minX = panels[0].x;
    minY = panels[0].y;
    maxX = panels[0].x + rotatedWidth(panels[0]);
    maxY = panels[0].y + rotatedHeight(panels[0]);
    for (const Panel &panel : panels) {
      minX = std::min(minX, panel.x);
      minY = std::min(minY, panel.y);
      maxX = std::max(maxX, panel.x + rotatedWidth(panel));
      maxY = std::max(maxY, panel.y + rotatedHeight(panel));
    }
  }

  out << "#pragma once\n\n";
  out << "#include <stdint.h>\n\n";
  out << "struct MatrixPortalPanelLayoutEntry {\n";
  out << "  int16_t sourceX;\n";
  out << "  int16_t sourceY;\n";
  out << "  int16_t width;\n";
  out << "  int16_t height;\n";
  out << "  int16_t x;\n";
  out << "  int16_t y;\n";
  out << "  uint16_t rotation;\n";
  out << "};\n\n";
  out << "#define PANEL_LAYOUT_PANEL_COUNT " << panels.size() << "\n";
  out << "#define PANEL_LAYOUT_PANEL_WIDTH " << defaultPanelWidth << "\n";
  out << "#define PANEL_LAYOUT_PANEL_HEIGHT " << defaultPanelHeight << "\n";
  out << "#define PANEL_LAYOUT_SOURCE_WIDTH " << logicalWidth << "\n";
  out << "#define PANEL_LAYOUT_SOURCE_HEIGHT " << logicalHeight << "\n";
  out << "#define PANEL_LAYOUT_PHYSICAL_WIDTH " << std::max(1, maxX - minX) << "\n";
  out << "#define PANEL_LAYOUT_PHYSICAL_HEIGHT " << std::max(1, maxY - minY) << "\n";
  out << "#define PANEL_LAYOUT_IDE_LAYOUT_MODE \"Custom\"\n";
  out << "#define PANEL_LAYOUT_IDE_ROTATION 0\n\n";
  out << "static const MatrixPortalPanelLayoutEntry PANEL_LAYOUT[] = {\n";
  for (const Panel &panel : panels) {
    out << "  {" << panel.sourceX << ", " << panel.sourceY << ", "
        << panel.width << ", " << panel.height << ", "
        << panel.x - minX << ", " << panel.y - minY << ", "
        << panel.rotation << "},\n";
  }
  out << "};\n";
  std::cerr << "Saved layout: " << layoutPath << "\n";
  return true;
}

bool loadLayout() {
  std::ifstream in(layoutPath);
  if (!in) {
    return false;
  }

  std::vector<Panel> loaded;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::vector<int> values = parseIntegers(line);
    if (values.size() == 7) {
      Panel panel;
      panel.sourceX = values[0];
      panel.sourceY = values[1];
      panel.width = values[2];
      panel.height = values[3];
      panel.x = values[4];
      panel.y = values[5];
      panel.rotation = ((values[6] % 360) + 360) % 360;
      loaded.push_back(panel);
      continue;
    }

    std::istringstream stream(line);
    Panel panel;
    if (stream >> panel.sourceX >> panel.sourceY >> panel.width >> panel.height
        >> panel.x >> panel.y >> panel.rotation) {
      panel.rotation = ((panel.rotation % 360) + 360) % 360;
      loaded.push_back(panel);
    }
  }

  if (loaded.empty()) {
    return false;
  }
  panels = loaded;
  selectedPanel = 0;
  std::cerr << "Loaded layout: " << layoutPath << "\n";
  requestPreviewDisplay();
  return true;
}

void toggleGrid() {
  showGrid = !showGrid;
  requestPreviewDisplay();
}

void zoomPreview(int delta) {
  scale = std::max(1, std::min(24, scale + delta));
  requestPreviewDisplay();
}

int selectedPanelAt(int mouseX, int mouseY) {
  for (int i = static_cast<int>(panels.size()) - 1; i >= 0; --i) {
    Rect rect = panelRect(panels[static_cast<size_t>(i)]);
    if (mouseX >= rect.x && mouseX < rect.x + rect.w &&
        mouseY >= rect.y && mouseY < rect.y + rect.h) {
      return i;
    }
  }
  return -1;
}

void handleMouseDownPoint(NSPoint point) {
  int x = static_cast<int>(point.x);
  int y = static_cast<int>(point.y);
  int hit = selectedPanelAt(x, y);
  if (hit >= 0) {
    selectedPanel = hit;
    Panel &panel = panels[static_cast<size_t>(selectedPanel)];
    int worldX = (x - viewOriginX) / scale + viewMinX;
    int worldY = (y - viewOriginY) / scale + viewMinY;
    dragOffsetX = worldX - panel.x;
    dragOffsetY = worldY - panel.y;
    dragging = true;
    requestPreviewDisplay();
  }
}

void handleMouseDraggedPoint(NSPoint point) {
  if (!dragging || selectedPanel < 0 || selectedPanel >= static_cast<int>(panels.size())) {
    return;
  }
  int x = static_cast<int>(point.x);
  int y = static_cast<int>(point.y);
  Panel &panel = panels[static_cast<size_t>(selectedPanel)];
  int worldX = (x - viewOriginX) / scale + viewMinX;
  int worldY = (y - viewOriginY) / scale + viewMinY;
  panel.x = snapCoord(worldX - dragOffsetX);
  panel.y = snapCoord(worldY - dragOffsetY);
  layoutDirty = true;
  requestPreviewDisplay();
}

void handleKeyEvent(NSEvent *event, bool down) {
  NSString *characters = [[event charactersIgnoringModifiers] lowercaseString];
  unichar character = [characters length] > 0 ? [characters characterAtIndex:0] : 0;
  unsigned short keyCode = [event keyCode];

  if (character == '1') {
    hardwareState.buttonUpPressed = down;
    return;
  }
  if (character == '2') {
    hardwareState.buttonDownPressed = down;
    return;
  }
  if (!down) {
    return;
  }

  switch (keyCode) {
  case 53:
    keepRunning = false;
    return;
  case 51:
  case 117:
    deleteSelectedPanel();
    return;
  case 123:
    hardwareState.autoTilt = false;
    hardwareState.accelX = std::max(-9.8f, hardwareState.accelX - 1.0f);
    return;
  case 124:
    hardwareState.autoTilt = false;
    hardwareState.accelX = std::min(9.8f, hardwareState.accelX + 1.0f);
    return;
  case 125:
    hardwareState.autoTilt = false;
    hardwareState.accelY = std::min(9.8f, hardwareState.accelY + 1.0f);
    return;
  case 126:
    hardwareState.autoTilt = false;
    hardwareState.accelY = std::max(-9.8f, hardwareState.accelY - 1.0f);
    return;
  case 48:
    if (!panels.empty()) {
      selectedPanel = (selectedPanel + 1) % static_cast<int>(panels.size());
      requestPreviewDisplay();
    }
    return;
  default:
    break;
  }

  switch (character) {
  case 'a': addPanel(); break;
  case 'r': rotateSelectedPanel(); break;
  case 'f': fitPanelsInRow(); break;
  case 's': saveLayout(); break;
  case 'l': loadLayout(); break;
  case 'g': toggleGrid(); break;
  case '0':
    hardwareState.autoTilt = true;
    hardwareState.accelX = 0.0f;
    hardwareState.accelY = 0.0f;
    hardwareState.accelZ = 9.8f;
    break;
  case '+':
  case '=':
    zoomPreview(1);
    break;
  case '-':
    zoomPreview(-1);
    break;
  case '[':
    hardwareState.analogValues[0] = std::max(0, hardwareState.analogValues[0] - 32);
    break;
  case ']':
    hardwareState.analogValues[0] = std::min(1023, hardwareState.analogValues[0] + 32);
    break;
  default:
    break;
  }
}

void drawPreview(CGContextRef context, NSRect bounds) {
  CGContextSetRGBFillColor(context, 0.03, 0.035, 0.045, 1.0);
  CGContextFillRect(context, NSRectToCGRect(bounds));

  calculateBounds();
  if (showGrid) {
    CGContextSetRGBStrokeColor(context, 0.13, 0.15, 0.18, 1.0);
    CGContextSetLineWidth(context, 1.0);
    int gridWidth = std::max(1, defaultPanelWidth * scale);
    int gridHeight = std::max(1, defaultPanelHeight * scale);
    for (int x = viewOriginX; x < static_cast<int>(bounds.size.width); x += gridWidth) {
      CGContextMoveToPoint(context, x + 0.5, viewOriginY);
      CGContextAddLineToPoint(context, x + 0.5, bounds.size.height);
    }
    for (int y = viewOriginY; y < static_cast<int>(bounds.size.height); y += gridHeight) {
      CGContextMoveToPoint(context, viewOriginX, y + 0.5);
      CGContextAddLineToPoint(context, bounds.size.width, y + 0.5);
    }
    CGContextStrokePath(context);
  }

  if (!activeMatrix) {
    return;
  }

  const auto &pixels = activeMatrix->pixels();
  for (size_t panelIndex = 0; panelIndex < panels.size(); ++panelIndex) {
    const Panel &panel = panels[panelIndex];
    Rect rect = panelRect(panel);

    int physicalWidth = rotatedWidth(panel);
    int physicalHeight = rotatedHeight(panel);
    for (int py = 0; py < physicalHeight; ++py) {
      for (int px = 0; px < physicalWidth; ++px) {
        int sx = 0;
        int sy = 0;
        uint8_t r = 4;
        uint8_t g = 4;
        uint8_t b = 5;
        if (mapPanelPixel(panel, px, py, sx, sy)) {
          colorToRgb(pixels[static_cast<size_t>(sy) * activeMatrix->width() + sx], r, g, b);
        }
        CGContextSetRGBFillColor(context, r / 255.0, g / 255.0, b / 255.0, 1.0);
        CGContextFillRect(context, CGRectMake(rect.x + px * scale, rect.y + py * scale, scale, scale));
      }
    }

    bool selected = panelIndex == static_cast<size_t>(selectedPanel);
    CGContextSetRGBStrokeColor(context,
                               selected ? 1.0 : 0.35,
                               selected ? 0.86 : 0.42,
                               selected ? 0.31 : 0.50,
                               1.0);
    CGContextSetLineWidth(context, selected ? 2.0 : 1.0);
    CGContextStrokeRect(context, CGRectMake(rect.x + 0.5, rect.y + 0.5,
                                           std::max(1, rect.w - 1), std::max(1, rect.h - 1)));
  }
}

NSSize preferredPreviewSize() {
  int maxX = logicalWidth;
  int maxY = logicalHeight;
  calculateBounds();
  for (const Panel &panel : panels) {
    maxX = std::max(maxX, panel.x - viewMinX + rotatedWidth(panel));
    maxY = std::max(maxY, panel.y - viewMinY + rotatedHeight(panel));
  }
  return NSMakeSize(std::max(980, viewOriginX + maxX * scale + 24),
                    std::max(560, viewOriginY + maxY * scale + 24));
}

void ensurePreviewLayout(const Adafruit_Protomatter &matrix) {
  logicalWidth = matrix.width();
  logicalHeight = matrix.height();
  if (layoutInitialized) {
    return;
  }

  bool loadedLayout = loadLayout();
  ensureDefaultPanels();
  if (!loadedLayout) {
    saveLayout();
  }
  layoutInitialized = true;
}

void writeFrameFile(const Adafruit_Protomatter &matrix) {
  if (framePath.empty() || panels.empty()) {
    return;
  }

  int minX = panels[0].x;
  int minY = panels[0].y;
  int maxX = panels[0].x + rotatedWidth(panels[0]);
  int maxY = panels[0].y + rotatedHeight(panels[0]);
  for (const Panel &panel : panels) {
    minX = std::min(minX, panel.x);
    minY = std::min(minY, panel.y);
    maxX = std::max(maxX, panel.x + rotatedWidth(panel));
    maxY = std::max(maxY, panel.y + rotatedHeight(panel));
  }

  int width = std::max(1, maxX - minX);
  int height = std::max(1, maxY - minY);
  std::vector<uint8_t> rgb(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, 3);
  const auto &pixels = matrix.pixels();

  for (const Panel &panel : panels) {
    int physicalWidth = rotatedWidth(panel);
    int physicalHeight = rotatedHeight(panel);
    for (int py = 0; py < physicalHeight; ++py) {
      for (int px = 0; px < physicalWidth; ++px) {
        int sx = 0;
        int sy = 0;
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        if (mapPanelPixel(panel, px, py, sx, sy)) {
          colorToRgb(pixels[static_cast<size_t>(sy) * matrix.width() + sx], r, g, b);
        }
        int dx = panel.x - minX + px;
        int dy = panel.y - minY + py;
        size_t offset = (static_cast<size_t>(dy) * static_cast<size_t>(width) + static_cast<size_t>(dx)) * 3;
        rgb[offset] = r;
        rgb[offset + 1] = g;
        rgb[offset + 2] = b;
      }
    }
  }

  std::string tempPath = framePath + ".tmp";
  std::ofstream out(tempPath, std::ios::binary);
  if (!out) {
    return;
  }
  out << "P6\n" << width << ' ' << height << "\n255\n";
  out.write(reinterpret_cast<const char *>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
  out.close();
  std::rename(tempPath.c_str(), framePath.c_str());
}

void ensureNativePreview(const Adafruit_Protomatter &matrix) {
  ensurePreviewLayout(matrix);
  if (previewWindow) {
    return;
  }

  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

  NSSize size = preferredPreviewSize();

  previewView = [[MatrixPreviewView alloc] initWithFrame:NSMakeRect(0, 0, size.width, size.height)];
  [previewView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                     NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
  previewWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, size.width, size.height)
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
  [previewWindow center];
  [previewWindow setContentView:previewView];
  [previewWindow setReleasedWhenClosed:NO];

  windowDelegate = [[PreviewWindowDelegate alloc] init];
  [previewWindow setDelegate:windowDelegate];

  actionTarget = [[PreviewActionTarget alloc] init];
  toolbarDelegate = [[PreviewToolbarDelegate alloc] init];
  NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"MatrixPreviewToolbar"];
  [toolbar setDelegate:toolbarDelegate];
  [toolbar setDisplayMode:NSToolbarDisplayModeIconAndLabel];
  [toolbar setAllowsUserCustomization:YES];
  [previewWindow setToolbar:toolbar];

  updateWindowTitle();
  [previewWindow makeFirstResponder:previewView];
  [previewWindow makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
  MatrixPortalSim::pumpEvents();
}
}

@implementation MatrixPreviewView
- (BOOL)isFlipped {
  return YES;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;
  CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
  drawPreview(context, [self bounds]);
}

- (void)mouseDown:(NSEvent *)event {
  [self.window makeFirstResponder:self];
  handleMouseDownPoint([self convertPoint:[event locationInWindow] fromView:nil]);
}

- (void)mouseDragged:(NSEvent *)event {
  handleMouseDraggedPoint([self convertPoint:[event locationInWindow] fromView:nil]);
}

- (void)mouseUp:(NSEvent *)event {
  (void)event;
  if (layoutDirty) {
    saveLayout();
    layoutDirty = false;
  }
  dragging = false;
}

- (void)keyDown:(NSEvent *)event {
  handleKeyEvent(event, true);
}

- (void)keyUp:(NSEvent *)event {
  handleKeyEvent(event, false);
}
@end

@implementation PreviewWindowDelegate
- (void)windowWillClose:(NSNotification *)notification {
  (void)notification;
  keepRunning = false;
}
@end

@implementation PreviewActionTarget
- (void)addPanel:(id)sender {
  (void)sender;
  addPanel();
}

- (void)deletePanel:(id)sender {
  (void)sender;
  deleteSelectedPanel();
}

- (void)rotatePanel:(id)sender {
  (void)sender;
  rotateSelectedPanel();
}

- (void)fitPanels:(id)sender {
  (void)sender;
  fitPanelsInRow();
}

- (void)saveLayout:(id)sender {
  (void)sender;
  saveLayout();
}

- (void)loadLayout:(id)sender {
  (void)sender;
  loadLayout();
}

- (void)toggleGrid:(id)sender {
  (void)sender;
  toggleGrid();
}

- (void)zoomIn:(id)sender {
  (void)sender;
  zoomPreview(1);
}

- (void)zoomOut:(id)sender {
  (void)sender;
  zoomPreview(-1);
}
@end

@implementation PreviewToolbarDelegate
- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
  (void)toolbar;
  return @[
    @"addPanel", @"deletePanel", @"rotatePanel", @"fitPanels",
    @"saveLayout", @"loadLayout", @"toggleGrid", @"zoomIn", @"zoomOut",
    NSToolbarFlexibleSpaceItemIdentifier
  ];
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
  (void)toolbar;
  return @[
    @"addPanel", @"deletePanel", @"rotatePanel", @"fitPanels",
    NSToolbarFlexibleSpaceItemIdentifier,
    @"toggleGrid", @"zoomOut", @"zoomIn",
    NSToolbarFlexibleSpaceItemIdentifier,
    @"saveLayout", @"loadLayout"
  ];
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
     itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier
 willBeInsertedIntoToolbar:(BOOL)flag {
  (void)toolbar;
  (void)flag;

  NSDictionary<NSString *, NSDictionary<NSString *, NSString *> *> *items = @{
    @"addPanel": @{@"label": @"Add", @"symbol": @"plus", @"action": @"addPanel:"},
    @"deletePanel": @{@"label": @"Delete", @"symbol": @"trash", @"action": @"deletePanel:"},
    @"rotatePanel": @{@"label": @"Rotate", @"symbol": @"rotate.right", @"action": @"rotatePanel:"},
    @"fitPanels": @{@"label": @"Fit Row", @"symbol": @"rectangle.grid.1x2", @"action": @"fitPanels:"},
    @"saveLayout": @{@"label": @"Save", @"symbol": @"square.and.arrow.down", @"action": @"saveLayout:"},
    @"loadLayout": @{@"label": @"Load", @"symbol": @"folder", @"action": @"loadLayout:"},
    @"toggleGrid": @{@"label": @"Grid", @"symbol": @"grid", @"action": @"toggleGrid:"},
    @"zoomIn": @{@"label": @"Zoom In", @"symbol": @"plus.magnifyingglass", @"action": @"zoomIn:"},
    @"zoomOut": @{@"label": @"Zoom Out", @"symbol": @"minus.magnifyingglass", @"action": @"zoomOut:"}
  };

  NSDictionary<NSString *, NSString *> *info = items[itemIdentifier];
  if (!info) {
    return nil;
  }

  NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
  item.label = info[@"label"];
  item.paletteLabel = info[@"label"];
  item.toolTip = info[@"label"];
  item.target = actionTarget;
  item.action = NSSelectorFromString(info[@"action"]);
  NSImage *image = [NSImage imageWithSystemSymbolName:info[@"symbol"]
                            accessibilityDescription:info[@"label"]];
  item.image = image;
  return item;
}
@end

Adafruit_Protomatter::Adafruit_Protomatter(
    int16_t width, uint8_t bitDepth, uint8_t numChains, uint8_t *rgbPins,
    uint8_t numAddrPins, uint8_t *addrPins, uint8_t clockPin,
    uint8_t latchPin, uint8_t oePin, bool doubleBuffer)
    : width_(width), height_(static_cast<int16_t>(2 << numAddrPins)),
      pixels_(static_cast<size_t>(width_) * static_cast<size_t>(height_), 0) {
  (void)bitDepth;
  (void)numChains;
  (void)rgbPins;
  (void)addrPins;
  (void)clockPin;
  (void)latchPin;
  (void)oePin;
  (void)doubleBuffer;
}

ProtomatterStatus Adafruit_Protomatter::begin() {
  return PROTOMATTER_OK;
}

int16_t Adafruit_Protomatter::width() const {
  return width_;
}

int16_t Adafruit_Protomatter::height() const {
  return height_;
}

uint16_t Adafruit_Protomatter::color565(uint8_t red, uint8_t green, uint8_t blue) const {
  return static_cast<uint16_t>(((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3));
}

void Adafruit_Protomatter::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) {
    return;
  }
  pixels_[static_cast<size_t>(y) * width_ + x] = color;
}

void Adafruit_Protomatter::fillScreen(uint16_t color) {
  std::fill(pixels_.begin(), pixels_.end(), color);
}

void Adafruit_Protomatter::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  drawLine(x, y, x + w - 1, y, color);
  drawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
  drawLine(x, y, x, y + h - 1, color);
  drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void Adafruit_Protomatter::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  for (int16_t yy = y; yy < y + h; ++yy) {
    for (int16_t xx = x; xx < x + w; ++xx) {
      drawPixel(xx, yy, color);
    }
  }
}

void Adafruit_Protomatter::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  const int16_t dx = static_cast<int16_t>(std::abs(x1 - x0));
  const int16_t sx = x0 < x1 ? 1 : -1;
  const int16_t dy = static_cast<int16_t>(-std::abs(y1 - y0));
  const int16_t sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;

  while (true) {
    drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int16_t e2 = static_cast<int16_t>(2 * err);
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void Adafruit_Protomatter::drawCircle(int16_t x0, int16_t y0, int16_t radius, uint16_t color) {
  int16_t x = radius;
  int16_t y = 0;
  int16_t err = 0;
  while (x >= y) {
    drawPixel(x0 + x, y0 + y, color);
    drawPixel(x0 + y, y0 + x, color);
    drawPixel(x0 - y, y0 + x, color);
    drawPixel(x0 - x, y0 + y, color);
    drawPixel(x0 - x, y0 - y, color);
    drawPixel(x0 - y, y0 - x, color);
    drawPixel(x0 + y, y0 - x, color);
    drawPixel(x0 + x, y0 - y, color);
    if (err <= 0) {
      ++y;
      err += 2 * y + 1;
    }
    if (err > 0) {
      --x;
      err -= 2 * x + 1;
    }
  }
}

void Adafruit_Protomatter::fillCircle(int16_t x0, int16_t y0, int16_t radius, uint16_t color) {
  for (int16_t y = -radius; y <= radius; ++y) {
    for (int16_t x = -radius; x <= radius; ++x) {
      if (x * x + y * y <= radius * radius) {
        drawPixel(x0 + x, y0 + y, color);
      }
    }
  }
}

void Adafruit_Protomatter::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                        int16_t x2, int16_t y2, uint16_t color) {
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

void Adafruit_Protomatter::println(const char *text) {
  Serial.println(text);
}

void Adafruit_Protomatter::show() {
  ++frameCount_;
  MatrixPortalSim::present(*this);
}

uint32_t Adafruit_Protomatter::getFrameCount() const {
  return frameCount_;
}

const std::vector<uint16_t> &Adafruit_Protomatter::pixels() const {
  return pixels_;
}

namespace MatrixPortalSim {
HardwareState &hardware() {
  return hardwareState;
}

void setStatusPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
  if (index == 0) {
    hardwareState.statusPixel = Rgb{r, g, b};
  }
}

void configureFromArgs(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--scale" && i + 1 < argc) {
      scale = std::max(1, std::atoi(argv[++i]));
    } else if (arg == "--max-frames" && i + 1 < argc) {
      maxFrames = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    } else if (arg == "--panel" && i + 1 < argc) {
      parseSize(argv[++i], defaultPanelWidth, defaultPanelHeight);
    } else if (arg == "--layout" && i + 1 < argc) {
      layoutPath = argv[++i];
    } else if (arg == "--control" && i + 1 < argc) {
      controlPath = argv[++i];
    } else if (arg == "--frame-file" && i + 1 < argc) {
      framePath = argv[++i];
    } else if (arg == "--no-window") {
      suppressPreviewWindow = true;
    }
  }
}

bool running() {
  return keepRunning;
}

void pumpEvents() {
  if (!previewWindow) {
    return;
  }
  NSEvent *event = nil;
  while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                     untilDate:[NSDate distantPast]
                                        inMode:NSDefaultRunLoopMode
                                       dequeue:YES])) {
    [NSApp sendEvent:event];
  }
  if (![previewWindow isVisible]) {
    keepRunning = false;
  }
}

void present(const Adafruit_Protomatter &matrix) {
  ensurePreviewLayout(matrix);
  activeMatrix = &matrix;
  if (!suppressPreviewWindow) {
    ensureNativePreview(matrix);
    pumpEvents();
  }
  applyControlFile();

  if (hardwareState.autoTilt) {
    auto elapsed = std::chrono::steady_clock::now() - startTime;
    double t = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 1000.0;
    hardwareState.accelX = static_cast<float>(std::sin(t * 0.9) * 4.5);
    hardwareState.accelY = static_cast<float>(std::cos(t * 0.7) * 4.5);
    hardwareState.accelZ = 9.8f;
  }

  writeFrameFile(matrix);
  if (!suppressPreviewWindow) {
    [previewView setNeedsDisplay:YES];
    [previewView displayIfNeeded];
  }
  ++presentedFrames;
  if (maxFrames > 0 && presentedFrames >= maxFrames) {
    keepRunning = false;
  }
}

void shutdown() {
  if (previewWindow) {
    [previewWindow orderOut:nil];
    [previewWindow close];
    previewWindow = nil;
  }
  previewView = nil;
  windowDelegate = nil;
  actionTarget = nil;
  toolbarDelegate = nil;
  activeMatrix = nullptr;
}

void sleep(unsigned long ms) {
  const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  while (std::chrono::steady_clock::now() < end) {
    pumpEvents();
    if (!keepRunning) {
      return;
    }
    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now());
    std::this_thread::sleep_for(std::min(std::chrono::milliseconds(10), remaining));
  }
}
}
