#include "broccoli/sample/sample1.hh"

namespace broccoli {
  SampleActivity1::SampleActivity1(broccoli::Engine &engine)
  : broccoli::Activity(engine),
    m_cube_mesh(buildCubeMesh(engine))
  {}
  Mesh SampleActivity1::buildCubeMesh(broccoli::Engine &engine) {
    auto mb = engine.createMeshBuilder();
    mb.triangle(
      {.offset=glm::dvec3{+1.00, -1.00, -5.0}, .color=glm::dvec4{1.0, 0.0, 0.0, 1.0}},
      {.offset=glm::dvec3{+0.00, +1.00, -5.0}, .color=glm::dvec4{0.0, 1.0, 0.0, 1.0}},
      {.offset=glm::dvec3{-1.00, -1.00, -5.0}, .color=glm::dvec4{0.0, 0.0, 1.0, 1.0}}
    );
    return mb.finish();
  }
  void SampleActivity1::draw(broccoli::RenderFrame &frame) {
    frame.clear(glm::dvec3{0.20, 0.60, 0.00});
    auto renderer = frame.use_camera({.transform=glm::mat4x4{1.0}, .fovy_deg=90.0});
    renderer.draw(m_cube_mesh);
  }
}
