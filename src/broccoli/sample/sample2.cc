#include "broccoli/sample/sample2.hh"

#include "broccoli/engine.hh"
#include "glm/gtx/transform.hpp"

namespace broccoli {
  constexpr static bool USE_BLINN_PHONG = false;
  constexpr static bool USE_POINT_LIGHT = false;
  constexpr static double PBR_HDR_EXPOSURE_BIAS = 1.0f;
}

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
    return engine.renderManager().createGeometryFactory().createCuboid(glm::dvec3{2.0, 6.0, 2.0});
  }
  Geometry SampleActivity2::buildTreeLeavesGeometry(broccoli::Engine &engine) {
    return engine.renderManager().createGeometryFactory().createCuboid(glm::dvec3{8.0});
  }
  Material SampleActivity2::buildFloorMaterial(broccoli::Engine &engine) {
    if (USE_BLINN_PHONG) {
      return engine.renderManager().createBlinnPhongMaterial(
        "Sample3.Floor",
        {glm::normalize(glm::dvec3{0.51, 0.33, 0.30})},
        {glm::dvec3{0.00, 0.00, 1.00}},
        0.0
      );
    } else {
      return engine.renderManager().createPbrMaterial(
        "Sample3.Floor",
        {glm::dvec3{0.51, 0.33, 0.30}},
        {glm::dvec3{0.00, 0.00, 1.00}},
        {0.00},
        {1.00},
        glm::dvec3{0.04}
      );
    }
  }
  Material SampleActivity2::buildTreeTrunkMaterial(broccoli::Engine &engine) {
    if (USE_BLINN_PHONG) {
      return engine.renderManager().createBlinnPhongMaterial(
        "Sample3.Tree.Trunk",
        {glm::dvec3{0.60, 0.03, 0.05}},
        {glm::dvec3{0.00, 0.00, 1.00}},
        1.0
      );
    } else {
      return engine.renderManager().createPbrMaterial(
        "Sample3.Tree.Leaf",
        {glm::dvec3{0.60, 0.03, 0.05}},
        {glm::dvec3{0.00, 0.00, 1.00}},
        {0.00},
        {1.00},
        glm::dvec3{0.03}
      );
    }
  }
  Material SampleActivity2::buildTreeLeavesMaterial(broccoli::Engine &engine) {
    if (USE_BLINN_PHONG) {
      return engine.renderManager().createBlinnPhongMaterial(
        "Sample3.Tree.Leaf",
        {glm::dvec3{0.05, 0.49, 0.21}},
        {glm::dvec3{0.00, 0.00, 1.00}},
        32.0
      );
    } else {
      return engine.renderManager().createPbrMaterial(
        "Sample3.Tree.Leaf",
        {glm::dvec3{0.05, 0.49, 0.21}},
        {glm::dvec3{0.00, 0.00, 1.00}},
        {0.00},
        {0.15},
        glm::dvec3{0.03}
      );
    }
  }
  void SampleActivity2::draw(broccoli::RenderFrame &frame) {
    frame.clear(glm::dvec3{0.34, 0.55, 0.90});
    frame.draw(
      RenderCamera::createFromLookAt(
        glm::vec3{30.0f, 30.0f, 45.0f},
        glm::vec3{00.0f, 04.0f, 00.0f},
        glm::vec3{00.0f, 01.0f, 00.0f},
        30.0f,
        PBR_HDR_EXPOSURE_BIAS
      ),
      [this] (Renderer &renderer) {
        auto sun_animation_duration_sec = 5.0;
        auto sun_animation_progress = std::fmod(engine().currUpdateTimestampSec(), sun_animation_duration_sec) / sun_animation_duration_sec;
        auto sun_angular_position = sun_animation_progress * 2 * M_PI;
        auto sun_rake = 5.0;
        glm::dvec3 sun_dir = -glm::normalize(glm::dvec3{cos(sun_angular_position), sun_rake, sin(sun_angular_position)});
        float sun_intensity = USE_BLINN_PHONG ? 1.0f : 100.0f;
        renderer.addDirectionalLight(sun_dir, sun_intensity, glm::dvec3{0.96, 0.76, 0.39});
        if (USE_POINT_LIGHT) {
          renderer.addPointLight(glm::dvec3{3.0, 2.0, 0.0}, 1.0f, glm::dvec3{0.96, 0.76, 0.39});
          renderer.addPointLight(glm::dvec3{0.0, 14.0, 0.0}, 1.0f, glm::dvec3{0.96, 0.76, 0.39});
          renderer.addPointLight(glm::dvec3{0.0, 10.0, 4.5}, 1.0f, glm::dvec3{0.96, 0.76, 0.39});
        }
        renderer.addMesh(m_floor_material, m_floor_geometry, glm::mat4x4{1.0});
        renderer.addMesh(m_tree_trunk_material, m_tree_trunk_geometry, glm::translate(glm::dvec3{0.0, 2.0, 0.0}));
        renderer.addMesh(m_tree_leaves_material, m_tree_leaves_geometry, glm::translate(glm::dvec3{0.0, 9.0, 0.0}));
      }
    );
    frame.overlay(
      [this] (OverlayRenderer &renderer) {
        renderer.drawShadowMapTexture(
          glm::i32vec2{100, 100},
          glm::i32vec2{100},
          LightType::Directional,
          0, 0
        );
        renderer.drawShadowMapTexture(
          glm::i32vec2{100, 210},
          glm::i32vec2{100},
          LightType::Directional,
          0, 1
        );
        renderer.drawShadowMapTexture(
          glm::i32vec2{100, 320},
          glm::i32vec2{100},
          LightType::Directional,
          0, 2
        );
        renderer.drawShadowMapTexture(
          glm::i32vec2{100, 430},
          glm::i32vec2{100},
          LightType::Directional,
          0, 3
        );
      }
    );
  }
}
