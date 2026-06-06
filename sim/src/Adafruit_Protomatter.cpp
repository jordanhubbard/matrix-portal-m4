#include "Adafruit_Protomatter.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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

enum class ToolAction {
  Stop,
  Examples,
  Verify,
  Upload,
  OpenSketch,
  Add,
  Delete,
  Rotate,
  FitRow,
  ToggleSize,
  Save,
  Load,
  Grid
};

enum class OverlayMode {
  None,
  Examples,
  Upload
};

struct Button {
  SDL_Rect rect;
  ToolAction action;
  const char *tooltip;
};

SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
bool keepRunning = true;
bool showGrid = true;
bool dragging = false;
OverlayMode overlayMode = OverlayMode::None;
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
int viewOriginY = 58;
int mouseX = 0;
int mouseY = 0;
uint32_t maxFrames = 0;
uint32_t presentedFrames = 0;
uint32_t statusUntil = 0;
std::string baseWindowTitle = "Matrix Portal M4 SDL Preview";
std::string layoutPath = "sim-panel-layout.txt";
std::string currentSketchPath =
#ifdef SKETCH_NAME
  SKETCH_NAME;
#else
  "Arduino/rainbow/rainbow.ino";
#endif
std::string statusMessage;
std::vector<Panel> panels;
std::vector<Button> buttons;
std::vector<std::string> availableSketches;
std::vector<std::string> availablePorts;
MatrixPortalSim::HardwareState hardwareState;

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

const char **glyphRows(char c) {
  static const char *space[] = {"00000","00000","00000","00000","00000","00000","00000"};
  static const char *unknown[] = {"11111","10001","00010","00100","00100","00000","00100"};
  static const char *digits[][7] = {
    {"01110","10001","10011","10101","11001","10001","01110"},
    {"00100","01100","00100","00100","00100","00100","01110"},
    {"01110","10001","00001","00010","00100","01000","11111"},
    {"11110","00001","00001","01110","00001","00001","11110"},
    {"00010","00110","01010","10010","11111","00010","00010"},
    {"11111","10000","11110","00001","00001","10001","01110"},
    {"00110","01000","10000","11110","10001","10001","01110"},
    {"11111","00001","00010","00100","01000","01000","01000"},
    {"01110","10001","10001","01110","10001","10001","01110"},
    {"01110","10001","10001","01111","00001","00010","01100"}
  };
  static const char *letters[][7] = {
    {"01110","10001","10001","11111","10001","10001","10001"},
    {"11110","10001","10001","11110","10001","10001","11110"},
    {"01110","10001","10000","10000","10000","10001","01110"},
    {"11110","10001","10001","10001","10001","10001","11110"},
    {"11111","10000","10000","11110","10000","10000","11111"},
    {"11111","10000","10000","11110","10000","10000","10000"},
    {"01110","10001","10000","10111","10001","10001","01110"},
    {"10001","10001","10001","11111","10001","10001","10001"},
    {"01110","00100","00100","00100","00100","00100","01110"},
    {"00111","00010","00010","00010","00010","10010","01100"},
    {"10001","10010","10100","11000","10100","10010","10001"},
    {"10000","10000","10000","10000","10000","10000","11111"},
    {"10001","11011","10101","10101","10001","10001","10001"},
    {"10001","11001","10101","10011","10001","10001","10001"},
    {"01110","10001","10001","10001","10001","10001","01110"},
    {"11110","10001","10001","11110","10000","10000","10000"},
    {"01110","10001","10001","10001","10101","10010","01101"},
    {"11110","10001","10001","11110","10100","10010","10001"},
    {"01111","10000","10000","01110","00001","00001","11110"},
    {"11111","00100","00100","00100","00100","00100","00100"},
    {"10001","10001","10001","10001","10001","10001","01110"},
    {"10001","10001","10001","10001","10001","01010","00100"},
    {"10001","10001","10001","10101","10101","10101","01010"},
    {"10001","10001","01010","00100","01010","10001","10001"},
    {"10001","10001","01010","00100","00100","00100","00100"},
    {"11111","00001","00010","00100","01000","10000","11111"}
  };
  static const char *dash[] = {"00000","00000","00000","11111","00000","00000","00000"};
  static const char *plus[] = {"00000","00100","00100","11111","00100","00100","00000"};
  static const char *slash[] = {"00001","00010","00010","00100","01000","01000","10000"};
  static const char *backslash[] = {"10000","01000","01000","00100","00010","00010","00001"};
  static const char *dot[] = {"00000","00000","00000","00000","00000","01100","01100"};
  static const char *colon[] = {"00000","01100","01100","00000","01100","01100","00000"};
  static const char *underscore[] = {"00000","00000","00000","00000","00000","00000","11111"};
  static const char *comma[] = {"00000","00000","00000","00000","01100","01100","01000"};
  static const char *parenL[] = {"00010","00100","01000","01000","01000","00100","00010"};
  static const char *parenR[] = {"01000","00100","00010","00010","00010","00100","01000"};
  static const char *gt[] = {"10000","01000","00100","00010","00100","01000","10000"};
  static const char *lt[] = {"00001","00010","00100","01000","00100","00010","00001"};
  static const char *bang[] = {"00100","00100","00100","00100","00100","00000","00100"};

  c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  if (c == ' ') return space;
  if (c >= '0' && c <= '9') return digits[c - '0'];
  if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
  switch (c) {
  case '-': return dash;
  case '+': return plus;
  case '/': return slash;
  case '\\': return backslash;
  case '.': return dot;
  case ':': return colon;
  case '_': return underscore;
  case ',': return comma;
  case '(': return parenL;
  case ')': return parenR;
  case '>': return gt;
  case '<': return lt;
  case '!': return bang;
  default: return unknown;
  }
}

int textWidth(const std::string &text, int textScale = 2) {
  return static_cast<int>(text.size()) * 6 * textScale;
}

void drawText(int x, int y, const std::string &text,
              SDL_Color color = SDL_Color{230, 236, 244, 255},
              int textScale = 2) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (char c : text) {
    const char **rows = glyphRows(c);
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 5; ++col) {
        if (rows[row][col] == '1') {
          SDL_Rect pixel{x + col * textScale, y + row * textScale, textScale, textScale};
          SDL_RenderFillRect(renderer, &pixel);
        }
      }
    }
    x += 6 * textScale;
  }
}

std::string shellQuote(const std::string &value) {
  std::string quoted = "'";
  for (char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

std::string sketchDisplayName(const std::string &path) {
  size_t slash = path.rfind('/');
  size_t start = slash == std::string::npos ? 0 : slash + 1;
  size_t dot = path.rfind('.');
  if (dot == std::string::npos || dot < start) {
    dot = path.size();
  }
  return path.substr(start, dot - start);
}

std::vector<std::string> readCommandLines(const std::string &command) {
  std::vector<std::string> lines;
  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return lines;
  }
  char buffer[512];
  while (fgets(buffer, sizeof(buffer), pipe)) {
    std::string line(buffer);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
      line.pop_back();
    }
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  pclose(pipe);
  return lines;
}

void refreshSketchList() {
  availableSketches = readCommandLines("find Arduino -mindepth 2 -maxdepth 2 -name '*.ino' | LC_ALL=C sort");
}

void refreshPortList() {
  availablePorts = readCommandLines("ls /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.* 2>/dev/null | LC_ALL=C sort -u");
}

void setStatus(const std::string &message, uint32_t durationMs = 5000) {
  statusMessage = message;
  statusUntil = SDL_GetTicks() + durationMs;
  std::cerr << message << "\n";
}

void runAsync(const std::string &command) {
  std::string full = command + " >/tmp/matrix-portal-sim-command.log 2>&1 &";
  int rc = std::system(full.c_str());
  if (rc != 0) {
    setStatus("Command launch failed");
  }
}

std::string makeGeometryArgs() {
  int panelCount = defaultPanelWidth > 0
    ? std::max(1, (logicalWidth + defaultPanelWidth - 1) / defaultPanelWidth)
    : 1;
  return " PANEL_COUNT=" + shellQuote(std::to_string(panelCount)) +
         " PANEL_WIDTH=" + shellQuote(std::to_string(defaultPanelWidth)) +
         " PANEL_HEIGHT=" + shellQuote(std::to_string(defaultPanelHeight)) +
         " SIM_PANEL=" + shellQuote(std::to_string(defaultPanelWidth) + "x" + std::to_string(defaultPanelHeight));
}

void relaunchSketch(const std::string &sketch) {
  setStatus("Launching " + sketchDisplayName(sketch));
  runAsync("make run SKETCH=" + shellQuote(sketch) +
           makeGeometryArgs() +
           " SIM_LAYOUT=" + shellQuote(layoutPath));
  keepRunning = false;
}

void verifySketch(const std::string &sketch) {
  setStatus("Verify started for " + sketchDisplayName(sketch) + " (see /tmp/matrix-portal-sim-command.log)");
  runAsync("make build SKETCH=" + shellQuote(sketch) + makeGeometryArgs());
}

void uploadSketch(const std::string &sketch, const std::string &port) {
  setStatus("Upload started to " + port + " (see /tmp/matrix-portal-sim-command.log)");
  runAsync("make upload SKETCH=" + shellQuote(sketch) + " PORT=" + shellQuote(port) + makeGeometryArgs());
}

void openSketch(const std::string &sketch) {
  setStatus("Opening " + sketchDisplayName(sketch));
  runAsync("open " + shellQuote(sketch));
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

void updateWindowTitle() {
  if (!window) {
    return;
  }
  std::ostringstream title;
  title << baseWindowTitle
        << " | " << currentSketchPath
        << " | examples E, verify V, upload U, open O"
        << " | panels: drag, A add, Del delete, R rotate, F row, P 32/64, S save, L load"
        << " | board: arrows tilt, 0 auto tilt, 1/2 buttons, +/- zoom";
  if (!panels.empty() && selectedPanel >= 0 && selectedPanel < static_cast<int>(panels.size())) {
    const Panel &panel = panels[static_cast<size_t>(selectedPanel)];
    title << " | selected " << (selectedPanel + 1)
          << " src " << panel.sourceX << "," << panel.sourceY
          << " pos " << panel.x << "," << panel.y
          << " rot " << panel.rotation;
  }
  SDL_SetWindowTitle(window, title.str().c_str());
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

SDL_Rect panelRect(const Panel &panel) {
  return SDL_Rect{
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
  updateWindowTitle();
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
  updateWindowTitle();
}

void rotateSelectedPanel() {
  if (panels.empty() || selectedPanel < 0 || selectedPanel >= static_cast<int>(panels.size())) {
    return;
  }
  Panel &panel = panels[static_cast<size_t>(selectedPanel)];
  panel.rotation = (panel.rotation + 90) % 360;
  updateWindowTitle();
}

void fitPanelsInRow() {
  int x = 0;
  for (Panel &panel : panels) {
    panel.x = x;
    panel.y = 0;
    x += rotatedWidth(panel);
  }
  updateWindowTitle();
}

void togglePanelSize() {
  if (defaultPanelWidth == 32 && defaultPanelHeight == 32) {
    defaultPanelWidth = 64;
    defaultPanelHeight = 64;
  } else {
    defaultPanelWidth = 32;
    defaultPanelHeight = 32;
  }
  updateWindowTitle();
}

bool saveLayout() {
  std::ofstream out(layoutPath);
  if (!out) {
    std::cerr << "Could not write layout: " << layoutPath << "\n";
    return false;
  }
  out << "# source_x source_y width height physical_x physical_y rotation\n";
  for (const Panel &panel : panels) {
    out << panel.sourceX << ' ' << panel.sourceY << ' '
        << panel.width << ' ' << panel.height << ' '
        << panel.x << ' ' << panel.y << ' '
        << panel.rotation << '\n';
  }
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
  updateWindowTitle();
  return true;
}

void drawIcon(const SDL_Rect &rect, ToolAction action) {
  int cx = rect.x + rect.w / 2;
  int cy = rect.y + rect.h / 2;
  SDL_SetRenderDrawColor(renderer, 224, 230, 238, 255);

  switch (action) {
  case ToolAction::Stop:
    {
      SDL_Rect stopRect{cx - 7, cy - 7, 14, 14};
      SDL_RenderDrawRect(renderer, &stopRect);
    }
    break;
  case ToolAction::Examples:
    {
      SDL_Rect leftPage{rect.x + 7, rect.y + 7, 7, 16};
      SDL_Rect rightPage{rect.x + 16, rect.y + 7, 7, 16};
      SDL_RenderDrawRect(renderer, &leftPage);
      SDL_RenderDrawRect(renderer, &rightPage);
    }
    SDL_RenderDrawLine(renderer, rect.x + 14, rect.y + 9, rect.x + 14, rect.y + 23);
    break;
  case ToolAction::Verify:
    SDL_RenderDrawLine(renderer, cx - 8, cy, cx - 2, cy + 7);
    SDL_RenderDrawLine(renderer, cx - 2, cy + 7, cx + 9, cy - 8);
    break;
  case ToolAction::Upload:
    SDL_RenderDrawLine(renderer, cx, cy + 9, cx, cy - 8);
    SDL_RenderDrawLine(renderer, cx - 7, cy - 1, cx, cy - 8);
    SDL_RenderDrawLine(renderer, cx + 7, cy - 1, cx, cy - 8);
    SDL_RenderDrawLine(renderer, cx - 9, cy + 9, cx + 9, cy + 9);
    break;
  case ToolAction::OpenSketch:
    {
      SDL_Rect folder{cx - 9, cy - 7, 18, 14};
      SDL_RenderDrawRect(renderer, &folder);
    }
    SDL_RenderDrawLine(renderer, cx - 5, cy - 10, cx + 1, cy - 10);
    SDL_RenderDrawLine(renderer, cx + 1, cy - 10, cx + 4, cy - 7);
    break;
  case ToolAction::Add:
    SDL_RenderDrawLine(renderer, cx - 8, cy, cx + 8, cy);
    SDL_RenderDrawLine(renderer, cx, cy - 8, cx, cy + 8);
    break;
  case ToolAction::Delete:
    SDL_RenderDrawLine(renderer, cx - 8, cy - 8, cx + 8, cy + 8);
    SDL_RenderDrawLine(renderer, cx + 8, cy - 8, cx - 8, cy + 8);
    break;
  case ToolAction::Rotate:
    SDL_RenderDrawLine(renderer, cx - 8, cy - 2, cx - 1, cy - 9);
    SDL_RenderDrawLine(renderer, cx - 1, cy - 9, cx + 8, cy - 2);
    SDL_RenderDrawLine(renderer, cx + 8, cy - 2, cx + 4, cy - 2);
    SDL_RenderDrawLine(renderer, cx + 8, cy - 2, cx + 8, cy - 6);
    SDL_RenderDrawLine(renderer, cx - 8, cy + 5, cx + 8, cy + 5);
    break;
  case ToolAction::FitRow:
    for (int i = 0; i < 3; ++i) {
      SDL_Rect block{rect.x + 7 + i * 10, cy - 6, 7, 12};
      SDL_RenderDrawRect(renderer, &block);
    }
    break;
  case ToolAction::ToggleSize:
    {
      SDL_Rect sizeRect{cx - 9, cy - 9, 18, 18};
      SDL_RenderDrawRect(renderer, &sizeRect);
    }
    SDL_RenderDrawLine(renderer, cx, cy - 9, cx, cy + 9);
    SDL_RenderDrawLine(renderer, cx - 9, cy, cx + 9, cy);
    break;
  case ToolAction::Save:
    {
      SDL_Rect saveRect{cx - 9, cy - 9, 18, 18};
      SDL_RenderDrawRect(renderer, &saveRect);
    }
    SDL_RenderDrawLine(renderer, cx - 5, cy + 5, cx + 5, cy + 5);
    break;
  case ToolAction::Load:
    SDL_RenderDrawLine(renderer, cx, cy - 9, cx, cy + 5);
    SDL_RenderDrawLine(renderer, cx - 6, cy - 1, cx, cy + 6);
    SDL_RenderDrawLine(renderer, cx + 6, cy - 1, cx, cy + 6);
    SDL_RenderDrawLine(renderer, cx - 9, cy + 9, cx + 9, cy + 9);
    break;
  case ToolAction::Grid:
    for (int i = 0; i < 3; ++i) {
      SDL_RenderDrawLine(renderer, cx - 9 + i * 9, cy - 9, cx - 9 + i * 9, cy + 9);
      SDL_RenderDrawLine(renderer, cx - 9, cy - 9 + i * 9, cx + 9, cy - 9 + i * 9);
    }
    break;
  }
}

void rebuildButtons() {
  buttons.clear();
  Button buttonSpecs[] = {
    {SDL_Rect{0, 0, 30, 30}, ToolAction::Stop, "Stop: stop the current simulation"},
    {SDL_Rect{0, 0, 30, 30}, ToolAction::Examples, "Examples: choose and launch another demo sketch"},
    {SDL_Rect{0, 0, 30, 30}, ToolAction::Verify, "Verify: cross-compile the current sketch for Matrix Portal M4"},
    {SDL_Rect{0, 0, 30, 30}, ToolAction::Upload, "Upload: choose a serial port and flash the current sketch"},
    {SDL_Rect{0, 0, 30, 30}, ToolAction::OpenSketch, "Open Sketch: open this Arduino file in your editor"},
    {SDL_Rect{0, 0, 30, 30}, ToolAction::Add, "Add Panel: add another virtual LED panel"},
    {SDL_Rect{0, 0, 30, 30}, ToolAction::Delete, "Delete Panel: remove the selected virtual panel"},
    {SDL_Rect{0, 0, 30, 30}, ToolAction::Rotate, "Rotate Panel: rotate the selected panel by 90 degrees"},
    {SDL_Rect{0, 0, 30, 30}, ToolAction::FitRow, "Fit Row: arrange panels side by side in one row"},
    {SDL_Rect{0, 0, 30, 30}, ToolAction::ToggleSize, "Panel Size: toggle new panels between 32x32 and 64x64"},
    {SDL_Rect{0, 0, 30, 30}, ToolAction::Save, "Save Layout: write the current panel arrangement to disk"},
    {SDL_Rect{0, 0, 30, 30}, ToolAction::Load, "Load Layout: reload the saved panel arrangement"},
    {SDL_Rect{0, 0, 30, 30}, ToolAction::Grid, "Grid: show or hide the panel alignment grid"}
  };

  for (size_t i = 0; i < sizeof(buttonSpecs) / sizeof(buttonSpecs[0]); ++i) {
    buttonSpecs[i].rect = SDL_Rect{10 + static_cast<int>(i) * 38, 8, 30, 30};
    buttons.push_back(buttonSpecs[i]);
  }
}

void drawToolbar() {
  SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
  SDL_Rect toolbarRect{0, 0, 2000, 50};
  SDL_RenderFillRect(renderer, &toolbarRect);

  rebuildButtons();
  for (const Button &button : buttons) {
    SDL_SetRenderDrawColor(renderer, 45, 54, 66, 255);
    SDL_RenderFillRect(renderer, &button.rect);
    SDL_SetRenderDrawColor(renderer, 96, 115, 138, 255);
    SDL_RenderDrawRect(renderer, &button.rect);
    drawIcon(button.rect, button.action);
  }

  int x = 520;
  int y = 12;

  SDL_SetRenderDrawColor(renderer, hardwareState.d13Led ? 255 : 72,
                         hardwareState.d13Led ? 40 : 16, 32, 255);
  SDL_Rect d13{x, y + 7, 16, 16};
  SDL_RenderFillRect(renderer, &d13);
  SDL_SetRenderDrawColor(renderer, 112, 64, 72, 255);
  SDL_RenderDrawRect(renderer, &d13);
  x += 28;

  SDL_SetRenderDrawColor(renderer, hardwareState.statusPixel.r,
                         hardwareState.statusPixel.g,
                         hardwareState.statusPixel.b, 255);
  SDL_Rect neo{x, y + 5, 20, 20};
  SDL_RenderFillRect(renderer, &neo);
  SDL_SetRenderDrawColor(renderer, 130, 142, 158, 255);
  SDL_RenderDrawRect(renderer, &neo);
  x += 34;

  SDL_SetRenderDrawColor(renderer, hardwareState.buttonUpPressed ? 250 : 62,
                         hardwareState.buttonUpPressed ? 196 : 72,
                         hardwareState.buttonUpPressed ? 92 : 86, 255);
  SDL_Rect upButton{x, y + 3, 22, 9};
  SDL_RenderFillRect(renderer, &upButton);
  SDL_SetRenderDrawColor(renderer, hardwareState.buttonDownPressed ? 250 : 62,
                         hardwareState.buttonDownPressed ? 196 : 72,
                         hardwareState.buttonDownPressed ? 92 : 86, 255);
  SDL_Rect downButton{x, y + 18, 22, 9};
  SDL_RenderFillRect(renderer, &downButton);
  x += 36;

  SDL_SetRenderDrawColor(renderer, 44, 54, 64, 255);
  SDL_Rect tiltBox{x, y, 54, 30};
  SDL_RenderDrawRect(renderer, &tiltBox);
  int cx = x + 27;
  int cy = y + 15;
  int tx = cx + static_cast<int>(std::clamp(hardwareState.accelX, -9.8f, 9.8f) * 2.2f);
  int ty = cy + static_cast<int>(std::clamp(hardwareState.accelY, -9.8f, 9.8f) * 1.2f);
  SDL_SetRenderDrawColor(renderer, hardwareState.autoTilt ? 96 : 244,
                         hardwareState.autoTilt ? 170 : 196,
                         hardwareState.autoTilt ? 255 : 88, 255);
  SDL_RenderDrawLine(renderer, cx, cy, tx, ty);
  SDL_Rect dot{tx - 2, ty - 2, 4, 4};
  SDL_RenderFillRect(renderer, &dot);
  x += 66;

  for (int i = 0; i < 5; ++i) {
    SDL_SetRenderDrawColor(renderer, 42, 50, 60, 255);
    SDL_Rect slot{x + i * 10, y + 2, 6, 26};
    SDL_RenderDrawRect(renderer, &slot);
    int filled = std::clamp(hardwareState.analogValues[i], 0, 1023) * 24 / 1023;
    SDL_SetRenderDrawColor(renderer, 88, 176, 160, 255);
    SDL_Rect bar{slot.x + 1, slot.y + slot.h - 1 - filled, slot.w - 2, filled};
    SDL_RenderFillRect(renderer, &bar);
  }

  std::string sketch = "SKETCH " + sketchDisplayName(currentSketchPath);
  drawText(10, 40, sketch, SDL_Color{158, 178, 198, 255}, 1);
  if (!statusMessage.empty() && SDL_GetTicks() < statusUntil) {
    drawText(170, 40, statusMessage, SDL_Color{244, 210, 120, 255}, 1);
  }
}

int hoveredButtonIndex() {
  for (size_t i = 0; i < buttons.size(); ++i) {
    const SDL_Rect &rect = buttons[i].rect;
    if (mouseX >= rect.x && mouseX < rect.x + rect.w &&
        mouseY >= rect.y && mouseY < rect.y + rect.h) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void drawTooltip() {
  int index = hoveredButtonIndex();
  if (index < 0) {
    return;
  }

  const char *tip = buttons[static_cast<size_t>(index)].tooltip;
  int windowWidth = 0;
  int windowHeight = 0;
  SDL_GetWindowSize(window, &windowWidth, &windowHeight);

  int width = textWidth(tip, 2) + 12;
  int height = 24;
  int x = std::min(std::max(8, mouseX + 14), std::max(8, windowWidth - width - 8));
  int y = mouseY + 18;
  if (y + height > windowHeight) {
    y = mouseY - height - 10;
  }

  SDL_SetRenderDrawColor(renderer, 16, 20, 25, 245);
  SDL_Rect box{x, y, width, height};
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 116, 138, 164, 255);
  SDL_RenderDrawRect(renderer, &box);
  drawText(x + 6, y + 6, tip, SDL_Color{236, 240, 246, 255}, 2);
}

SDL_Rect modalRect() {
  int windowWidth = 0;
  int windowHeight = 0;
  SDL_GetWindowSize(window, &windowWidth, &windowHeight);
  int width = std::min(760, std::max(420, windowWidth - 120));
  int height = std::min(520, std::max(280, windowHeight - 120));
  return SDL_Rect{(windowWidth - width) / 2, (windowHeight - height) / 2, width, height};
}

SDL_Rect modalRowRect(int rowIndex) {
  SDL_Rect modal = modalRect();
  return SDL_Rect{modal.x + 18, modal.y + 58 + rowIndex * 24, modal.w - 36, 21};
}

void drawModalFrame(const std::string &title, const std::string &subtitle) {
  SDL_Rect modal = modalRect();
  SDL_SetRenderDrawColor(renderer, 13, 17, 22, 244);
  SDL_RenderFillRect(renderer, &modal);
  SDL_SetRenderDrawColor(renderer, 94, 116, 140, 255);
  SDL_RenderDrawRect(renderer, &modal);
  SDL_Rect header{modal.x, modal.y, modal.w, 42};
  SDL_SetRenderDrawColor(renderer, 28, 36, 46, 255);
  SDL_RenderFillRect(renderer, &header);
  drawText(modal.x + 18, modal.y + 13, title, SDL_Color{238, 242, 248, 255}, 2);
  drawText(modal.x + 18, modal.y + 38, subtitle, SDL_Color{154, 176, 196, 255}, 1);
}

void drawExamplesOverlay() {
  if (availableSketches.empty()) {
    refreshSketchList();
  }

  drawModalFrame("EXAMPLES", "CLICK A SKETCH TO BUILD AND RELAUNCH. ESC CLOSES.");
  int maxRows = std::min(static_cast<int>(availableSketches.size()), 18);
  for (int i = 0; i < maxRows; ++i) {
    SDL_Rect row = modalRowRect(i);
    bool hover = mouseX >= row.x && mouseX < row.x + row.w && mouseY >= row.y && mouseY < row.y + row.h;
    bool current = availableSketches[static_cast<size_t>(i)] == currentSketchPath;
    SDL_SetRenderDrawColor(renderer, current ? 50 : (hover ? 42 : 24),
                           current ? 88 : (hover ? 54 : 30),
                           current ? 104 : (hover ? 64 : 36), 255);
    SDL_RenderFillRect(renderer, &row);
    drawText(row.x + 8, row.y + 5,
             sketchDisplayName(availableSketches[static_cast<size_t>(i)]) + "  " + availableSketches[static_cast<size_t>(i)],
             current ? SDL_Color{118, 222, 255, 255} : SDL_Color{224, 232, 240, 255}, 1);
  }
}

void drawUploadOverlay() {
  if (availablePorts.empty()) {
    refreshPortList();
  }

  drawModalFrame("UPLOAD CURRENT SKETCH", "CLICK A SERIAL PORT TO COMPILE AND FLASH. ESC CLOSES.");
  SDL_Rect sketchRow = modalRowRect(0);
  drawText(sketchRow.x + 8, sketchRow.y + 5, "SKETCH " + currentSketchPath, SDL_Color{246, 220, 138, 255}, 1);

  if (availablePorts.empty()) {
    SDL_Rect row = modalRowRect(2);
    drawText(row.x + 8, row.y + 5, "NO SERIAL PORTS FOUND. CONNECT MATRIX PORTAL M4 AND REOPEN UPLOAD.", SDL_Color{242, 134, 116, 255}, 1);
    return;
  }

  for (int i = 0; i < static_cast<int>(availablePorts.size()) && i < 16; ++i) {
    SDL_Rect row = modalRowRect(i + 2);
    bool hover = mouseX >= row.x && mouseX < row.x + row.w && mouseY >= row.y && mouseY < row.y + row.h;
    SDL_SetRenderDrawColor(renderer, hover ? 42 : 24, hover ? 64 : 32, hover ? 54 : 36, 255);
    SDL_RenderFillRect(renderer, &row);
    drawText(row.x + 8, row.y + 5, "UPLOAD TO " + availablePorts[static_cast<size_t>(i)], SDL_Color{224, 232, 240, 255}, 1);
  }
}

void drawOverlay() {
  if (overlayMode == OverlayMode::Examples) {
    drawExamplesOverlay();
  } else if (overlayMode == OverlayMode::Upload) {
    drawUploadOverlay();
  }
}

void runAction(ToolAction action) {
  switch (action) {
  case ToolAction::Stop:
    keepRunning = false;
    break;
  case ToolAction::Examples:
    refreshSketchList();
    overlayMode = OverlayMode::Examples;
    break;
  case ToolAction::Verify:
    verifySketch(currentSketchPath);
    break;
  case ToolAction::Upload:
    availablePorts.clear();
    refreshPortList();
    overlayMode = OverlayMode::Upload;
    break;
  case ToolAction::OpenSketch:
    openSketch(currentSketchPath);
    break;
  case ToolAction::Add: addPanel(); break;
  case ToolAction::Delete: deleteSelectedPanel(); break;
  case ToolAction::Rotate: rotateSelectedPanel(); break;
  case ToolAction::FitRow: fitPanelsInRow(); break;
  case ToolAction::ToggleSize: togglePanelSize(); break;
  case ToolAction::Save: saveLayout(); break;
  case ToolAction::Load: loadLayout(); break;
  case ToolAction::Grid: showGrid = !showGrid; break;
  }
}

int selectedPanelAt(int mouseX, int mouseY) {
  for (int i = static_cast<int>(panels.size()) - 1; i >= 0; --i) {
    SDL_Rect rect = panelRect(panels[static_cast<size_t>(i)]);
    if (mouseX >= rect.x && mouseX < rect.x + rect.w &&
        mouseY >= rect.y && mouseY < rect.y + rect.h) {
      return i;
    }
  }
  return -1;
}

bool handleOverlayMouseDown(const SDL_MouseButtonEvent &event) {
  if (overlayMode == OverlayMode::None || event.button != SDL_BUTTON_LEFT) {
    return false;
  }

  SDL_Rect modal = modalRect();
  bool insideModal = event.x >= modal.x && event.x < modal.x + modal.w &&
                     event.y >= modal.y && event.y < modal.y + modal.h;
  if (!insideModal) {
    overlayMode = OverlayMode::None;
    return true;
  }

  if (overlayMode == OverlayMode::Examples) {
    int maxRows = std::min(static_cast<int>(availableSketches.size()), 18);
    for (int i = 0; i < maxRows; ++i) {
      SDL_Rect row = modalRowRect(i);
      if (event.x >= row.x && event.x < row.x + row.w &&
          event.y >= row.y && event.y < row.y + row.h) {
        relaunchSketch(availableSketches[static_cast<size_t>(i)]);
        return true;
      }
    }
  } else if (overlayMode == OverlayMode::Upload) {
    for (int i = 0; i < static_cast<int>(availablePorts.size()) && i < 16; ++i) {
      SDL_Rect row = modalRowRect(i + 2);
      if (event.x >= row.x && event.x < row.x + row.w &&
          event.y >= row.y && event.y < row.y + row.h) {
        uploadSketch(currentSketchPath, availablePorts[static_cast<size_t>(i)]);
        overlayMode = OverlayMode::None;
        return true;
      }
    }
  }

  return true;
}

void handleMouseDown(const SDL_MouseButtonEvent &event) {
  if (event.button != SDL_BUTTON_LEFT) {
    return;
  }

  mouseX = event.x;
  mouseY = event.y;

  if (handleOverlayMouseDown(event)) {
    return;
  }

  for (const Button &button : buttons) {
    if (event.x >= button.rect.x && event.x < button.rect.x + button.rect.w &&
        event.y >= button.rect.y && event.y < button.rect.y + button.rect.h) {
      runAction(button.action);
      return;
    }
  }

  int hit = selectedPanelAt(event.x, event.y);
  if (hit >= 0) {
    selectedPanel = hit;
    Panel &panel = panels[static_cast<size_t>(selectedPanel)];
    int worldX = (event.x - viewOriginX) / scale + viewMinX;
    int worldY = (event.y - viewOriginY) / scale + viewMinY;
    dragOffsetX = worldX - panel.x;
    dragOffsetY = worldY - panel.y;
    dragging = true;
    updateWindowTitle();
  }
}

void handleMouseMotion(const SDL_MouseMotionEvent &event) {
  mouseX = event.x;
  mouseY = event.y;

  if (!dragging || selectedPanel < 0 || selectedPanel >= static_cast<int>(panels.size())) {
    return;
  }
  Panel &panel = panels[static_cast<size_t>(selectedPanel)];
  int worldX = (event.x - viewOriginX) / scale + viewMinX;
  int worldY = (event.y - viewOriginY) / scale + viewMinY;
  panel.x = snapCoord(worldX - dragOffsetX);
  panel.y = snapCoord(worldY - dragOffsetY);
}

void handleKey(const SDL_KeyboardEvent &event) {
  bool down = event.type == SDL_KEYDOWN;

  if (event.keysym.sym == SDLK_1) {
    hardwareState.buttonUpPressed = down;
    return;
  }
  if (event.keysym.sym == SDLK_2) {
    hardwareState.buttonDownPressed = down;
    return;
  }

  if (!down) {
    return;
  }

  switch (event.keysym.sym) {
  case SDLK_ESCAPE:
    if (overlayMode != OverlayMode::None) {
      overlayMode = OverlayMode::None;
    } else {
      keepRunning = false;
    }
    break;
  case SDLK_e:
    refreshSketchList();
    overlayMode = OverlayMode::Examples;
    break;
  case SDLK_v:
    verifySketch(currentSketchPath);
    break;
  case SDLK_u:
    availablePorts.clear();
    refreshPortList();
    overlayMode = OverlayMode::Upload;
    break;
  case SDLK_o:
    openSketch(currentSketchPath);
    break;
  case SDLK_a: addPanel(); break;
  case SDLK_BACKSPACE:
  case SDLK_DELETE: deleteSelectedPanel(); break;
  case SDLK_r: rotateSelectedPanel(); break;
  case SDLK_f: fitPanelsInRow(); break;
  case SDLK_p: togglePanelSize(); break;
  case SDLK_s: saveLayout(); break;
  case SDLK_l: loadLayout(); break;
  case SDLK_g: showGrid = !showGrid; break;
  case SDLK_0:
    hardwareState.autoTilt = true;
    hardwareState.accelX = 0.0f;
    hardwareState.accelY = 0.0f;
    hardwareState.accelZ = 9.8f;
    break;
  case SDLK_LEFT:
    hardwareState.autoTilt = false;
    hardwareState.accelX = std::max(-9.8f, hardwareState.accelX - 1.0f);
    break;
  case SDLK_RIGHT:
    hardwareState.autoTilt = false;
    hardwareState.accelX = std::min(9.8f, hardwareState.accelX + 1.0f);
    break;
  case SDLK_UP:
    hardwareState.autoTilt = false;
    hardwareState.accelY = std::max(-9.8f, hardwareState.accelY - 1.0f);
    break;
  case SDLK_DOWN:
    hardwareState.autoTilt = false;
    hardwareState.accelY = std::min(9.8f, hardwareState.accelY + 1.0f);
    break;
  case SDLK_TAB:
    if (!panels.empty()) {
      selectedPanel = (selectedPanel + 1) % static_cast<int>(panels.size());
      updateWindowTitle();
    }
    break;
  case SDLK_EQUALS:
  case SDLK_PLUS:
    scale = std::min(24, scale + 1);
    updateWindowTitle();
    break;
  case SDLK_MINUS:
    scale = std::max(1, scale - 1);
    updateWindowTitle();
    break;
  case SDLK_LEFTBRACKET:
    hardwareState.analogValues[0] = std::max(0, hardwareState.analogValues[0] - 32);
    break;
  case SDLK_RIGHTBRACKET:
    hardwareState.analogValues[0] = std::min(1023, hardwareState.analogValues[0] + 32);
    break;
  default:
    break;
  }
}

void ensureSdl(const Adafruit_Protomatter &matrix) {
  logicalWidth = matrix.width();
  logicalHeight = matrix.height();

  if (window) {
    return;
  }
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
    std::exit(1);
  }

  loadLayout();
  ensureDefaultPanels();
  calculateBounds();

  int maxX = logicalWidth;
  int maxY = logicalHeight;
  for (const Panel &panel : panels) {
    maxX = std::max(maxX, panel.x - viewMinX + rotatedWidth(panel));
    maxY = std::max(maxY, panel.y - viewMinY + rotatedHeight(panel));
  }

  window = SDL_CreateWindow(baseWindowTitle.c_str(), SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED,
                            std::max(980, viewOriginX + maxX * scale + 24),
                            std::max(560, viewOriginY + maxY * scale + 24),
                            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
    std::exit(1);
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!renderer) {
    std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
    std::exit(1);
  }

  updateWindowTitle();
}
}

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
  if (argc > 0 && argv[0]) {
    baseWindowTitle = argv[0];
  }
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
    }
  }
}

bool running() {
  return keepRunning;
}

void pumpEvents() {
  if (!window) {
    return;
  }
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      keepRunning = false;
    } else if (event.type == SDL_MOUSEBUTTONDOWN) {
      handleMouseDown(event.button);
    } else if (event.type == SDL_MOUSEBUTTONUP) {
      dragging = false;
    } else if (event.type == SDL_MOUSEMOTION) {
      handleMouseMotion(event.motion);
    } else if (event.type == SDL_KEYDOWN) {
      handleKey(event.key);
    } else if (event.type == SDL_KEYUP) {
      handleKey(event.key);
    }
  }
}

void present(const Adafruit_Protomatter &matrix) {
  ensureSdl(matrix);
  pumpEvents();
  calculateBounds();

  if (hardwareState.autoTilt) {
    double t = SDL_GetTicks() / 1000.0;
    hardwareState.accelX = static_cast<float>(std::sin(t * 0.9) * 4.5);
    hardwareState.accelY = static_cast<float>(std::cos(t * 0.7) * 4.5);
    hardwareState.accelZ = 9.8f;
  }

  SDL_SetRenderDrawColor(renderer, 8, 10, 13, 255);
  SDL_RenderClear(renderer);
  drawToolbar();

  if (showGrid) {
    SDL_SetRenderDrawColor(renderer, 30, 36, 44, 255);
    for (int x = viewOriginX; x < 2000; x += defaultPanelWidth * scale) {
      SDL_RenderDrawLine(renderer, x, viewOriginY, x, 2000);
    }
    for (int y = viewOriginY; y < 2000; y += defaultPanelHeight * scale) {
      SDL_RenderDrawLine(renderer, viewOriginX, y, 2000, y);
    }
  }

  const auto &pixels = matrix.pixels();
  for (size_t panelIndex = 0; panelIndex < panels.size(); ++panelIndex) {
    const Panel &panel = panels[panelIndex];
    SDL_Rect rect = panelRect(panel);

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
          colorToRgb(pixels[static_cast<size_t>(sy) * matrix.width() + sx], r, g, b);
        }
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_Rect pixelRect{rect.x + px * scale, rect.y + py * scale, scale, scale};
        SDL_RenderFillRect(renderer, &pixelRect);
      }
    }

    SDL_SetRenderDrawColor(renderer, panelIndex == static_cast<size_t>(selectedPanel) ? 255 : 88,
                           panelIndex == static_cast<size_t>(selectedPanel) ? 220 : 100,
                           panelIndex == static_cast<size_t>(selectedPanel) ? 80 : 116, 255);
    SDL_RenderDrawRect(renderer, &rect);
    SDL_Rect inset{rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2};
    SDL_RenderDrawRect(renderer, &inset);
  }

  drawOverlay();
  drawTooltip();

  SDL_RenderPresent(renderer);

  ++presentedFrames;
  if (maxFrames > 0 && presentedFrames >= maxFrames) {
    keepRunning = false;
  }
}

void shutdown() {
  if (renderer) {
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
  }
  if (window) {
    SDL_DestroyWindow(window);
    window = nullptr;
  }
  SDL_Quit();
}

void sleep(unsigned long ms) {
  const uint32_t end = SDL_GetTicks() + static_cast<uint32_t>(ms);
  do {
    pumpEvents();
    if (!keepRunning) {
      return;
    }
    SDL_Delay(std::min<uint32_t>(10, end > SDL_GetTicks() ? end - SDL_GetTicks() : 0));
  } while (SDL_GetTicks() < end);
}
}
