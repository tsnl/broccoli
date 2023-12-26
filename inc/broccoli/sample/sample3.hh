#pragma once

#include "broccoli/engine.hh"

namespace broccoli {
  class SampleActivity3: public broccoli::Activity {
  private:
    Bitmap m_rainbow_rgba_bitmap;
    Bitmap m_rainbow_gray_bitmap;
  public:
    SampleActivity3(broccoli::Engine &engine);
  public:
    void draw(broccoli::RenderFrame &frame) override;
  };
}
