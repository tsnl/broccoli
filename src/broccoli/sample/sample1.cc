#include "broccoli/sample/sample1.hh"

#include "broccoli/engine.hh"
#include "glm/gtx/transform.hpp"

namespace broccoli {
  SampleActivity1::SampleActivity1(broccoli::Engine &engine)
  : broccoli::Activity(engine),
    m_cube_mesh(buildCubeMesh(engine))
  {}
  Mesh SampleActivity1::buildCubeMesh(broccoli::Engine &engine) {
    auto mb = engine.createMeshBuilder();
    mb.triangle(
      {.offset=2.0*glm::dvec3{+1.00, -1.00, 0.00}, .color=glm::dvec4{1.0, 0.0, 0.0, 1.0}},
      {.offset=2.0*glm::dvec3{+0.00, +1.00, 0.00}, .color=glm::dvec4{0.0, 1.0, 0.0, 1.0}},
      {.offset=2.0*glm::dvec3{-1.00, -1.00, 0.00}, .color=glm::dvec4{0.0, 0.0, 1.0, 1.0}},
      true
    );
    return mb.finish();
  }
  void SampleActivity1::draw(broccoli::RenderFrame &frame) {
    frame.clear(glm::dvec3{0.00, 0.00, 0.00});
    auto renderer = frame.use_camera({.transform=glm::mat4x4{1.0}, .fovy_deg=60.0});
    auto angular_position_normalized = std::fmod(engine().currUpdateTimestampSec(), 2.0) / 2.0;
    auto angular_position = static_cast<float>(angular_position_normalized * 2 * M_PI);
    auto rotation = glm::rotate(angular_position, glm::vec3{0.0f, 1.0f, 0.0f});
    auto transform1 = glm::translate(glm::vec3{+4.00f, 0.00f, -10.00f}) * rotation;
    auto transform2 = glm::translate(glm::vec3{-4.00f, 0.00f, -10.00f}) * rotation;
    std::array<glm::mat4x4, 2> instance_transforms = {transform1, transform2};
    renderer.draw(m_cube_mesh, std::span{instance_transforms.begin(), instance_transforms.size()});
  }
}
