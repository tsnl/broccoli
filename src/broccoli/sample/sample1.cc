#include "broccoli/sample/sample1.hh"

#include "broccoli/engine.hh"
#include "glm/gtx/transform.hpp"

namespace broccoli {
  SampleActivity1::SampleActivity1(broccoli::Engine &engine)
  : broccoli::Activity(engine),
    m_cube_geometry(buildCubeGeometry(engine)),
    m_cube_material(buildCubeMaterial(engine))
  {}
  Geometry SampleActivity1::buildCubeGeometry(broccoli::Engine &engine) {
    return engine.renderManager().createGeometryFactory().createCuboid(glm::dvec3{1.0});
  }
  Material SampleActivity1::buildCubeMaterial(broccoli::Engine &engine) {
    return engine.renderManager().createPbrMaterial(
      "Sample1.Cube",
      {glm::dvec3{0.77, 0.57, 0.29}},
      {glm::dvec3{0.00, 0.00, 1.00}},
      {1.0},
      {0.0},
      {1.00, 0.71, 0.29}
    );
  }
  void SampleActivity1::draw(broccoli::RenderFrame &frame) {
    frame.clear(glm::dvec3{0.00});
    auto renderer = frame.useCamera(RenderCamera::createDefault(45.0f, 25.0f));
    auto animation_progress = std::fmod(engine().currUpdateTimestampSec(), 5.0) / 5.0;
    auto angular_position = static_cast<float>(animation_progress * 2 * M_PI);
    auto translation = glm::translate(glm::vec3{0.00f, 1.00f * std::cos(animation_progress * 2*M_PI), -5.00f});
    auto rotation = glm::rotate(angular_position, glm::vec3{0.0f, 1.0f, 0.0f});
    auto xform = translation * rotation;
    std::array<glm::mat4x4, 2> instance_transforms = {xform};
    renderer.addMesh(m_cube_material, m_cube_geometry, std::span{instance_transforms.begin(), instance_transforms.size()});
    // renderer.addDirectionalLight(glm::vec3{+1.0f, -1.0f, -1.0f}, 1e2f, glm::vec3{1.0f, 1.0f, 1.0f});
    renderer.addPointLight(glm::vec3{0.0f, 0.0f, 2.0f}, 10.0f, glm::vec3{1.0f, 1.0f, 1.0f});
  }
}
