#pragma once

#include "broccoli/engine.hh"

namespace broccoli {
  class SampleActivity2: public broccoli::Activity {
  private:
    Geometry m_floor_geometry;
    Geometry m_tree_trunk_geometry;
    Geometry m_tree_leaves_geometry;
    Material m_floor_material;
    Material m_tree_trunk_material;
    Material m_tree_leaves_material;
  public:
    SampleActivity2(broccoli::Engine &engine);
  private:
    static Geometry buildFloorGeometry(broccoli::Engine &engine);
    static Geometry buildTreeTrunkGeometry(broccoli::Engine &engine);
    static Geometry buildTreeLeavesGeometry(broccoli::Engine &engine);
    static Material buildFloorMaterial(broccoli::Engine &engine);
    static Material buildTreeTrunkMaterial(broccoli::Engine &engine);
    static Material buildTreeLeavesMaterial(broccoli::Engine &engine);
  public:
    void draw(broccoli::RenderFrame &frame) override;
  };
}
