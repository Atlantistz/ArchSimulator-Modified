#include "options.h"
#include "simulator.h"
#include "utils.h"

int main(int argc, char **argv) {
  // parse commandline arguments
  // and init simulator from parsed options
  if (auto const simulator = Simulator::Create(Options::Parse(argc, argv));
      simulator) {
    simulator->Run();
  }

  // won't exit here
  return 0;
}