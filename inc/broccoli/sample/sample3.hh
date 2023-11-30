#pragma once

#include "broccoli/engine.hh"

namespace broccoli {
  class SampleActivity3: public broccoli::Activity {
  public:
    SampleActivity3(broccoli::Engine &engine);
  public:
    void draw(broccoli::RenderFrame &frame) override;
  };
}
