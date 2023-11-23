#include "broccoli/sample/sample2.hh"

#include "broccoli/engine.hh"
#include "glm/gtx/transform.hpp"

namespace broccoli {
  SampleActivity2::SampleActivity2(broccoli::Engine &engine)
  : broccoli::Activity(engine),
    m_cube_mesh(buildCubeMesh(engine))
  {}
  Mesh SampleActivity2::buildCubeMesh(broccoli::Engine &engine) {
    auto mb = engine.createMeshBuilder();
    
    // -X face, +X face:
    mb.quad(
      {.offset=glm::dvec3{-1.00, -1.00, -1.00}, .color=glm::dvec4{0.5, 0.0, 0.0, 1.0}},
      {.offset=glm::dvec3{-1.00, -1.00, +1.00}, .color=glm::dvec4{0.5, 0.0, 0.0, 1.0}},
      {.offset=glm::dvec3{-1.00, +1.00, +1.00}, .color=glm::dvec4{0.5, 0.0, 0.0, 1.0}},
      {.offset=glm::dvec3{-1.00, +1.00, -1.00}, .color=glm::dvec4{0.5, 0.0, 0.0, 1.0}}
    );
    mb.quad(
      {.offset=glm::dvec3{+1.00, -1.00, +1.00}, .color=glm::dvec4{1.0, 0.5, 0.5, 1.0}},
      {.offset=glm::dvec3{+1.00, -1.00, -1.00}, .color=glm::dvec4{1.0, 0.5, 0.5, 1.0}},
      {.offset=glm::dvec3{+1.00, +1.00, -1.00}, .color=glm::dvec4{1.0, 0.5, 0.5, 1.0}},
      {.offset=glm::dvec3{+1.00, +1.00, +1.00}, .color=glm::dvec4{1.0, 0.5, 0.5, 1.0}}
    );

    // -Y face, +Y face:
    mb.quad(
      {.offset=glm::dvec3{+1.00, -1.00, +1.00}, .color=glm::dvec4{0.0, 0.5, 0.0, 1.0}},
      {.offset=glm::dvec3{-1.00, -1.00, +1.00}, .color=glm::dvec4{0.0, 0.5, 0.0, 1.0}},
      {.offset=glm::dvec3{-1.00, -1.00, -1.00}, .color=glm::dvec4{0.0, 0.5, 0.0, 1.0}},
      {.offset=glm::dvec3{+1.00, -1.00, -1.00}, .color=glm::dvec4{0.0, 0.5, 0.0, 1.0}}
    );
    mb.quad(
      {.offset=glm::dvec3{-1.00, +1.00, -1.00}, .color=glm::dvec4{0.5, 1.0, 0.5, 1.0}},
      {.offset=glm::dvec3{-1.00, +1.00, +1.00}, .color=glm::dvec4{0.5, 1.0, 0.5, 1.0}},
      {.offset=glm::dvec3{+1.00, +1.00, +1.00}, .color=glm::dvec4{0.5, 1.0, 0.5, 1.0}},
      {.offset=glm::dvec3{+1.00, +1.00, -1.00}, .color=glm::dvec4{0.5, 1.0, 0.5, 1.0}}
    );

    // -Z face, +Z face:
    mb.quad(
      {.offset=glm::dvec3{-1.00, -1.00, -1.00}, .color=glm::dvec4{0.0, 0.0, 0.5, 1.0}},
      {.offset=glm::dvec3{-1.00, +1.00, -1.00}, .color=glm::dvec4{0.0, 0.0, 0.5, 1.0}},
      {.offset=glm::dvec3{+1.00, +1.00, -1.00}, .color=glm::dvec4{0.0, 0.0, 0.5, 1.0}},
      {.offset=glm::dvec3{+1.00, -1.00, -1.00}, .color=glm::dvec4{0.0, 0.0, 0.5, 1.0}}
    );
    mb.quad(
      {.offset=glm::dvec3{+1.00, -1.00, +1.00}, .color=glm::dvec4{0.5, 0.5, 1.0, 1.0}},
      {.offset=glm::dvec3{+1.00, +1.00, +1.00}, .color=glm::dvec4{0.5, 0.5, 1.0, 1.0}},
      {.offset=glm::dvec3{-1.00, +1.00, +1.00}, .color=glm::dvec4{0.5, 0.5, 1.0, 1.0}},
      {.offset=glm::dvec3{-1.00, -1.00, +1.00}, .color=glm::dvec4{0.5, 0.5, 1.0, 1.0}}
    );

    return mb.finish();
  }
  void SampleActivity2::draw(broccoli::RenderFrame &frame) {
    frame.clear(glm::dvec3{0.00, 0.00, 0.00});
    auto renderer = frame.use_camera({.transform=glm::mat4x4{1.0}, .fovy_deg=60.0});
    auto animation_progress = std::fmod(engine().currUpdateTimestampSec(), 5.0) / 5.0;
    auto angular_position = static_cast<float>(animation_progress * 2 * M_PI);
    auto rotation = glm::rotate(angular_position, glm::vec3{0.0f, 1.0f, 0.0f});
    auto transform1 = glm::translate(glm::vec3{0.00f, 2.00f * std::cos(animation_progress * 2*M_PI), -5.00f}) * rotation;
    std::array<glm::mat4x4, 2> instance_transforms = {transform1};
    renderer.draw(m_cube_mesh, std::span{instance_transforms.begin(), instance_transforms.size()});
  }
}
