#pragma once

#include <span>
#include <vector>
#include <optional>
#include <cstdint>
#include <cstddef>

#include "webgpu/webgpu_cpp.h"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "robin_hood.h"

#include "core.hh"

namespace broccoli {
  class Engine;
}

namespace broccoli {
  struct Vertex;
  struct Mesh;
}
namespace broccoli {
  class RenderManager;
  class RenderFrame;
  class Renderer;
  class MeshBuilder;
}

//
// RenderManager, RenderFrame, Renderer
//

namespace broccoli {
  struct RenderTarget { wgpu::TextureView &texture_view; glm::ivec2 size; };
  struct RenderCamera { glm::mat4x4 transform; float fovy_deg; };
}

namespace broccoli {
  class RenderManager {
  private:
    wgpu::Device &m_wgpu_device;
    wgpu::ShaderModule m_wgpu_shader_module;
    wgpu::Buffer m_wgpu_camera_uniform_buffer;
    wgpu::Buffer m_wgpu_transform_uniform_buffer;
    wgpu::BindGroupLayout m_wgpu_bind_group_0_layout;
    wgpu::PipelineLayout m_wgpu_render_pipeline_layout;
    wgpu::RenderPipeline m_wgpu_render_pipeline;
    wgpu::BindGroup m_wgpu_bind_group_0;
  public:
    RenderManager(wgpu::Device &device);
  public:
    RenderManager(RenderManager const &other) = delete;
    RenderManager(RenderManager &&other) = default;
    ~RenderManager() = default;
  private:
    void initShaderModule();
    void initUniforms();
    void initBindGroup0Layout();
    void initRenderPipelineLayout();
    void initRenderPipeline();
    void initBindGroup0();
  public:
    wgpu::Device const &wgpu_device();
    wgpu::RenderPipeline const &wgpu_render_pipeline();
    wgpu::Buffer const &wgpu_camera_uniform_buffer();
    wgpu::Buffer const &wgpu_transform_uniform_buffer();
    wgpu::BindGroup const &wgpu_bind_group_0();
  public:
    RenderFrame frame(RenderTarget target);
  };
}

namespace broccoli {
  class RenderFrame {
    friend RenderManager;
  private:
    RenderManager &m_manager;
    RenderTarget m_target;
  public:
    RenderFrame(RenderManager &manager, RenderTarget target);
  public:
    void clear(glm::dvec3 clear_color);
    Renderer use_camera(RenderCamera camera);
  };
}

namespace broccoli {
  class Renderer {
    friend RenderFrame;
  private:
    RenderManager &m_manager;
    RenderTarget m_target;
  private:
    static wgpu::CommandEncoderDescriptor s_draw_command_encoder_descriptor;
  private:
    Renderer(RenderManager &renderer, RenderTarget target, RenderCamera camera);
  public:
    void draw(Mesh mesh);
    void draw(Mesh mesh, std::span<glm::mat4x4> const &instance_transforms);
  };
}

//
// MeshBuilder:
//

namespace broccoli {
  struct Vertex { glm::tvec4<int16_t> offset; glm::tvec4<uint8_t> color; uint32_t normal; };
  inline bool operator== (Vertex v1, Vertex v2);
  static_assert(sizeof(Vertex) == 16, "expected sizeof(Vertex) == 16");
}
namespace broccoli {
  inline bool operator== (Vertex v1, Vertex v2) {
    return v1.offset == v2.offset && v1.color == v2.color && v1.normal == v2.normal;
  }
}
namespace std {
  template <>
  struct hash<broccoli::Vertex> {
    inline size_t operator() (broccoli::Vertex vertex) const {
      broccoli::Fnv1aHasher hasher;
      hasher.write(&vertex);
      return hasher.finish();
    }
  };
}
namespace broccoli {
  struct Mesh {
    wgpu::Buffer vtx_buffer;
    wgpu::Buffer idx_buffer;
    uint32_t vtx_count;
    uint32_t idx_count;
  };
}
namespace broccoli {
  class MeshBuilder {
    friend Engine;
  public:
    struct Vtx { glm::dvec3 offset; glm::dvec4 color; };
  private:
    wgpu::Device &m_wgpu_device;
    robin_hood::unordered_map<Vertex, size_t> m_vtx_compression_map;
    std::vector<Vertex> m_vtx_buf;
    std::vector<uint16_t> m_idx_buf;
  private:
    MeshBuilder(wgpu::Device &device);
  public:
    MeshBuilder() = delete;
    MeshBuilder(MeshBuilder const &other) = delete;
    MeshBuilder(MeshBuilder &&other) = default;
  public:
    void triangle(Vtx v1, Vtx v2, Vtx v3);
    Mesh finish();
  private:
    uint16_t vertex(glm::dvec3 offset, glm::dvec4 color, uint32_t packed_normal);
  private:
    static glm::tvec4<int16_t> pack_offset(glm::dvec3 offset);
    static glm::tvec4<uint8_t> pack_color(glm::dvec4 color);
    static uint32_t pack_normal_unorm_10x3_1x2(glm::dvec3 normal);
  };
}
