#include "broccoli/sample/sample1.hh"

#include "broccoli/engine.hh"
#include "glm/gtx/transform.hpp"

namespace broccoli {
  SampleActivity1::SampleActivity1(broccoli::Engine &engine)
  : broccoli::Activity(engine),
    m_cube_mesh(buildCubeMesh(engine))
  {}
  Mesh SampleActivity1::buildCubeMesh(broccoli::Engine &engine) {
    return engine.createMeshFactory().createCuboid(
      glm::dvec3{1.0},
      MeshFactory::Facet{.color=glm::dvec3{0.0, 1.0, 1.0}, .shininess=16.0},
      MeshFactory::Facet{.color=glm::dvec3{1.0, 0.0, 0.0}, .shininess=16.0},
      MeshFactory::Facet{.color=glm::dvec3{1.0, 0.0, 1.0}, .shininess=16.0},
      MeshFactory::Facet{.color=glm::dvec3{0.0, 1.0, 0.0}, .shininess=16.0},
      MeshFactory::Facet{.color=glm::dvec3{1.0, 1.0, 0.0}, .shininess=16.0},
      MeshFactory::Facet{.color=glm::dvec3{0.0, 0.0, 1.0}, .shininess=16.0}
    );
  }
  void SampleActivity1::draw(broccoli::RenderFrame &frame) {
    frame.clear(glm::dvec3{0.00});
    auto renderer = frame.use_camera({.transform=glm::mat4x4{1.0}, .fovy_deg=45.0});
    auto animation_progress = std::fmod(engine().currUpdateTimestampSec(), 5.0) / 5.0;
    auto angular_position = static_cast<float>(animation_progress * 2 * M_PI);
    auto translation = glm::translate(glm::vec3{0.00f, 1.00f * std::cos(animation_progress * 2*M_PI), -5.00f});
    auto rotation = glm::rotate(angular_position, glm::vec3{0.0f, 1.0f, 0.0f});
    auto xform = translation * rotation;
    std::array<glm::mat4x4, 2> instance_transforms = {xform};
    renderer.draw(m_cube_mesh, std::span{instance_transforms.begin(), instance_transforms.size()});
    renderer.addDirectionalLight(glm::vec3{+1.0f, -1.0f, -1.0f}, 1.0f, glm::vec3{1.0f, 1.0f, 1.0f});
    // renderer.addPointLight(glm::vec3{5.0f, 0.0f, -2.5f}, 1.0f, glm::vec3{0.0f, 1.0f, 0.0f});
  }
}
