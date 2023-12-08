#include "broccoli/engine/render.hh"

#include <iostream>
#include <deque>
#include <sstream>
#include <limits>

#include "glm/gtc/packing.hpp"

#include "broccoli/engine/core.hh"

//
// Constants:
//

namespace broccoli {
  static const char *R3D_SHADER_FILEPATH = "res/shader/3d/ubershader.wgsl";
  static const char *R3D_SHADER_VS_ENTRY_POINT_NAME = "vertexShaderMain";
  static const char *R3D_SHADER_FS_ENTRY_POINT_NAME = "fragmentShaderMain";
}
namespace broccoli {
  static const uint64_t R3D_VERTEX_BUFFER_CAPACITY = 1 << 16;
  static const uint64_t R3D_INDEX_BUFFER_CAPACITY = 1 << 20;
  static const uint64_t R3D_INSTANCE_CAPACITY = 1 << 10;
  static const uint64_t R3D_DIRECTIONAL_LIGHT_CAPACITY = 4;
  static const uint64_t R3D_POINT_LIGHT_CAPACITY = 16;
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
    uint32_t lighting_model = 0;
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
  };
  static_assert(sizeof(LightUniform) == 1024, "invalid LightUniform size");
  static_assert(sizeof(CameraUniform) == 128, "invalid CameraUniform size");
  static_assert(sizeof(MaterialUniform) == 128, "invalid MaterialUniform size");
}

//
// Interface: RenderCamera
//

namespace broccoli {
  RenderCamera::RenderCamera(glm::mat4x4 view_matrix, glm::vec3 position, float fovy_deg, float exposure_bias)
  : m_view_matrix(view_matrix),
    m_position(position),
    m_fovy_deg(fovy_deg),
    m_exposure_bias(exposure_bias)
  {}
}
namespace broccoli {
  RenderCamera RenderCamera::createDefault(float fovy_deg, float exposure_bias) {
    return {glm::mat4x4{1.0f}, glm::vec3{0.0f}, fovy_deg, exposure_bias};
  }
  RenderCamera RenderCamera::fromTransform(glm::mat4x4 transform, float fovy_deg, float exposure_bias) {
    return {glm::inverse(transform), transform[3], fovy_deg, exposure_bias};
  }
  RenderCamera RenderCamera::fromLookAt(glm::vec3 eye, glm::vec3 target, glm::vec3 up, float fovy_deg, float exposure_bias) {
    return {glm::lookAt(eye, target, up), eye, fovy_deg, exposure_bias};
  }
}
namespace broccoli {
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
  RenderTexture::RenderTexture(wgpu::Device &device, std::string name, Bitmap bitmap, wgpu::FilterMode filter)
  : m_texture(nullptr),
    m_view(nullptr),
    m_sampler(nullptr),
    m_bitmap(std::move(bitmap)),
    m_name(std::move(name)),
    m_wgpu_texture_label(m_name + ".WGPUTexture"),
    m_wgpu_view_label(m_name + ".WGPUTextureView"),
    m_wgpu_sampler_label(m_name + ".WGPUSampler")
  {
    upload(device, filter);
  }
}
namespace broccoli {
  void RenderTexture::upload(wgpu::Device &device, wgpu::FilterMode filter) {
    CHECK(!m_texture, "expected WGPU texture to be uninit.");
    CHECK(!m_view, "expected WGPU texture view to be uninit.");
    CHECK(m_bitmap.size().z == 1 || m_bitmap.size().z == 4, "invalid bitmap depth");
    
    // Determine key properties:
    auto dim = m_bitmap.size();
    auto format = dim.z == 1 ? wgpu::TextureFormat::R8Unorm : wgpu::TextureFormat::RGBA8Unorm;
    
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

    // Write to GPU texture:
    wgpu::ImageCopyTexture copy_dst_desc = {.texture = m_texture};
    wgpu::TextureDataLayout copy_src_layout_desc = {
      .bytesPerRow = static_cast<uint32_t>(m_bitmap.pitch()),
      .rowsPerImage = static_cast<uint32_t>(m_bitmap.rows()),
    };
    wgpu::Extent3D copy_size = {static_cast<uint32_t>(m_bitmap.size().x), static_cast<uint32_t>(m_bitmap.size().y)};
    device.GetQueue().WriteTexture(
      &copy_dst_desc,
      m_bitmap.data(),
      m_bitmap.dataSize(),
      &copy_src_layout_desc,
      &copy_size
    );
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
  Bitmap const &RenderTexture::bitmap() const {
    return m_bitmap;
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

//
// Interface: Renderer
//

namespace broccoli {
  RenderManager::RenderManager(wgpu::Device &device, glm::ivec2 framebuffer_size)
  : m_wgpu_device(device),
    m_wgpu_shader_module(nullptr),
    m_wgpu_light_uniform_buffer(nullptr),
    m_wgpu_camera_uniform_buffer(nullptr),
    m_wgpu_transform_uniform_buffer(nullptr),
    m_wgpu_bind_group_0_layout(nullptr),
    m_wgpu_render_pipeline_layout(nullptr),
    m_wgpu_render_pipeline(nullptr),
    m_wgpu_bind_group_0(nullptr),
    m_wgpu_render_target_color_texture(nullptr),
    m_wgpu_render_target_color_texture_view(nullptr),
    m_wgpu_render_target_depth_stencil_texture(nullptr),
    m_wgpu_render_target_depth_stencil_texture_view(nullptr),
    m_rgb_palette(
      m_wgpu_device,
      "Broccoli.Render.Texture.RgbPalette",
      initRgbPalette(),
      wgpu::FilterMode::Nearest
    ),
    m_monochrome_palette(
      m_wgpu_device,
      "Broccoli.Render.Texture.MonochromePalette",
      initMonochromePalette(),
      wgpu::FilterMode::Nearest
    ),
    m_materials(),
    m_materials_locked(false)
  {
    initShaderModule();
    initUniforms();
    initBindGroup0Layout();
    initBindGroup1Layout();
    initRenderPipelineLayout();
    initRenderPipeline();
    initBindGroup0();
    reinitColorTexture(framebuffer_size);
    reinitDepthStencilTexture(framebuffer_size);
  }
  Bitmap RenderManager::initMonochromePalette() {
    Bitmap bitmap{glm::i64vec3{R3D_MONO_PALETTE_TEXTURE_SIZE, R3D_MONO_PALETTE_TEXTURE_SIZE, 1}};
    for (int64_t r = 0; r <= 255; r++) {
      auto ptr = bitmap((r & 0x0F) >> 0, (r & 0xF0) >> 4);
      *ptr = static_cast<uint8_t>(r);
    }
    return bitmap;
  }
  Bitmap RenderManager::initRgbPalette() {
    Bitmap bitmap{glm::i64vec3{R3D_RGB_PALETTE_TEXTURE_SIZE, R3D_RGB_PALETTE_TEXTURE_SIZE, 4}};
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
  void RenderManager::initShaderModule() {
    const char *filepath = R3D_SHADER_FILEPATH;
    std::string shader_text = readTextFile(filepath);
    
    wgpu::ShaderModuleWGSLDescriptor shader_module_wgsl_descriptor;
    shader_module_wgsl_descriptor.nextInChain = nullptr;
    shader_module_wgsl_descriptor.code = shader_text.c_str();
    wgpu::ShaderModuleDescriptor shader_module_descriptor = {
      .nextInChain = &shader_module_wgsl_descriptor,
      .label = "Broccoli.Render.Draw3D.ShaderModule",
    };
    auto shader_module = m_wgpu_device.CreateShaderModule(&shader_module_descriptor);
    
    struct ShaderCompileResult { const char *filepath; bool completed; };
    ShaderCompileResult result = {filepath, false};
    shader_module.GetCompilationInfo(
      [](WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo *info, void *userdata) {
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

    m_wgpu_shader_module = shader_module;
  }
  void RenderManager::initUniforms() {
    wgpu::BufferDescriptor light_uniform_buffer_descriptor = {
      .label = "Broccoli.Render.Draw.LightUniformBuffer",
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = sizeof(LightUniform),
    };
    m_wgpu_light_uniform_buffer = m_wgpu_device.CreateBuffer(&light_uniform_buffer_descriptor);

    wgpu::BufferDescriptor camera_uniform_buffer_descriptor = {
      .label = "Broccoli.Render.Draw.CameraUniformBuffer",
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = sizeof(CameraUniform),
    };
    m_wgpu_camera_uniform_buffer = m_wgpu_device.CreateBuffer(&camera_uniform_buffer_descriptor);
    
    wgpu::BufferDescriptor draw_transform_uniform_buffer_descriptor = {
      .label = "Broccoli.Render.Draw.TransformUniformBuffer",
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = R3D_INSTANCE_CAPACITY * sizeof(glm::mat4x4),
    };
    m_wgpu_transform_uniform_buffer = m_wgpu_device.CreateBuffer(&draw_transform_uniform_buffer_descriptor);
  }
  void RenderManager::reinitColorTexture(glm::ivec2 framebuffer_size) {
    if (m_wgpu_render_target_color_texture) {
      m_wgpu_render_target_color_texture.Destroy();
      m_wgpu_render_target_color_texture = nullptr;
    }

    wgpu::TextureDescriptor color_texture_descriptor = {
      .label = "Broccoli.Render.Draw.RenderTarget.ColorTexture",
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
      .label = "Broccoli.Render.Draw.RenderTarget.ColorTextureView",
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
      .label = "Broccoli.Render.Draw.RenderTarget.DepthStencilTexture",
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
      .label = "Broccoli.Render.Draw.RenderTarget.DepthStencilTextureView",
      .format = R3D_DEPTH_TEXTURE_FORMAT,
      .dimension = wgpu::TextureViewDimension::e2D,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .aspect = wgpu::TextureAspect::DepthOnly,
    };
    m_wgpu_render_target_depth_stencil_texture_view = m_wgpu_render_target_depth_stencil_texture.CreateView(&depth_view_descriptor);
  }
  void RenderManager::initBindGroup0Layout() {
    auto bind_group_layout_entries = std::to_array({
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
    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor = {
      .label = "Broccoli.Render.Draw3D.BindGroup0Layout",
      .entryCount = bind_group_layout_entries.size(),
      .entries = bind_group_layout_entries.data()
    };
    m_wgpu_bind_group_0_layout = m_wgpu_device.CreateBindGroupLayout(&bind_group_layout_descriptor);
  }
  void RenderManager::initBindGroup1Layout() {
    auto bind_group_layout_entries = std::to_array({
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
    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor = {
      .label = "Broccoli.Render.Draw3D.BindGroup1Layout",
      .entryCount = bind_group_layout_entries.size(),
      .entries = bind_group_layout_entries.data()
    };
    m_wgpu_bind_group_1_layout = m_wgpu_device.CreateBindGroupLayout(&bind_group_layout_descriptor);
  }
  void RenderManager::initRenderPipelineLayout() {
    auto bind_group_layouts = std::to_array({
      m_wgpu_bind_group_0_layout,
      m_wgpu_bind_group_1_layout
    });
    wgpu::PipelineLayoutDescriptor render_pipeline_layout_descriptor = {
      .label = "Broccoli.Render.Draw3D.RenderPipelineLayout",
      .bindGroupLayoutCount = bind_group_layouts.size(),
      .bindGroupLayouts = bind_group_layouts.data(),
    };
    m_wgpu_render_pipeline_layout = m_wgpu_device.CreatePipelineLayout(&render_pipeline_layout_descriptor);
  }
  void RenderManager::initRenderPipeline() {
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
    std::array<wgpu::VertexAttribute, 4> vertex_buffer_attrib_layout = {
      wgpu::VertexAttribute{.format=wgpu::VertexFormat::Sint32x3, .offset=offsetof(Vertex, offset), .shaderLocation=0},
      wgpu::VertexAttribute{.format=wgpu::VertexFormat::Unorm10_10_10_2, .offset=offsetof(Vertex, normal), .shaderLocation=1},
      wgpu::VertexAttribute{.format=wgpu::VertexFormat::Unorm10_10_10_2, .offset=offsetof(Vertex, tangent), .shaderLocation=2},
      wgpu::VertexAttribute{.format=wgpu::VertexFormat::Unorm16x2, .offset=offsetof(Vertex, uv), .shaderLocation=3},
    };
    wgpu::VertexBufferLayout vertex_buffer_layout = {
      .arrayStride = sizeof(Vertex),
      .stepMode = wgpu::VertexStepMode::Vertex,
      .attributeCount = vertex_buffer_attrib_layout.size(),
      .attributes = vertex_buffer_attrib_layout.data(),
    };
    wgpu::VertexState vertex_state = {
      .module = m_wgpu_shader_module,
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
      .module = m_wgpu_shader_module,
      .entryPoint = R3D_SHADER_FS_ENTRY_POINT_NAME,
      .targetCount = 1,
      .targets = &color_target
    };
    wgpu::RenderPipelineDescriptor render_pipeline_descriptor = {
      .label = "Broccoli.Render.Draw3D.RenderPipeline",
      .layout = m_wgpu_render_pipeline_layout,
      .vertex = vertex_state,
      .primitive = primitive_state,
      .depthStencil = &depth_stencil_state,
      .multisample = multisample_state,
      .fragment = &fragment_state,
    };
    m_wgpu_render_pipeline = m_wgpu_device.CreateRenderPipeline(&render_pipeline_descriptor);
  }
  void RenderManager::initBindGroup0() {
    std::array<wgpu::BindGroupEntry, 3> bind_group_entries = {
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
    };
    wgpu::BindGroupDescriptor bind_group_descriptor = {
      .label = "Broccoli.Render.Draw3D.BindGroup0",
      .layout = m_wgpu_bind_group_0_layout,
      .entryCount = bind_group_entries.size(),
      .entries = bind_group_entries.data(),
    };
    m_wgpu_bind_group_0 = m_wgpu_device.CreateBindGroup(&bind_group_descriptor);
  }
  void RenderManager::initMaterialTable() {
    m_materials.reserve(R3D_MATERIAL_TABLE_INIT_CAPACITY);
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
  void RenderManager::lockMaterials() {
    CHECK(!m_materials_locked, "Cannot lock materials while materials are already locked.");
    m_materials_locked = true;
  }
  void RenderManager::unlockMaterials() {
    CHECK(m_materials_locked, "Cannot unlock materials unless materials are already locked.");
    m_materials_locked = false;
  }
  const wgpu::BindGroup &RenderManager::getMaterialBindGroup(Material material) const {
    CHECK(m_materials_locked, "Cannot access material bind group unless materials are already locked.");
    return m_materials[material.value].wgpuMaterialBindGroup();
  }
}
namespace broccoli {
  wgpu::Device const &RenderManager::wgpuDevice() const {
    return m_wgpu_device;
  }
  wgpu::RenderPipeline const &RenderManager::wgpuRenderPipeline() const {
    return m_wgpu_render_pipeline;
  }
  wgpu::Buffer const &RenderManager::wgpuLightUniformBuffer() const {
    return m_wgpu_light_uniform_buffer;
  }
  wgpu::Buffer const &RenderManager::wgpuCameraUniformBuffer() const {
    return m_wgpu_camera_uniform_buffer;
  }
  wgpu::Buffer const &RenderManager::wgpuTransformUniformBuffer() const {
    return m_wgpu_transform_uniform_buffer;
  }
  wgpu::BindGroup const &RenderManager::wgpuBindGroup0() const {
    return m_wgpu_bind_group_0;
  }
  wgpu::BindGroupLayout const &RenderManager::wgpuBindGroup1Layout() const {
    return m_wgpu_bind_group_1_layout;
  }
  wgpu::TextureView const &RenderManager::wgpuRenderTargetColorTextureView() const {
    return m_wgpu_render_target_color_texture_view;
  }
  wgpu::TextureView const &RenderManager::wgpuRenderTargetDepthStencilTextureView() const {
    return m_wgpu_render_target_depth_stencil_texture_view;
  }
  RenderTexture const &RenderManager::rgbPalette() const {
    return m_rgb_palette;
  }
  RenderTexture const &RenderManager::monochromePalette() const {
    return m_monochrome_palette;
  }
  std::vector<MaterialTableEntry> const &RenderManager::materials() const {
    return m_materials;
  }
}
namespace broccoli {
  RenderFrame RenderManager::frame(RenderTarget target) {
    return {*this, target};
  }
}

//
// Interface: Renderer:
//

namespace broccoli {
  RenderFrame::RenderFrame(RenderManager &manager, RenderTarget target)
  : m_manager(manager),
    m_target(target)
  {}
}
namespace broccoli {
  void RenderFrame::clear(glm::dvec3 cc) {
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
      .label = "Broccoli.RenderPass3D.WGPURenderPassEncoder",
      .colorAttachmentCount = 1,
      .colorAttachments = &rp_color_attachment,
      .depthStencilAttachment = &rp_depth_attachment,
    };
    wgpu::RenderPassEncoder rp = command_encoder.BeginRenderPass(&rp_descriptor);
    rp.End();
    wgpu::CommandBuffer command_buffer = command_encoder.Finish();
    m_manager.wgpuDevice().GetQueue().Submit(1, &command_buffer);
  }
  
  Renderer RenderFrame::useCamera(RenderCamera camera) {
    return {m_manager, m_target, camera};
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
    manager.lockMaterials();
    m_directional_light_vec.reserve(R3D_DIRECTIONAL_LIGHT_CAPACITY);
    m_point_light_vec.reserve(R3D_POINT_LIGHT_CAPACITY);
    m_mesh_instance_lists.resize(manager.materials().size());
  }
  Renderer::~Renderer() {
    sendCameraData(m_camera, m_target);
    sendLightData(std::move(m_directional_light_vec), std::move(m_point_light_vec));
    sendDrawMeshInstanceListVec(std::move(m_mesh_instance_lists));
    m_manager.unlockMaterials();
  }
}
namespace broccoli {
  void Renderer::draw(Material material_id, const Geometry &mesh) {
    draw(material_id, mesh, glm::mat4x4{1.0f});
  }
  void Renderer::draw(Material material_id, const Geometry &mesh, glm::mat4x4 transform) {
    draw(material_id, std::move(mesh), std::span<glm::mat4x4>{&transform, 1});
  }
  void Renderer::draw(Material material_id, const Geometry &mesh, std::span<glm::mat4x4> transforms_span) {
    std::vector<glm::mat4x4> transforms(transforms_span.begin(), transforms_span.end());
    draw(material_id, std::move(mesh), std::move(transforms));
  }
  void Renderer::draw(Material material_id, const Geometry &mesh, std::vector<glm::mat4x4> transforms) {
    MeshInstanceList mil = {mesh, std::move(transforms)};
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
  void Renderer::sendLightData(std::vector<DirectionalLight> directional_light_vec, std::vector<PointLight> point_light_vec) {
    CHECK(directional_light_vec.size() < R3D_DIRECTIONAL_LIGHT_CAPACITY, "Direction light overflow");
    CHECK(point_light_vec.size() < R3D_POINT_LIGHT_CAPACITY, "Point light overflow");
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
  void Renderer::sendDrawMeshInstanceListVec(std::vector<std::vector<MeshInstanceList>> mesh_instance_list_vec) {
    for (size_t material_idx = 0; material_idx < mesh_instance_list_vec.size(); material_idx++) {
      Material material{material_idx};
      for (auto const &mesh_instance_list: mesh_instance_list_vec[material_idx]) {
        sendDrawMeshInstanceList(material, mesh_instance_list);
      }
    }
  }
  void Renderer::sendDrawMeshInstanceList(Material material, const MeshInstanceList &mesh_instance_list) {
    if (mesh_instance_list.instance_list.empty()) {
      return;
    }
    auto const &mesh = mesh_instance_list.mesh;
    auto const &transform_list = mesh_instance_list.instance_list;
    auto queue = m_manager.wgpuDevice().GetQueue();
    auto uniform_buffer_size = transform_list.size() * sizeof(glm::mat4x4);
    queue.WriteBuffer(m_manager.wgpuTransformUniformBuffer(), 0, transform_list.data(), uniform_buffer_size);
    wgpu::CommandEncoderDescriptor command_encoder_descriptor = {.label = "Broccoli.Render.Draw.CommandEncoder"};
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
      .label = "Broccoli.RenderPass3D.WGPURenderPassEncoder",
      .colorAttachmentCount = 1,
      .colorAttachments = &rp_color_attachment,
      .depthStencilAttachment = &rp_depth_attachment,
    };
    wgpu::RenderPassEncoder rp = command_encoder.BeginRenderPass(&rp_descriptor);
    {
      rp.SetPipeline(m_manager.wgpuRenderPipeline());
      rp.SetBindGroup(0, m_manager.wgpuBindGroup0());
      rp.SetBindGroup(1, m_manager.getMaterialBindGroup(material));
      rp.SetIndexBuffer(mesh.idx_buffer, wgpu::IndexFormat::Uint32);
      rp.SetVertexBuffer(0, mesh.vtx_buffer);
      rp.DrawIndexed(mesh.idx_count, static_cast<uint32_t>(transform_list.size()));
    }
    rp.End();
    wgpu::CommandBuffer command_buffer = command_encoder.Finish();
    queue.Submit(1, &command_buffer);
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
    MaterialLightingModel lighting_model
  )
  : m_render_manager(render_manager),
    m_wgpu_material_buffer(nullptr),
    m_wgpu_material_bind_group(nullptr),
    m_name("Broccoli.Material." + name),
    m_wgpu_material_buffer_label(m_name + ".MaterialUbo"),
    m_wgpu_material_bind_group_label(m_name + ".BindGroup1")
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
  const wgpu::BindGroup &MaterialTableEntry::wgpuMaterialBindGroup() const {
    return m_wgpu_material_bind_group;
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
      .lighting_model = static_cast<uint32_t>(lighting_model),
    };
    wgpu::BufferDescriptor buffer_desc = {
      .label = m_wgpu_material_buffer_label.c_str(),
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = sizeof(MaterialUniform),
      .mappedAtCreation = false,
    };
    m_wgpu_material_buffer = device.CreateBuffer(&buffer_desc);
    device.GetQueue().WriteBuffer(m_wgpu_material_buffer, 0, &uniform, sizeof(uniform));

    auto bind_group_layout = m_render_manager.wgpuBindGroup1Layout();
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
      .layout = bind_group_layout,
      .entryCount = entries.size(),
      .entries = entries.data(),
    };
    m_wgpu_material_bind_group = device.CreateBindGroup(&bind_group_desc);
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
      glm::i64vec3 isize = tex.bitmap().size();
      CHECK(isize.z == 4, "Expected RGBA texture.");
      glm::dvec2 size{isize.x, isize.y};
      return {rm.rgbPalette(), glm::dvec2{0.0}, size};
    } else {
      PANIC("Invalid map contents.");
    }
  }
  RenderTextureView MaterialTableEntry::getColorOrTexture(RenderManager &rm, std::variant<double, RenderTexture> const &map) {
    if (std::holds_alternative<double>(map)) {
      uint8_t val = glm::round(glm::clamp(std::get<double>(map), 0.0, 1.0) * 255.0);
      glm::u16vec2 uv_fixpt{(val & 0x0F) >> 0, (val & 0xF0) >> 4};
      glm::dvec2 uv{uv_fixpt.x / 16.0, uv_fixpt.y / 16.0};
      return {rm.monochromePalette(), uv + glm::dvec2{0.5}, glm::dvec2{0.0}};
    } else if (std::holds_alternative<RenderTexture>(map)) {
      RenderTexture const &tex = std::get<RenderTexture>(map);
      glm::i64vec3 isize = tex.bitmap().size();
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
