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
  static const wgpu::TextureFormat SWAPCHAIN_TEXTURE_FORMAT = wgpu::TextureFormat::BGRA8Unorm;
}
namespace broccoli {
  static const char *R3D_SHADER_FILEPATH = "res/render3d.wgsl";
  static const char *R3D_SHADER_VS_ENTRY_POINT_NAME = "vs_main";
  static const char *R3D_SHADER_FS_ENTRY_POINT_NAME = "fs_main";
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
  static const wgpu::TextureFormat R3D_DEPTH_TEXTURE_FORMAT = wgpu::TextureFormat::Depth24Plus;
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
    uint32_t rsv00 = 0;
    uint32_t rsv01 = 0;
    uint32_t rsv02 = 0;
    uint32_t rsv03 = 0;
    uint32_t rsv04 = 0;
    uint32_t rsv05 = 0;
    uint32_t rsv06 = 0;
  };
  static_assert(sizeof(LightUniform) == 1024, "invalid LightUniform size");
  static_assert(sizeof(CameraUniform) == 128, "invalid CameraUniform size");
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
    m_wgpu_depth_stencil_texture(nullptr),
    m_wgpu_depth_stencil_texture_view(nullptr)
  {
    initShaderModule();
    initUniforms();
    initBindGroup0Layout();
    initRenderPipelineLayout();
    initRenderPipeline();
    initBindGroup0();
    initDepthStencilTexture(framebuffer_size);
  }
  void RenderManager::initShaderModule() {
    const char *filepath = R3D_SHADER_FILEPATH;
    std::string shader_text = readTextFile(filepath);
    
    wgpu::ShaderModuleWGSLDescriptor shader_module_wgsl_descriptor;
    shader_module_wgsl_descriptor.nextInChain = nullptr;
    shader_module_wgsl_descriptor.code = shader_text.c_str();
    wgpu::ShaderModuleDescriptor shader_module_descriptor = {
      .nextInChain = &shader_module_wgsl_descriptor,
      .label = "Broccoli.Renderer.Draw3D.ShaderModule",
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
      .label = "Broccoli.Renderer.Draw.LightUniformBuffer",
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = sizeof(LightUniform),
    };
    m_wgpu_light_uniform_buffer = m_wgpu_device.CreateBuffer(&light_uniform_buffer_descriptor);

    wgpu::BufferDescriptor camera_uniform_buffer_descriptor = {
      .label = "Broccoli.Renderer.Draw.CameraUniformBuffer",
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = sizeof(CameraUniform),
    };
    m_wgpu_camera_uniform_buffer = m_wgpu_device.CreateBuffer(&camera_uniform_buffer_descriptor);
    
    wgpu::BufferDescriptor draw_transform_uniform_buffer_descriptor = {
      .label = "Broccoli.Renderer.Draw.TransformUniformBuffer",
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = R3D_INSTANCE_CAPACITY * sizeof(glm::mat4x4),
    };
    m_wgpu_transform_uniform_buffer = m_wgpu_device.CreateBuffer(&draw_transform_uniform_buffer_descriptor);
  }
  void RenderManager::initDepthStencilTexture(glm::ivec2 framebuffer_size) {
    wgpu::TextureDescriptor depth_texture_descriptor = {
      .label = "Broccoli.Renderer.Draw.RenderTarget.DepthStencilTexture",
      .usage = wgpu::TextureUsage::RenderAttachment,
      .dimension = wgpu::TextureDimension::e2D,
      .size = wgpu::Extent3D {
        .width = static_cast<uint32_t>(framebuffer_size.x),
        .height = static_cast<uint32_t>(framebuffer_size.y),
      },
      .format = R3D_DEPTH_TEXTURE_FORMAT,
      .mipLevelCount = 1,
      .sampleCount = 1,
      .viewFormatCount = 1,
      .viewFormats = &R3D_DEPTH_TEXTURE_FORMAT,
    };
    m_wgpu_depth_stencil_texture = m_wgpu_device.CreateTexture(&depth_texture_descriptor);
    
    wgpu::TextureViewDescriptor depth_view_descriptor = {
      .label = "Broccoli.Renderer.Draw.RenderTarget.DepthStencilTextureView",
      .format = R3D_DEPTH_TEXTURE_FORMAT,
      .dimension = wgpu::TextureViewDimension::e2D,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .aspect = wgpu::TextureAspect::DepthOnly,
    };
    m_wgpu_depth_stencil_texture_view = m_wgpu_depth_stencil_texture.CreateView(&depth_view_descriptor);
  }
  void RenderManager::initBindGroup0Layout() {
    std::array<wgpu::BindGroupLayoutEntry, 3> bind_group_layout_entries = {
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
    };
    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Renderer.Draw3D.BindGroup0Layout",
      .entryCount = bind_group_layout_entries.size(),
      .entries = bind_group_layout_entries.data()
    };
    m_wgpu_bind_group_0_layout = m_wgpu_device.CreateBindGroupLayout(&bind_group_layout_descriptor);
  }
  void RenderManager::initRenderPipelineLayout() {
    wgpu::PipelineLayoutDescriptor render_pipeline_layout_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Renderer.Draw3D.RenderPipelineLayout",
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = &m_wgpu_bind_group_0_layout,
    };
    m_wgpu_render_pipeline_layout = m_wgpu_device.CreatePipelineLayout(&render_pipeline_layout_descriptor);
  }
  void RenderManager::initRenderPipeline() {
    wgpu::BlendState blend_state = {
      .color = {
        .operation = wgpu::BlendOperation::Add,
        .srcFactor = wgpu::BlendFactor::SrcAlpha,
        .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
      },
      .alpha = {
        .operation = wgpu::BlendOperation::Add,
        .srcFactor = wgpu::BlendFactor::Zero,
        .dstFactor = wgpu::BlendFactor::One,
      },
    };
    wgpu::ColorTargetState color_target = {
      .nextInChain = nullptr,
      .format = SWAPCHAIN_TEXTURE_FORMAT,
      .blend = &blend_state,
      .writeMask = wgpu::ColorWriteMask::All,
    };
    std::array<wgpu::VertexAttribute, 3> vertex_buffer_attrib_layout = {
      wgpu::VertexAttribute{.format=wgpu::VertexFormat::Sint16x4, .offset=offsetof(Vertex, offset), .shaderLocation=0},
      wgpu::VertexAttribute{.format=wgpu::VertexFormat::Uint8x4, .offset=offsetof(Vertex, color), .shaderLocation=1},
      wgpu::VertexAttribute{.format=wgpu::VertexFormat::Unorm10_10_10_2, .offset=offsetof(Vertex, normal), .shaderLocation=2},
    };
    wgpu::VertexBufferLayout vertex_buffer_layout = {
      .arrayStride = sizeof(Vertex),
      .stepMode = wgpu::VertexStepMode::Vertex,
      .attributeCount = 3,
      .attributes = &vertex_buffer_attrib_layout[0],
    };
    wgpu::VertexState vertex_state = {
      .nextInChain = nullptr,
      .module = m_wgpu_shader_module,
      .entryPoint = R3D_SHADER_VS_ENTRY_POINT_NAME,
      .bufferCount = 1,
      .buffers = &vertex_buffer_layout,
    };
    wgpu::PrimitiveState primitive_state = {
      .nextInChain = nullptr,
      .topology = wgpu::PrimitiveTopology::TriangleList,
      .stripIndexFormat = wgpu::IndexFormat::Undefined,
      .frontFace = wgpu::FrontFace::CCW,
      .cullMode = wgpu::CullMode::Back,
    };
    wgpu::DepthStencilState depth_stencil_state = {
      .nextInChain = nullptr,
      .format = R3D_DEPTH_TEXTURE_FORMAT,
      .depthWriteEnabled = true,
      .depthCompare = wgpu::CompareFunction::Less,
      .stencilReadMask = 0,
      .stencilWriteMask = 0,
    };
    wgpu::MultisampleState multisample_state = {
      .nextInChain = nullptr,
      .count = 1,
      .mask = 0xFFFFFFFF,
      .alphaToCoverageEnabled = false,
    };
    wgpu::FragmentState fragment_state = {
      .nextInChain = nullptr,
      .module = m_wgpu_shader_module,
      .entryPoint = R3D_SHADER_FS_ENTRY_POINT_NAME,
      .targetCount = 1,
      .targets = &color_target
    };
    wgpu::RenderPipelineDescriptor render_pipeline_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Renderer.Draw3D.RenderPipeline",
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
      .label = "Broccoli.Renderer.Draw3D.BindGroup0",
      .layout = m_wgpu_bind_group_0_layout,
      .entryCount = bind_group_entries.size(),
      .entries = bind_group_entries.data(),
    };
    m_wgpu_bind_group_0 = m_wgpu_device.CreateBindGroup(&bind_group_descriptor);
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
  wgpu::TextureView const &RenderManager::wgpuDepthStencilTextureView() const {
    return m_wgpu_depth_stencil_texture_view;
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
    wgpu::CommandEncoderDescriptor command_encoder_descriptor = {.label = "Broccoli.Renderer.Clear.CommandEncoder"};
    wgpu::CommandEncoder command_encoder = m_manager.wgpuDevice().CreateCommandEncoder(&command_encoder_descriptor);
    wgpu::RenderPassColorAttachment rp_color_attachment = {
      .view = m_target.texture_view,
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue = wgpu::Color{.r=cc.x, .g=cc.y, .b=cc.z, .a=1.0},
    };
    wgpu::RenderPassDepthStencilAttachment rp_depth_attachment = {
      .view = m_manager.wgpuDepthStencilTextureView(),
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
  
  Renderer RenderFrame::use_camera(RenderCamera camera) {
    return {m_manager, m_target, camera};
  }
}

//
// Interface: Renderer:
//

namespace broccoli {
  Renderer::Renderer(RenderManager &renderer, RenderTarget target, RenderCamera camera)
  : m_manager(renderer),
    m_target(target),
    m_camera(camera),
    m_mesh_instance_list_vec(),
    m_directional_light_vec(),
    m_point_light_vec()
  {
    m_directional_light_vec.reserve(R3D_DIRECTIONAL_LIGHT_CAPACITY);
    m_point_light_vec.reserve(R3D_POINT_LIGHT_CAPACITY);
  }
  Renderer::~Renderer() {
    sendCameraData(m_camera, m_target);
    sendLightData(std::move(m_directional_light_vec), std::move(m_point_light_vec));
    sendDrawMeshInstanceListVec(m_mesh_instance_list_vec);
  }
}
namespace broccoli {
  void Renderer::draw(Mesh mesh) {
    glm::mat4x4 identity{1.0f};
    draw(mesh, std::span{&identity, 1});
  }
  void Renderer::draw(Mesh mesh, std::span<glm::mat4x4> const &transforms_span) {
    std::vector<glm::mat4x4> transforms(transforms_span.begin(), transforms_span.end());
    draw(mesh, std::move(transforms));
  }
  void Renderer::draw(Mesh mesh, std::vector<glm::mat4x4> transforms) {
    m_mesh_instance_list_vec.emplace_back(mesh, std::move(transforms));
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
    const float fovy_rad = (camera.fovy_deg / 360.0f) * (2.0f * static_cast<float>(M_PI));
    const float aspect = target.size.x / static_cast<float>(target.size.y);
    const CameraUniform buf = {
      .view_matrix = glm::inverse(camera.transform),
      .world_position = camera.transform * glm::vec4{glm::vec3{0.0f}, 1.0f},
      .camera_cot_half_fovy = 1.0f / std::tanf(fovy_rad / 2.0f),
      .camera_aspect_inv = 1.0f / aspect,
      .camera_zmin = 000.050f,
      .camera_zmax = 100.000f,
      .camera_logarithmic_z_scale = 1.0,
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
  void Renderer::sendDrawMeshInstanceListVec(std::vector<MeshInstanceList> mesh_instance_list_vec) {
    for (auto const &mesh_instance_list: m_mesh_instance_list_vec) {
      sendDrawMeshInstanceList(mesh_instance_list);
    }
  }
  void Renderer::sendDrawMeshInstanceList(MeshInstanceList const &mesh_instance_list) {
    auto const &mesh = mesh_instance_list.mesh;
    auto const &transform_list = mesh_instance_list.instance_list;
    auto queue = m_manager.wgpuDevice().GetQueue();
    auto uniform_buffer_size = transform_list.size() * sizeof(glm::mat4x4);
    queue.WriteBuffer(m_manager.wgpuTransformUniformBuffer(), 0, transform_list.data(), uniform_buffer_size);
    wgpu::CommandEncoderDescriptor command_encoder_descriptor = {.label = "Broccoli.Renderer.Draw.CommandEncoder"};
    wgpu::CommandEncoder command_encoder = m_manager.wgpuDevice().CreateCommandEncoder(&command_encoder_descriptor);
    wgpu::RenderPassColorAttachment rp_color_attachment = {
      .view = m_target.texture_view,
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
    };
    wgpu::RenderPassDepthStencilAttachment rp_depth_attachment = {
      .view = m_manager.wgpuDepthStencilTextureView(),
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
// Interface: MeshBuilder
//

namespace broccoli {
  MeshBuilder::MeshBuilder(wgpu::Device &device)
  : m_wgpu_device(device),
    m_vtx_compression_map(),
    m_vtx_buf(),
    m_idx_buf()
  {
    m_vtx_compression_map.reserve(R3D_VERTEX_BUFFER_CAPACITY);
    m_vtx_buf.reserve(R3D_VERTEX_BUFFER_CAPACITY);
    m_idx_buf.reserve(R3D_INDEX_BUFFER_CAPACITY);
  }
  void MeshBuilder::triangle(Vtx v1, Vtx v2, Vtx v3, bool double_faced) {
    singleFaceTriangle(v1, v2, v3);
    if (double_faced) {
      singleFaceTriangle(v1, v3, v2);
    }
  }
  void MeshBuilder::quad(Vtx v1, Vtx v2, Vtx v3, Vtx v4, bool double_faced) {
    triangle(v1, v2, v3, double_faced);
    triangle(v1, v3, v4, double_faced);
  }
}
namespace broccoli {
  void MeshBuilder::singleFaceTriangle(Vtx v1, Vtx v2, Vtx v3) {
    auto e1 = v2.offset - v1.offset;
    auto e2 = v3.offset - v1.offset;
    auto normal = glm::normalize(glm::cross(e1, e2));
    if (glm::dot(normal, normal) <= 1e-9) {
      // Degenerate triangle: normal of length close to 0 means that angle of separation between edges is close to 0.
      return;
    }
    auto unorm_normal = pack_normal_unorm_10x3_1x2(normal);
    auto iv1 = vertex(v1.offset, v1.color, v1.shininess, unorm_normal);
    auto iv2 = vertex(v2.offset, v2.color, v2.shininess, unorm_normal);
    auto iv3 = vertex(v3.offset, v3.color, v3.shininess, unorm_normal);
    m_idx_buf.push_back(iv1);
    m_idx_buf.push_back(iv2);
    m_idx_buf.push_back(iv3);
  }
  Mesh MeshBuilder::finish() {
    wgpu::BufferDescriptor vtx_buf_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Mesh.VertexBuffer",
      .usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst,
      .size = m_vtx_buf.size() * sizeof(Vertex),
    };
    wgpu::BufferDescriptor idx_buf_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Mesh.IndexBuffer",
      .usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst,
      .size = m_idx_buf.size() * sizeof(uint32_t),
    };
    Mesh mesh = {
      .vtx_buffer = m_wgpu_device.CreateBuffer(&vtx_buf_descriptor),
      .idx_buffer = m_wgpu_device.CreateBuffer(&idx_buf_descriptor),
      .vtx_count = static_cast<uint32_t>(m_vtx_buf.size()),
      .idx_count = static_cast<uint32_t>(m_idx_buf.size()),
    };
    wgpu::Queue queue = m_wgpu_device.GetQueue();
    auto vtx_buf_size = m_vtx_buf.size() * sizeof(Vertex);
    auto idx_buf_size = m_idx_buf.size() * sizeof(uint32_t);
    queue.WriteBuffer(mesh.vtx_buffer, 0, m_vtx_buf.data(), vtx_buf_size);
    queue.WriteBuffer(mesh.idx_buffer, 0, m_idx_buf.data(), idx_buf_size);
    return mesh;
  }
  uint32_t MeshBuilder::vertex(glm::dvec3 offset, glm::dvec3 color, float shininess, uint32_t packed_normal) {
    auto packed_offset = pack_offset(offset);
    auto packed_color = pack_color(color, shininess);
    broccoli::Vertex packed_vertex = {.offset=packed_offset, .color=packed_color, .normal=packed_normal};
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
  glm::tvec4<int16_t> MeshBuilder::pack_offset(glm::dvec3 pos) {
    // Packing into 6.10 signed fix-point per-component
    auto pos_fixpt = glm::round(glm::clamp(pos, -32768.0, +32767.0) * 1024.0);
    return {
      static_cast<int16_t>(pos_fixpt.x),
      static_cast<int16_t>(pos_fixpt.y),
      static_cast<int16_t>(pos_fixpt.z),
      1024,
    };
  }
  glm::tvec4<uint8_t> MeshBuilder::pack_color(glm::dvec3 color, float shininess) {
    auto pos_fixpt = glm::round(glm::clamp(color, 0.0, 1.0) * 255.0);
    auto shininess_fixpt = glm::round(glm::clamp(shininess, 0.0f, 1.0f) * 255.0);
    return {
      static_cast<uint8_t>(pos_fixpt.x),
      static_cast<uint8_t>(pos_fixpt.y),
      static_cast<uint8_t>(pos_fixpt.z),
      static_cast<uint8_t>(shininess_fixpt),
    };
  }
  uint32_t MeshBuilder::pack_normal_unorm_10x3_1x2(glm::dvec3 normal) {
    auto unsigned_normal = (normal + 1.0) * 0.5;
    auto packed_normal = glm::packUnorm3x10_1x2({unsigned_normal, 0.0});
    return packed_normal;
  }
}
