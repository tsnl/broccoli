#pragma once

#include "broccoli/engine.hh"

namespace broccoli {
  class SampleActivity1: public broccoli::Activity {
  private:
    Geometry m_cube_geometry;
    Material m_cube_material;
    Geometry m_wall_geometry;
    Material m_wall_material;
    int m_csm_saved_state = 0;
  public:
    SampleActivity1(broccoli::Engine &engine);
  private:
    static Geometry buildCubeGeometry(broccoli::Engine &engine);
    static Material buildCubeMaterial(broccoli::Engine &engine);
    static Geometry buildWallGeometry(broccoli::Engine &engine);
    static Material buildWallMaterial(broccoli::Engine &engine);
  public:
    void draw(broccoli::RenderFrame &frame) override;
  };
}
