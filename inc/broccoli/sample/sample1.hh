#pragma once

#include "broccoli/engine.hh"

namespace broccoli {
  class SampleActivity1: public broccoli::Activity {
  private:
    Geometry m_cube_geometry;
    Material m_cube_material;
  public:
    SampleActivity1(broccoli::Engine &engine);
  private:
    static Geometry buildCubeGeometry(broccoli::Engine &engine);
    static Material buildCubeMaterial(broccoli::Engine &engine);
  public:
    void draw(broccoli::RenderFrame &frame) override;
  };
}
