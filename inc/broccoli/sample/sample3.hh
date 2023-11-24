#pragma once

#include "broccoli/engine.hh"

namespace broccoli {
  class SampleActivity3: public broccoli::Activity {
  private:
    Mesh m_cube_mesh;
  public:
    SampleActivity3(broccoli::Engine &engine);
  private:
    static Mesh buildCubeMesh(broccoli::Engine &engine);
  public:
    void draw(broccoli::RenderFrame &frame) override;
  };
}
