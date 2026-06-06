#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
enum class Action {
  Stop,
  Refresh,
  Run,
  Verify,
  Upload,
  Setup,
  Ports,
  Monitor,
  OpenSketch,
  OpenLog
};

struct Button {
  SDL_Rect rect;
  Action action;
  const char *tooltip;
};

SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
bool running = true;
int mouseX = 0;
int mouseY = 0;
int scale = 8;
int selectedSketch = 0;
int selectedPort = 0;
uint32_t statusUntil = 0;
std::string panelSize = "64x64";
std::string layoutPath = "sim-panel-layout.txt";
std::string statusMessage;
std::vector<std::string> sketches;
std::vector<std::string> ports;
std::vector<Button> buttons;
const char *commandLog = "/tmp/matrix-portal-sim-command.log";

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
  static const char *dot[] = {"00000","00000","00000","00000","00000","01100","01100"};
  static const char *colon[] = {"00000","01100","01100","00000","01100","01100","00000"};
  static const char *underscore[] = {"00000","00000","00000","00000","00000","00000","11111"};

  c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  if (c == ' ') return space;
  if (c >= '0' && c <= '9') return digits[c - '0'];
  if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
  switch (c) {
  case '-': return dash;
  case '+': return plus;
  case '/': return slash;
  case '.': return dot;
  case ':': return colon;
  case '_': return underscore;
  default: return unknown;
  }
}

int textWidth(const std::string &text, int textScale = 2) {
  return static_cast<int>(text.size()) * 6 * textScale;
}

void drawText(int x, int y, const std::string &text, SDL_Color color, int textScale = 2) {
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

std::string sketchName(const std::string &path) {
  size_t slash = path.rfind('/');
  size_t start = slash == std::string::npos ? 0 : slash + 1;
  size_t dot = path.rfind('.');
  if (dot == std::string::npos || dot < start) {
    dot = path.size();
  }
  return path.substr(start, dot - start);
}

void refreshSketches() {
  sketches = readCommandLines("find Arduino -mindepth 2 -maxdepth 2 -name '*.ino' | LC_ALL=C sort");
  if (selectedSketch >= static_cast<int>(sketches.size())) {
    selectedSketch = sketches.empty() ? 0 : static_cast<int>(sketches.size()) - 1;
  }
}

void refreshPorts() {
  ports = readCommandLines("ls /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.* 2>/dev/null | LC_ALL=C sort -u");
  if (selectedPort >= static_cast<int>(ports.size())) {
    selectedPort = ports.empty() ? 0 : static_cast<int>(ports.size()) - 1;
  }
}

void setStatus(const std::string &message, uint32_t durationMs = 6000) {
  statusMessage = message;
  statusUntil = SDL_GetTicks() + durationMs;
  std::cerr << message << "\n";
}

std::string selectedSketchPath() {
  if (sketches.empty()) {
    return "";
  }
  selectedSketch = std::max(0, std::min(selectedSketch, static_cast<int>(sketches.size()) - 1));
  return sketches[static_cast<size_t>(selectedSketch)];
}

std::string selectedPortPath() {
  if (ports.empty()) {
    return "";
  }
  selectedPort = std::max(0, std::min(selectedPort, static_cast<int>(ports.size()) - 1));
  return ports[static_cast<size_t>(selectedPort)];
}

void runAsync(const std::string &command, const std::string &message) {
  std::string full = command + " >" + shellQuote(commandLog) + " 2>&1 &";
  setStatus(message + " (log: /tmp/matrix-portal-sim-command.log)");
  std::system(full.c_str());
}

void launchSketch(const std::string &sketch) {
  std::string command =
    "make sim-run SKETCH=" + shellQuote(sketch) +
    " SIM_SCALE=" + shellQuote(std::to_string(scale)) +
    " SIM_PANEL=" + shellQuote(panelSize) +
    " SIM_LAYOUT=" + shellQuote(layoutPath) +
    " >" + shellQuote(commandLog) + " 2>&1 &";
  setStatus("Launching " + sketchName(sketch));
  std::system(command.c_str());
  running = false;
}

void runSelectedSketch() {
  std::string sketch = selectedSketchPath();
  if (sketch.empty()) {
    setStatus("No sketch selected");
    return;
  }
  launchSketch(sketch);
}

void verifySelectedSketch() {
  std::string sketch = selectedSketchPath();
  if (sketch.empty()) {
    setStatus("No sketch selected");
    return;
  }
  runAsync("make arduino-compile SKETCH=" + shellQuote(sketch), "Verifying " + sketchName(sketch));
}

void uploadSelectedSketch() {
  std::string sketch = selectedSketchPath();
  std::string port = selectedPortPath();
  if (sketch.empty()) {
    setStatus("No sketch selected");
    return;
  }
  if (port.empty()) {
    setStatus("No serial port selected");
    return;
  }
  runAsync("make arduino-upload SKETCH=" + shellQuote(sketch) + " PORT=" + shellQuote(port),
           "Uploading " + sketchName(sketch) + " to " + port);
}

void setupToolchain() {
  runAsync("make setup", "Installing Arduino core and libraries");
}

void monitorSelectedPort() {
  std::string port = selectedPortPath();
  if (port.empty()) {
    setStatus("No serial port selected");
    return;
  }
  runAsync(".tools/bin/arduino-cli --config-file arduino-cli.yaml monitor --port " + shellQuote(port),
           "Starting serial monitor for " + port);
}

void openSelectedSketch() {
  std::string sketch = selectedSketchPath();
  if (sketch.empty()) {
    setStatus("No sketch selected");
    return;
  }
  runAsync("open " + shellQuote(sketch), "Opening " + sketchName(sketch));
}

void openCommandLog() {
  runAsync("touch " + shellQuote(commandLog) + " && open " + shellQuote(commandLog), "Opening command log");
}

void parseArgs(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--scale" && i + 1 < argc) {
      scale = std::max(1, std::atoi(argv[++i]));
    } else if (arg == "--panel" && i + 1 < argc) {
      panelSize = argv[++i];
    } else if (arg == "--layout" && i + 1 < argc) {
      layoutPath = argv[++i];
    }
  }
}

void rebuildButtons() {
  buttons.clear();
  Button specs[] = {
    {SDL_Rect{0, 0, 34, 34}, Action::Stop, "Stop: close the simulator IDE"},
    {SDL_Rect{0, 0, 34, 34}, Action::Refresh, "Refresh: rescan examples and serial ports"},
    {SDL_Rect{0, 0, 34, 34}, Action::Run, "Run: compile and launch the selected sketch in the simulator"},
    {SDL_Rect{0, 0, 34, 34}, Action::Verify, "Verify: compile the selected sketch for Matrix Portal M4"},
    {SDL_Rect{0, 0, 34, 34}, Action::Upload, "Upload: compile and flash the selected sketch to the selected port"},
    {SDL_Rect{0, 0, 34, 34}, Action::Setup, "Setup: install Arduino core and pinned libraries"},
    {SDL_Rect{0, 0, 34, 34}, Action::Ports, "Ports: refresh serial port list"},
    {SDL_Rect{0, 0, 34, 34}, Action::Monitor, "Monitor: start Arduino CLI serial monitor for the selected port"},
    {SDL_Rect{0, 0, 34, 34}, Action::OpenSketch, "Open Sketch: open the selected .ino file"},
    {SDL_Rect{0, 0, 34, 34}, Action::OpenLog, "Open Log: open the latest verify/upload/monitor command log"}
  };
  for (size_t i = 0; i < sizeof(specs) / sizeof(specs[0]); ++i) {
    specs[i].rect = SDL_Rect{14 + static_cast<int>(i) * 42, 14, 34, 34};
    buttons.push_back(specs[i]);
  }
}

void drawIcon(const Button &button) {
  const SDL_Rect &rect = button.rect;
  int cx = rect.x + rect.w / 2;
  int cy = rect.y + rect.h / 2;
  SDL_SetRenderDrawColor(renderer, 232, 238, 246, 255);
  if (button.action == Action::Stop) {
    SDL_Rect stopRect{cx - 7, cy - 7, 14, 14};
    SDL_RenderDrawRect(renderer, &stopRect);
  } else if (button.action == Action::Refresh) {
    SDL_RenderDrawLine(renderer, cx - 9, cy, cx - 3, cy - 7);
    SDL_RenderDrawLine(renderer, cx - 3, cy - 7, cx - 3, cy - 2);
    SDL_RenderDrawLine(renderer, cx + 9, cy, cx + 3, cy + 7);
    SDL_RenderDrawLine(renderer, cx + 3, cy + 7, cx + 3, cy + 2);
    SDL_RenderDrawLine(renderer, cx - 7, cy - 7, cx + 5, cy - 7);
    SDL_RenderDrawLine(renderer, cx + 7, cy + 7, cx - 5, cy + 7);
  } else if (button.action == Action::Run) {
    SDL_RenderDrawLine(renderer, cx - 6, cy - 9, cx + 8, cy);
    SDL_RenderDrawLine(renderer, cx + 8, cy, cx - 6, cy + 9);
    SDL_RenderDrawLine(renderer, cx - 6, cy + 9, cx - 6, cy - 9);
  } else if (button.action == Action::Verify) {
    SDL_RenderDrawLine(renderer, cx - 9, cy, cx - 2, cy + 8);
    SDL_RenderDrawLine(renderer, cx - 2, cy + 8, cx + 10, cy - 9);
  } else if (button.action == Action::Upload) {
    SDL_RenderDrawLine(renderer, cx, cy + 9, cx, cy - 9);
    SDL_RenderDrawLine(renderer, cx - 7, cy - 2, cx, cy - 9);
    SDL_RenderDrawLine(renderer, cx + 7, cy - 2, cx, cy - 9);
    SDL_RenderDrawLine(renderer, cx - 9, cy + 9, cx + 9, cy + 9);
  } else if (button.action == Action::Setup) {
    SDL_Rect wrench{cx - 8, cy - 3, 16, 6};
    SDL_RenderDrawRect(renderer, &wrench);
    SDL_RenderDrawLine(renderer, cx + 5, cy - 8, cx + 10, cy - 3);
    SDL_RenderDrawLine(renderer, cx + 5, cy + 8, cx + 10, cy + 3);
  } else if (button.action == Action::Ports) {
    SDL_RenderDrawLine(renderer, cx - 9, cy - 7, cx + 9, cy - 7);
    SDL_RenderDrawLine(renderer, cx - 9, cy, cx + 9, cy);
    SDL_RenderDrawLine(renderer, cx - 9, cy + 7, cx + 9, cy + 7);
    SDL_Rect dot{cx - 2, cy - 2, 4, 4};
    SDL_RenderFillRect(renderer, &dot);
  } else if (button.action == Action::Monitor) {
    SDL_RenderDrawLine(renderer, cx - 10, cy + 7, cx - 5, cy - 5);
    SDL_RenderDrawLine(renderer, cx - 5, cy - 5, cx, cy + 4);
    SDL_RenderDrawLine(renderer, cx, cy + 4, cx + 5, cy - 6);
    SDL_RenderDrawLine(renderer, cx + 5, cy - 6, cx + 10, cy + 7);
  } else if (button.action == Action::OpenSketch) {
    SDL_Rect file{cx - 7, cy - 9, 14, 18};
    SDL_RenderDrawRect(renderer, &file);
    SDL_RenderDrawLine(renderer, cx - 3, cy - 4, cx + 4, cy - 4);
    SDL_RenderDrawLine(renderer, cx - 3, cy + 2, cx + 4, cy + 2);
  } else if (button.action == Action::OpenLog) {
    SDL_Rect log{cx - 8, cy - 8, 16, 16};
    SDL_RenderDrawRect(renderer, &log);
    SDL_RenderDrawLine(renderer, cx - 4, cy - 3, cx + 4, cy - 3);
    SDL_RenderDrawLine(renderer, cx - 4, cy + 3, cx + 4, cy + 3);
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

int hoveredSketchIndex() {
  for (int i = 0; i < static_cast<int>(sketches.size()); ++i) {
    SDL_Rect row{42, 92 + i * 28, 610, 24};
    if (mouseX >= row.x && mouseX < row.x + row.w &&
        mouseY >= row.y && mouseY < row.y + row.h) {
      return i;
    }
  }
  return -1;
}

void drawTooltip() {
  std::string tip;
  int buttonIndex = hoveredButtonIndex();
  if (buttonIndex >= 0) {
    tip = buttons[static_cast<size_t>(buttonIndex)].tooltip;
  } else {
    int sketchIndex = hoveredSketchIndex();
    if (sketchIndex >= 0) {
      tip = "Select: click. Run: double-click or use Run button.";
    }
  }

  if (tip.empty()) {
    return;
  }

  int windowWidth = 0;
  int windowHeight = 0;
  SDL_GetWindowSize(window, &windowWidth, &windowHeight);
  int width = std::min(textWidth(tip, 2) + 12, windowWidth - 24);
  int x = std::min(std::max(8, mouseX + 14), std::max(8, windowWidth - width - 8));
  int y = mouseY + 18;
  if (y + 24 > windowHeight) {
    y = mouseY - 34;
  }

  SDL_SetRenderDrawColor(renderer, 16, 20, 25, 246);
  SDL_Rect box{x, y, width, 24};
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 116, 138, 164, 255);
  SDL_RenderDrawRect(renderer, &box);
  drawText(x + 6, y + 6, tip.substr(0, static_cast<size_t>((width - 12) / 12)), SDL_Color{236, 240, 246, 255}, 2);
}

void render() {
  SDL_SetRenderDrawColor(renderer, 8, 10, 13, 255);
  SDL_RenderClear(renderer);

  SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
  SDL_Rect toolbar{0, 0, 980, 64};
  SDL_RenderFillRect(renderer, &toolbar);

  rebuildButtons();
  for (const Button &button : buttons) {
    SDL_SetRenderDrawColor(renderer, 45, 54, 66, 255);
    SDL_RenderFillRect(renderer, &button.rect);
    SDL_SetRenderDrawColor(renderer, 96, 115, 138, 255);
    SDL_RenderDrawRect(renderer, &button.rect);
    drawIcon(button);
  }

  drawText(448, 18, "MATRIX PORTAL M4 SIMULATOR IDE", SDL_Color{236, 240, 246, 255}, 2);
  drawText(448, 42, "SELECT, RUN, VERIFY, UPLOAD, MONITOR. ESC OR STOP CLOSES.", SDL_Color{156, 178, 198, 255}, 1);

  SDL_SetRenderDrawColor(renderer, 20, 26, 34, 255);
  SDL_Rect browser{32, 82, 630, 470};
  SDL_RenderFillRect(renderer, &browser);
  SDL_SetRenderDrawColor(renderer, 80, 98, 118, 255);
  SDL_RenderDrawRect(renderer, &browser);

  for (int i = 0; i < static_cast<int>(sketches.size()) && i < 16; ++i) {
    SDL_Rect row{42, 92 + i * 28, 610, 24};
    bool hover = i == hoveredSketchIndex();
    bool selected = i == selectedSketch;
    SDL_SetRenderDrawColor(renderer, selected ? 52 : (hover ? 42 : 25),
                           selected ? 84 : (hover ? 58 : 34),
                           selected ? 96 : (hover ? 70 : 42), 255);
    SDL_RenderFillRect(renderer, &row);
    drawText(row.x + 8, row.y + 6,
             sketchName(sketches[static_cast<size_t>(i)]) + "  " + sketches[static_cast<size_t>(i)],
             SDL_Color{224, 232, 240, 255}, 1);
  }

  if (sketches.empty()) {
    drawText(52, 102, "NO ARDUINO EXAMPLES FOUND", SDL_Color{244, 164, 120, 255}, 2);
  }

  SDL_SetRenderDrawColor(renderer, 20, 26, 34, 255);
  SDL_Rect side{678, 82, 270, 470};
  SDL_RenderFillRect(renderer, &side);
  SDL_SetRenderDrawColor(renderer, 80, 98, 118, 255);
  SDL_RenderDrawRect(renderer, &side);

  drawText(690, 98, "SELECTED", SDL_Color{246, 220, 138, 255}, 1);
  drawText(690, 118, selectedSketchPath().empty() ? "NONE" : sketchName(selectedSketchPath()), SDL_Color{224, 232, 240, 255}, 2);
  drawText(690, 150, "PORTS", SDL_Color{246, 220, 138, 255}, 1);

  if (ports.empty()) {
    drawText(690, 172, "NO PORTS FOUND", SDL_Color{244, 164, 120, 255}, 1);
  } else {
    for (int i = 0; i < static_cast<int>(ports.size()) && i < 9; ++i) {
      SDL_Rect row{690, 170 + i * 24, 246, 21};
      bool hover = mouseX >= row.x && mouseX < row.x + row.w && mouseY >= row.y && mouseY < row.y + row.h;
      bool selected = i == selectedPort;
      SDL_SetRenderDrawColor(renderer, selected ? 48 : (hover ? 42 : 26),
                             selected ? 80 : (hover ? 62 : 36),
                             selected ? 88 : (hover ? 70 : 44), 255);
      SDL_RenderFillRect(renderer, &row);
      drawText(row.x + 6, row.y + 5, ports[static_cast<size_t>(i)], SDL_Color{222, 232, 240, 255}, 1);
    }
  }

  drawText(690, 406, "STATUS", SDL_Color{246, 220, 138, 255}, 1);
  if (!statusMessage.empty() && SDL_GetTicks() < statusUntil) {
    drawText(690, 428, statusMessage.substr(0, 34), SDL_Color{156, 220, 178, 255}, 1);
  } else {
    drawText(690, 428, "READY", SDL_Color{156, 178, 198, 255}, 1);
  }

  drawTooltip();
  SDL_RenderPresent(renderer);
}

void handleMouseDown(const SDL_MouseButtonEvent &event) {
  mouseX = event.x;
  mouseY = event.y;

  int buttonIndex = hoveredButtonIndex();
  if (buttonIndex >= 0) {
    Action action = buttons[static_cast<size_t>(buttonIndex)].action;
    if (action == Action::Stop) {
      running = false;
    } else if (action == Action::Refresh) {
      refreshSketches();
      refreshPorts();
      setStatus("Refreshed examples and ports");
    } else if (action == Action::Run) {
      runSelectedSketch();
    } else if (action == Action::Verify) {
      verifySelectedSketch();
    } else if (action == Action::Upload) {
      uploadSelectedSketch();
    } else if (action == Action::Setup) {
      setupToolchain();
    } else if (action == Action::Ports) {
      refreshPorts();
      setStatus("Refreshed serial ports");
    } else if (action == Action::Monitor) {
      monitorSelectedPort();
    } else if (action == Action::OpenSketch) {
      openSelectedSketch();
    } else if (action == Action::OpenLog) {
      openCommandLog();
    }
    return;
  }

  for (int i = 0; i < static_cast<int>(ports.size()) && i < 9; ++i) {
    SDL_Rect row{690, 170 + i * 24, 246, 21};
    if (event.x >= row.x && event.x < row.x + row.w &&
        event.y >= row.y && event.y < row.y + row.h) {
      selectedPort = i;
      return;
    }
  }

  int sketchIndex = hoveredSketchIndex();
  if (sketchIndex >= 0) {
    selectedSketch = sketchIndex;
    if (event.clicks >= 2) {
      runSelectedSketch();
    }
  }
}

void eventLoop() {
  SDL_Event event;
  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_MOUSEMOTION) {
        mouseX = event.motion.x;
        mouseY = event.motion.y;
      } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        handleMouseDown(event.button);
      } else if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        running = false;
      } else if (event.key.keysym.sym == SDLK_r) {
        refreshSketches();
        refreshPorts();
      } else if (event.key.keysym.sym == SDLK_RETURN) {
        runSelectedSketch();
      } else if (event.key.keysym.sym == SDLK_v) {
        verifySelectedSketch();
      } else if (event.key.keysym.sym == SDLK_u) {
        uploadSelectedSketch();
      } else if (event.key.keysym.sym == SDLK_m) {
        monitorSelectedPort();
      } else if (event.key.keysym.sym == SDLK_o) {
        openSelectedSketch();
      } else if (event.key.keysym.sym == SDLK_UP) {
        selectedSketch = std::max(0, selectedSketch - 1);
      } else if (event.key.keysym.sym == SDLK_DOWN) {
        if (!sketches.empty()) {
          selectedSketch = std::min(static_cast<int>(sketches.size()) - 1, selectedSketch + 1);
        }
      }
    }
    }
    render();
    SDL_Delay(16);
  }
}
}

int main(int argc, char **argv) {
  parseArgs(argc, argv);
  refreshSketches();
  refreshPorts();

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
    return 1;
  }

  window = SDL_CreateWindow("Matrix Portal M4 Simulator IDE", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 980, 590,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
    SDL_Quit();
    return 1;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!renderer) {
    std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  eventLoop();

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
