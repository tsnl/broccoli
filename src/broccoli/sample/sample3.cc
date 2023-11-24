#include "broccoli/sample/sample3.hh"

#include "broccoli/engine.hh"
#include "glm/gtx/transform.hpp"

namespace broccoli {
  SampleActivity3::SampleActivity3(broccoli::Engine &engine)
  : broccoli::Activity(engine),
    m_cube_mesh(buildCubeMesh(engine))
  {}
  Mesh SampleActivity3::buildCubeMesh(broccoli::Engine &engine) {
    auto mb = engine.createMeshBuilder();
    
    // -X face, +X face:
    mb.quad(
      {.offset=glm::dvec3{-1.00, -1.00, -1.00}, .color=glm::dvec4{0.5, 0.0, 0.0, 1.0}, .shininess=0.05f},
      {.offset=glm::dvec3{-1.00, -1.00, +1.00}, .color=glm::dvec4{0.5, 0.0, 0.0, 1.0}, .shininess=0.05f},
      {.offset=glm::dvec3{-1.00, +1.00, +1.00}, .color=glm::dvec4{0.5, 0.0, 0.0, 1.0}, .shininess=0.05f},
      {.offset=glm::dvec3{-1.00, +1.00, -1.00}, .color=glm::dvec4{0.5, 0.0, 0.0, 1.0}, .shininess=0.05f}
    );
    mb.quad(
      {.offset=glm::dvec3{+1.00, -1.00, +1.00}, .color=glm::dvec4{1.0, 0.5, 0.5, 1.0}, .shininess=0.05f},
      {.offset=glm::dvec3{+1.00, -1.00, -1.00}, .color=glm::dvec4{1.0, 0.5, 0.5, 1.0}, .shininess=0.05f},
      {.offset=glm::dvec3{+1.00, +1.00, -1.00}, .color=glm::dvec4{1.0, 0.5, 0.5, 1.0}, .shininess=0.05f},
      {.offset=glm::dvec3{+1.00, +1.00, +1.00}, .color=glm::dvec4{1.0, 0.5, 0.5, 1.0}, .shininess=0.05f}
    );

    // -Y face, +Y face:
    mb.quad(
      {.offset=glm::dvec3{+1.00, -1.00, +1.00}, .color=glm::dvec4{0.0, 0.5, 0.0, 1.0}, .shininess=16.0f},
      {.offset=glm::dvec3{-1.00, -1.00, +1.00}, .color=glm::dvec4{0.0, 0.5, 0.0, 1.0}, .shininess=16.0f},
      {.offset=glm::dvec3{-1.00, -1.00, -1.00}, .color=glm::dvec4{0.0, 0.5, 0.0, 1.0}, .shininess=16.0f},
      {.offset=glm::dvec3{+1.00, -1.00, -1.00}, .color=glm::dvec4{0.0, 0.5, 0.0, 1.0}, .shininess=16.0f}
    );
    mb.quad(
      {.offset=glm::dvec3{-1.00, +1.00, -1.00}, .color=glm::dvec4{0.5, 1.0, 0.5, 1.0}, .shininess=16.0f},
      {.offset=glm::dvec3{-1.00, +1.00, +1.00}, .color=glm::dvec4{0.5, 1.0, 0.5, 1.0}, .shininess=16.0f},
      {.offset=glm::dvec3{+1.00, +1.00, +1.00}, .color=glm::dvec4{0.5, 1.0, 0.5, 1.0}, .shininess=16.0f},
      {.offset=glm::dvec3{+1.00, +1.00, -1.00}, .color=glm::dvec4{0.5, 1.0, 0.5, 1.0}, .shininess=16.0f}
    );

    // -Z face, +Z face:
    mb.quad(
      {.offset=glm::dvec3{-1.00, -1.00, -1.00}, .color=glm::dvec4{0.0, 0.0, 0.5, 1.0}, .shininess=16.0f},
      {.offset=glm::dvec3{-1.00, +1.00, -1.00}, .color=glm::dvec4{0.0, 0.0, 0.5, 1.0}, .shininess=16.0f},
      {.offset=glm::dvec3{+1.00, +1.00, -1.00}, .color=glm::dvec4{0.0, 0.0, 0.5, 1.0}, .shininess=16.0f},
      {.offset=glm::dvec3{+1.00, -1.00, -1.00}, .color=glm::dvec4{0.0, 0.0, 0.5, 1.0}, .shininess=16.0f}
    );
    mb.quad(
      {.offset=glm::dvec3{+1.00, -1.00, +1.00}, .color=glm::dvec4{0.5, 0.5, 1.0, 1.0}, .shininess=16.0f},
      {.offset=glm::dvec3{+1.00, +1.00, +1.00}, .color=glm::dvec4{0.5, 0.5, 1.0, 1.0}, .shininess=16.0f},
      {.offset=glm::dvec3{-1.00, +1.00, +1.00}, .color=glm::dvec4{0.5, 0.5, 1.0, 1.0}, .shininess=16.0f},
      {.offset=glm::dvec3{-1.00, -1.00, +1.00}, .color=glm::dvec4{0.5, 0.5, 1.0, 1.0}, .shininess=16.0f}
    );

    return mb.finish();
  }
  void SampleActivity3::draw(broccoli::RenderFrame &frame) {
    frame.clear(glm::dvec3{0.00, 0.00, 0.00});
    auto renderer = frame.use_camera({.transform=glm::mat4x4{1.0}, .fovy_deg=60.0});
    auto animation_progress = std::fmod(engine().currUpdateTimestampSec(), 5.0) / 5.0;
    auto angular_position = static_cast<float>(animation_progress * 2 * M_PI);
    auto translation = glm::translate(glm::vec3{0.00f, 0.00f * std::cos(animation_progress * 2*M_PI), -8.00f});
    auto rotation = glm::rotate(angular_position, glm::vec3{0.0f, 1.0f, 0.0f});
    auto xform = translation * rotation;
    std::array<glm::mat4x4, 2> instance_transforms = {xform};
    renderer.draw(m_cube_mesh, std::span{instance_transforms.begin(), instance_transforms.size()});
    renderer.addDirectionalLight(glm::vec3{+1.0f, -1.0f, -1.0f}, 1e5f, glm::vec3{0.98f, 0.90f, 0.61f});
    // renderer.addPointLight(glm::vec3{0.0f}, 1e3f, glm::vec3{1.0f, 1.0f, 1.0f});
  }
}
