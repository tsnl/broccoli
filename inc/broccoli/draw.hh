#pragma once

#include <span>
#include <cstdint>
#include <cstddef>

#include "webgpu/webgpu_cpp.h"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "robin_hood.h"

namespace broccoli {
  class Engine;
}
namespace broccoli {
  class Renderer;
  class RenderPass3D;
}

namespace broccoli {
  class Renderer {
    friend Engine;
  private:
    wgpu::CommandEncoder m_command_encoder;
    wgpu::TextureView m_texture_view;
    wgpu::Device m_device;
  public:
    Renderer(wgpu::Device device, wgpu::TextureView texture_view);
    ~Renderer();
  private:
    static void initStaticResources(wgpu::Device device);
    static void dropStaticResources();
  public:
    RenderPass3D beginRenderPass3D() const;
    RenderPass3D beginRenderPass3D(glm::dvec3 clear_color) const;
  };
}

namespace broccoli {
  struct Vertex { glm::tvec4<int16_t> offset; glm::tvec4<uint8_t> color; glm::tvec4<int8_t> normal; };
  static_assert(sizeof(Vertex) == 16, "expected sizeof(Vertex) == 16");
}
namespace std {
  template <>
  struct hash<broccoli::Vertex> {
    inline size_t operator() (broccoli::Vertex vertex) const {
      // FNV1A
      uint8_t *data = reinterpret_cast<uint8_t*>(&vertex);
      uint64_t res = 0XCBF29CE484222325;
      for (size_t i = 0; i < sizeof(broccoli::Vertex); i++) {
        res ^= data[i];
        res *= 0x00000100000001B3;
      }
      return static_cast<size_t>(res);
    }
  };
}

namespace broccoli {
  class RenderPass3D {
    friend Renderer;
  public:
    struct PooledResources { robin_hood::unordered_map<Vertex, uint16_t> vtx_map; wgpu::Buffer vtx; wgpu::Buffer idx; };
  private:
    wgpu::CommandEncoder m_command_encoder;
    wgpu::RenderPassEncoder m_render_pass_encoder;
    PooledResources m_pooled_resources;
    std::span<Vertex> m_vtx_buf_data;
    std::span<uint16_t> m_idx_buf_data;
  private:
    RenderPass3D(
      wgpu::Device device,
      wgpu::CommandEncoder command_encoder,
      wgpu::TextureView texture_view,
      glm::dvec3 clear_color
    );
  public:
    RenderPass3D() = delete;
    RenderPass3D(RenderPass3D const &other) = delete;
    RenderPass3D(RenderPass3D &&other) = default;
    ~RenderPass3D();
  private:
    static void initStaticResources(wgpu::Device device);
    static void dropStaticResources();
  public:

  };
}