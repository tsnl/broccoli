#include "broccoli/sample/sample2.hh"

#include "broccoli/engine.hh"
#include "glm/gtx/transform.hpp"

namespace broccoli {
  SampleActivity2::SampleActivity2(broccoli::Engine &engine)
  : broccoli::Activity(engine),
    m_floor_geometry(buildFloorGeometry(engine)),
    m_tree_trunk_geometry(buildTreeTrunkGeometry(engine)),
    m_tree_leaves_geometry(buildTreeLeavesGeometry(engine)),
    m_floor_material(buildFloorMaterial(engine)),
    m_tree_trunk_material(buildTreeTrunkMaterial(engine)),
    m_tree_leaves_material(buildTreeLeavesMaterial(engine))
  {}
  Geometry SampleActivity2::buildFloorGeometry(broccoli::Engine &engine) {
    return engine.renderManager().createGeometryFactory().createCuboid(glm::dvec3{64.0, 1.0, 64.0});
  }
  Geometry SampleActivity2::buildTreeTrunkGeometry(broccoli::Engine &engine) {
    return engine.renderManager().createGeometryFactory().createCuboid(glm::dvec3{2.0, 8.0, 2.0});
  }
  Geometry SampleActivity2::buildTreeLeavesGeometry(broccoli::Engine &engine) {
    return engine.renderManager().createGeometryFactory().createCuboid(glm::dvec3{8.0});
  }
  Material SampleActivity2::buildFloorMaterial(broccoli::Engine &engine) {
    // return engine.renderManager().createBlinnPhongMaterial(
    //   "Sample3.Floor",
    //   {glm::dvec3{0.51, 0.33, 0.30} * 0.25},
    //   {glm::dvec3{0.00, 0.00, 1.00}},
    //   0.0
    // );
    return engine.renderManager().createPbrMaterial(
      "Sample3.Floor",
      {glm::dvec3{0.51, 0.33, 0.30}},
      {glm::dvec3{0.00, 0.00, 1.00}},
      {0.00},
      {1.00},
      glm::dvec3{0.04}
    );
  }
  Material SampleActivity2::buildTreeTrunkMaterial(broccoli::Engine &engine) {
    // return engine.renderManager().createBlinnPhongMaterial(
    //   "Sample3.Tree.Trunk",
    //   {glm::dvec3{0.60, 0.03, 0.05}},
    //   {glm::dvec3{0.00, 0.00, 1.00}},
    //   1.0
    // );
    return engine.renderManager().createPbrMaterial(
      "Sample3.Tree.Leaf",
      {glm::dvec3{0.60, 0.03, 0.05}},
      {glm::dvec3{0.00, 0.00, 1.00}},
      {0.00},
      {1.00},
      glm::dvec3{0.03}
    );
  }
  Material SampleActivity2::buildTreeLeavesMaterial(broccoli::Engine &engine) {
    // return engine.renderManager().createBlinnPhongMaterial(
    //   "Sample3.Tree.Leaf",
    //   {glm::dvec3{0.05, 0.49, 0.21}},
    //   {glm::dvec3{0.00, 0.00, 1.00}},
    //   32.0
    // );
    return engine.renderManager().createPbrMaterial(
      "Sample3.Tree.Leaf",
      {glm::dvec3{0.05, 0.49, 0.21}},
      {glm::dvec3{0.00, 0.00, 1.00}},
      {0.00},
      {0.15},
      glm::dvec3{0.03}
    );
  }
  void SampleActivity2::draw(broccoli::RenderFrame &frame) {
    frame.clear(glm::dvec3{0.34, 0.55, 0.90});
    auto renderer = frame.useCamera(
      RenderCamera::fromLookAt(
        glm::vec3{30.0f, 30.0f, 45.0f},
        glm::vec3{00.0f, 04.0f, 00.0f},
        glm::vec3{00.0f, 01.0f, 00.0f},
        30.0f,
        15.0f
      )
    );
    renderer.addDirectionalLight(glm::dvec3{-10.0, -10.0, -1.0}, 100.0f, glm::dvec3{0.96, 0.76, 0.39});
    renderer.addPointLight(glm::dvec3{3.0, 2.0, 0.0}, 1.0f, glm::dvec3{0.96, 0.76, 0.39});
    renderer.addPointLight(glm::dvec3{0.0, 14.0, 0.0}, 1.0f, glm::dvec3{0.96, 0.76, 0.39});
    renderer.addPointLight(glm::dvec3{0.0, 10.0, 4.5}, 1.0f, glm::dvec3{0.96, 0.76, 0.39});
    renderer.draw(m_floor_material, m_floor_geometry, glm::mat4x4{1.0});
    renderer.draw(m_tree_trunk_material, m_tree_trunk_geometry, glm::translate(glm::dvec3{0.0, 1.0, 0.0}));
    renderer.draw(m_tree_leaves_material, m_tree_leaves_geometry, glm::translate(glm::dvec3{0.0, 9.0, 0.0}));
  }
}
