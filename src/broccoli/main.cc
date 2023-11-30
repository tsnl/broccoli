#include <iostream>
#include <thread>
#include <chrono>

#include "cxxopts.hpp"

#include "broccoli/engine.hh"
#include "broccoli/sample.hh"

static cxxopts::ParseResult parseCliArgs(int argc, const char *argv[]);
static void runSample(int sample_id);

int main(int argc, const char *argv[]) {
  auto result = parseCliArgs(argc, argv);

  // Trying to run a sample if requested:
  int sample_id = 0;
  if (result.count("sample")) {
    sample_id = result["sample"].as<int>();
    CHECK(
      sample_id > 0, 
      [sample_id] () {
        return fmt::format("Invalid chosen sample: {}", sample_id);
      }
    );
  }
  if (sample_id) {
    runSample(sample_id);
    return 0;
  }

  // Otherwise, running main game:
  PANIC("Not implemented: main flow.");
  return 0;
}

static cxxopts::ParseResult parseCliArgs(int argc, const char *argv[]) {
  cxxopts::Options options{"broccoli", "A video-game"};
  options.add_options()
    ("s,sample", "Run a specific sample instead of the main flow", cxxopts::value<int>());
  return options.parse(argc, argv);
}

static void runSample(int sample_id) {
  #define PUSH_SAMPLE_ACTIVITY(N)                                                               \
    engine.pushActivity([] (broccoli::Engine &engine) -> std::unique_ptr<broccoli::Activity> {  \
      return std::make_unique<broccoli::SampleActivity##N>(engine);                             \
    })

  broccoli::Engine engine{glm::ivec2{1280, 720}, "broccoli"};
  switch (sample_id) {
    case 1: PUSH_SAMPLE_ACTIVITY(1); break;
    case 2: PUSH_SAMPLE_ACTIVITY(2); break;
    case 3: PUSH_SAMPLE_ACTIVITY(3); break;
    default: PANIC("Not implemented: sample {}", sample_id);
  }
  engine.run();

  #undef PUSH_SAMPLE_ACTIVITY
}
