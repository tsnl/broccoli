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

  int sample_id = 0;
  int min_sample_id = 1;
  int max_sample_id = 3;
  if (result.count("sample")) {
    sample_id = result["sample"].as<int>();
    CHECK(
      sample_id >= min_sample_id && sample_id <= max_sample_id, 
      [sample_id] () {
        return fmt::format("Invalid chosen sample: {}", sample_id);
      }
    );
  }

  if (sample_id) {
    runSample(sample_id);
    return 0;
  }

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
  broccoli::Engine engine{"broccoli", 800, 450};
  switch (sample_id) {
    case 1:
      engine.pushActivity([] (broccoli::Engine &engine) -> std::unique_ptr<broccoli::Activity> {
        return std::make_unique<broccoli::SampleActivity1>(engine);
      });
      break;
    case 2:
      engine.pushActivity([] (broccoli::Engine &engine) -> std::unique_ptr<broccoli::Activity> {
        return std::make_unique<broccoli::SampleActivity2>(engine);
      });
      break;
    case 3:
      engine.pushActivity([] (broccoli::Engine &engine) -> std::unique_ptr<broccoli::Activity> {
        return std::make_unique<broccoli::SampleActivity3>(engine);
      });
      break;
    default:
      PANIC("Not implemented: choosing a constructor for sample_id={}", sample_id);
  }
  engine.run();
}
