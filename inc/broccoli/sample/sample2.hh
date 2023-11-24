#pragma once

#include "broccoli/engine.hh"

namespace broccoli {
  class SampleActivity2: public broccoli::Activity {
  private:
    Mesh m_floor_mesh;
    Mesh m_tree_trunk_mesh;
    Mesh m_tree_leaves_mesh;
  public:
    SampleActivity2(broccoli::Engine &engine);
  private:
    static Mesh buildFloorMesh(broccoli::Engine &engine);
    static Mesh buildTreeTrunkMesh(broccoli::Engine &engine);
    static Mesh buildTreeLeavesMesh(broccoli::Engine &engine);
  public:
    void draw(broccoli::RenderFrame &frame) override;
  };
}
