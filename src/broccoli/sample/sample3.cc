#include "broccoli/sample/sample3.hh"

#include "broccoli/engine.hh"
#include "glm/gtx/transform.hpp"

namespace broccoli {
  const char *RAINBOW_BITMAP_RGBA_FILEPATH = "res/texture/3rdparty/rainbow/rgba.png";
  const char *RAINBOW_BITMAP_GRAY_FILEPATH = "res/texture/3rdparty/rainbow/gray.png";
}

namespace broccoli {
  SampleActivity3::SampleActivity3(broccoli::Engine &engine)
  : broccoli::Activity(engine),
    m_rainbow_rgba_bitmap(Bitmap::loadFromFile(RAINBOW_BITMAP_RGBA_FILEPATH, 4)),
    m_rainbow_gray_bitmap(Bitmap::loadFromFile(RAINBOW_BITMAP_GRAY_FILEPATH, 1))
  {
    CHECK(m_rainbow_rgba_bitmap.dim().x == m_rainbow_gray_bitmap.dim().x, "Bad test image dimensions (X mismatch)");
    CHECK(m_rainbow_rgba_bitmap.dim().y == m_rainbow_gray_bitmap.dim().y, "Bad test image dimensions (Y mismatch)");
  }
  void SampleActivity3::draw(broccoli::RenderFrame &frame) {
    frame.clear(glm::dvec3{0.70, 0.73, 0.80});
    frame.draw(
      RenderCamera::createFromLookAt(
        glm::vec3{30.0f, 30.0f, 45.0f},
        glm::vec3{00.0f, 04.0f, 00.0f},
        glm::vec3{00.0f, 01.0f, 00.0f},
        30.0f
      ),
      [] (Renderer &renderer) {
        // TODO: implement me
        (void)renderer;
      }
    );
    frame.overlay(
      [this] (OverlayRenderer &renderer) {
        std::array<uint8_t, 4> clear_pixel = {0x00, 0x00, 0x00, 0x7F};
        glm::i32vec2 screen_size{renderer.bitmap().dim()};
        glm::i32vec2 image_size{m_rainbow_rgba_bitmap.dim()};
        glm::i32vec2 dst_offset = (screen_size - image_size) / 2;
        renderer.bitmap().clear(std::span{clear_pixel.data(), clear_pixel.size()});
        renderer.bitmap().blit(m_rainbow_rgba_bitmap, dst_offset);
      }
    );
  }
}
