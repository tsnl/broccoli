#include "broccoli/engine/render.hh"

#include <iostream>
#include <deque>
#include <sstream>
#include <limits>
#include <algorithm>

#include "glm/gtc/packing.hpp"

#include "broccoli/engine/config.hh"
#include "broccoli/engine/core.hh"

//
// Constants:
//

namespace broccoli {
  static const char *R3D_UBERSHADER_FILEPATH = "res/shader/3d/ubershader.wgsl";
  static const char *R3D_SHADOW_SHADER_FILEPATH = "res/shader/3d/shadow.wgsl";
  static const char *R3D_OVERLAY_SHADER_FILEPATH = "res/shader/overlay.wgsl";
  static const char *R3D_SHADER_VS_ENTRY_POINT_NAME = "vertexShaderMain";
  static const char *R3D_SHADER_FS_ENTRY_POINT_NAME = "fragmentShaderMain";
}
namespace broccoli {
  static const uint64_t R3D_VERTEX_BUFFER_CAPACITY = 1 << 16;
  static const uint64_t R3D_INDEX_BUFFER_CAPACITY = 1 << 20;
  static const uint64_t R3D_INSTANCE_CAPACITY = 1 << 10;
  static const uint64_t R3D_DIRECTIONAL_LIGHT_CAPACITY_LG2 = 2;
  static const uint64_t R3D_POINT_LIGHT_CAPACITY_LG2 = 4;
  static const uint64_t R3D_DIRECTIONAL_LIGHT_CAPACITY = 1LLU << R3D_DIRECTIONAL_LIGHT_CAPACITY_LG2;
  static const uint64_t R3D_POINT_LIGHT_CAPACITY = 1LLU << R3D_POINT_LIGHT_CAPACITY_LG2;
  static const float R3D_DEFAULT_AMBIENT_GLOW = 0.05f;
}
namespace broccoli {
  static const wgpu::TextureFormat R3D_SWAPCHAIN_TEXTURE_FORMAT = wgpu::TextureFormat::BGRA8Unorm;
  static const wgpu::TextureFormat R3D_COLOR_TEXTURE_FORMAT = wgpu::TextureFormat::BGRA8Unorm;
  static const wgpu::TextureFormat R3D_DEPTH_TEXTURE_FORMAT = wgpu::TextureFormat::Depth24Plus;
}
namespace broccoli {
  static const uint32_t R3D_MSAA_SAMPLE_COUNT = 4;
}
namespace broccoli {
  static const int64_t R3D_RGB_PALETTE_TEXTURE_SIZE = 4096;
  static const int64_t R3D_MONO_PALETTE_TEXTURE_SIZE = 16;
  static_assert(R3D_RGB_PALETTE_TEXTURE_SIZE * R3D_RGB_PALETTE_TEXTURE_SIZE == 256 * 256 * 256);
  static_assert(R3D_MONO_PALETTE_TEXTURE_SIZE * R3D_MONO_PALETTE_TEXTURE_SIZE == 256);
}
namespace broccoli {
  static const size_t R3D_MATERIAL_TABLE_INIT_CAPACITY = 256;
}
namespace broccoli {
  // Common:
  static const wgpu::TextureFormat R3D_CSM_TEXTURE_FORMAT = wgpu::TextureFormat::Depth32Float;

  // Directional lights:
  static const int32_t R3D_DIR_LIGHT_SHADOW_CASCADE_COUNT_LG2 = 2;
  static const int32_t R3D_DIR_LIGHT_SHADOW_CASCADE_COUNT = 1LLU << R3D_DIR_LIGHT_SHADOW_CASCADE_COUNT_LG2;
  static const int32_t R3D_DIR_LIGHT_SHADOW_CASCADE_MAP_RESOLUTION_LG2 = 10;
  static const std::array<double, R3D_DIR_LIGHT_SHADOW_CASCADE_COUNT> R3D_DIR_LIGHT_SHADOW_CASCADE_MAX_DISTANCES = 
    std::to_array({2.0, 16.0, 128.0, 1024.0});
  static const double R3D_DIR_LIGHT_SHADOW_RADIUS = 1024.0;

  // Point lights:
  static const int32_t R3D_POINT_LIGHT_SHADOW_CASCADE_COUNT_LG2 = 2;
  static const int32_t R3D_POINT_LIGHT_SHADOW_CASCADE_COUNT = 1LLU << R3D_POINT_LIGHT_SHADOW_CASCADE_COUNT_LG2;
  static const size_t R3D_POINT_LIGHT_SHADOW_CASCADE_MAP_RESOLUTION_LG2 = 10;
  // static const std::array<double, R3D_POINT_LIGHT_SHADOW_CASCADE_COUNT> R3D_POINT_LIGHT_SHADOW_CASCADE_MAX_DISTANCES = 
  //   std::to_array({2.0, 16.0, 128.0, 1024.0});
}
namespace broccoli {
  inline int32_t r3d_light_capacity_lg2(LightType light_type) {
    switch (light_type) {
      case LightType::Directional: return R3D_DIRECTIONAL_LIGHT_CAPACITY_LG2;
      case LightType::Point: return R3D_POINT_LIGHT_CAPACITY_LG2;
      default: PANIC("unknown/invalid light_type");
    }
  }
  inline int32_t r3d_light_capacity(LightType light_type) {
    switch (light_type) {
      case LightType::Directional: return R3D_DIRECTIONAL_LIGHT_CAPACITY;
      case LightType::Point: return R3D_POINT_LIGHT_CAPACITY;
      default: PANIC("unknown/invalid light_type");
    }
  }
  inline int32_t r3d_shadow_cascade_map_count_lg2(LightType light_type) {
    switch (light_type) {
      case LightType::Directional: return R3D_DIR_LIGHT_SHADOW_CASCADE_COUNT_LG2;
      case LightType::Point: return R3D_POINT_LIGHT_SHADOW_CASCADE_COUNT_LG2;
      default: PANIC("unknown/invalid light_type");
    }
  }
  inline int32_t r3d_shadow_cascade_map_count(LightType light_type) {
    switch (light_type) {
      case LightType::Directional: return R3D_DIR_LIGHT_SHADOW_CASCADE_COUNT;
      case LightType::Point: return R3D_POINT_LIGHT_SHADOW_CASCADE_COUNT;
      default: PANIC("unknown/invalid light_type");
    }
  }
  inline int32_t r3d_shadow_cascade_map_resolution_lg2(LightType light_type) {
    switch (light_type) {
      case LightType::Directional: return R3D_DIR_LIGHT_SHADOW_CASCADE_MAP_RESOLUTION_LG2;
      case LightType::Point: return R3D_POINT_LIGHT_SHADOW_CASCADE_MAP_RESOLUTION_LG2;
      default: PANIC("unknown/invalid light_type");
    }
  }
  inline double minDirLightCascadeDistance(size_t cascade_index) {
    CHECK(cascade_index < R3D_DIR_LIGHT_SHADOW_CASCADE_COUNT, "Bad cascade index");
    if (cascade_index == 0) {
      return 0.0;
    } else {
      return R3D_DIR_LIGHT_SHADOW_CASCADE_MAX_DISTANCES[cascade_index - 1];
    }
  }
  inline double maxDirLightCascadeDistance(size_t cascade_index) {
    CHECK(cascade_index < R3D_DIR_LIGHT_SHADOW_CASCADE_COUNT, "Bad cascade index");
    return R3D_DIR_LIGHT_SHADOW_CASCADE_MAX_DISTANCES[cascade_index];
  }
}
namespace broccoli {
  static const uint32_t R3D_DEBUG_TAKEOUT_BUFFER_SIZE = std::max<uint32_t>({
    (1U << (2 * R3D_DIR_LIGHT_SHADOW_CASCADE_MAP_RESOLUTION_LG2)) * sizeof(float),
    (1U << (2 * R3D_POINT_LIGHT_SHADOW_CASCADE_MAP_RESOLUTION_LG2)) * sizeof(float),
    0
  });
}

//
// GPU binary interface:
//

namespace broccoli {
  struct LightUniform {
    std::array<glm::vec4, R3D_DIRECTIONAL_LIGHT_CAPACITY> directional_light_dir_array;
    std::array<glm::vec4, R3D_DIRECTIONAL_LIGHT_CAPACITY> directional_light_color_array;
    std::array<glm::vec4, R3D_POINT_LIGHT_CAPACITY> point_light_pos_array;
    std::array<glm::vec4, R3D_POINT_LIGHT_CAPACITY> point_light_color_array;
    uint32_t directional_light_count;
    uint32_t point_light_count;
    float ambient_glow;
    std::array<uint32_t, 93> rsv;
  };
  struct CameraUniform {
    glm::mat4x4 view_matrix;
    glm::vec4 world_position;
    float camera_cot_half_fovy;
    float camera_aspect_inv;
    float camera_zmin;
    float camera_zmax;
    float camera_logarithmic_z_scale;
    float hdr_exposure_bias;
    uint32_t rsv00 = 0;
    uint32_t rsv01 = 0;
    uint32_t rsv02 = 0;
    uint32_t rsv03 = 0;
    uint32_t rsv04 = 0;
    uint32_t rsv05 = 0;
  };
  struct MaterialUniform {
    glm::fvec2 albedo_uv_offset = {0.0f, 0.0f};
    glm::fvec2 albedo_uv_size = {0.0f, 0.0f};
    glm::fvec2 normal_uv_offset = {0.0f, 0.0f};
    glm::fvec2 normal_uv_size = {0.0f, 0.0f};
    glm::fvec2 roughness_uv_offset = {0.0f, 0.0f};
    glm::fvec2 roughness_uv_size = {0.0f, 0.0f};
    glm::fvec2 metalness_uv_offset = {0.0f, 0.0f};
    glm::fvec2 metalness_uv_size = {0.0f, 0.0f};
    glm::fvec4 pbr_fresnel0 = {0.0f, 0.0f, 0.0f, 0.0f};
    float blinn_phong_shininess = 0.0f;
    uint32_t rsv00 = 0;
    uint32_t rsv01 = 0;
    uint32_t rsv02 = 0;
    uint32_t rsv03 = 0;
    uint32_t rsv04 = 0;
    uint32_t rsv05 = 0;
    uint32_t rsv06 = 0;
    uint32_t rsv07 = 0;
    uint32_t rsv08 = 0;
    uint32_t rsv09 = 0;
    uint32_t rsv10 = 0;
  };
  struct OverlayUniform {
    glm::i32vec2 screen_size;
    glm::i32vec2 rect_size;
    glm::i32vec2 rect_center;
  };
  static_assert(sizeof(LightUniform) == 1024, "invalid LightUniform size");
  static_assert(sizeof(CameraUniform) == 128, "invalid CameraUniform size");
  static_assert(sizeof(MaterialUniform) == 128, "invalid MaterialUniform size");
}
namespace broccoli {
  static bool operator== (ShadowUniform s1, ShadowUniform s2) {
    return s1.proj_view_matrix == s2.proj_view_matrix;
  }
}

//
// Object-object binary interface (POD):
//

namespace broccoli {
  enum class OverlayTextureDrawRequestType {
    ShadowMap,
  };
  struct OverlayTextureDrawRequestInfo {
    OverlayTextureDrawRequestType type;
    glm::i32vec2 viewport_center;
    glm::i32vec2 viewport_size;
    union {
      struct { LightType light_type; int32_t light_idx; int32_t cascade_idx; } shadow_map;
    } more;
  };
  struct WgpuShadowMapTextureOverlayResource {
    wgpu::BindGroup bind_group;
    wgpu::Sampler sampler;
  };
}

//
// Interface: RenderCamera
//

namespace broccoli {
  RenderCamera::RenderCamera(glm::mat4x4 view_matrix, glm::mat4x4 transform_matrix, glm::vec3 position, float fovy_deg, float exposure_bias)
  : m_view_matrix(view_matrix),
    m_transform_matrix(transform_matrix),
    m_position(position),
    m_fovy_deg(fovy_deg),
    m_exposure_bias(exposure_bias)
  {}
}
namespace broccoli {
  RenderCamera RenderCamera::createDefault(float fovy_deg, float exposure_bias) {
    return {glm::mat4x4{1.0f}, glm::mat4x4{1.0f}, glm::vec3{0.0f}, fovy_deg, exposure_bias};
  }
  RenderCamera RenderCamera::createFromTransform(glm::mat4x4 transform, float fovy_deg, float exposure_bias) {
    return {glm::inverse(transform), transform, transform[3], fovy_deg, exposure_bias};
  }
  RenderCamera RenderCamera::createFromLookAt(glm::vec3 eye, glm::vec3 target, glm::vec3 up, float fovy_deg, float exposure_bias) {
    glm::mat4x4 view = glm::lookAt(eye, target, up);
    return {view, glm::inverse(view), eye, fovy_deg, exposure_bias};
  }
}
namespace broccoli {
  glm::mat4x4 RenderCamera::transformMatrix() const {
    return m_transform_matrix;
  }
  glm::mat4x4 RenderCamera::viewMatrix() const {
    return m_view_matrix;
  }
  glm::vec3 RenderCamera::position() const {
    return m_position;
  }
  float RenderCamera::fovyDeg() const {
    return m_fovy_deg;
  }
  float RenderCamera::exposureBias() const {
    return m_exposure_bias;
  }
}

//
// Interface: RenderTexture
//

namespace broccoli {
  RenderTexture::RenderTexture(
    wgpu::Device &device,
    std::string name,
    glm::i64vec3 dim,
    wgpu::FilterMode filter,
    bool is_depth
  )
  : m_texture(nullptr),
    m_view(nullptr),
    m_sampler(nullptr),
    m_name(std::move(name)),
    m_wgpu_texture_label(m_name + ".WGPUTexture"),
    m_wgpu_view_label(m_name + ".WGPUTextureView"),
    m_wgpu_sampler_label(m_name + ".WGPUSampler")
  {
    create(device, dim, filter, is_depth);
  }
}
namespace broccoli {
  void RenderTexture::create(wgpu::Device &device, glm::i64vec3 dim, wgpu::FilterMode filter, bool is_depth) {
    CHECK(!m_texture, "expected WGPU texture to be uninit.");
    CHECK(!m_view, "expected WGPU texture view to be uninit.");
    
    // Check parameters:
    if (is_depth) {
      CHECK(dim.z == 1 || dim.z == 2, "Expected either 1 or 2 channels for depth.");
    } else {
      CHECK(dim.z == 1 || dim.z == 4, "Expected either 1 or 4 channels for color.");
    }

    // Select a format:
    auto format = 
      is_depth ?
      (dim.z == 1 ? wgpu::TextureFormat::Depth24Plus : wgpu::TextureFormat::Depth24PlusStencil8) :
      (dim.z == 1 ? wgpu::TextureFormat::R8Unorm : wgpu::TextureFormat::RGBA8Unorm);
    
    // Create a texture:
    wgpu::TextureDescriptor texture_desc = {
      .label = m_wgpu_texture_label.c_str(),
      .usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding,
      .dimension = wgpu::TextureDimension::e2D,
      .size = {.width=static_cast<uint32_t>(dim.x), .height=static_cast<uint32_t>(dim.y), .depthOrArrayLayers=1},
      .format = format,
    };
    m_texture = device.CreateTexture(&texture_desc);

    // Create a view:
    wgpu::TextureViewDescriptor view_desc = {
      .label = m_wgpu_view_label.c_str(),
      .format = format,
      .dimension = wgpu::TextureViewDimension::e2D,
    };
    m_view = m_texture.CreateView(&view_desc);

    // Create a sampler:
    wgpu::SamplerDescriptor sampler_desc = {
      .label = m_wgpu_sampler_label.c_str(),
      .magFilter = filter,
      .minFilter = filter,
    };
    m_sampler = device.CreateSampler(&sampler_desc);
  }
  void RenderTexture::upload(wgpu::Device &device, Bitmap const &bitmap) {
    wgpu::ImageCopyTexture copy_dst_desc = {.texture = m_texture};
    wgpu::TextureDataLayout copy_src_layout_desc = {
      .bytesPerRow = static_cast<uint32_t>(bitmap.pitch()),
      .rowsPerImage = static_cast<uint32_t>(bitmap.rows()),
    };
    wgpu::Extent3D copy_size = {static_cast<uint32_t>(bitmap.dim().x), static_cast<uint32_t>(bitmap.dim().y)};
    device.GetQueue().WriteTexture(
      &copy_dst_desc,
      bitmap.data(),
      bitmap.dataSize(),
      &copy_src_layout_desc,
      &copy_size
    );
  }
  RenderTexture RenderTexture::createForDepth(wgpu::Device &device, std::string name, glm::i64vec3 dim, wgpu::FilterMode filter) {
    bool is_depth = true;
    return {device, std::move(name), dim, filter, is_depth};
  }
  RenderTexture RenderTexture::createForColor(wgpu::Device &device, std::string name, glm::i64vec3 dim, wgpu::FilterMode filter) {
    bool is_depth = false;
    return {device, std::move(name), dim, filter, is_depth};
  }
  RenderTexture RenderTexture::createForColorFromBitmap(wgpu::Device &device, std::string name, Bitmap bitmap, wgpu::FilterMode filter) {
    RenderTexture res = createForColor(device, std::move(name), bitmap.dim(), filter);
    res.upload(device, bitmap);
    return res;
  }
}
namespace broccoli {
  wgpu::Texture const &RenderTexture::texture() const {
    return m_texture;
  }
  wgpu::TextureView const &RenderTexture::view() const {
    return m_view;
  }
  wgpu::Sampler const &RenderTexture::sampler() const {
    return m_sampler;
  }
  glm::i64vec3 RenderTexture::dim() const {
    return m_dim;
  }
}
namespace broccoli {
  RenderTextureView::RenderTextureView(RenderTexture const &rt, glm::dvec2 uv_offset, glm::dvec2 uv_size)
  : m_render_texture(rt),
    m_uv_offset(uv_offset),
    m_uv_size(uv_size)
  {}
}
namespace broccoli {
  RenderTexture const &RenderTextureView::render_texture() const {
    return m_render_texture;
  }
  glm::dvec2 RenderTextureView::uv_offset() const {
    return m_uv_offset;
  }
  glm::dvec2 RenderTextureView::uv_size() const {
    return m_uv_size;
  }
}

namespace broccoli {
  void RenderShadowMaps::init(
    wgpu::Device &dev,
    const ShadowRenderPipeline &pipeline,
    int32_t light_count_lg2,
    int32_t cascades_per_light_lg2,
    int32_t size_lg2,
    std::string name
  ) {
    CHECK(light_count_lg2 >= 0, "invalid light count lg2");
    CHECK(cascades_per_light_lg2 >= 0, "invalid cascade count lg2");
    CHECK(size_lg2 >= 0, "invalid shadow map size");
    CHECK(!m_texture, "Expected shadow map texture to be uninitialized");
    CHECK(m_write_views.empty(), "Expected shadow map texture views to be uninitialized");
    CHECK(m_read_views.empty(), "Expected shadow map texture views to be uninitialized");
    CHECK(m_shadow_map_ubo_vec.empty(), "Expected shadow map UBOs to be uninitialized");

    m_light_count_lg2 = light_count_lg2;
    m_cascades_per_light_lg2 = cascades_per_light_lg2;
    m_size_lg2 = size_lg2;
    int32_t shadow_map_count = 1 << (light_count_lg2 + cascades_per_light_lg2);

    std::string texture_descriptor_label = name + ".Texture";
    wgpu::TextureUsage texture_usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    texture_usage |= BROCCOLI_DEBUG ? wgpu::TextureUsage::CopySrc : static_cast<wgpu::TextureUsage>(0);
    wgpu::TextureDescriptor texture_descriptor = {
      .label = texture_descriptor_label.c_str(),
      .usage = texture_usage,
      .dimension = wgpu::TextureDimension::e2D,
      .size = wgpu::Extent3D {
        .width = 1U << size_lg2,
        .height = 1U << size_lg2,
        .depthOrArrayLayers = static_cast<uint32_t>(shadow_map_count),
      },
      .format = R3D_CSM_TEXTURE_FORMAT,
    };
    m_texture = dev.CreateTexture(&texture_descriptor);

    m_write_views.reserve(shadow_map_count);
    m_read_views.reserve(shadow_map_count);
    for (int32_t i = 0; i < shadow_map_count; i++) {
      std::string write_view_descriptor_label = name + ".WriteView";
      std::string read_view_descriptor_label = name + ".ReadView";
      wgpu::TextureViewDescriptor write_view_descriptor = {
        .label = write_view_descriptor_label.c_str(),
        .format = R3D_CSM_TEXTURE_FORMAT,
        .dimension = wgpu::TextureViewDimension::e2D,
        .baseArrayLayer = static_cast<uint32_t>(i),
        .arrayLayerCount = 1,
        .aspect = wgpu::TextureAspect::DepthOnly,
      };
      wgpu::TextureViewDescriptor read_view_descriptor = {
        .label = read_view_descriptor_label.c_str(),
        .format = R3D_CSM_TEXTURE_FORMAT,
        .dimension = wgpu::TextureViewDimension::e2D,
        .baseArrayLayer = static_cast<uint32_t>(i),
        .arrayLayerCount = 1,
        .aspect = wgpu::TextureAspect::All,
      };
      m_write_views.push_back(m_texture.CreateView(&write_view_descriptor));
      m_read_views.push_back(m_texture.CreateView(&read_view_descriptor));
    }

    m_shadow_map_ubo_vec.reserve(shadow_map_count);
    for (int32_t i = 0; i < shadow_map_count; i++) {
      std::string buffer_label = name + ".ShadowMapUbo.Buffer";
      std::string bind_group_label = name + ".ShadowMapUbo.BindGroup";
      wgpu::BufferDescriptor buffer_descriptor = {
        .label = buffer_label.c_str(),
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
        .size = sizeof(ShadowUniform),
      };
      wgpu::Buffer buffer = dev.CreateBuffer(&buffer_descriptor);
      auto bind_group_entries = std::to_array({
        wgpu::BindGroupEntry {
          .binding = 0,
          .buffer = buffer,
          .size = sizeof(ShadowUniform),
        },
      });
      wgpu::BindGroupDescriptor bind_group_descriptor = {
        .label = bind_group_label.c_str(),
        .layout = pipeline.shadowUniformBindGroupLayout(),
        .entryCount = bind_group_entries.size(),
        .entries = bind_group_entries.data(),
      };
      ShadowMapUbo ubo = {
        .state = {},
        .buffer = buffer,
        .bind_group = dev.CreateBindGroup(&bind_group_descriptor),
      };
      m_shadow_map_ubo_vec.push_back(std::move(ubo));
    }
  }
}
namespace broccoli {
  wgpu::Texture RenderShadowMaps::texture() const {
    return m_texture;
  }
  const wgpu::TextureView &RenderShadowMaps::getWriteView(int32_t light_idx, int32_t cascade_idx) const {
    return m_write_views[(light_idx << m_cascades_per_light_lg2) + cascade_idx];
  }
  const wgpu::TextureView &RenderShadowMaps::getReadView(int32_t light_idx, int32_t cascade_idx) const {
    return m_read_views[(light_idx << m_cascades_per_light_lg2) + cascade_idx];
  }
  const RenderShadowMaps::ShadowMapUbo &RenderShadowMaps::getShadowMapUbo(int32_t light_idx, int32_t cascade_idx) const {
    return m_shadow_map_ubo_vec[(light_idx << m_cascades_per_light_lg2) + cascade_idx];
  }
}

//
// Interface: Renderer
//

namespace broccoli {
  RenderManager::RenderManager(wgpu::Device &device, glm::ivec2 framebuffer_size)
  : m_wgpu_device(device),
    m_rgb_palette(
      RenderTexture::createForColorFromBitmap(
        m_wgpu_device,
        "Broccoli.Render.Texture.RgbPalette",
        initRgbPalette(),
        wgpu::FilterMode::Nearest
      )
    ),
    m_monochrome_palette(
      RenderTexture::createForColorFromBitmap(
        m_wgpu_device,
        "Broccoli.Render.Texture.MonochromePalette",
        initMonochromePalette(),
        wgpu::FilterMode::Nearest
      )
    ),
    m_framebuffer_size(0)
  {
    initFinalShaderModules();
    initShadowShaderModule();
    initOverlayShaderModules();
    initBuffers();
    initFinalPbrRenderPipeline();
    initFinalBlinnPhongRenderPipeline();
    initShadowRenderPipeline();
    initOverlayRenderPipeline();
    initShadowMaps();
    resize(framebuffer_size);
  }

  Bitmap RenderManager::initMonochromePalette() {
    Bitmap bitmap{glm::i32vec3{R3D_MONO_PALETTE_TEXTURE_SIZE, R3D_MONO_PALETTE_TEXTURE_SIZE, 1}};
    for (int64_t r = 0; r <= 255; r++) {
      auto ptr = bitmap((r & 0x0F) >> 0, (r & 0xF0) >> 4);
      *ptr = static_cast<uint8_t>(r);
    }
    return bitmap;
  }

  Bitmap RenderManager::initRgbPalette() {
    Bitmap bitmap{glm::i32vec3{R3D_RGB_PALETTE_TEXTURE_SIZE, R3D_RGB_PALETTE_TEXTURE_SIZE, 4}};
    for (int64_t r = 0; r <= 255; r++) {
      for (int64_t g = 0; g <= 255; g++) {
        for (int64_t b = 0; b <= 255; b++) {
          int64_t index = (r << 0) | (g << 8) | (b << 16);
          auto ptr = bitmap((index & 0x000FFF) >> 0, (index & 0xFFF000) >> 12);
          ptr[0] = static_cast<uint8_t>(r);
          ptr[1] = static_cast<uint8_t>(g);
          ptr[2] = static_cast<uint8_t>(b);
          ptr[3] = static_cast<uint8_t>(0xFF);
        }
      }
    }
    return bitmap;
  }
  
  void RenderManager::initFinalShaderModules() {
    const char *filepath = R3D_UBERSHADER_FILEPATH;
    std::string raw_shader_text = readTextFile(filepath);
    m_wgpu_pbr_shader_module = initShaderModuleVariant(
      filepath, 
      raw_shader_text,
      std::unordered_map<std::string, std::string> {
        {"p_LIGHTING_MODEL", std::to_string(static_cast<uint32_t>(MaterialLightingModel::PhysicallyBased))},
        {"p_INSTANCE_COUNT", std::to_string(static_cast<uint32_t>(R3D_INSTANCE_CAPACITY))},
        {"p_DIRECTIONAL_LIGHT_COUNT", std::to_string(static_cast<uint32_t>(R3D_DIRECTIONAL_LIGHT_CAPACITY))},
        {"p_POINT_LIGHT_COUNT", std::to_string(static_cast<uint32_t>(R3D_POINT_LIGHT_CAPACITY))},
      }
    );
    m_wgpu_blinn_phong_shader_module = initShaderModuleVariant(
      filepath, 
      raw_shader_text,
      std::unordered_map<std::string, std::string> {
        {"p_LIGHTING_MODEL", std::to_string(static_cast<uint32_t>(MaterialLightingModel::BlinnPhong))},
        {"p_INSTANCE_COUNT", std::to_string(static_cast<uint32_t>(R3D_INSTANCE_CAPACITY))},
        {"p_DIRECTIONAL_LIGHT_COUNT", std::to_string(static_cast<uint32_t>(R3D_DIRECTIONAL_LIGHT_CAPACITY))},
        {"p_POINT_LIGHT_COUNT", std::to_string(static_cast<uint32_t>(R3D_POINT_LIGHT_CAPACITY))},
      }
    );
  }

  void RenderManager::initShadowShaderModule() {
    const char *filepath = R3D_SHADOW_SHADER_FILEPATH;
    std::string raw_shader_text = readTextFile(filepath);
    m_wgpu_shadow_shader_module = initShaderModuleVariant(
      filepath, 
      raw_shader_text,
      std::unordered_map<std::string, std::string> {
        {"p_INSTANCE_COUNT", std::to_string(static_cast<uint32_t>(R3D_INSTANCE_CAPACITY))},
      }
    );
  }

  void RenderManager::initOverlayShaderModules() {
    const char *filepath = R3D_OVERLAY_SHADER_FILEPATH;
    std::string shader_text = readTextFile(filepath);
    m_wgpu_overlay_monochrome_shader_module = initShaderModuleVariant(
      filepath, 
      shader_text,
      std::unordered_map<std::string, std::string> {
        {"p_SAMPLE_MODE", "1"}
      }
    );
    m_wgpu_overlay_rgba_shader_module = initShaderModuleVariant(
      filepath, 
      shader_text,
      std::unordered_map<std::string, std::string> {
        {"p_SAMPLE_MODE", "4"}
      }
    );
  }

  wgpu::ShaderModule RenderManager::initShaderModuleVariant(
    const char *filepath,
    const std::string &raw_shader_text,
    std::unordered_map<std::string, std::string> rw_map
  ) {
    std::string shader_text = replaceAll(raw_shader_text, rw_map);
    return initShaderModuleVariant(filepath, shader_text);
  }

  wgpu::ShaderModule RenderManager::initShaderModuleVariant(const char *filepath, const std::string &shader_text) {
    // Compiling:
    wgpu::ShaderModuleWGSLDescriptor shader_module_wgsl_descriptor;
    shader_module_wgsl_descriptor.nextInChain = nullptr;
    shader_module_wgsl_descriptor.code = shader_text.c_str();
    wgpu::ShaderModuleDescriptor shader_module_descriptor = {
      .nextInChain = &shader_module_wgsl_descriptor,
      .label = "Broccoli.Render.Final.ShaderModule",
    };
    auto shader_module = m_wgpu_device.CreateShaderModule(&shader_module_descriptor);
    
    // Checking:
    struct ShaderCompileResult { const char *filepath; bool completed; };
    ShaderCompileResult result = {filepath, false};
    shader_module.GetCompilationInfo(
      [] (WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo *info, void *userdata) {
        auto data = reinterpret_cast<ShaderCompileResult*>(userdata);
        auto error_thunk = [data, info] () {
          std::stringstream ss;
          ss << "Shader compilation failed:" << std::endl;
          for (size_t i = 0; i < info->messageCount; i++) {
            auto const &message = info->messages[i];
            ss 
              << "* " << message.message << std::endl
              << "  see: " << data->filepath << ":" << message.lineNum << ":" << message.linePos << std::endl;
          }
          return ss.str();
        };
        CHECK(status == WGPUCompilationInfoRequestStatus_Success, error_thunk);
        data->completed = true;
      },
      &result
    );
    CHECK(result.completed, "Expected shader compilation check to be sync, not async");

    // All done:
    return shader_module;
  }

  void RenderManager::initBuffers() {
    // light uniform buffer:
    {
      wgpu::BufferDescriptor light_uniform_buffer_descriptor = {
        .label = "Broccoli.Render.LightUniformBuffer",
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = sizeof(LightUniform),
      };
      m_wgpu_light_uniform_buffer = m_wgpu_device.CreateBuffer(&light_uniform_buffer_descriptor);
    }

    // camera uniform buffer:
    {
      wgpu::BufferDescriptor camera_uniform_buffer_descriptor = {
        .label = "Broccoli.Render.CameraUniformBuffer",
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = sizeof(CameraUniform),
      };
      m_wgpu_camera_uniform_buffer = m_wgpu_device.CreateBuffer(&camera_uniform_buffer_descriptor);
    }

    // transform uniform buffer:
    {
      wgpu::BufferDescriptor draw_transform_uniform_buffer_descriptor = {
        .label = "Broccoli.Render.TransformUniformBuffer",
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = R3D_INSTANCE_CAPACITY * sizeof(glm::mat4x4),
      };
      m_wgpu_transform_uniform_buffer = m_wgpu_device.CreateBuffer(&draw_transform_uniform_buffer_descriptor);
    }

    // overlay vertex buffer:
    {
      wgpu::BufferDescriptor overlay_vertex_buffer_descriptor = {
        .label = "Broccoli.Render.Overlay.VertexBuffer",
        .usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst,
        .size = sizeof(glm::f32vec2) * 6,
      };
      m_wgpu_overlay_vertex_buffer = m_wgpu_device.CreateBuffer(&overlay_vertex_buffer_descriptor);
      auto vertex_data = std::to_array({
        glm::fvec2{+1.0, +1.0},
        glm::fvec2{-1.0, +1.0},
        glm::fvec2{-1.0, -1.0},
        glm::fvec2{+1.0, +1.0},
        glm::fvec2{-1.0, -1.0},
        glm::fvec2{+1.0, -1.0},
      });
      m_wgpu_device.GetQueue().WriteBuffer(
        m_wgpu_overlay_vertex_buffer,
        0,
        &vertex_data,
        sizeof(vertex_data)
      );
    }

    // overlay uniform buffer:
    {
      wgpu::BufferDescriptor overlay_uniform_buffer_descriptor = {
        .label = "Broccoli.Render.Overlay.UniformBuffer",
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = sizeof(OverlayUniform),
      };
      m_wgpu_overlay_uniform_buffer = m_wgpu_device.CreateBuffer(&overlay_uniform_buffer_descriptor);
    }
  }

  void RenderManager::reinitColorTexture(glm::ivec2 framebuffer_size) {
    if (m_wgpu_render_target_color_texture) {
      m_wgpu_render_target_color_texture.Destroy();
      m_wgpu_render_target_color_texture = nullptr;
    }

    wgpu::TextureDescriptor color_texture_descriptor = {
      .label = "Broccoli.Render.Target.ColorTexture",
      .usage = wgpu::TextureUsage::RenderAttachment,
      .dimension = wgpu::TextureDimension::e2D,
      .size = wgpu::Extent3D {
        .width = static_cast<uint32_t>(framebuffer_size.x),
        .height = static_cast<uint32_t>(framebuffer_size.y),
      },
      .format = R3D_COLOR_TEXTURE_FORMAT,
      .mipLevelCount = 1,
      .sampleCount = R3D_MSAA_SAMPLE_COUNT,
    };
    m_wgpu_render_target_color_texture = m_wgpu_device.CreateTexture(&color_texture_descriptor);
    
    wgpu::TextureViewDescriptor color_texture_view_descriptor = {
      .label = "Broccoli.Render.Target.ColorTextureView",
      .format = R3D_COLOR_TEXTURE_FORMAT,
      .dimension = wgpu::TextureViewDimension::e2D,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .aspect = wgpu::TextureAspect::All,
    };
    m_wgpu_render_target_color_texture_view = m_wgpu_render_target_color_texture.CreateView(&color_texture_view_descriptor);
  }

  void RenderManager::reinitDepthStencilTexture(glm::ivec2 framebuffer_size) {
    if (m_wgpu_render_target_depth_stencil_texture) {
      m_wgpu_render_target_depth_stencil_texture.Destroy();
      m_wgpu_render_target_depth_stencil_texture = nullptr;
    }
    
    wgpu::TextureDescriptor depth_texture_view_descriptor = {
      .label = "Broccoli.Render.Target.DepthStencilTexture",
      .usage = wgpu::TextureUsage::RenderAttachment,
      .dimension = wgpu::TextureDimension::e2D,
      .size = wgpu::Extent3D {
        .width = static_cast<uint32_t>(framebuffer_size.x),
        .height = static_cast<uint32_t>(framebuffer_size.y),
      },
      .format = R3D_DEPTH_TEXTURE_FORMAT,
      .mipLevelCount = 1,
      .sampleCount = R3D_MSAA_SAMPLE_COUNT,
    };
    m_wgpu_render_target_depth_stencil_texture = m_wgpu_device.CreateTexture(&depth_texture_view_descriptor);
    
    wgpu::TextureViewDescriptor depth_view_descriptor = {
      .label = "Broccoli.Render.Target.DepthStencilTextureView",
      .format = R3D_DEPTH_TEXTURE_FORMAT,
      .dimension = wgpu::TextureViewDimension::e2D,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .aspect = wgpu::TextureAspect::DepthOnly,
    };
    auto texture_view = m_wgpu_render_target_depth_stencil_texture.CreateView(&depth_view_descriptor);
    m_wgpu_render_target_depth_stencil_texture_view = texture_view;
  }

  void RenderManager::reinitOverlayTexture(glm::ivec2 framebuffer_size) {
    // Overlay texture:
    {
      if (m_wgpu_final_overlay_texture) {
        m_wgpu_final_overlay_texture.Destroy();
      }
      wgpu::TextureDescriptor overlay_texture_descriptor = {
        .label = "Broccoli.Render.Overlay.Texture",
        .usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding,
        .size = wgpu::Extent3D {
          .width=static_cast<uint32_t>(framebuffer_size.x),
          .height=static_cast<uint32_t>(framebuffer_size.y)
        },
        .format = wgpu::TextureFormat::RGBA8Unorm,
      };
      m_wgpu_final_overlay_texture = m_wgpu_device.CreateTexture(&overlay_texture_descriptor);
    }

    // Overlay texture view:
    {
      wgpu::TextureViewDescriptor overlay_texture_view_descriptor = {
        .label = "Broccoli.Render.Overlay.TextureView",
      };
      m_wgpu_final_overlay_texture_view = m_wgpu_final_overlay_texture.CreateView(&overlay_texture_view_descriptor);
    }

    // Overlay sampler:
    {
      wgpu::SamplerDescriptor overlay_sampler_descriptor = {
        .label = "Broccoli.Render.Overlay.Sampler"
      };
      m_wgpu_final_overlay_sampler = m_wgpu_device.CreateSampler(&overlay_sampler_descriptor);
    }

    // Bind group 1:
    {
      auto bind_group_1_entries = std::to_array({
        wgpu::BindGroupEntry{.binding=0, .textureView=m_wgpu_final_overlay_texture_view},
        wgpu::BindGroupEntry{.binding=1, .sampler=m_wgpu_final_overlay_sampler},
      });
      wgpu::BindGroupDescriptor bind_group_1_descriptor = {
        .label = "Broccoli.Render.Overlay.BindGroup1",
        .layout = m_overlay_rgba_render_pipeline.textureSelectBindGroupLayout(),
        .entryCount = bind_group_1_entries.size(),
        .entries = bind_group_1_entries.data(),
      };
      m_wgpu_final_overlay_bind_group_1 = m_wgpu_device.CreateBindGroup(&bind_group_1_descriptor);
    }

    // Overlay bitmap:
    {
      m_final_overlay_bitmap = Bitmap{glm::i32vec3{framebuffer_size, 4}};
    }
  }
}

namespace broccoli {
  void RenderManager::lazyInitDebugTakeoutBuffer() {
#if !BROCCOLI_DEBUG
    PANIC("Cannot initialize debug takout buffer unless in debug mode.");
#else
    if (m_wgpu_debug_takeout_buffer) {
      return;
    }
    wgpu::BufferDescriptor desc = {
      .label = "Broccoli.Render.Debug.TakoutBuffer",
      .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
      .size = R3D_DEBUG_TAKEOUT_BUFFER_SIZE,
      .mappedAtCreation = false,
    };
    m_wgpu_debug_takeout_buffer = m_wgpu_device.CreateBuffer(&desc);
#endif
  }
}

namespace broccoli {
  FinalRenderPipeline RenderManager::helpInitFinalRenderPipeline(uint32_t lighting_model_id) {
    FinalRenderPipeline out;

    // bind group layout 0:
    {
      auto entries = std::to_array({
        wgpu::BindGroupLayoutEntry {
          .binding = 0,
          .visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
          .buffer = wgpu::BufferBindingLayout {
            .type = wgpu::BufferBindingType::Uniform,
            .minBindingSize = sizeof(CameraUniform),
          },
        },
        wgpu::BindGroupLayoutEntry {
          .binding = 1,
          .visibility = wgpu::ShaderStage::Fragment,
          .buffer = wgpu::BufferBindingLayout {
            .type = wgpu::BufferBindingType::Uniform,
            .minBindingSize = sizeof(LightUniform),
          },
        },
        wgpu::BindGroupLayoutEntry {
          .binding = 2,
          .visibility = wgpu::ShaderStage::Vertex,
          .buffer = wgpu::BufferBindingLayout {
            .type = wgpu::BufferBindingType::Uniform,
            .minBindingSize = sizeof(glm::mat4x4) * R3D_INSTANCE_CAPACITY,
          },
        },
      });
      wgpu::BindGroupLayoutDescriptor descriptor = {
        .label = "Broccoli.Render.Final.BindGroup0Layout",
        .entryCount = entries.size(),
        .entries = entries.data()
      };
      out.bind_group_layouts[0] = m_wgpu_device.CreateBindGroupLayout(&descriptor);
    }

    // bind group layout 1:
    {
      auto entries = std::to_array({
        wgpu::BindGroupLayoutEntry {
          .binding = 0,
          .visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
          .buffer = wgpu::BufferBindingLayout {
            .type = wgpu::BufferBindingType::Uniform,
            .minBindingSize = sizeof(MaterialUniform),
          },
        },
        wgpu::BindGroupLayoutEntry {
          .binding = 1,
          .visibility = wgpu::ShaderStage::Fragment,
          .texture = {
            .sampleType = wgpu::TextureSampleType::Float,
            .viewDimension = wgpu::TextureViewDimension::e2D,
          }
        },
        wgpu::BindGroupLayoutEntry {
          .binding = 2,
          .visibility = wgpu::ShaderStage::Fragment,
          .texture = {
            .sampleType = wgpu::TextureSampleType::Float,
            .viewDimension = wgpu::TextureViewDimension::e2D,
          }
        },
        wgpu::BindGroupLayoutEntry {
          .binding = 3,
          .visibility = wgpu::ShaderStage::Fragment,
          .texture = {
            .sampleType = wgpu::TextureSampleType::Float,
            .viewDimension = wgpu::TextureViewDimension::e2D,
          }
        },
        wgpu::BindGroupLayoutEntry {
          .binding = 4,
          .visibility = wgpu::ShaderStage::Fragment,
          .texture = {
            .sampleType = wgpu::TextureSampleType::Float,
            .viewDimension = wgpu::TextureViewDimension::e2D,
          }
        },
        wgpu::BindGroupLayoutEntry {
          .binding = 5,
          .visibility = wgpu::ShaderStage::Fragment,
          .sampler = {.type = wgpu::SamplerBindingType::Filtering},
        },
        wgpu::BindGroupLayoutEntry {
          .binding = 6,
          .visibility = wgpu::ShaderStage::Fragment,
          .sampler = {.type = wgpu::SamplerBindingType::Filtering},
        },
        wgpu::BindGroupLayoutEntry {
          .binding = 7,
          .visibility = wgpu::ShaderStage::Fragment,
          .sampler = {.type = wgpu::SamplerBindingType::Filtering},
        },
        wgpu::BindGroupLayoutEntry {
          .binding = 8,
          .visibility = wgpu::ShaderStage::Fragment,
          .sampler = {.type = wgpu::SamplerBindingType::Filtering},
        },
      });
      wgpu::BindGroupLayoutDescriptor descriptor = {
        .label = "Broccoli.Render.Final.BindGroup1Layout",
        .entryCount = entries.size(),
        .entries = entries.data()
      };
      out.bind_group_layouts[1] = m_wgpu_device.CreateBindGroupLayout(&descriptor);
    }

    // pipeline layout:
    {
      wgpu::PipelineLayoutDescriptor descriptor = {
        .label = "Broccoli.Render.Final.RenderPipelineLayout",
        .bindGroupLayoutCount = out.bind_group_layouts.size(),
        .bindGroupLayouts = out.bind_group_layouts.data(),
      };
      out.pipeline_layout = m_wgpu_device.CreatePipelineLayout(&descriptor);
    }

    // render pipeline:
    {
      wgpu::BlendState blend_state = {
        .color = {
          .operation = wgpu::BlendOperation::Add,
          .srcFactor = wgpu::BlendFactor::One,
          .dstFactor = wgpu::BlendFactor::Zero,
        },
        .alpha = {
          .operation = wgpu::BlendOperation::Add,
          .srcFactor = wgpu::BlendFactor::One,
          .dstFactor = wgpu::BlendFactor::Zero,
        },
      };
      wgpu::ColorTargetState color_target = {
        .format = R3D_SWAPCHAIN_TEXTURE_FORMAT,
        .blend = &blend_state,
        .writeMask = wgpu::ColorWriteMask::All,
      };
      auto vertex_buffer_attrib_layout = std::to_array({
        wgpu::VertexAttribute{wgpu::VertexFormat::Sint32x3, offsetof(Vertex, offset), 0},
        wgpu::VertexAttribute{wgpu::VertexFormat::Unorm10_10_10_2, offsetof(Vertex, normal), 1},
        wgpu::VertexAttribute{wgpu::VertexFormat::Unorm10_10_10_2, offsetof(Vertex, tangent), 2},
        wgpu::VertexAttribute{wgpu::VertexFormat::Unorm16x2, offsetof(Vertex, uv), 3},
      });
      wgpu::VertexBufferLayout vertex_buffer_layout = {
        .arrayStride = sizeof(Vertex),
        .stepMode = wgpu::VertexStepMode::Vertex,
        .attributeCount = vertex_buffer_attrib_layout.size(),
        .attributes = vertex_buffer_attrib_layout.data(),
      };
      wgpu::VertexState vertex_state = {
        .module = wgpuFinalShaderModule(lighting_model_id),
        .entryPoint = R3D_SHADER_VS_ENTRY_POINT_NAME,
        .bufferCount = 1,
        .buffers = &vertex_buffer_layout,
      };
      wgpu::PrimitiveState primitive_state = {
        .topology = wgpu::PrimitiveTopology::TriangleList,
        .stripIndexFormat = wgpu::IndexFormat::Undefined,
        .frontFace = wgpu::FrontFace::CCW,
        .cullMode = wgpu::CullMode::Back,
      };
      wgpu::DepthStencilState depth_stencil_state = {
        .format = R3D_DEPTH_TEXTURE_FORMAT,
        .depthWriteEnabled = true,
        .depthCompare = wgpu::CompareFunction::LessEqual,
      };
      wgpu::MultisampleState multisample_state = {
        .count = R3D_MSAA_SAMPLE_COUNT,
      };
      wgpu::FragmentState fragment_state = {
        .module = wgpuFinalShaderModule(lighting_model_id),
        .entryPoint = R3D_SHADER_FS_ENTRY_POINT_NAME,
        .targetCount = 1,
        .targets = &color_target,
      };
      wgpu::RenderPipelineDescriptor descriptor = {
        .label = "Broccoli.Render.Final.RenderPipeline",
        .layout = out.pipeline_layout,
        .vertex = vertex_state,
        .primitive = primitive_state,
        .depthStencil = &depth_stencil_state,
        .multisample = multisample_state,
        .fragment = &fragment_state,
      };
      out.pipeline = m_wgpu_device.CreateRenderPipeline(&descriptor);
    }

    // bind group 0:
    {
      auto bind_group_entries = std::to_array({
        wgpu::BindGroupEntry {
          .binding = 0,
          .buffer = m_wgpu_camera_uniform_buffer,
          .size = sizeof(CameraUniform),
        },
        wgpu::BindGroupEntry {
          .binding = 1,
          .buffer = m_wgpu_light_uniform_buffer,
          .size = sizeof(LightUniform),
        },
        wgpu::BindGroupEntry {
          .binding = 2,
          .buffer = m_wgpu_transform_uniform_buffer,
          .size = sizeof(glm::mat4x4) * R3D_INSTANCE_CAPACITY,
        },
      });
      wgpu::BindGroupDescriptor bind_group_descriptor = {
        .label = "Broccoli.Render.Final.BindGroup0",
        .layout = out.bind_group_layouts[0],
        .entryCount = bind_group_entries.size(),
        .entries = bind_group_entries.data(),
      };
      out.bind_groups_prefix[0] = m_wgpu_device.CreateBindGroup(&bind_group_descriptor);
    }

    // all done:
    return out;
  }
  void RenderManager::initFinalPbrRenderPipeline() {
    auto render_pipeline = helpInitFinalRenderPipeline(static_cast<uint32_t>(MaterialLightingModel::PhysicallyBased));
    m_wgpu_pbr_final_render_pipeline = render_pipeline;
  }
  void RenderManager::initFinalBlinnPhongRenderPipeline() {
    auto render_pipeline = helpInitFinalRenderPipeline(static_cast<uint32_t>(MaterialLightingModel::BlinnPhong));
    m_wgpu_blinn_phong_final_render_pipeline = render_pipeline;
  }
  void RenderManager::initShadowRenderPipeline() {
    // bind group layout 0:
    {
      auto entries = std::to_array({
        wgpu::BindGroupLayoutEntry {
          .binding = 0,
          .visibility = wgpu::ShaderStage::Vertex,
          .buffer = wgpu::BufferBindingLayout {
            .type = wgpu::BufferBindingType::Uniform,
            .minBindingSize = sizeof(glm::mat4x4) * R3D_INSTANCE_CAPACITY,
          },
        },
      });
      wgpu::BindGroupLayoutDescriptor descriptor = {
        .label = "Broccoli.Render.Shadow.BindGroup0Layout",
        .entryCount = entries.size(),
        .entries = entries.data()
      };
      auto bind_group_layout = m_wgpu_device.CreateBindGroupLayout(&descriptor);
      m_shadow_render_pipeline.bind_group_layouts[0] = bind_group_layout;
    }

    // bind group layout 1:
    {
      auto entries = std::to_array({
        wgpu::BindGroupLayoutEntry {
          .binding = 0,
          .visibility = wgpu::ShaderStage::Vertex,
          .buffer = wgpu::BufferBindingLayout {
            .type = wgpu::BufferBindingType::Uniform,
            .minBindingSize = sizeof(ShadowUniform),
          },
        },
      });
      wgpu::BindGroupLayoutDescriptor descriptor = {
        .label = "Broccoli.Render.Shadow.BindGroup1Layout",
        .entryCount = entries.size(),
        .entries = entries.data()
      };
      auto bind_group_layout = m_wgpu_device.CreateBindGroupLayout(&descriptor);
      m_shadow_render_pipeline.bind_group_layouts[1] = bind_group_layout;
    }

    // pipeline layout:
    {
      wgpu::PipelineLayoutDescriptor descriptor = {
        .label = "Broccoli.Render.Shadow.RenderPipelineLayout",
        .bindGroupLayoutCount = m_shadow_render_pipeline.bind_group_layouts.size(),
        .bindGroupLayouts = m_shadow_render_pipeline.bind_group_layouts.data(),
      };
      m_shadow_render_pipeline.pipeline_layout = m_wgpu_device.CreatePipelineLayout(&descriptor);
    }

    // render pipeline:
    // TODO: must disable back-face culling for dir-lights, but can leave enabled for other types of lights.
    {
      auto vertex_buffer_attrib_layout = std::to_array({
        wgpu::VertexAttribute{wgpu::VertexFormat::Sint32x3, offsetof(Vertex, offset), 0},
      });
      wgpu::VertexBufferLayout vertex_buffer_layout = {
        .arrayStride = sizeof(Vertex),
        .stepMode = wgpu::VertexStepMode::Vertex,
        .attributeCount = vertex_buffer_attrib_layout.size(),
        .attributes = vertex_buffer_attrib_layout.data(),
      };
      wgpu::VertexState vertex_state = {
        .module = m_wgpu_shadow_shader_module,
        .entryPoint = R3D_SHADER_VS_ENTRY_POINT_NAME,
        .bufferCount = 1,
        .buffers = &vertex_buffer_layout,
      };
      wgpu::PrimitiveState primitive_state = {
        .topology = wgpu::PrimitiveTopology::TriangleList,
        .stripIndexFormat = wgpu::IndexFormat::Undefined,
        .frontFace = wgpu::FrontFace::CCW,
        .cullMode = wgpu::CullMode::None,
      };
      wgpu::DepthStencilState depth_stencil_state = {
        .format = R3D_CSM_TEXTURE_FORMAT,
        .depthWriteEnabled = true,
        .depthCompare = wgpu::CompareFunction::LessEqual,
      };
      wgpu::MultisampleState multisample_state = {
        .count = 1,
      };
      wgpu::FragmentState fragment_state = {
        .module = m_wgpu_shadow_shader_module,
        .entryPoint = R3D_SHADER_FS_ENTRY_POINT_NAME,
        .targetCount = 0,
      };
      wgpu::RenderPipelineDescriptor descriptor = {
        .label = "Broccoli.Render.Shadow.RenderPipeline",
        .layout = m_shadow_render_pipeline.pipeline_layout,
        .vertex = vertex_state,
        .primitive = primitive_state,
        .depthStencil = &depth_stencil_state,
        .multisample = multisample_state,
        .fragment = &fragment_state,
      };
      m_shadow_render_pipeline.pipeline = m_wgpu_device.CreateRenderPipeline(&descriptor);
    }

    // bind group 0:
    {
      auto bind_group_entries = std::to_array({
        wgpu::BindGroupEntry {
          .binding = 0,
          .buffer = m_wgpu_transform_uniform_buffer,
          .size = sizeof(glm::mat4x4) * R3D_INSTANCE_CAPACITY,
        },
      });
      wgpu::BindGroupDescriptor descriptor = {
        .label = "Broccoli.Render.Shadow.BindGroup0",
        .layout = m_shadow_render_pipeline.bind_group_layouts[0],
        .entryCount = bind_group_entries.size(),
        .entries = bind_group_entries.data(),
      };
      m_shadow_render_pipeline.bind_groups_prefix[0] = m_wgpu_device.CreateBindGroup(&descriptor);
    }
  }

  OverlayRenderPipeline RenderManager::helpInitOverlayRenderPipeline(uint32_t overlay_texture_type) {
    OverlayRenderPipeline render_pipeline;

    // bind group layout 0
    {
      auto entries = std::to_array({
        wgpu::BindGroupLayoutEntry {
          .binding = 0,
          .visibility = wgpu::ShaderStage::Vertex,
          .buffer = wgpu::BufferBindingLayout {
            .type = wgpu::BufferBindingType::Uniform,
            .minBindingSize = sizeof(OverlayUniform),
          }
        },
      });
      wgpu::BindGroupLayoutDescriptor desc = {
        .label = "Broccoli.Render.Overlay.BindGroup0Layout",
        .entryCount = entries.size(),
        .entries = entries.data(),
      };
      render_pipeline.bind_group_layouts[0] = m_wgpu_device.CreateBindGroupLayout(&desc);
    }

    // bind group layout 1
    {
      auto entries = std::to_array({
        wgpu::BindGroupLayoutEntry {
          .binding = 0,
          .visibility = wgpu::ShaderStage::Fragment,
          .texture = wgpu::TextureBindingLayout {
            .sampleType = wgpu::TextureSampleType::UnfilterableFloat,
            .viewDimension = wgpu::TextureViewDimension::e2D,
          },
        },
        wgpu::BindGroupLayoutEntry {
          .binding = 1,
          .visibility = wgpu::ShaderStage::Fragment,
          .sampler = {.type = wgpu::SamplerBindingType::NonFiltering},
        },
      });
      wgpu::BindGroupLayoutDescriptor desc = {
        .label = "Broccoli.Render.Overlay.BindGroup1Layout",
        .entryCount = entries.size(),
        .entries = entries.data(),
      };
      render_pipeline.bind_group_layouts[1] = m_wgpu_device.CreateBindGroupLayout(&desc);
    }

    // pipeline layout:
    {
      wgpu::PipelineLayoutDescriptor desc = {
        .label = "Broccoli.Render.Overlay.PipelineLayout",
        .bindGroupLayoutCount = render_pipeline.bind_group_layouts.size(),
        .bindGroupLayouts = render_pipeline.bind_group_layouts.data(),
      };
      render_pipeline.pipeline_layout = m_wgpu_device.CreatePipelineLayout(&desc);
    }

    // render pipeline:
    {
      auto vertex_attributes = std::to_array({
        wgpu::VertexAttribute{.format=wgpu::VertexFormat::Float32x2, .offset=0, .shaderLocation=0},
      });
      // Using premultiplied alpha 'blend over' operator:
      // See: https://en.wikipedia.org/wiki/Alpha_compositing
      wgpu::BlendState blend_state = {
        .color = {
          .operation = wgpu::BlendOperation::Add,
          .srcFactor = wgpu::BlendFactor::One,
          .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
        },
        .alpha = {
          .operation = wgpu::BlendOperation::Add,
          .srcFactor = wgpu::BlendFactor::One,
          .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
        },
      };
      wgpu::ColorTargetState color_target_state = {
        .format = wgpu::TextureFormat::BGRA8Unorm,
        .blend = &blend_state,
        .writeMask = wgpu::ColorWriteMask::All,
      };
      wgpu::VertexBufferLayout vertex_buffer_layout = {
        .arrayStride = sizeof(glm::f32vec2),
        .attributeCount = vertex_attributes.size(),
        .attributes = vertex_attributes.data(),
      };
      wgpu::VertexState vertex_state = {
        .module = wgpuOverlayShaderModule(overlay_texture_type),
        .entryPoint = R3D_SHADER_VS_ENTRY_POINT_NAME,
        .bufferCount = 1,
        .buffers = &vertex_buffer_layout,
      };
      wgpu::FragmentState fragment_state = {
        .module = wgpuOverlayShaderModule(overlay_texture_type),
        .entryPoint = R3D_SHADER_FS_ENTRY_POINT_NAME,
        .targetCount = 1,
        .targets = &color_target_state
      };
      wgpu::RenderPipelineDescriptor desc = {
        .label = "Broccoli.Render.Overlay.Pipeline",
        .layout = render_pipeline.pipeline_layout,
        .vertex = vertex_state,
        .multisample = {.count=R3D_MSAA_SAMPLE_COUNT},
        .fragment = &fragment_state,
      };
      render_pipeline.pipeline = m_wgpu_device.CreateRenderPipeline(&desc);
    }

    // bind group 0:
    {
      auto entries = std::to_array({
        wgpu::BindGroupEntry{.binding=0, .buffer=m_wgpu_overlay_uniform_buffer, .offset=0, .size=sizeof(OverlayUniform)},
      });
      wgpu::BindGroupDescriptor desc = {
        .label = "Broccoli.Render.Overlay.BindGroup0",
        .layout = render_pipeline.bind_group_layouts[0],
        .entryCount = entries.size(),
        .entries = entries.data(),
      };
      render_pipeline.bind_groups_prefix[0] = m_wgpu_device.CreateBindGroup(&desc);
    }

    // all done:
    return render_pipeline;
  }
  void RenderManager::initOverlayRenderPipeline() {
    m_overlay_monochrome_render_pipeline = helpInitOverlayRenderPipeline(1);
    m_overlay_rgba_render_pipeline = helpInitOverlayRenderPipeline(4);
  }

  void RenderManager::initShadowMaps() {
    getShadowMaps(LightType::Directional).init(
      m_wgpu_device, 
      m_shadow_render_pipeline,
      static_cast<int32_t>(R3D_DIRECTIONAL_LIGHT_CAPACITY_LG2),
      static_cast<int32_t>(R3D_DIR_LIGHT_SHADOW_CASCADE_COUNT_LG2),
      static_cast<int32_t>(R3D_DIR_LIGHT_SHADOW_CASCADE_MAP_RESOLUTION_LG2), 
      "Broccoli.Render.ShadowMaps.DirLight"
    );
    getShadowMaps(LightType::Point).init(
      m_wgpu_device, 
      m_shadow_render_pipeline,
      static_cast<int32_t>(R3D_POINT_LIGHT_CAPACITY_LG2),
      static_cast<int32_t>(R3D_POINT_LIGHT_SHADOW_CASCADE_COUNT_LG2),
      static_cast<int32_t>(R3D_POINT_LIGHT_SHADOW_CASCADE_MAP_RESOLUTION_LG2), 
      "Broccoli.Render.ShadowMaps.PtLight"
    );
  }

  void RenderManager::initMaterialTable() {
    m_materials.reserve(R3D_MATERIAL_TABLE_INIT_CAPACITY);
  }
}
namespace broccoli {
  void RenderManager::resize(glm::i32vec2 framebuffer_size) {
    if (m_framebuffer_size == framebuffer_size) {
      return;
    }
    reinitColorTexture(framebuffer_size);
    reinitDepthStencilTexture(framebuffer_size);
    reinitOverlayTexture(framebuffer_size);
    m_framebuffer_size = framebuffer_size;
  }
}

namespace broccoli {
  GeometryBuilder RenderManager::createGeometryBuilder() {
    return {*this};
  }
  GeometryFactory RenderManager::createGeometryFactory() {
    return {*this};
  }
}
namespace broccoli {
  Material RenderManager::emplaceMaterial(MaterialTableEntry material) {
    CHECK(!m_materials_locked, "Cannot create new materials while material list is locked.");
    Material material_id{m_materials.size()};
    m_materials.emplace_back(std::move(material));
    return material_id;
  }
  Material RenderManager::createBlinnPhongMaterial(
    std::string name,
    std::variant<glm::dvec3, RenderTexture> albedo_map, 
    std::variant<glm::dvec3, RenderTexture> normal_map,
    double shininess
  ) {
    return emplaceMaterial(
      MaterialTableEntry::createBlinnPhongMaterial(
        *this,
        std::move(name),
        std::move(albedo_map),
        std::move(normal_map),
        shininess
      )
    );
  }
  Material RenderManager::createPbrMaterial(
    std::string name,
    std::variant<glm::dvec3, RenderTexture> albedo_map, 
    std::variant<glm::dvec3, RenderTexture> normal_map,
    std::variant<double, RenderTexture> metalness_map,
    std::variant<double, RenderTexture> roughness_map,
    glm::dvec3 fresnel0
  ) {
    return emplaceMaterial(
      MaterialTableEntry::createPbrMaterial(
        *this,
        std::move(name),
        std::move(albedo_map),
        std::move(normal_map),
        std::move(metalness_map),
        std::move(roughness_map),
        fresnel0
      )
    );
  }
  void RenderManager::lockMaterialsTable() {
    CHECK(!m_materials_locked, "Cannot lock materials while materials are already locked.");
    m_materials_locked = true;
  }
  void RenderManager::unlockMaterialsTable() {
    CHECK(m_materials_locked, "Cannot unlock materials unless materials are already locked.");
    m_materials_locked = false;
  }
  const MaterialTableEntry &RenderManager::getMaterialInfo(Material material) const {
    CHECK(m_materials_locked, "Cannot access material bind group unless materials are already locked.");
    return m_materials[material.value];
  }
}
namespace broccoli {
  glm::i32vec2 RenderManager::framebufferSize() const {
    return m_framebuffer_size;
  }
  const wgpu::Device &RenderManager::wgpuDevice() const {
    return m_wgpu_device;
  }
  const wgpu::Buffer &RenderManager::wgpuLightUniformBuffer() const {
    return m_wgpu_light_uniform_buffer;
  }
  const wgpu::Buffer &RenderManager::wgpuCameraUniformBuffer() const {
    return m_wgpu_camera_uniform_buffer;
  }
  const wgpu::Buffer &RenderManager::wgpuTransformUniformBuffer() const {
    return m_wgpu_transform_uniform_buffer;
  }
  wgpu::ShaderModule RenderManager::wgpuFinalShaderModule(uint32_t lighting_model_id) const {
    switch (static_cast<MaterialLightingModel>(lighting_model_id)) {
      case MaterialLightingModel::BlinnPhong: return m_wgpu_blinn_phong_shader_module;
      case MaterialLightingModel::PhysicallyBased: return m_wgpu_pbr_shader_module;
      default: PANIC("Invalid lighting model ID");
    }
  }
  wgpu::ShaderModule RenderManager::wgpuOverlayShaderModule(uint32_t sample_mode_id) const {
    switch (sample_mode_id) {
      case 1: return m_wgpu_overlay_monochrome_shader_module;
      case 4: return m_wgpu_overlay_rgba_shader_module;
      default: PANIC("Invalid sample mode ID");
    }
  }
  const FinalRenderPipeline &RenderManager::getFinalRenderPipeline(uint32_t lighting_model_id) const {
    switch (static_cast<MaterialLightingModel>(lighting_model_id)) {
      case MaterialLightingModel::BlinnPhong:
        return m_wgpu_blinn_phong_final_render_pipeline;
      case MaterialLightingModel::PhysicallyBased:
        return m_wgpu_pbr_final_render_pipeline;
      default:
        PANIC("Invalid lighting model ID");
    }
  }
  const ShadowRenderPipeline &RenderManager::getShadowRenderPipeline() const {
    return m_shadow_render_pipeline;
  }
  const OverlayRenderPipeline &RenderManager::getOverlayRenderPipeline(uint32_t sample_mode_id) const {
    switch (sample_mode_id) {
      case 1: return m_overlay_monochrome_render_pipeline;
      case 4: return m_overlay_rgba_render_pipeline;
      default: PANIC("Invalid sample mode ID");
    }
  }
  const wgpu::Texture RenderManager::wgpuOverlayTexture() const {
    return m_wgpu_final_overlay_texture;
  }
  const wgpu::Buffer &RenderManager::wgpuOverlayUniformBuffer() const {
    return m_wgpu_overlay_uniform_buffer;
  }
  const wgpu::Buffer &RenderManager::wgpuOverlayVertexBuffer() const {
    return m_wgpu_overlay_vertex_buffer;
  }
  wgpu::BindGroup RenderManager::wgpuOverlayBindGroup1() const {
    return m_wgpu_final_overlay_bind_group_1;
  }
  const wgpu::TextureView &RenderManager::wgpuRenderTargetColorTextureView() const {
    return m_wgpu_render_target_color_texture_view;
  }
  const wgpu::TextureView &RenderManager::wgpuRenderTargetDepthStencilTextureView() const {
    return m_wgpu_render_target_depth_stencil_texture_view;
  }
  RenderShadowMaps &RenderManager::getShadowMaps(LightType light_type) {
    return m_light_shadow_maps[light_type];
  }
  const RenderShadowMaps &RenderManager::getShadowMaps(LightType light_type) const {
    return m_light_shadow_maps[light_type];
  }
  const RenderTexture &RenderManager::rgbPalette() const {
    return m_rgb_palette;
  }
  const RenderTexture &RenderManager::monochromePalette() const {
    return m_monochrome_palette;
  }
  const std::vector<MaterialTableEntry> &RenderManager::materials() const {
    return m_materials;
  }
}
namespace broccoli {
  void RenderManager::debug_takeoutShadowMap(LightType light_type, int32_t light_index, int32_t cascade_index, std::function<void(FloatBitmap)> cb) {
    lazyInitDebugTakeoutBuffer();

    wgpu::CommandEncoderDescriptor command_encoder_descriptor = {
      .label = "Broccoli.Render.Debug.ShadowMapTakeoutCommandEncoder",
    };
    wgpu::CommandEncoder command_encoder = m_wgpu_device.CreateCommandEncoder(&command_encoder_descriptor);
    {
      const wgpu::ImageCopyTexture source = {
        .texture = getShadowMaps(light_type).texture(),
        .origin = wgpu::Origin3D {
          .x = 0,
          .y = 0,
          .z = static_cast<uint32_t>((light_index << r3d_shadow_cascade_map_count_lg2(light_type)) + cascade_index),
        },
      };
      const wgpu::ImageCopyBuffer destination = {
        .layout = {
          .bytesPerRow = static_cast<size_t>((1U << r3d_shadow_cascade_map_resolution_lg2(light_type)) * sizeof(float)),
          .rowsPerImage = static_cast<size_t>((1U << r3d_shadow_cascade_map_resolution_lg2(light_type))),
        },
        .buffer = m_wgpu_debug_takeout_buffer,
      };
      const wgpu::Extent3D copy_size = {
        .width = 1U << r3d_shadow_cascade_map_resolution_lg2(light_type),
        .height = 1U << r3d_shadow_cascade_map_resolution_lg2(light_type),
        .depthOrArrayLayers = 1,
      };
      command_encoder.CopyTextureToBuffer(&source, &destination, &copy_size);
    }
    wgpu::CommandBuffer command_buffer = command_encoder.Finish();
    m_wgpu_device.GetQueue().Submit(1, &command_buffer);

    struct MapUserData {
      LightType light_type;
      wgpu::Buffer mapped_buffer;
      std::function<void(FloatBitmap)> cb;
    };
    m_wgpu_debug_takeout_buffer.MapAsync(
      wgpu::MapMode::Read,
      0,
      static_cast<size_t>(1U << (2 * r3d_shadow_cascade_map_resolution_lg2(light_type))) * sizeof(float),
      [] (WGPUBufferMapAsyncStatus status, void *userdata) {
        CHECK(status == WGPUBufferMapAsyncStatus_Success, "Mapping takeout buffer failed when fetching shadow map.");
        MapUserData *data = reinterpret_cast<MapUserData*>(userdata);
        
        glm::i32vec3 output_dimension = {
          1 << r3d_shadow_cascade_map_resolution_lg2(data->light_type),
          1 << r3d_shadow_cascade_map_resolution_lg2(data->light_type),
          1,
        };
        FloatBitmap output{output_dimension};

        CHECK(data->mapped_buffer.GetMapState() == wgpu::BufferMapState::Mapped, "Expected buffer to be mapped.");
        const void *src = data->mapped_buffer.GetConstMappedRange();
        CHECK(src, "Expected mapped buffer range to be valid");
        memcpy(output.data(), src, output.dataSize());

        data->cb(std::move(output));

        data->mapped_buffer.Unmap();
        delete data;
      },
      new MapUserData{light_type, m_wgpu_debug_takeout_buffer, std::move(cb)}
    );
  }
}
namespace broccoli {
  RenderFrame RenderManager::frame(RenderTarget target) {
    resize(target.size);
    return {*this, target, m_final_overlay_bitmap};
  }
}

//
// Interface: RenderFrame:
//

namespace broccoli {
  RenderFrame::RenderFrame(RenderManager &manager, RenderTarget target, Bitmap &overlay_bitmap)
  : m_manager(manager),
    m_target(target),
    m_overlay_bitmap(overlay_bitmap),
    m_state(0)
  {}
}
namespace broccoli {
  void RenderFrame::clear(glm::dvec3 cc) {
    CHECK((m_state & static_cast<uint32_t>(State::CLEAR_COMPLETE)) == 0, "Clear can only be called once per-frame.");
    CHECK((m_state & static_cast<uint32_t>(State::DRAW_COMPLETE)) == 0, "Clear cannot be called after draw.");
    CHECK((m_state & static_cast<uint32_t>(State::OVERLAY_COMPLETE)) == 0, "Clear cannot be called after overlay.");
    drawClear(cc);
    m_state |= static_cast<uint32_t>(State::CLEAR_COMPLETE);
  }
  void RenderFrame::draw(RenderCamera camera, std::function<void(Renderer &)> draw_cb) {
    CHECK((m_state & static_cast<uint32_t>(State::DRAW_COMPLETE)) == 0, "Draw can only be called once per-frame.");
    CHECK((m_state & static_cast<uint32_t>(State::OVERLAY_COMPLETE)) == 0, "Draw cannot be called after overlay.");
    {
      Renderer renderer = {m_manager, m_target, camera};
      draw_cb(renderer);
      // allow Renderer::~Renderer to run, thereby applying all drawing.
    }
    m_state |= static_cast<uint32_t>(State::DRAW_COMPLETE);
  }
  void RenderFrame::overlay(std::function<void(OverlayRenderer&)> draw_cb) {
    m_overlay_bitmap.clear();
    {
      OverlayRenderer overlay_renderer{m_manager, m_target, m_overlay_bitmap};
      draw_cb(overlay_renderer);
      // allow OverlayRenderer::~OverlayRenderer to run, thereby applying all drawing.
    }
    m_state |= static_cast<uint32_t>(State::OVERLAY_COMPLETE);
  }
}
namespace broccoli {
  void RenderFrame::drawClear(glm::dvec3 cc) {
    wgpu::CommandEncoderDescriptor command_encoder_descriptor = {.label = "Broccoli.Render.Clear.CommandEncoder"};
    wgpu::CommandEncoder command_encoder = m_manager.wgpuDevice().CreateCommandEncoder(&command_encoder_descriptor);
    wgpu::RenderPassColorAttachment rp_color_attachment = {
      .view = m_manager.wgpuRenderTargetColorTextureView(),
      .resolveTarget = m_target.texture_view,
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue = wgpu::Color{.r=cc.x, .g=cc.y, .b=cc.z, .a=1.0},
    };
    wgpu::RenderPassDepthStencilAttachment rp_depth_attachment = {
      .view = m_manager.wgpuRenderTargetDepthStencilTextureView(),
      .depthLoadOp = wgpu::LoadOp::Clear,
      .depthStoreOp = wgpu::StoreOp::Store,
      .depthClearValue = 1.0f,
    };
    wgpu::RenderPassDescriptor rp_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Render.Clear.RenderPassEncoder",
      .colorAttachmentCount = 1,
      .colorAttachments = &rp_color_attachment,
      .depthStencilAttachment = &rp_depth_attachment,
    };
    wgpu::RenderPassEncoder rp = command_encoder.BeginRenderPass(&rp_descriptor);
    rp.End();
    wgpu::CommandBuffer command_buffer = command_encoder.Finish();
    m_manager.wgpuDevice().GetQueue().Submit(1, &command_buffer);
  }
}

//
// Interface: OverlayRenderer
//

namespace broccoli {
  OverlayRenderer::OverlayRenderer(RenderManager &manager, RenderTarget &target, Bitmap &bitmap)
  : m_manager(manager),
    m_target(target),
    m_bitmap(bitmap),
    m_texture_draw_requests()
  {
    initAllShadowMapViewResources();
  }
  OverlayRenderer::~OverlayRenderer() {
    // NOTE: requests to draw a texture from the GPU directly are processed AFTER drawing the bitmap overlay.
    // It is the user's responsibility to ensure anything in the bitmap obscured by a texture overlay can be safely 
    // ignored.
    drawOverlayBitmap();
    drawOverlayTextures();
  }
}
namespace broccoli {
  void OverlayRenderer::initAllShadowMapViewResources() {
    for (uint32_t i_light_type = 0; i_light_type < static_cast<uint32_t>(LightType::Metadata_Count); i_light_type++) {
      LightType light_type = static_cast<LightType>(i_light_type);
      size_t capacity = 1 << (r3d_light_capacity_lg2(light_type) + r3d_shadow_cascade_map_count_lg2(light_type));
      m_shadow_map_view_resources[light_type].reserve(capacity);
      for (int32_t i_light = 0; i_light < r3d_light_capacity(light_type); i_light++) {
        for (int32_t i_cascade = 0; i_cascade < r3d_shadow_cascade_map_count(light_type); i_cascade++) {
          wgpu::SamplerDescriptor sampler_desc = {
            .label = "Broccoli.Render.Overlay.ShadowViz.Sampler",
          };
          const wgpu::TextureView &view = m_manager.getShadowMaps(light_type).getReadView(i_light, i_cascade);
          wgpu::Sampler sampler = m_manager.wgpuDevice().CreateSampler(&sampler_desc);
          auto bind_group_entries = std::to_array({
            wgpu::BindGroupEntry{.binding=0, .textureView=view},
            wgpu::BindGroupEntry{.binding=1, .sampler=sampler},
          });
          wgpu::BindGroupDescriptor bind_group_desc = {
            .label = "Broccoli.Render.Overlay.ShadowViz.BindGroup1",
            .layout = m_manager.getOverlayRenderPipeline(1).textureSelectBindGroupLayout(),
            .entryCount = bind_group_entries.size(),
            .entries = bind_group_entries.data(),
          };
          wgpu::BindGroup bind_group = m_manager.wgpuDevice().CreateBindGroup(&bind_group_desc);
          WgpuShadowMapTextureOverlayResource resource = {
            .bind_group = bind_group,
            .sampler = sampler,
          };
          m_shadow_map_view_resources[light_type].emplace_back(std::move(resource));
        }
      }
    }
  }
}
namespace broccoli {
  RenderManager &OverlayRenderer::manager() {
    return m_manager;
  }
  Bitmap &OverlayRenderer::bitmap() {
    return m_bitmap;
  }
  void OverlayRenderer::drawShadowMapTexture(glm::i32vec2 vp_center, glm::i32vec2 vp_size, LightType light_type, int32_t light_idx, int32_t cascade_idx) {
    m_texture_draw_requests.emplace_back(
      OverlayTextureDrawRequestInfo {
        .type = OverlayTextureDrawRequestType::ShadowMap,
        .viewport_center = vp_center,
        .viewport_size = vp_size,
        .more = {.shadow_map = {light_type, light_idx, cascade_idx}},
      }  
    );
  }
}
namespace broccoli {
  void OverlayRenderer::drawOverlayBitmap() const {
    // copy overlay bitmap to texture
    {
      wgpu::ImageCopyTexture dst_desc = {
        .texture = m_manager.wgpuOverlayTexture(),
        .origin = wgpu::Origin3D{},
      };
      wgpu::TextureDataLayout dst_data_layout = {
        .bytesPerRow = static_cast<uint32_t>(m_bitmap.pitch()),
        .rowsPerImage = static_cast<uint32_t>(m_bitmap.rows()),
      };
      wgpu::Extent3D write_size = {
        .width = static_cast<uint32_t>(m_bitmap.dim().x),
        .height = static_cast<uint32_t>(m_bitmap.dim().y),
      };
      m_manager.wgpuDevice().GetQueue().WriteTexture(
        &dst_desc,
        m_bitmap.data(),
        m_bitmap.dataSize(),
        &dst_data_layout,
        &write_size
      );
    }

    // write screen size into uniform buffer:
    {
      OverlayUniform overlay_uniform_buffer = {
        .screen_size = m_manager.framebufferSize(),
        .rect_size = m_manager.framebufferSize(),
        .rect_center = m_manager.framebufferSize() / 2,
      };
      m_manager.wgpuDevice().GetQueue().WriteBuffer(
        m_manager.wgpuOverlayUniformBuffer(),
        0,
        &overlay_uniform_buffer,
        sizeof(overlay_uniform_buffer)
      );
    }

    // draw overlay texture using a loaded render pipeline
    {
      wgpu::CommandEncoderDescriptor ce_desc = {
        .label = "Broccoli.Render.Overlay.CommandEncoder",
      };
      wgpu::CommandEncoder command_encoder = m_manager.wgpuDevice().CreateCommandEncoder(&ce_desc);
      
      wgpu::RenderPassColorAttachment rp_color_attachment = {
        .view = m_manager.wgpuRenderTargetColorTextureView(),
        .resolveTarget = m_target.texture_view,
        .loadOp = wgpu::LoadOp::Load,
        .storeOp = wgpu::StoreOp::Store,
      };
      wgpu::RenderPassDescriptor rp_desc = {
        .label = "Broccoli.Render.Overlay.RenderPass",
        .colorAttachmentCount = 1,
        .colorAttachments = &rp_color_attachment,
      };
      wgpu::RenderPassEncoder rp_encoder = command_encoder.BeginRenderPass(&rp_desc);
      {
        rp_encoder.SetPipeline(m_manager.getOverlayRenderPipeline(4).pipeline);
        rp_encoder.SetVertexBuffer(0, m_manager.wgpuOverlayVertexBuffer());
        m_manager.getOverlayRenderPipeline(4).setBindGroups(rp_encoder, std::to_array({m_manager.wgpuOverlayBindGroup1()}));
        rp_encoder.Draw(6);
      }
      rp_encoder.End();
      wgpu::CommandBuffer command_buffer = command_encoder.Finish();
      m_manager.wgpuDevice().GetQueue().Submit(1, &command_buffer);
    }
  }
  void OverlayRenderer::drawOverlayTextures() const {
    for (const auto &request: m_texture_draw_requests) {
      switch (request.type) {
        case OverlayTextureDrawRequestType::ShadowMap: {
          const auto &more = request.more.shadow_map;
          drawShadowMapOverlayTexture(request.viewport_center, request.viewport_size, more.light_type, more.light_idx, more.cascade_idx);
        }
      }
    }
  }
  void OverlayRenderer::drawShadowMapOverlayTexture(glm::i32vec2 vp_center, glm::i32vec2 vp_size, LightType light_type, int32_t light_idx, int32_t cascade_idx) const {
    helpDrawOverlayTexture(
      vp_center,
      vp_size,
      1,
      [this, light_type, light_idx, cascade_idx] (wgpu::RenderPassEncoder &encoder, const OverlayRenderPipeline &rp) {
        const size_t offset = (light_idx << r3d_light_capacity_lg2(light_type)) + cascade_idx;
        const WgpuShadowMapTextureOverlayResource &res = m_shadow_map_view_resources[light_type][offset];
        rp.setBindGroups(encoder, std::to_array({res.bind_group}));
      }
    );
  }
  void OverlayRenderer::helpDrawOverlayTexture(glm::i32vec2 vp_center, glm::i32vec2 vp_size, uint32_t sample_mode_id, std::function<void(wgpu::RenderPassEncoder&, OverlayRenderPipeline const&)> cb) const {
    wgpu::CommandEncoderDescriptor ce_desc = {
      .label = "Broccoli.Render.Overlay.CommandEncoder",
    };
    wgpu::CommandEncoder command_encoder = m_manager.wgpuDevice().CreateCommandEncoder(&ce_desc);
    
    wgpu::RenderPassColorAttachment rp_color_attachment = {
      .view = m_manager.wgpuRenderTargetColorTextureView(),
      .resolveTarget = m_target.texture_view,
      .loadOp = wgpu::LoadOp::Load,
      .storeOp = wgpu::StoreOp::Store,
    };
    wgpu::RenderPassDescriptor rp_desc = {
      .label = "Broccoli.Render.Overlay.RenderPass",
      .colorAttachmentCount = 1,
      .colorAttachments = &rp_color_attachment,
    };
    wgpu::RenderPassEncoder rp_encoder = command_encoder.BeginRenderPass(&rp_desc);
    {
      const OverlayRenderPipeline &rp = m_manager.getOverlayRenderPipeline(sample_mode_id);
      glm::i32vec2 min_vp_xy = vp_center - vp_size / 2;
      rp_encoder.SetViewport(min_vp_xy.x, min_vp_xy.y, vp_size.x, vp_size.y, 0.0, 1.0);
      rp_encoder.SetPipeline(rp.pipeline);
      rp_encoder.SetVertexBuffer(0, m_manager.wgpuOverlayVertexBuffer());
      cb(rp_encoder, rp);
      rp_encoder.Draw(6);
    }
    rp_encoder.End();
    wgpu::CommandBuffer command_buffer = command_encoder.Finish();

    m_manager.wgpuDevice().GetQueue().Submit(1, &command_buffer);
  }
}

//
// Interface: Renderer:
//

namespace broccoli {
  Renderer::Renderer(RenderManager &manager, RenderTarget target, RenderCamera camera)
  : m_manager(manager),
    m_target(target),
    m_camera(camera),
    m_mesh_instance_lists(),
    m_directional_light_vec(),
    m_point_light_vec()
  {
    manager.lockMaterialsTable();
    m_directional_light_vec.reserve(R3D_DIRECTIONAL_LIGHT_CAPACITY);
    m_point_light_vec.reserve(R3D_POINT_LIGHT_CAPACITY);
    m_mesh_instance_lists.resize(manager.materials().size());
  }
  Renderer::~Renderer() {
    sendCameraData(m_camera, m_target);
    sendLightData(m_directional_light_vec, m_point_light_vec);
    drawShadowMaps(m_camera, m_target, m_directional_light_vec, m_mesh_instance_lists);
    drawMeshInstanceListVec(std::move(m_mesh_instance_lists));
    m_manager.unlockMaterialsTable();
  }
}
namespace broccoli {
  void Renderer::addMesh(Material material_id, const Geometry &geometry) {
    addMesh(material_id, geometry, glm::mat4x4{1.0f});
  }
  void Renderer::addMesh(Material material_id, const Geometry &geometry, glm::mat4x4 transform) {
    addMesh(material_id, std::move(geometry), std::span<glm::mat4x4>{&transform, 1});
  }
  void Renderer::addMesh(Material material_id, const Geometry &geometry, std::span<glm::mat4x4> transforms_span) {
    std::vector<glm::mat4x4> transforms(transforms_span.begin(), transforms_span.end());
    addMesh(material_id, std::move(geometry), std::move(transforms));
  }
  void Renderer::addMesh(Material material_id, const Geometry &geometry, std::vector<glm::mat4x4> transforms) {
    MeshInstanceList mil = {geometry, std::move(transforms)};
    m_mesh_instance_lists[material_id.value].emplace_back(std::move(mil));
  }
  void Renderer::addDirectionalLight(glm::vec3 direction, float intensity, glm::vec3 color) {
    direction = glm::normalize(direction);
    color = glm::normalize(color) * intensity;
    DirectionalLight directional_list{direction, color};
    m_directional_light_vec.emplace_back(directional_list);
  }
  void Renderer::addPointLight(glm::vec3 position, float intensity, glm::vec3 color) {
    color = glm::normalize(color) * intensity;
    PointLight point_light{position, color};
    m_point_light_vec.emplace_back(point_light);
  }
}
namespace broccoli {
  void Renderer::sendCameraData(RenderCamera camera, RenderTarget target) {
    const float fovy_rad = (camera.fovyDeg() / 360.0f) * (2.0f * static_cast<float>(M_PI));
    const float aspect = target.size.x / static_cast<float>(target.size.y);
    const CameraUniform buf = {
      .view_matrix = camera.viewMatrix(),
      .world_position = glm::vec4{camera.position(), 1.0f},
      .camera_cot_half_fovy = 1.0f / std::tanf(fovy_rad / 2.0f),
      .camera_aspect_inv = 1.0f / aspect,
      .camera_zmin = 000.050f,
      .camera_zmax = 100.000f,
      .camera_logarithmic_z_scale = 1.0,
      .hdr_exposure_bias = camera.exposureBias(),
    };
    auto queue = m_manager.wgpuDevice().GetQueue();
    queue.WriteBuffer(m_manager.wgpuCameraUniformBuffer(), 0, &buf, sizeof(CameraUniform));
  }
  void Renderer::sendLightData(
    std::vector<DirectionalLight> const &directional_light_vec, 
    std::vector<PointLight> const &point_light_vec
  ) {
    CHECK(directional_light_vec.size() < R3D_DIRECTIONAL_LIGHT_CAPACITY, "Direction light overflow");
    CHECK(point_light_vec.size() < R3D_POINT_LIGHT_CAPACITY, "Point light overflow");
    
    // Uploading the LightUniform UBO
    LightUniform buf = {
      .directional_light_count = static_cast<uint32_t>(directional_light_vec.size()),
      .point_light_count = static_cast<uint32_t>(point_light_vec.size()),
      .ambient_glow = R3D_DEFAULT_AMBIENT_GLOW,
    };
    for (size_t i = 0; i < directional_light_vec.size(); i++) {
      buf.directional_light_color_array[i] = glm::vec4{directional_light_vec[i].color, 0.0f};
      buf.directional_light_dir_array[i] = glm::vec4{directional_light_vec[i].direction, 0.0f};
    }
    for (size_t i = 0; i < point_light_vec.size(); i++) {
      buf.point_light_color_array[i] = glm::vec4{point_light_vec[i].color, 0.0f};
      buf.point_light_pos_array[i] = glm::vec4{point_light_vec[i].position, 0.0f};
    }
    auto queue = m_manager.wgpuDevice().GetQueue();
    queue.WriteBuffer(m_manager.wgpuLightUniformBuffer(), 0, &buf, sizeof(LightUniform));
  }
  void Renderer::drawShadowMaps(RenderCamera camera, RenderTarget target, std::vector<DirectionalLight> const &light_vec, const std::vector<std::vector<MeshInstanceList>> &mesh_instance_lists) {
    // Resources on cascaded shadow maps:
    // - https://ogldev.org/www/tutorial49/tutorial49.html
    // - https://www.youtube.com/watch?v=u0pk1LyLKYQ

    for (int32_t light_idx = 0; light_idx < light_vec.size(); light_idx++) {
      const auto &light = light_vec[light_idx];

      // Getting perpendicular vectors efficiently:
      glm::dmat3x3 light_transform_matrix = computeDirLightTransform(light.direction);
      glm::dmat3x3 light_view_matrix = glm::inverse(light_transform_matrix);

      // Rendering each cascade:
      // FIXME: we currently need to use a separate render pipeline for each mesh drawn because we need to re-upload the
      // transforms uniform buffer in between. We also need a separate render pass before these to clear the shadow map
      // so that each per-mesh-instance-list render-pass can load the depth texture.
      // In past renderers, I have used a large uniform buffer containing all transform matrices coupled with a layer of
      // indirection and dynamic offsets to circumvent this.
      // Consider a similar solution.
      for (int32_t i = 0; i < R3D_DIR_LIGHT_SHADOW_CASCADE_COUNT; i++) {
        glm::mat4x4 cascade_projection_matrix = computeDirLightCascadeProjectionMatrix(camera, target, light_view_matrix, i);
        glm::mat4x4 proj_view_matrix = cascade_projection_matrix * glm::mat4x4{light_view_matrix};
        const RenderShadowMaps &shadow_maps = m_manager.getShadowMaps(LightType::Directional);
        drawShadowMap(proj_view_matrix, shadow_maps, light_idx, i, mesh_instance_lists);
      }
    }
  }
  void Renderer::drawShadowMap(glm::mat4x4 proj_view_matrix, const RenderShadowMaps &shadow_maps, int32_t light_idx, int32_t cascade_idx, const std::vector<std::vector<MeshInstanceList>> &mesh_instance_list_vec) {
    const wgpu::Queue queue = m_manager.wgpuDevice().GetQueue();
    clearShadowMap(shadow_maps, light_idx, cascade_idx);
    drawShadowMapMeshInstanceListVec(proj_view_matrix, shadow_maps, light_idx, cascade_idx, mesh_instance_list_vec);
  }
  void Renderer::clearShadowMap(const RenderShadowMaps &shadow_maps, int32_t light_idx, int32_t cascade_idx) {
    const auto &view = shadow_maps.getWriteView(light_idx, cascade_idx);
    auto queue = m_manager.wgpuDevice().GetQueue();
    
    wgpu::CommandEncoderDescriptor command_encoder_descriptor = {.label="Broccoli.Render.ShadowMap.CommandEncoder"};
    wgpu::CommandEncoder command_encoder = m_manager.wgpuDevice().CreateCommandEncoder(&command_encoder_descriptor);
    auto render_pass_depth_stencil_attachment = wgpu::RenderPassDepthStencilAttachment {
      .view = view,
      .depthLoadOp = wgpu::LoadOp::Clear,
      .depthStoreOp = wgpu::StoreOp::Store,
      .depthClearValue = 1.0,
    };
    auto render_pass_encoder_descriptor = wgpu::RenderPassDescriptor {
      .label = "Broccoli.Render.ShadowMap.RenderPass",
      .colorAttachmentCount = 0,
      .depthStencilAttachment = &render_pass_depth_stencil_attachment,
    };
    wgpu::RenderPassEncoder rp_encoder = command_encoder.BeginRenderPass(&render_pass_encoder_descriptor);
    rp_encoder.End();

    wgpu::CommandBuffer command_buffer = command_encoder.Finish();
    queue.Submit(1, &command_buffer);
  }
  void Renderer::drawShadowMapMeshInstanceListVec(glm::mat4x4 proj_view_matrix, const RenderShadowMaps &shadow_maps, int32_t light_idx, int32_t cascade_idx, const std::vector<std::vector<MeshInstanceList>> &mesh_instance_list_vec) {
    for (size_t material_idx = 0; material_idx < mesh_instance_list_vec.size(); material_idx++) {
      Material material{material_idx};
      if (m_manager.getMaterialInfo(material).isShadowCasting()) {
        for (auto const &mesh_instance_list: mesh_instance_list_vec[material_idx]) {
          drawShadowMapMeshInstanceList(proj_view_matrix, shadow_maps, light_idx, cascade_idx, mesh_instance_list);
        }
      }
    }
  }
  void Renderer::drawShadowMapMeshInstanceList(glm::mat4x4 proj_view_matrix, const RenderShadowMaps &shadow_maps, int32_t light_idx, int32_t cascade_idx, const MeshInstanceList &mesh_instance_list) {
    if (mesh_instance_list.instance_list.empty()) {
      return;
    }
  
    const auto &render_pipeline = m_manager.getShadowRenderPipeline();
    const auto &mesh = mesh_instance_list.mesh;
    const auto &transform_list = mesh_instance_list.instance_list;
    const auto &ubo = shadow_maps.getShadowMapUbo(light_idx, cascade_idx);
    const auto &view = shadow_maps.getWriteView(light_idx, cascade_idx);
    auto queue = m_manager.wgpuDevice().GetQueue();
    
    if (true) {
      auto uniform_buffer_size = transform_list.size() * sizeof(glm::mat4x4);
      queue.WriteBuffer(m_manager.wgpuTransformUniformBuffer(), 0, transform_list.data(), uniform_buffer_size);
    }
    
    ShadowUniform freshest_ubo = {.proj_view_matrix=proj_view_matrix};
    if (freshest_ubo != ubo.state) {
      queue.WriteBuffer(ubo.buffer, 0, &freshest_ubo, sizeof(freshest_ubo));
    }

    wgpu::CommandEncoderDescriptor command_encoder_descriptor = {.label="Broccoli.Render.ShadowMap.CommandEncoder"};
    wgpu::CommandEncoder command_encoder = m_manager.wgpuDevice().CreateCommandEncoder(&command_encoder_descriptor);
    auto render_pass_depth_stencil_attachment = wgpu::RenderPassDepthStencilAttachment {
      .view = view,
      .depthLoadOp = wgpu::LoadOp::Load,
      .depthStoreOp = wgpu::StoreOp::Store,
    };
    auto render_pass_encoder_descriptor = wgpu::RenderPassDescriptor {
      .label = "Broccoli.Render.ShadowMap.RenderPass",
      .colorAttachmentCount = 0,
      .depthStencilAttachment = &render_pass_depth_stencil_attachment,
    };
    wgpu::RenderPassEncoder rp_encoder = command_encoder.BeginRenderPass(&render_pass_encoder_descriptor);
    {
      rp_encoder.SetPipeline(render_pipeline.pipeline);
      render_pipeline.setBindGroups(rp_encoder, std::to_array({ubo.bind_group}));
      rp_encoder.SetIndexBuffer(mesh.idx_buffer, wgpu::IndexFormat::Uint32);
      rp_encoder.SetVertexBuffer(0, mesh.vtx_buffer);
      rp_encoder.DrawIndexed(mesh.idx_count, static_cast<uint32_t>(transform_list.size()));
    }
    rp_encoder.End();

    wgpu::CommandBuffer command_buffer = command_encoder.Finish();
    queue.Submit(1, &command_buffer);
  }
  void Renderer::drawMeshInstanceListVec(std::vector<std::vector<MeshInstanceList>> mesh_instance_list_vec) {
    for (size_t material_idx = 0; material_idx < mesh_instance_list_vec.size(); material_idx++) {
      Material material{material_idx};
      for (auto const &mesh_instance_list: mesh_instance_list_vec[material_idx]) {
        drawMeshInstanceList(material, mesh_instance_list);
      }
    }
  }
  void Renderer::drawMeshInstanceList(Material material, const MeshInstanceList &mesh_instance_list) {
    if (mesh_instance_list.instance_list.empty()) {
      return;
    }
    auto const &mesh = mesh_instance_list.mesh;
    auto const &transform_list = mesh_instance_list.instance_list;
    auto const &material_info = m_manager.getMaterialInfo(material);
    auto queue = m_manager.wgpuDevice().GetQueue();
    auto uniform_buffer_size = transform_list.size() * sizeof(glm::mat4x4);
    queue.WriteBuffer(m_manager.wgpuTransformUniformBuffer(), 0, transform_list.data(), uniform_buffer_size);
    wgpu::CommandEncoderDescriptor command_encoder_descriptor = {.label="Broccoli.Render.Draw.CommandEncoder"};
    wgpu::CommandEncoder command_encoder = m_manager.wgpuDevice().CreateCommandEncoder(&command_encoder_descriptor);
    wgpu::RenderPassColorAttachment rp_color_attachment = {
      .view = m_manager.wgpuRenderTargetColorTextureView(),
      .resolveTarget = m_target.texture_view,
      .loadOp = wgpu::LoadOp::Load,
      .storeOp = wgpu::StoreOp::Store,
    };
    wgpu::RenderPassDepthStencilAttachment rp_depth_attachment = {
      .view = m_manager.wgpuRenderTargetDepthStencilTextureView(),
      .depthLoadOp = wgpu::LoadOp::Load,
      .depthStoreOp = wgpu::StoreOp::Store,
    };
    wgpu::RenderPassDescriptor rp_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Render.Final.RenderPassEncoder",
      .colorAttachmentCount = 1,
      .colorAttachments = &rp_color_attachment,
      .depthStencilAttachment = &rp_depth_attachment,
    };
    wgpu::RenderPassEncoder rp_encoder = command_encoder.BeginRenderPass(&rp_descriptor);
    {
      const auto &render_pipeline = m_manager.getFinalRenderPipeline(material_info.lightingModelID());
      rp_encoder.SetPipeline(render_pipeline.pipeline);
      render_pipeline.setBindGroups(rp_encoder, std::to_array({material_info.wgpuMaterialBindGroup()}));
      rp_encoder.SetIndexBuffer(mesh.idx_buffer, wgpu::IndexFormat::Uint32);
      rp_encoder.SetVertexBuffer(0, mesh.vtx_buffer);
      rp_encoder.DrawIndexed(mesh.idx_count, static_cast<uint32_t>(transform_list.size()));
    }
    rp_encoder.End();
    wgpu::CommandBuffer command_buffer = command_encoder.Finish();
    queue.Submit(1, &command_buffer);
  }
  glm::mat4x4 Renderer::computeDirLightCascadeProjectionMatrix(RenderCamera camera, RenderTarget target, glm::dmat3x3 inv_light_transform, size_t cascade_index) {
    double min_distance = minDirLightCascadeDistance(cascade_index);
    double max_distance = maxDirLightCascadeDistance(cascade_index);
    glm::dmat4x3 min_world_section = computeFrustumSection(camera, target, min_distance);
    glm::dmat4x3 max_world_section = computeFrustumSection(camera, target, max_distance);
    glm::dmat4x2 min_light_section = projectFrustumSection(min_world_section, inv_light_transform);
    glm::dmat4x2 max_light_section = projectFrustumSection(max_world_section, inv_light_transform);
    glm::dvec2 min_proj_xy = glm::floor(
      glm::min(
        glm::min(min_light_section[0], min_light_section[1], min_light_section[2], min_light_section[3]),
        glm::min(max_light_section[0], max_light_section[1], max_light_section[2], max_light_section[3])
      )
    );
    glm::dvec2 max_proj_xy = glm::ceil(
      glm::max(
        glm::max(min_light_section[0], min_light_section[1], min_light_section[2], min_light_section[3]),
        glm::max(max_light_section[0], max_light_section[1], max_light_section[2], max_light_section[3])
      )
    );
    glm::dvec2 proj_cascade_size = max_proj_xy - min_proj_xy;
    glm::dvec2 max_cascade_size{1.0 + glm::ceil(glm::length(max_world_section[2] - min_world_section[0]))};
    glm::dvec2 padding = (max_cascade_size - proj_cascade_size) / 2.0;
    glm::dvec2 min_xy = min_proj_xy - glm::dvec2{padding};
    glm::dvec2 max_xy = max_proj_xy + glm::dvec2{padding};
    CHECK(proj_cascade_size.x <= max_cascade_size.x && proj_cascade_size.y <= max_cascade_size.y, "Bad max cascade size.");
    CHECK(max_xy - min_xy == max_cascade_size, "Expected orthographic projection to be fixed size.");
    return glm::ortho(min_xy.x, max_xy.x, min_xy.y, max_xy.y, -R3D_DIR_LIGHT_SHADOW_RADIUS, +R3D_DIR_LIGHT_SHADOW_RADIUS);
  }
  glm::dmat4x3 Renderer::computeFrustumSection(RenderCamera camera, RenderTarget target, double distance) {
    double aspect = target.size.x / static_cast<double>(target.size.y);
    double fovy_rad = (static_cast<double>(camera.fovyDeg()) / 360.0) * (2 * M_PI);
    glm::dvec3 right = camera.transformMatrix()[0];
    glm::dvec3 up = camera.transformMatrix()[1];
    glm::dvec3 forward = -glm::dvec3{camera.transformMatrix()[2]};
    double up_offset = distance * std::tan(fovy_rad / 2.0);
    double right_offset = aspect * up_offset;
    return {
      forward * distance + right * +right_offset + up * +up_offset,
      forward * distance + right * -right_offset + up * +up_offset,
      forward * distance + right * -right_offset + up * -up_offset,
      forward * distance + right * +right_offset + up * -up_offset,
    };
  }
  glm::dmat3x3 Renderer::computeDirLightTransform(glm::dvec3 forward) {
    glm::dvec3 guess_up1{0.0, 1.0, 0.0};
    glm::dvec3 guess_up2{0.0, 0.0, 1.0};
    glm::dvec3 guess_right1 = glm::cross(forward, guess_up1);
    if (glm::dot(guess_right1, guess_right1) > 1e-5) [[likely]] {
      // forward and guess_up1 are not parallel => use guess_up1 as up.
      glm::dvec3 right1 = glm::normalize(guess_right1);
      glm::dvec3 up1 = glm::cross(right1, forward);
      return {right1, up1, -forward};
    } else {
      // forward and guess_up1 are parallel => use guess_up2 as up.
      glm::dvec3 guess_right2 = glm::cross(forward, guess_up2);
      glm::dvec3 right2 = glm::normalize(guess_right2);
      glm::dvec3 up2 = glm::cross(right2, forward);
      return {right2, up2, -forward};
    }
  }
  glm::dmat4x2 Renderer::projectFrustumSection(glm::dmat4x3 world_section, glm::dmat3x3 transform) {
    // We want to apply the transform to each column in world_section.
    // This is equivalent to a matrix multiplication: you apply the left matrix to each basis vector in the right
    // matrix.
    glm::dmat4x3 transformed_section = transform * world_section;

    // Next, we want to discard the Z components to project along that axis onto the XY plane.
    return {
      transformed_section[0],
      transformed_section[1],
      transformed_section[2],
      transformed_section[3],
    };
  }
}

//
// Interface: GeometryFactory
//

namespace broccoli {
  GeometryFactory::GeometryFactory(RenderManager &manager)
  : m_manager(manager)
  {}
}
namespace broccoli {
  Geometry GeometryFactory::createCuboid(glm::dvec3 dimensions) {
    GeometryBuilder mb = m_manager.createGeometryBuilder();
    
    auto hd = dimensions * 0.5;

    // -X face, +X face:
    mb.quad(
      {.offset=glm::dvec3{-hd.x, -hd.y, -hd.z}},
      {.offset=glm::dvec3{-hd.x, -hd.y, +hd.z}},
      {.offset=glm::dvec3{-hd.x, +hd.y, +hd.z}},
      {.offset=glm::dvec3{-hd.x, +hd.y, -hd.z}}
    );
    mb.quad(
      {.offset=glm::dvec3{+hd.x, -hd.y, +hd.z}},
      {.offset=glm::dvec3{+hd.x, -hd.y, -hd.z}},
      {.offset=glm::dvec3{+hd.x, +hd.y, -hd.z}},
      {.offset=glm::dvec3{+hd.x, +hd.y, +hd.z}}
    );

    // -Y face, +Y face:
    mb.quad(
      {.offset=glm::dvec3{+hd.x, -hd.y, +hd.z}},
      {.offset=glm::dvec3{-hd.x, -hd.y, +hd.z}},
      {.offset=glm::dvec3{-hd.x, -hd.y, -hd.z}},
      {.offset=glm::dvec3{+hd.x, -hd.y, -hd.z}}
    );
    mb.quad(
      {.offset=glm::dvec3{-hd.x, +hd.y, -hd.z}},
      {.offset=glm::dvec3{-hd.x, +hd.y, +hd.z}},
      {.offset=glm::dvec3{+hd.x, +hd.y, +hd.z}},
      {.offset=glm::dvec3{+hd.x, +hd.y, -hd.z}}
    );

    // -Z face, +Z face:
    mb.quad(
      {.offset=glm::dvec3{-hd.x, -hd.y, -hd.z}},
      {.offset=glm::dvec3{-hd.x, +hd.y, -hd.z}},
      {.offset=glm::dvec3{+hd.x, +hd.y, -hd.z}},
      {.offset=glm::dvec3{+hd.x, -hd.y, -hd.z}}
    );
    mb.quad(
      {.offset=glm::dvec3{+hd.x, -hd.y, +hd.z}},
      {.offset=glm::dvec3{+hd.x, +hd.y, +hd.z}},
      {.offset=glm::dvec3{-hd.x, +hd.y, +hd.z}},
      {.offset=glm::dvec3{-hd.x, -hd.y, +hd.z}}
    );

    return GeometryBuilder::finish(std::move(mb));
  }
}

//
// Interface: GeometryBuilder
//

namespace broccoli {
  GeometryBuilder::GeometryBuilder(RenderManager &manager)
  : m_manager(manager),
    m_vtx_compression_map(),
    m_vtx_buf(),
    m_idx_buf()
  {
    m_vtx_compression_map.reserve(R3D_VERTEX_BUFFER_CAPACITY);
    m_vtx_buf.reserve(R3D_VERTEX_BUFFER_CAPACITY);
    m_idx_buf.reserve(R3D_INDEX_BUFFER_CAPACITY);
  }
  void GeometryBuilder::triangle(Vtx v1, Vtx v2, Vtx v3, bool double_faced) {
    singleFaceTriangle(v1, v2, v3);
    if (double_faced) {
      singleFaceTriangle(v1, v3, v2);
    }
  }
  void GeometryBuilder::quad(Vtx v1, Vtx v2, Vtx v3, Vtx v4, bool double_faced) {
    triangle(v1, v2, v3, double_faced);
    triangle(v1, v3, v4, double_faced);
  }
}
namespace broccoli {
  void GeometryBuilder::singleFaceTriangle(Vtx v1, Vtx v2, Vtx v3) {
    auto e1 = v2.offset - v1.offset;
    auto e2 = v3.offset - v1.offset;
    auto duv1 = v2.uv - v1.uv;
    auto duv2 = v3.uv - v1.uv;
    auto normal = glm::cross(e1, e2);
    auto tangent = duv2.y * e1 - duv1.y * e2;
    if (glm::dot(normal, normal) <= 1e-9) {
      // Degenerate triangle: normal of length close to 0 means that angle of separation between edges is close to 0.
      return;
    }
    if (glm::dot(tangent, tangent) > 1e-5) {
      // Tangent is usually 0.0 when UVs for multiple vertices' UVs coincide.
      tangent = glm::normalize(tangent);
    }
    auto unorm_normal = packNormal(normal);
    auto unorm_tangent = packNormal(tangent);
    auto iv1 = vertex(v1.offset, v1.uv, unorm_normal, unorm_tangent);
    auto iv2 = vertex(v2.offset, v2.uv, unorm_normal, unorm_tangent);
    auto iv3 = vertex(v3.offset, v3.uv, unorm_normal, unorm_tangent);
    m_idx_buf.push_back(iv1);
    m_idx_buf.push_back(iv2);
    m_idx_buf.push_back(iv3);
  }
  Geometry GeometryBuilder::finish(GeometryBuilder &&mb) {
    wgpu::BufferDescriptor vtx_buf_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Mesh.VertexBuffer",
      .usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst,
      .size = mb.m_vtx_buf.size() * sizeof(Vertex),
    };
    wgpu::BufferDescriptor idx_buf_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Mesh.IndexBuffer",
      .usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst,
      .size = mb.m_idx_buf.size() * sizeof(uint32_t),
    };
    Geometry mesh = {
      .mesh_type = GeometryType::Static,
      .vtx_buffer = mb.m_manager.wgpuDevice().CreateBuffer(&vtx_buf_descriptor),
      .idx_buffer = mb.m_manager.wgpuDevice().CreateBuffer(&idx_buf_descriptor),
      .vtx_count = static_cast<uint32_t>(mb.m_vtx_buf.size()),
      .idx_count = static_cast<uint32_t>(mb.m_idx_buf.size()),
    };
    wgpu::Queue queue = mb.m_manager.wgpuDevice().GetQueue();
    auto vtx_buf_size = mb.m_vtx_buf.size() * sizeof(Vertex);
    auto idx_buf_size = mb.m_idx_buf.size() * sizeof(uint32_t);
    queue.WriteBuffer(mesh.vtx_buffer, 0, mb.m_vtx_buf.data(), vtx_buf_size);
    queue.WriteBuffer(mesh.idx_buffer, 0, mb.m_idx_buf.data(), idx_buf_size);
    return mesh;
  }
  uint32_t GeometryBuilder::vertex(glm::dvec3 offset, glm::dvec2 uv, uint32_t packed_normal, uint32_t packed_tangent) {
    auto packed_offset = packOffset(offset);
    auto packed_uv = packUv(uv);
    broccoli::Vertex packed_vertex = {
      .offset=packed_offset, 
      .normal=packed_normal, 
      .tangent=packed_tangent, 
      .uv=packed_uv,
    };
    auto packed_vertex_it = m_vtx_compression_map.find(packed_vertex);
    if (packed_vertex_it != m_vtx_compression_map.end()) {
      return packed_vertex_it->second;
    }
    CHECK(
      m_vtx_compression_map.size() == m_vtx_buf.size(),
      "Expected vertex compression map and vertex buffer to be parallel."
    );
    CHECK(
      m_vtx_buf.size() < UINT32_MAX,
      "Overflow of vertex indices detected: too many vertices inserted."
    );
    auto new_vtx_idx = static_cast<uint32_t>(m_vtx_compression_map.size());
    m_vtx_compression_map.insert({packed_vertex, new_vtx_idx});
    m_vtx_buf.emplace_back(packed_vertex);
    return new_vtx_idx;
  }
  glm::ivec3 GeometryBuilder::packOffset(glm::dvec3 pos) {
    // Packing into 12.20 signed fix-point per-component.
    // This packing is accurate to at least 6 decimal places.
    // This degree of precision is overkill, but this is the point: it's something the user should never need to worry
    // about.
    constexpr auto lo = static_cast<double>(INT32_MIN) / 1048576.0;
    constexpr auto hi = static_cast<double>(INT32_MAX) / 1048576.0;
    DEBUG_CHECK(lo <= pos.x && pos.x <= hi, "Vertex position X does not fit in fixed-point scheme.");
    DEBUG_CHECK(lo <= pos.y && pos.y <= hi, "Vertex position Y does not fit in fixed-point scheme.");
    DEBUG_CHECK(lo <= pos.z && pos.z <= hi, "Vertex position Z does not fit in fixed-point scheme.");
    glm::dvec3 pos_fixpt = pos;
    pos_fixpt = glm::round(pos_fixpt * 1048576.0);
    pos_fixpt = glm::clamp(pos_fixpt, static_cast<double>(INT32_MIN), static_cast<double>(INT32_MAX));
    return {
      static_cast<int32_t>(pos_fixpt.x),
      static_cast<int32_t>(pos_fixpt.y),
      static_cast<int32_t>(pos_fixpt.z),
    };
  }
  glm::tvec2<uint16_t> GeometryBuilder::packUv(glm::dvec2 uv) {
    // Packing into 2xu16 normalized.
    auto uv_fixpt = glm::round(glm::clamp(uv, 0.0, 1.0) * 65535.0);
    return {
      static_cast<uint16_t>(uv_fixpt.x),
      static_cast<uint16_t>(uv_fixpt.y),
    };
  }
  uint32_t GeometryBuilder::packNormal(glm::dvec3 normal) {
    auto unsigned_normal = (normal + 1.0) * 0.5;
    auto packed_normal = glm::packUnorm3x10_1x2({unsigned_normal, 1.0});
    return packed_normal;
  }
}

//
// Material:
//

namespace broccoli {
  MaterialTableEntry::MaterialTableEntry(
    RenderManager &render_manager, 
    std::string name,
    std::variant<glm::dvec3, RenderTexture> albedo_map, 
    std::variant<glm::dvec3, RenderTexture> normal_map,
    std::variant<double, RenderTexture> metalness_map,
    std::variant<double, RenderTexture> roughness_map,
    glm::dvec3 pbr_fresnel0,
    double blinn_phong_shininess,
    MaterialLightingModel lighting_model,
    bool is_shadow_casting
  )
  : m_render_manager(render_manager),
    m_wgpu_material_buffer(nullptr),
    m_wgpu_material_bind_group(nullptr),
    m_name("Broccoli.Material." + name),
    m_wgpu_material_buffer_label(m_name + ".MaterialUbo"),
    m_wgpu_material_bind_group_label(m_name + ".BindGroup1"),
    m_is_shadow_casting(is_shadow_casting)
  {
    init(
      std::move(albedo_map),
      std::move(normal_map),
      std::move(metalness_map),
      std::move(roughness_map),
      pbr_fresnel0,
      blinn_phong_shininess,
      lighting_model
    );
  }
  uint32_t MaterialTableEntry::lightingModelID() const {
    return static_cast<uint32_t>(m_lighting_model);
  }
  const wgpu::BindGroup &MaterialTableEntry::wgpuMaterialBindGroup() const {
    return m_wgpu_material_bind_group;
  }
  bool MaterialTableEntry::isShadowCasting() const {
    return m_is_shadow_casting;
  }
}
namespace broccoli {
  void MaterialTableEntry::init(
    std::variant<glm::dvec3, RenderTexture> const &albedo_map, 
    std::variant<glm::dvec3, RenderTexture> const &normal_map,
    std::variant<double, RenderTexture> const &metalness_map,
    std::variant<double, RenderTexture> const &roughness_map,
    glm::dvec3 pbr_fresnel0,
    double blinn_phong_shininess,
    MaterialLightingModel lighting_model
  ) {
    auto &manager = m_render_manager;
    auto &device = manager.wgpuDevice();
    auto lighting_model_id = static_cast<uint32_t>(lighting_model);
    
    bool all_constants = 
      std::holds_alternative<glm::dvec3>(albedo_map) &&
      std::holds_alternative<glm::dvec3>(normal_map) &&
      std::holds_alternative<double>(metalness_map) &&
      std::holds_alternative<double>(roughness_map);
    bool all_textures =
      std::holds_alternative<RenderTexture>(albedo_map) &&
      std::holds_alternative<RenderTexture>(normal_map) &&
      std::holds_alternative<RenderTexture>(metalness_map) &&
      std::holds_alternative<RenderTexture>(roughness_map);;
    CHECK(all_constants || all_textures, "Invalid material: mixed textures and constants.");

    auto albedo = getColorOrTexture(manager, albedo_map);
    auto normal = getColorOrTexture(manager, normal_map);
    auto roughness = getColorOrTexture(manager, metalness_map);
    auto metalness = getColorOrTexture(manager, roughness_map);
    
    MaterialUniform uniform = {
      .albedo_uv_offset = albedo.uv_offset(),
      .albedo_uv_size = albedo.uv_size(),
      .normal_uv_offset = normal.uv_offset(),
      .normal_uv_size = normal.uv_size(),
      .roughness_uv_offset = roughness.uv_offset(),
      .roughness_uv_size = roughness.uv_size(),
      .metalness_uv_offset = metalness.uv_offset(),
      .metalness_uv_size = metalness.uv_size(),
      .pbr_fresnel0 = glm::dvec4{pbr_fresnel0, 1.0f},
      .blinn_phong_shininess = static_cast<float>(blinn_phong_shininess),
    };
    wgpu::BufferDescriptor buffer_desc = {
      .label = m_wgpu_material_buffer_label.c_str(),
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = sizeof(MaterialUniform),
      .mappedAtCreation = false,
    };
    m_wgpu_material_buffer = device.CreateBuffer(&buffer_desc);
    device.GetQueue().WriteBuffer(m_wgpu_material_buffer, 0, &uniform, sizeof(uniform));

    auto const &render_pipeline = m_render_manager.getFinalRenderPipeline(lighting_model_id);
    auto entries = std::to_array({
      wgpu::BindGroupEntry {.binding=0, .buffer=m_wgpu_material_buffer, .size=sizeof(MaterialUniform)},
      wgpu::BindGroupEntry {.binding=1, .textureView=albedo.render_texture().view()},
      wgpu::BindGroupEntry {.binding=2, .textureView=normal.render_texture().view()},
      wgpu::BindGroupEntry {.binding=3, .textureView=metalness.render_texture().view()},
      wgpu::BindGroupEntry {.binding=4, .textureView=roughness.render_texture().view()},
      wgpu::BindGroupEntry {.binding=5, .sampler=albedo.render_texture().sampler()},
      wgpu::BindGroupEntry {.binding=6, .sampler=normal.render_texture().sampler()},
      wgpu::BindGroupEntry {.binding=7, .sampler=metalness.render_texture().sampler()},
      wgpu::BindGroupEntry {.binding=8, .sampler=roughness.render_texture().sampler()},
    });
    wgpu::BindGroupDescriptor bind_group_desc = {
      .label = m_wgpu_material_bind_group_label.c_str(),
      .layout = render_pipeline.materialBindGroupLayout(),
      .entryCount = entries.size(),
      .entries = entries.data(),
    };
    m_wgpu_material_bind_group = device.CreateBindGroup(&bind_group_desc);

    m_lighting_model = lighting_model;
  }
}
namespace broccoli {
  RenderTextureView MaterialTableEntry::getColorOrTexture(RenderManager &rm, std::variant<glm::dvec3, RenderTexture> const &map) {
    if (std::holds_alternative<glm::dvec3>(map)) {
      glm::u8vec3 val{glm::round(glm::clamp(std::get<glm::dvec3>(map), 0.0, 1.0) * 255.0)};
      int64_t idx = (val.r << 0) | (val.g << 8) | (val.b << 16);
      glm::u16vec2 uv_fixpt{(idx & 0x000FFF) >> 0, (idx & 0xFFF000) >> 12};
      glm::dvec2 uv{(uv_fixpt.x + 0.5) / 4096.0, (uv_fixpt.y + 0.5) / 4096.0};
      return {rm.rgbPalette(), uv, glm::dvec2{0.0}};
    } else if (std::holds_alternative<RenderTexture>(map)) {
      RenderTexture const &tex = std::get<RenderTexture>(map);
      glm::i64vec3 isize = tex.dim();
      CHECK(isize.z == 4, "Expected RGBA texture.");
      glm::dvec2 size{isize.x, isize.y};
      return {rm.rgbPalette(), glm::dvec2{0.0}, size};
    } else {
      PANIC("Invalid map contents.");
    }
  }
  RenderTextureView MaterialTableEntry::getColorOrTexture(RenderManager &rm, std::variant<double, RenderTexture> const &map) {
    if (std::holds_alternative<double>(map)) {
      uint8_t val = static_cast<uint8_t>(glm::round(glm::clamp(std::get<double>(map), 0.0, 1.0) * 255.0));
      glm::u16vec2 uv_fixpt{(val & 0x0F) >> 0, (val & 0xF0) >> 4};
      glm::dvec2 uv{uv_fixpt.x / 16.0, uv_fixpt.y / 16.0};
      return {rm.monochromePalette(), uv + glm::dvec2{0.5}, glm::dvec2{0.0}};
    } else if (std::holds_alternative<RenderTexture>(map)) {
      RenderTexture const &tex = std::get<RenderTexture>(map);
      glm::i64vec3 isize = tex.dim();
      CHECK(isize.z == 1, "Expected monochrome texture.");
      glm::dvec2 size{isize.x, isize.y};
      return {rm.monochromePalette(), glm::dvec2{0.0}, size};
    } else {
      PANIC("Invalid map contents.");
    }
  }
}
namespace broccoli {
  MaterialTableEntry MaterialTableEntry::createBlinnPhongMaterial(
    RenderManager &render_manager,
    std::string name,
    std::variant<glm::dvec3, RenderTexture> albedo_map, 
    std::variant<glm::dvec3, RenderTexture> normal_map,
    double shininess
  ) {
    return {
      render_manager,
      std::move(name),
      std::move(albedo_map),
      std::move(normal_map),
      {0.0},
      {0.0},
      glm::dvec3{0.0},
      shininess,
      MaterialLightingModel::BlinnPhong
    };
  }
}
namespace broccoli {
  MaterialTableEntry MaterialTableEntry::createPbrMaterial(
    RenderManager &render_manager,
    std::string name,
    std::variant<glm::dvec3, RenderTexture> albedo_map, 
    std::variant<glm::dvec3, RenderTexture> normal_map,
    std::variant<double, RenderTexture> metalness_map,
    std::variant<double, RenderTexture> roughness_map,
    glm::dvec3 fresnel0
  ) {
    return {
      render_manager,
      std::move(name),
      std::move(albedo_map),
      std::move(normal_map),
      std::move(metalness_map),
      std::move(roughness_map),
      fresnel0,
      0.0,
      MaterialLightingModel::PhysicallyBased
    };
  }
}
