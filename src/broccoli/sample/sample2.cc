#include "broccoli/sample/sample2.hh"

#include "broccoli/engine.hh"
#include "glm/gtx/transform.hpp"

namespace broccoli {
  SampleActivity2::SampleActivity2(broccoli::Engine &engine)
  : broccoli::Activity(engine),
    m_floor_mesh(buildFloorMesh(engine)),
    m_tree_trunk_mesh(buildTreeTrunkMesh(engine)),
    m_tree_leaves_mesh(buildTreeLeavesMesh(engine))
  {}
  Mesh SampleActivity2::buildFloorMesh(broccoli::Engine &engine) {
    return engine.createMeshFactory().createCuboid(
      glm::dvec3{64.0, 1.0, 64.0}, 
      MeshFactory::Facet{.color=glm::dvec3{0.51, 0.33, 0.30} * 0.25, .shininess=0.0}
    );
  }
  Mesh SampleActivity2::buildTreeTrunkMesh(broccoli::Engine &engine) {
    return engine.createMeshFactory().createCuboid(
      glm::dvec3{2.0, 8.0, 2.0}, 
      MeshFactory::Facet{.color=glm::dvec3{0.60, 0.03, 0.05}, .shininess=1.0}
    );
  }
  Mesh SampleActivity2::buildTreeLeavesMesh(broccoli::Engine &engine) {
    return engine.createMeshFactory().createCuboid(
      glm::dvec3{8.0}, 
      MeshFactory::Facet{.color=glm::dvec3{0.05, 0.49, 0.21}, .shininess=32.0}
    );
  }
  void SampleActivity2::draw(broccoli::RenderFrame &frame) {
    frame.clear(glm::dvec3{0.34, 0.55, 0.90});
    auto renderer = frame.useCamera(RenderCamera::fromLookAt(
      glm::vec3{30.0f, 30.0f, 45.0f},
      glm::vec3{00.0f, 04.0f, 00.0f},
      glm::vec3{00.0f, 01.0f, 00.0f},
      30.0f
    ));
    renderer.addDirectionalLight(glm::dvec3{-10.0, -10.0, -1.0}, 1.0f, glm::dvec3{0.96, 0.76, 0.39});
    renderer.addPointLight(glm::dvec3{3.0, 2.0, 0.0}, 1.0f, glm::dvec3{0.96, 0.76, 0.39});
    renderer.addPointLight(glm::dvec3{0.0, 14.0, 0.0}, 1.0f, glm::dvec3{0.96, 0.76, 0.39});
    renderer.addPointLight(glm::dvec3{0.0, 10.0, 4.5}, 1.0f, glm::dvec3{0.96, 0.76, 0.39});
    renderer.draw(m_floor_mesh, glm::mat4x4{1.0});
    renderer.draw(m_tree_trunk_mesh, glm::translate(glm::dvec3{0.0, 1.0, 0.0}));
    renderer.draw(m_tree_leaves_mesh, glm::translate(glm::dvec3{0.0, 9.0, 0.0}));
  }
}
