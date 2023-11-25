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
  struct MeshInstanceList;
  struct DirectionalLight;
  struct PointLight;
}
namespace broccoli {
  class RenderManager;
  class RenderFrame;
  class Renderer;
}
namespace broccoli {
  class MeshBuilder;
  class MeshFactory;
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
    wgpu::Buffer m_wgpu_light_uniform_buffer;
    wgpu::Buffer m_wgpu_camera_uniform_buffer;
    wgpu::Buffer m_wgpu_transform_uniform_buffer;
    wgpu::BindGroupLayout m_wgpu_bind_group_0_layout;
    wgpu::PipelineLayout m_wgpu_render_pipeline_layout;
    wgpu::RenderPipeline m_wgpu_render_pipeline;
    wgpu::BindGroup m_wgpu_bind_group_0;
    wgpu::Texture m_wgpu_render_target_color_texture;
    wgpu::TextureView m_wgpu_render_target_color_texture_view;
    wgpu::Texture m_wgpu_render_target_depth_stencil_texture;
    wgpu::TextureView m_wgpu_render_target_depth_stencil_texture_view;
  public:
    RenderManager(wgpu::Device &device, glm::ivec2 framebuffer_size);
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
    void reinitColorTexture(glm::ivec2 framebuffer_size);
    void reinitDepthStencilTexture(glm::ivec2 framebuffer_size);
  public:
    MeshBuilder createMeshBuilder();
    MeshFactory createMeshFactory();
  public:
    wgpu::Device const &wgpuDevice() const;
    wgpu::RenderPipeline const &wgpuRenderPipeline() const;
    wgpu::Buffer const &wgpuLightUniformBuffer() const;
    wgpu::Buffer const &wgpuCameraUniformBuffer() const;
    wgpu::Buffer const &wgpuTransformUniformBuffer() const;
    wgpu::BindGroup const &wgpuBindGroup0() const;
    wgpu::TextureView const &wgpuRenderTargetColorTextureView() const;
    wgpu::TextureView const &wgpuRenderTargetDepthStencilTextureView() const;
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
    RenderCamera m_camera;
    std::vector<MeshInstanceList> m_mesh_instance_list_vec;
    std::vector<DirectionalLight> m_directional_light_vec;
    std::vector<PointLight> m_point_light_vec;
  private:
    static wgpu::CommandEncoderDescriptor s_draw_command_encoder_descriptor;
  private:
    Renderer(RenderManager &renderer, RenderTarget target, RenderCamera camera);
  public:
    ~Renderer();
  public:
    void draw(const Mesh &mesh);
    void draw(const Mesh &mesh, glm::mat4x4 instance_transform);
    void draw(const Mesh &mesh, std::span<glm::mat4x4> instance_transforms);
    void draw(const Mesh &mesh, std::vector<glm::mat4x4> instance_transforms);
    void addDirectionalLight(glm::vec3 direction, float intensity, glm::vec3 color);
    void addPointLight(glm::vec3 position, float intensity, glm::vec3 color);
  private:
    void sendCameraData(RenderCamera camera, RenderTarget target);
    void sendLightData(std::vector<DirectionalLight> direction_light_vec, std::vector<PointLight> point_light_vec);
    void sendDrawMeshInstanceListVec(std::vector<MeshInstanceList> mesh_instance_list_vec);
    void sendDrawMeshInstanceList(const MeshInstanceList &mesh_instance_list);
  };
}

//
// MeshBuilder:
//

namespace broccoli {
  struct Vertex { glm::ivec3 offset; uint32_t shininess; glm::u8vec4 color; uint32_t normal; };
  inline bool operator== (Vertex v1, Vertex v2);
  static_assert(sizeof(Vertex) == 24, "expected sizeof(Vertex) == 24B");
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
    friend RenderManager;
  public:
    struct Vtx { glm::dvec3 offset; glm::dvec3 color; double shininess; };
  private:
    RenderManager &m_manager;
    robin_hood::unordered_map<Vertex, uint32_t> m_vtx_compression_map;
    std::vector<Vertex> m_vtx_buf;
    std::vector<uint32_t> m_idx_buf;
  private:
    MeshBuilder(RenderManager &device);
  public:
    MeshBuilder() = delete;
    MeshBuilder(MeshBuilder const &other) = delete;
    MeshBuilder(MeshBuilder &&other) = default;
  public:
    void triangle(Vtx v1, Vtx v2, Vtx v3, bool double_faced = false);
    void quad(Vtx v1, Vtx v2, Vtx v3, Vtx v4, bool double_faced = false);
  public:
    static Mesh finish(MeshBuilder &&mb);
  private:
    void singleFaceTriangle(Vtx v1, Vtx v2, Vtx v3);
  private:
    uint32_t vertex(glm::dvec3 offset, glm::dvec3 color, double shininess, uint32_t packed_normal);
  private:
    static glm::ivec3 packOffset(glm::dvec3 offset);
    static uint32_t packShininess(double shininess);
    static glm::u8vec4 packColor(glm::dvec3 color);
    static uint32_t packNormal(glm::dvec3 normal);
  };
}

//
// MeshFactory: one-shot function calls to construct common geometry.
//

namespace broccoli {
  class MeshFactory {
    friend RenderManager;
  public:
    struct Facet { glm::dvec3 color; float shininess; };
  private:
    RenderManager &m_manager;
  private:
    MeshFactory() = delete;
    MeshFactory(MeshFactory const &other) = delete;
    MeshFactory(MeshFactory &&other) = default;
  private:
    MeshFactory(RenderManager &manager);
  public:
    Mesh createCuboid(glm::dvec3 dimensions, Facet px, Facet nx, Facet py, Facet ny, Facet pz, Facet nz);
    Mesh createCuboid(glm::dvec3 dimensions, Facet common_facet);
  };
}

//
// Scene descriptors:
//

namespace broccoli {
  struct MeshInstanceList {
    const Mesh &mesh;
    std::vector<glm::mat4x4> instance_list;
  };
  struct DirectionalLight {
    glm::vec3 direction;
    glm::vec3 color;
  };
  struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
  };
}
