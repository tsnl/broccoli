#include "broccoli/sample/sample3.hh"

#include "broccoli/engine.hh"
#include "glm/gtx/transform.hpp"

namespace broccoli {
  SampleActivity3::SampleActivity3(broccoli::Engine &engine)
  : broccoli::Activity(engine)
  {}
  void SampleActivity3::draw(broccoli::RenderFrame &frame) {
    frame.clear(glm::dvec3{0.34, 0.55, 0.90});
    auto renderer = frame.useCamera(RenderCamera::fromLookAt(
      glm::vec3{30.0f, 30.0f, 45.0f},
      glm::vec3{00.0f, 04.0f, 00.0f},
      glm::vec3{00.0f, 01.0f, 00.0f},
      30.0f
    ));
  }
}
