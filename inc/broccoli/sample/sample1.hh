#pragma once

#include "broccoli/engine.hh"

namespace broccoli {
  class SampleActivity1: public broccoli::Activity {
  private:
    Mesh m_cube_mesh;
  public:
    SampleActivity1(broccoli::Engine &engine);
  private:
    static Mesh buildCubeMesh(broccoli::Engine &engine);
  public:
    void draw(broccoli::RenderFrame &frame) override;
  };
}
