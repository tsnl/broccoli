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
namespace broccoli {
  struct MaterialUniform;
  struct ShadowUniform;
}

//
// POD Uniform Buffer Objects (UBOs):
//

namespace broccoli {
  struct ShadowUniform {
    glm::mat4x4 proj_view_matrix;
  };
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
    glm::mat4x4 m_transform_matrix;
    glm::vec3 m_position;
    float m_fovy_deg;
    float m_exposure_bias;
  private:
    RenderCamera(glm::mat4x4 view_matrix, glm::mat4x4 transform_matrix, glm::vec3 position, float fovy_deg, float exposure_bias);
    RenderCamera() = delete;
  public:
    RenderCamera(const RenderCamera &other) = default;
    RenderCamera(RenderCamera &&other) = default;
  public:
    static RenderCamera createDefault(
      float fovy_deg = 90.0f,
      float exposure_bias = 1.0f
    );
    static RenderCamera createFromTransform(
      glm::mat4x4 transform,
      float fovy_deg,
      float exposure_bias = 1.0f
    );
    static RenderCamera createFromLookAt(
      glm::vec3 eye,
      glm::vec3 target,
      glm::vec3 up,
      float fovy_deg,
      float exposure_bias = 1.0f
    );
  public:
    glm::mat4x4 transformMatrix() const;
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
    std::string m_name;
    std::string m_wgpu_texture_label;
    std::string m_wgpu_view_label;
    std::string m_wgpu_sampler_label;
    glm::i64vec3 m_dim;
  private:
    RenderTexture(wgpu::Device &device, std::string name, glm::i64vec3 dim, wgpu::FilterMode filter, bool is_depth);
  public:
    RenderTexture(RenderTexture &&other) = default;
    ~RenderTexture() = default;
  private:
    void create(wgpu::Device &device, glm::i64vec3 dim, wgpu::FilterMode filter, bool is_depth);
    void upload(wgpu::Device &device, Bitmap const &bitmap);
  public:
    static RenderTexture createForDepth(wgpu::Device &device, std::string name, glm::i64vec3 dim, wgpu::FilterMode filter);
    static RenderTexture createForColor(wgpu::Device &device, std::string name, glm::i64vec3 dim, wgpu::FilterMode filter);
    static RenderTexture createForColorFromBitmap(wgpu::Device &device, std::string name, Bitmap bitmap, wgpu::FilterMode filter);
  public:
    wgpu::Texture const &texture() const;
    wgpu::TextureView const &view() const;
    wgpu::Sampler const &sampler() const;
    glm::i64vec3 dim() const;
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
  template <size_t bind_group_count, size_t bind_group_prefix_count>
  class RenderPipeline {
    static_assert(
      bind_group_prefix_count <= bind_group_count,
      "expected bind group prefix count to not exceed bind group count."
    );
  public:
    static constexpr const size_t BIND_GROUP_COUNT = bind_group_count;
    static constexpr const size_t BIND_GROUP_PREFIX_COUNT = bind_group_prefix_count;
    static constexpr const size_t BIND_GROUP_SUFFIX_COUNT = bind_group_count - bind_group_prefix_count;
  public:
    RenderPipeline() = default;
  public:
    std::array<wgpu::BindGroupLayout, bind_group_count> bind_group_layouts = {};
    std::array<wgpu::BindGroup, bind_group_prefix_count> bind_groups_prefix = {};
    wgpu::PipelineLayout pipeline_layout = nullptr;
    wgpu::RenderPipeline pipeline = nullptr;
  public:
    inline void setBindGroups(wgpu::RenderPassEncoder& encoder, std::span<wgpu::BindGroup, BIND_GROUP_SUFFIX_COUNT> suffix) const;
    inline void setBindGroups(wgpu::RenderPassEncoder& encoder, std::array<wgpu::BindGroup, BIND_GROUP_SUFFIX_COUNT> suffix) const;
  };
  class FinalRenderPipeline: public RenderPipeline<2, 1> {
  public:
    inline wgpu::BindGroupLayout materialBindGroupLayout() const;
  };
  class ShadowRenderPipeline: public RenderPipeline<2, 1> {
  public:
    inline wgpu::BindGroupLayout shadowUniformBindGroupLayout() const;
  };
}
namespace broccoli {
  class RenderShadowMaps {
  public:
    struct ShadowMapUbo { ShadowUniform state; wgpu::Buffer buffer; wgpu::BindGroup bind_group; };
  private:
    wgpu::Texture m_texture = nullptr;
    std::vector<wgpu::TextureView> m_write_views = {};
    std::vector<wgpu::TextureView> m_read_views = {};
    std::vector<ShadowMapUbo> m_shadow_map_ubo_vec = {};
    int32_t m_light_count_lg2 = 0;
    int32_t m_cascades_per_light_lg2 = 0;
    int32_t m_size_lg2 = 0;
  public:
    RenderShadowMaps() = default;
  public:
    void init(wgpu::Device &dev, const ShadowRenderPipeline &pipeline, int32_t light_count_lg2, int32_t cascades_per_light_lg2, int32_t size_lg2, std::string name);
  public:
    const wgpu::Texture &texture() const;
    const wgpu::TextureView &getWriteView(int32_t light_idx, int32_t cascade_idx) const;
    const wgpu::TextureView &getReadView(int32_t light_idx, int32_t cascade_idx) const;
    const ShadowMapUbo &getShadowMapUbo(int32_t light_idx, int32_t cascade_idx) const;
  };
}

namespace broccoli {
  class RenderManager {
  private:
    wgpu::Device &m_wgpu_device;
    wgpu::ShaderModule m_wgpu_pbr_shader_module = nullptr;
    wgpu::ShaderModule m_wgpu_blinn_phong_shader_module = nullptr;
    wgpu::ShaderModule m_wgpu_shadow_shader_module = nullptr;
    wgpu::Buffer m_wgpu_light_uniform_buffer = nullptr;
    wgpu::Buffer m_wgpu_camera_uniform_buffer = nullptr;
    wgpu::Buffer m_wgpu_transform_uniform_buffer = nullptr;
    FinalRenderPipeline m_wgpu_pbr_final_render_pipeline;
    FinalRenderPipeline m_wgpu_blinn_phong_final_render_pipeline;
    ShadowRenderPipeline m_shadow_render_pipeline;
    wgpu::Texture m_wgpu_render_target_color_texture = nullptr;
    wgpu::TextureView m_wgpu_render_target_color_texture_view = nullptr;
    wgpu::Texture m_wgpu_render_target_depth_stencil_texture = nullptr;
    wgpu::TextureView m_wgpu_render_target_depth_stencil_texture_view = nullptr;
    RenderShadowMaps m_dir_light_shadow_maps;
    RenderShadowMaps m_pt_light_shadow_maps;
    RenderTexture m_rgb_palette;
    RenderTexture m_monochrome_palette;
    std::vector<MaterialTableEntry> m_materials;
    bool m_materials_locked = false;
  public:
    RenderManager(wgpu::Device &device, glm::ivec2 framebuffer_size);
  public:
    RenderManager(RenderManager const &other) = delete;
    RenderManager(RenderManager &&other) = default;
    ~RenderManager() = default;
  private:
    static Bitmap initMonochromePalette();
    static Bitmap initRgbPalette();
    void initFinalShaderModules();
    void initShadowShaderModule();
    wgpu::ShaderModule initShaderModuleVariant(const char *filepath, const std::string &text, std::unordered_map<std::string, std::string> rw_map);
    wgpu::ShaderModule initShaderModuleVariant(const char *filepath, const std::string &text);
    void initUniformBuffers();
    void initFinalPbrRenderPipeline();
    void initFinalBlinnPhongRenderPipeline();
    void initShadowRenderPipeline();
    void initShadowMaps();
    void initMaterialTable();
    void reinitColorTexture(glm::ivec2 framebuffer_size);
    void reinitDepthStencilTexture(glm::ivec2 framebuffer_size);
  private:
    FinalRenderPipeline helpInitFinalRenderPipeline(uint32_t lighting_model_id);
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
    void lockMaterialsTable();
    void unlockMaterialsTable();
  public:
    const MaterialTableEntry &getMaterialInfo(Material material) const;
  public:
    const wgpu::Device &wgpuDevice() const;
    const wgpu::Buffer &wgpuLightUniformBuffer() const;
    const wgpu::Buffer &wgpuCameraUniformBuffer() const;
    const wgpu::Buffer &wgpuTransformUniformBuffer() const;
    wgpu::ShaderModule wgpuFinalShaderModule(uint32_t lighting_model_id) const;
    const FinalRenderPipeline &getFinalRenderPipeline(uint32_t lighting_model_id) const;
    const ShadowRenderPipeline &getShadowRenderPipeline() const;
    const wgpu::TextureView &wgpuRenderTargetColorTextureView() const;
    const wgpu::TextureView &wgpuRenderTargetDepthStencilTextureView() const;
    const RenderShadowMaps &dirLightShadowMaps() const;
    const RenderShadowMaps &pointLightShadowMaps() const;
    const RenderTexture &rgbPalette() const;
    const RenderTexture &monochromePalette() const;
    const std::vector<MaterialTableEntry> &materials() const;
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
    void addMesh(Material material_id, const Geometry &geometry);
    void addMesh(Material material_id, const Geometry &geometry, glm::mat4x4 instance_transform);
    void addMesh(Material material_id, const Geometry &geometry, std::span<glm::mat4x4> instance_transforms);
    void addMesh(Material material_id, const Geometry &geometry, std::vector<glm::mat4x4> instance_transforms);
    void addDirectionalLight(glm::vec3 direction, float intensity, glm::vec3 color);
    void addPointLight(glm::vec3 position, float intensity, glm::vec3 color);
  private:
    void sendCameraData(RenderCamera camera, RenderTarget target);
    void sendLightData(std::vector<DirectionalLight> const &direction_light_vec, std::vector<PointLight> const &point_light_vec);
    void drawShadowMaps(RenderCamera camera, RenderTarget target, std::vector<DirectionalLight> const &light_vec, const std::vector<std::vector<MeshInstanceList>> &mesh_instance_lists);
    void drawShadowMap(glm::mat4x4 proj_view_matrix, const RenderShadowMaps &shadow_maps, int32_t light_idx, int32_t cascade_idx, const std::vector<std::vector<MeshInstanceList>> &mesh_instance_lists);
    void drawShadowMapMeshInstanceListVec(glm::mat4x4 proj_view_matrix, const RenderShadowMaps &shadow_maps, int32_t light_idx, int32_t cascade_idx, const std::vector<std::vector<MeshInstanceList>> &mesh_instance_list_vec);
    void drawShadowMapMeshInstanceList(glm::mat4x4 proj_view_matrix, const RenderShadowMaps &shadow_maps, int32_t light_idx, int32_t cascade_idx, const MeshInstanceList &mesh_instance_list);
    void drawMeshInstanceListVec(std::vector<std::vector<MeshInstanceList>> mesh_instance_list_vec);
    void drawMeshInstanceList(Material material, const MeshInstanceList &mesh_instance_list);

  private:
    /// computeDirLightCascadeProjectionMatrix computes the orthographic projection matrix for drawing the cascaded 
    /// shadow map of this directional light at a specified cascade.
    static glm::mat4x4 computeDirLightCascadeProjectionMatrix(RenderCamera camera, RenderTarget target, glm::dmat3x3 inv_light_transform, size_t cascade_index);

    /// computeFrustumSection returns a matrix where each column is a corner of a conic section of the view frustum.
    /// The order of each point in the column mimics traditional 2D coordinate systems, where 0 is top-right and we go
    /// through each quadrant counter-clockwise when viewed from the camera's perspective.
    static glm::dmat4x3 computeFrustumSection(RenderCamera camera, RenderTarget target, double distance);

    /// computeDirLightTransform computes the linear transform of a directional light.
    static glm::dmat3x3 computeDirLightTransform(glm::dvec3 forward);

    /// projectFrustumSection applies a linear transform to each point in a conic section, then flattens along the Z
    /// axis.
    static glm::dmat4x2 projectFrustumSection(glm::dmat4x3 world_section, glm::dmat3x3 transform);
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
    MaterialLightingModel m_lighting_model;
    bool m_is_shadow_casting;
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
      MaterialLightingModel lighting_model,
      bool is_shadow_casting = true
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
    uint32_t lightingModelID() const;
    const wgpu::BindGroup &wgpuMaterialBindGroup() const;
    bool isShadowCasting() const;
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

//
// Inline method definitions:
//

namespace broccoli {
  template <size_t bg_count, size_t prefix_count>
  inline void RenderPipeline<bg_count, prefix_count>::setBindGroups(wgpu::RenderPassEncoder& encoder, std::span<wgpu::BindGroup, BIND_GROUP_SUFFIX_COUNT> suffix) const {
    for (size_t i = 0; i < BIND_GROUP_PREFIX_COUNT; i++) {
      encoder.SetBindGroup(i, this->bind_groups_prefix[i]);
    }
    for (size_t i = 0; i < BIND_GROUP_SUFFIX_COUNT; i++) {
      encoder.SetBindGroup(i + BIND_GROUP_PREFIX_COUNT, suffix[i]);
    }
  }
  template <size_t bg_count, size_t prefix_count>
  inline void RenderPipeline<bg_count, prefix_count>::setBindGroups(wgpu::RenderPassEncoder& encoder, std::array<wgpu::BindGroup, BIND_GROUP_SUFFIX_COUNT> suffix) const {
    setBindGroups(encoder, std::span<wgpu::BindGroup, BIND_GROUP_SUFFIX_COUNT>{suffix.data(), suffix.size()});
  }
}
namespace broccoli {
  inline wgpu::BindGroupLayout FinalRenderPipeline::materialBindGroupLayout() const {
    return this->bind_group_layouts[1];
  }
}
namespace broccoli {
  inline wgpu::BindGroupLayout ShadowRenderPipeline::shadowUniformBindGroupLayout() const {
    return this->bind_group_layouts[1];
  }
}

namespace broccoli {
  inline bool operator== (Vertex v1, Vertex v2) {
    return v1.offset == v2.offset && v1.normal == v2.normal && v1.tangent == v2.tangent && v1.uv == v2.uv;
  }
}
