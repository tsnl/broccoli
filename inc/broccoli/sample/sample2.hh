#pragma once

#include "broccoli/engine.hh"

namespace broccoli {
  class SampleActivity2: public broccoli::Activity {
  private:
    Mesh m_cube_mesh;
  public:
    SampleActivity2(broccoli::Engine &engine);
  private:
    static Mesh buildCubeMesh(broccoli::Engine &engine);
  public:
    void draw(broccoli::RenderFrame &frame) override;
  };
}
