#pragma once

#include <span>
#include <vector>
#include <optional>
#include <variant>
#include <cstdint>
#include <cstddef>

#include "webgpu/webgpu_cpp.h"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "robin_hood.h"

#include "core.hh"
#include "bitmap.hh"

namespace broccoli {
  class Engine;
}

namespace broccoli {
  class RenderCamera;
  class RenderTexture;
  class RenderTextureView;
  class RenderManager;
  class RenderFrame;
  class Renderer;
}
namespace broccoli {
  class GeometryBuilder;
  class GeometryFactory;
}
namespace broccoli {
  struct Material;
  class MaterialTableEntry;
}
namespace broccoli {
  struct MeshInstanceList;
  struct DirectionalLight;
  struct PointLight;
  struct Geometry;
  struct Vertex;
}

//
// RenderManager, RenderFrame, Renderer
//

namespace broccoli {
  struct RenderTarget { wgpu::TextureView &texture_view; glm::ivec2 size; };
}
namespace broccoli {
  class RenderCamera {
  public:
    glm::mat4x4 m_view_matrix;
    glm::vec3 m_position;
    float m_fovy_deg;
    float m_exposure_bias;
  private:
    RenderCamera(glm::mat4x4 view_matrix, glm::vec3 position, float fovy_deg, float exposure_bias);
    RenderCamera() = delete;
  public:
    RenderCamera(const RenderCamera &other) = default;
    RenderCamera(RenderCamera &&other) = default;
  public:
    static RenderCamera createDefault(
      float fovy_deg = 90.0f,
      float exposure_bias = 0.0f
    );
    static RenderCamera fromTransform(
      glm::mat4x4 transform,
      float fovy_deg,
      float exposure_bias = 0.0f
    );
    static RenderCamera fromLookAt(
      glm::vec3 eye,
      glm::vec3 target,
      glm::vec3 up,
      float fovy_deg,
      float exposure_bias = 0.0f
    );
  public:
    glm::mat4x4 viewMatrix() const;
    glm::vec3 position() const;
    float fovyDeg() const;
    float exposureBias() const;
  };
}
namespace broccoli {
  class RenderTexture {
  private:
    wgpu::Texture m_texture;
    wgpu::TextureView m_view;
    wgpu::Sampler m_sampler;
    Bitmap m_bitmap;
    std::string m_name;
    std::string m_wgpu_texture_label;
    std::string m_wgpu_view_label;
    std::string m_wgpu_sampler_label;
  public:
    RenderTexture(wgpu::Device &device, std::string name, Bitmap bitmap, wgpu::FilterMode filter);
    RenderTexture(RenderTexture &&other) = default;
    ~RenderTexture() = default;
  private:
    void upload(wgpu::Device &device, wgpu::FilterMode filter);
  public:
    wgpu::Texture const &texture() const;
    wgpu::TextureView const &view() const;
    wgpu::Sampler const &sampler() const;
    Bitmap const &bitmap() const;
  };
  class RenderTextureView {
  private:
    RenderTexture const &m_render_texture;
    glm::dvec2 m_uv_offset;
    glm::dvec2 m_uv_size;
  public:
    RenderTextureView(RenderTexture const &rt, glm::dvec2 uv_offset, glm::dvec2 uv_size);
  public:
    RenderTexture const &render_texture() const;
    glm::dvec2 uv_offset() const;
    glm::dvec2 uv_size() const;
  };
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
    wgpu::BindGroupLayout m_wgpu_bind_group_1_layout;
    wgpu::PipelineLayout m_wgpu_render_pipeline_layout;
    wgpu::RenderPipeline m_wgpu_render_pipeline;
    wgpu::BindGroup m_wgpu_bind_group_0;
    wgpu::Texture m_wgpu_render_target_color_texture;
    wgpu::TextureView m_wgpu_render_target_color_texture_view;
    wgpu::Texture m_wgpu_render_target_depth_stencil_texture;
    wgpu::TextureView m_wgpu_render_target_depth_stencil_texture_view;
    RenderTexture m_rgb_palette;
    RenderTexture m_monochrome_palette;
    std::vector<MaterialTableEntry> m_materials;
    bool m_materials_locked;
  public:
    RenderManager(wgpu::Device &device, glm::ivec2 framebuffer_size);
  public:
    RenderManager(RenderManager const &other) = delete;
    RenderManager(RenderManager &&other) = default;
    ~RenderManager() = default;
  private:
    static Bitmap initMonochromePalette();
    static Bitmap initRgbPalette();
    void initShaderModule();
    void initUniforms();
    void initBindGroup0Layout();
    void initBindGroup1Layout();
    void initRenderPipelineLayout();
    void initRenderPipeline();
    void initBindGroup0();
    void initMaterialTable();
    void reinitColorTexture(glm::ivec2 framebuffer_size);
    void reinitDepthStencilTexture(glm::ivec2 framebuffer_size);
  private:
    Material emplaceMaterial(MaterialTableEntry material);
  public:
    GeometryBuilder createGeometryBuilder();
    GeometryFactory createGeometryFactory();
  public:
    Material createBlinnPhongMaterial(
      std::string name,
      std::variant<glm::dvec3, RenderTexture> albedo_map, 
      std::variant<glm::dvec3, RenderTexture> normal_map,
      double shininess
    );
    Material createPbrMaterial(
      std::string name,
      std::variant<glm::dvec3, RenderTexture> albedo_map, 
      std::variant<glm::dvec3, RenderTexture> normal_map,
      std::variant<double, RenderTexture> metalness_map,
      std::variant<double, RenderTexture> roughness_map,
      glm::dvec3 fresnel0
    );
  public:
    void lockMaterials();
    void unlockMaterials();
    const wgpu::BindGroup &getMaterialBindGroup(Material material) const;
  public:
    wgpu::Device const &wgpuDevice() const;
    wgpu::RenderPipeline const &wgpuRenderPipeline() const;
    wgpu::Buffer const &wgpuLightUniformBuffer() const;
    wgpu::Buffer const &wgpuCameraUniformBuffer() const;
    wgpu::Buffer const &wgpuTransformUniformBuffer() const;
    wgpu::BindGroup const &wgpuBindGroup0() const;
    wgpu::BindGroupLayout const &wgpuBindGroup1Layout() const;
    wgpu::TextureView const &wgpuRenderTargetColorTextureView() const;
    wgpu::TextureView const &wgpuRenderTargetDepthStencilTextureView() const;
    RenderTexture const &rgbPalette() const;
    RenderTexture const &monochromePalette() const;
    std::vector<MaterialTableEntry> const &materials() const;
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
    Renderer useCamera(RenderCamera camera);
  };
}

namespace broccoli {
  class Renderer {
    friend RenderFrame;
  private:
    RenderManager &m_manager;
    RenderTarget m_target;
    RenderCamera m_camera;
    std::vector<std::vector<MeshInstanceList>> m_mesh_instance_lists;
    std::vector<DirectionalLight> m_directional_light_vec;
    std::vector<PointLight> m_point_light_vec;
  private:
    static wgpu::CommandEncoderDescriptor s_draw_command_encoder_descriptor;
  private:
    Renderer(RenderManager &manager, RenderTarget target, RenderCamera camera);
  public:
    ~Renderer();
  public:
    void draw(Material material_id, const Geometry &mesh);
    void draw(Material material_id, const Geometry &mesh, glm::mat4x4 instance_transform);
    void draw(Material material_id, const Geometry &mesh, std::span<glm::mat4x4> instance_transforms);
    void draw(Material material_id, const Geometry &mesh, std::vector<glm::mat4x4> instance_transforms);
    void addDirectionalLight(glm::vec3 direction, float intensity, glm::vec3 color);
    void addPointLight(glm::vec3 position, float intensity, glm::vec3 color);
  private:
    void sendCameraData(RenderCamera camera, RenderTarget target);
    void sendLightData(std::vector<DirectionalLight> direction_light_vec, std::vector<PointLight> point_light_vec);
    void sendDrawMeshInstanceListVec(std::vector<std::vector<MeshInstanceList>> mesh_instance_list_vec);
    void sendDrawMeshInstanceList(Material material, const MeshInstanceList &mesh_instance_list);
  };
}

//
// Geometry:
//

namespace broccoli {
  struct Vertex { glm::ivec3 offset; uint32_t normal; uint32_t tangent; glm::tvec2<uint16_t> uv; };
  inline bool operator== (Vertex v1, Vertex v2);
  static_assert(sizeof(Vertex) == 24, "expected sizeof(Vertex) == 24B");
}
namespace broccoli {
  inline bool operator== (Vertex v1, Vertex v2) {
    return v1.offset == v2.offset && v1.normal == v2.normal && v1.tangent == v2.tangent && v1.uv == v2.uv;
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
  enum class GeometryType {
    Static,
    Dynamic
  };
  struct Geometry {
    GeometryType mesh_type;
    wgpu::Buffer vtx_buffer;
    wgpu::Buffer idx_buffer;
    uint32_t vtx_count;
    uint32_t idx_count;
  };
}

namespace broccoli {
  class GeometryBuilder {
    friend RenderManager;
  public:
    struct Vtx { glm::dvec3 offset; glm::dvec2 uv = {0.0, 0.0}; };
  private:
    RenderManager &m_manager;
    robin_hood::unordered_map<Vertex, uint32_t> m_vtx_compression_map;
    std::vector<Vertex> m_vtx_buf;
    std::vector<uint32_t> m_idx_buf;
  private:
    GeometryBuilder(RenderManager &device);
  public:
    GeometryBuilder() = delete;
    GeometryBuilder(GeometryBuilder const &other) = delete;
    GeometryBuilder(GeometryBuilder &&other) = default;
  public:
    void triangle(Vtx v1, Vtx v2, Vtx v3, bool double_faced = false);
    void quad(Vtx v1, Vtx v2, Vtx v3, Vtx v4, bool double_faced = false);
  public:
    static Geometry finish(GeometryBuilder &&mb);
  private:
    void singleFaceTriangle(Vtx v1, Vtx v2, Vtx v3);
  private:
    uint32_t vertex(glm::dvec3 offset, glm::dvec2 uv, uint32_t packed_normal, uint32_t packed_tangent);
  private:
    static glm::ivec3 packOffset(glm::dvec3 offset);
    static glm::tvec2<uint16_t> packUv(glm::dvec2 uv);
    static uint32_t packNormal(glm::dvec3 normal);
  };
}

namespace broccoli {
  // TODO: consider removing this class from the interface, instead moving key methods as static functions in 
  // GeometryBuilder.
  class GeometryFactory {
    friend RenderManager;
  private:
    RenderManager &m_manager;
  private:
    GeometryFactory() = delete;
    GeometryFactory(GeometryFactory const &other) = delete;
    GeometryFactory(GeometryFactory &&other) = default;
  private:
    GeometryFactory(RenderManager &manager);
  public:
    Geometry createCuboid(glm::dvec3 dimensions);
  };
}

//
// Material
//

namespace broccoli {
  struct MaterialUniform;
}
namespace broccoli {
  enum class MaterialLightingModel: uint32_t {
    BlinnPhong = 1,
    PhysicallyBased = 2,
  };
}
namespace broccoli {
  struct Material {
    size_t value;
  };
}
namespace broccoli {
  class MaterialTableEntry {
  private:
    RenderManager &m_render_manager;
    wgpu::Buffer m_wgpu_material_buffer;
    wgpu::BindGroup m_wgpu_material_bind_group;
    std::string m_name;
    std::string m_wgpu_material_buffer_label;
    std::string m_wgpu_material_bind_group_label;
  public:
    MaterialTableEntry(
      RenderManager &render_manager,
      std::string name,
      std::variant<glm::dvec3, RenderTexture> albedo_map, 
      std::variant<glm::dvec3, RenderTexture> normal_map,
      std::variant<double, RenderTexture> metalness_map,
      std::variant<double, RenderTexture> roughness_map,
      glm::dvec3 pbr_fresnel0,
      double blinn_phong_shininess,
      MaterialLightingModel lighting_model
    );
    MaterialTableEntry(MaterialTableEntry const &other) = delete;
    MaterialTableEntry(MaterialTableEntry &&other) = default;
  public:
    virtual ~MaterialTableEntry() = default;
  private:
    void init(
      std::variant<glm::dvec3, RenderTexture> const &albedo_map, 
      std::variant<glm::dvec3, RenderTexture> const &normal_map,
      std::variant<double, RenderTexture> const &metalness_map,
      std::variant<double, RenderTexture> const &roughness_map,
      glm::dvec3 pbr_fresnel0,
      double blinn_phong_shininess,
      MaterialLightingModel lighting_model
    );
  public:
    const wgpu::BindGroup &wgpuMaterialBindGroup() const;
  private:
    RenderTextureView getColorOrTexture(RenderManager &rm, std::variant<glm::dvec3, RenderTexture> const &map);
    RenderTextureView getColorOrTexture(RenderManager &rm, std::variant<double, RenderTexture> const &map);
  public:
    static MaterialTableEntry createBlinnPhongMaterial(
      RenderManager &render_manager,
      std::string name,
      std::variant<glm::dvec3, RenderTexture> albedo_map, 
      std::variant<glm::dvec3, RenderTexture> normal_map,
      double shininess
    );
    static MaterialTableEntry createPbrMaterial(
      RenderManager &render_manager,
      std::string name,
      std::variant<glm::dvec3, RenderTexture> albedo_map, 
      std::variant<glm::dvec3, RenderTexture> normal_map,
      std::variant<double, RenderTexture> metalness_map,
      std::variant<double, RenderTexture> roughness_map,
      glm::dvec3 fresnel0
    );
  };
}

//
// Scene:
//


namespace broccoli {
  struct MeshInstanceList {
    const Geometry &mesh;
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
