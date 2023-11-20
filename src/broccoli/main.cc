#include <iostream>
#include <thread>
#include <chrono>

#include "SDL.h"

#include "broccoli/kernel.hh"

int main() {
  broccoli::Kernel kernel{"broccoli", 800, 450};
  kernel.run();
  return 0;
}
