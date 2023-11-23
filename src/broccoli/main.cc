#include <iostream>
#include <thread>
#include <chrono>

#include "broccoli/engine.hh"
#include "broccoli/sample.hh"

int main() {
  broccoli::Engine engine{"broccoli", 800, 450};
  engine.pushActivity(
    [] (broccoli::Engine &engine) -> std::unique_ptr<broccoli::Activity> {
      return std::make_unique<broccoli::SampleActivity2>(engine);
    }
  );
  engine.run();
  return 0;
}
