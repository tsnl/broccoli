#include <iostream>
#include <thread>
#include <chrono>

#include "broccoli/engine.hh"
#include "broccoli/draw.hh"

class TestActivity: public broccoli::Activity {
public:
  TestActivity() = default;
public:
  void draw(broccoli::Renderer &renderer) override;
};
void TestActivity::draw(broccoli::Renderer &renderer) {
  renderer.begin_render_pass_3d(glm::dvec3{1.0, 0.0, 1.0});
}

int main() {
  broccoli::Engine engine{"broccoli", 800, 450};
  engine.push_activity([] () -> std::unique_ptr<broccoli::Activity> { return std::make_unique<TestActivity>(); });
  engine.run();
  return 0;
}
