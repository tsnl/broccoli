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
    frame.draw(
      RenderCamera::createFromLookAt(
        glm::vec3{0.0f, 0.0f, 3.0f},
        glm::vec3{0.0f},
        glm::vec3{0.0f, 1.0f, 0.0f},
        45.0f,
        1.0f
      ),
      [this] (Renderer &renderer) {
        auto animation_progress = std::fmod(engine().currUpdateTimestampSec(), 5.0) / 5.0;
        auto angular_position = static_cast<float>(animation_progress * 2 * M_PI);
        auto translation = glm::translate(glm::vec3{0.00f, 1.00f * std::cos(animation_progress * 2*M_PI), -5.00f});
        auto rotation = glm::rotate(angular_position, glm::vec3{0.0f, 1.0f, 0.0f});
        auto xform = translation * rotation;
        std::array<glm::mat4x4, 2> instance_transforms = {xform};
        renderer.addMesh(m_cube_material, m_cube_geometry, std::span{instance_transforms.begin(), instance_transforms.size()});
        renderer.addDirectionalLight(glm::vec3{-1.0f, 0.0f, 0.0f}, 1e2f, glm::vec3{1.0f, 1.0f, 1.0f});
        // renderer.addPointLight(glm::vec3{0.0f, 0.0f, 2.0f}, 10.0f, glm::vec3{1.0f, 1.0f, 1.0f});
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
        // auto ts = engine().currUpdateTimestampSec();
        // if (ts >= 10.0 && (m_csm_saved_state & 0x1) == 0) {
        //   renderer.manager().debug_takeoutShadowMap(
        //     LightType::Directional,
        //     0, 0,
        //     [] (FloatBitmap bitmap) {
        //       bitmap.save("debug_csm_0_0.hdr");
        //     }
        //   );
        //   m_csm_saved_state |= 0x1;
        // }
        // if (ts >= 20.0 && (m_csm_saved_state & 0x2) == 0) {
        //   renderer.manager().debug_takeoutShadowMap(
        //     LightType::Directional,
        //     0, 1,
        //     [] (FloatBitmap bitmap) {
        //       bitmap.save("debug_csm_0_1.hdr");
        //     }
        //   );
        //   m_csm_saved_state |= 0x2;
        // }
        // if (ts >= 30.0 && (m_csm_saved_state & 0x4) == 0) {
        //   renderer.manager().debug_takeoutShadowMap(
        //     LightType::Directional,
        //     0, 2,
        //     [] (FloatBitmap bitmap) {
        //       bitmap.save("debug_csm_0_2.hdr");
        //     }
        //   );
        //   m_csm_saved_state |= 0x4;
        // }
        // if (ts >= 40.0 && (m_csm_saved_state & 0x8) == 0) {
        //   renderer.manager().debug_takeoutShadowMap(
        //     LightType::Directional,
        //     0, 3,
        //     [] (FloatBitmap bitmap) {
        //       bitmap.save("debug_csm_0_3.hdr");
        //     }
        //   );
        //   m_csm_saved_state |= 0x8;
        // }
      }
    );
  }
}
