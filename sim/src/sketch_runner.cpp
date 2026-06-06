#include "Adafruit_Protomatter.h"

#ifndef SKETCH_SOURCE
#error "SKETCH_SOURCE must be defined by the build"
#endif

#include SKETCH_SOURCE

int main(int argc, char **argv) {
  MatrixPortalSim::configureFromArgs(argc, argv);
  setup();

  while (MatrixPortalSim::running()) {
    loop();
    MatrixPortalSim::pumpEvents();
  }

  MatrixPortalSim::shutdown();
  return 0;
}
