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
}

//
// GPU binary interface:
//

namespace broccoli {
  struct CameraUniform {
    glm::mat4x4 view_matrix;
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
    uint32_t rsv07 = 0;
    uint32_t rsv08 = 0;
    uint32_t rsv09 = 0;
    uint32_t rsv10 = 0;
  };
}

//
// Static helpers:
//

namespace broccoli {
  static wgpu::RenderPassEncoder new_render_pass(
    wgpu::CommandEncoder command_encoder, 
    wgpu::TextureView texture_view,
    std::optional<glm::dvec3> clear_color
  ) {
    wgpu::Color wgpu_clear_color{};
    if (clear_color.has_value()) {
      auto cc = clear_color.value();
      wgpu_clear_color = wgpu::Color{.r=cc.x, .g=cc.y, .b=cc.z, .a=1.0};
    }
    wgpu::RenderPassColorAttachment rp_surface_color_attachment = {
      .view = texture_view,
      .loadOp = clear_color.has_value() ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue = wgpu_clear_color,
    };
    wgpu::RenderPassDescriptor rp_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.RenderPass3D.WGPURenderPassEncoder",
      .colorAttachmentCount = 1,
      .colorAttachments = &rp_surface_color_attachment,
      .depthStencilAttachment = nullptr,
    };
    return command_encoder.BeginRenderPass(&rp_descriptor);
  }
}

//
// Interface: Renderer
//

namespace broccoli {
  RenderManager::RenderManager(wgpu::Device &device)
  : m_wgpu_device(device),
    m_wgpu_shader_module(nullptr),
    m_wgpu_camera_uniform_buffer(nullptr),
    m_wgpu_transform_uniform_buffer(nullptr),
    m_wgpu_render_pipeline_layout(nullptr),
    m_wgpu_render_pipeline(nullptr)
  {
    initShaderModule();
    initUniforms();
    initBindGroup0Layout();
    initRenderPipelineLayout();
    initRenderPipeline();
    initBindGroup0();
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
    wgpu::BufferDescriptor global_uniform_buffer_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Renderer.Draw.GlobalUniformBuffer",
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = sizeof(CameraUniform),
    };
    m_wgpu_camera_uniform_buffer = m_wgpu_device.CreateBuffer(&global_uniform_buffer_descriptor);
    
    wgpu::BufferDescriptor draw_transform_uniform_buffer_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Renderer.Draw.TransformUniformBuffer",
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = R3D_INSTANCE_CAPACITY * sizeof(glm::mat4x4),
    };
    m_wgpu_transform_uniform_buffer = m_wgpu_device.CreateBuffer(&draw_transform_uniform_buffer_descriptor);
  }
  void RenderManager::initBindGroup0Layout() {
    std::array<wgpu::BindGroupLayoutEntry, 2> bind_group_layout_entries = {
      wgpu::BindGroupLayoutEntry {
        .binding = 0,
        .visibility = wgpu::ShaderStage::Vertex,
        .buffer = wgpu::BufferBindingLayout {
          .type = wgpu::BufferBindingType::Uniform,
          .minBindingSize = sizeof(CameraUniform),
        }
      },
      wgpu::BindGroupLayoutEntry {
        .binding = 1,
        .visibility = wgpu::ShaderStage::Vertex,
        .buffer = wgpu::BufferBindingLayout {
          .type = wgpu::BufferBindingType::Uniform,
          .minBindingSize = sizeof(glm::mat4x4) * R3D_INSTANCE_CAPACITY,
        }
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
      wgpu::VertexAttribute{.format=wgpu::VertexFormat::Sint8x4, .offset=offsetof(Vertex, normal), .shaderLocation=2},
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
    // wgpu::DepthStencilState depth_stencil_state = {
    //   .nextInChain = nullptr,
    //   .format = wgpu::TextureFormat::Depth24Plus,
    //   .depthWriteEnabled = true,
    //   .depthCompare = wgpu::CompareFunction::Less,
    //   .stencilReadMask = 0xFFFFFFFF,
    //   .stencilWriteMask = 0xFFFFFFFF,
    //   .depthBias = 0,
    //   .depthBiasSlopeScale = 0.0f,
    //   .depthBiasClamp = 0.0f,
    // };
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
      // .depthStencil = &depth_stencil_state,
      .depthStencil = nullptr,
      .multisample = multisample_state,
      .fragment = &fragment_state,
    };
    m_wgpu_render_pipeline = m_wgpu_device.CreateRenderPipeline(&render_pipeline_descriptor);
  }
  void RenderManager::initBindGroup0() {
    std::array<wgpu::BindGroupEntry, 2> bind_group_entries = {
      wgpu::BindGroupEntry {
        .binding = 0,
        .buffer = m_wgpu_camera_uniform_buffer,
        .size = sizeof(CameraUniform),
      },
      wgpu::BindGroupEntry {
        .binding = 1,
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
  wgpu::Device const &RenderManager::wgpu_device() {
    return m_wgpu_device;
  }
  wgpu::RenderPipeline const &RenderManager::wgpu_render_pipeline() {
    return m_wgpu_render_pipeline;
  }
  wgpu::Buffer const &RenderManager::wgpu_camera_uniform_buffer() {
    return m_wgpu_camera_uniform_buffer;
  }
  wgpu::Buffer const &RenderManager::wgpu_transform_uniform_buffer() {
    return m_wgpu_transform_uniform_buffer;
  }
  wgpu::BindGroup const &RenderManager::wgpu_bind_group_0() {
    return m_wgpu_bind_group_0;
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
  void RenderFrame::clear(glm::dvec3 clear_color) {
    wgpu::CommandEncoderDescriptor command_encoder_descriptor = {.label = "Broccoli.Renderer.Clear.CommandEncoder"};
    wgpu::CommandEncoder command_encoder = m_manager.wgpu_device().CreateCommandEncoder(&command_encoder_descriptor);
    wgpu::RenderPassEncoder rp = new_render_pass(command_encoder, m_target.texture_view, {clear_color});
    rp.End();
    wgpu::CommandBuffer command_buffer = command_encoder.Finish();
    m_manager.wgpu_device().GetQueue().Submit(1, &command_buffer);
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
    m_target(target)
  {
    const float fovy_rad = (camera.fovy_deg / 360.0f) * (2.0f * static_cast<float>(M_PI));
    const float aspect = target.size.x / static_cast<float>(target.size.y);
    const CameraUniform buf = {
      .view_matrix = glm::inverse(camera.transform),
      .camera_cot_half_fovy = 1.0f / std::tanf(fovy_rad / 2.0f),
      .camera_aspect_inv = 1.0f / aspect,
      .camera_zmin = 000.050f,
      .camera_zmax = 100.000f,
      .camera_logarithmic_z_scale = 1.0,
    };
    auto queue = m_manager.wgpu_device().GetQueue();
    queue.WriteBuffer(m_manager.wgpu_camera_uniform_buffer(), 0, &buf, sizeof(CameraUniform));
  }
}
namespace broccoli {
  void Renderer::draw(Mesh mesh) {
    glm::mat4x4 identity{1.0f};
    draw(mesh, std::span{&identity, 1});
  }
  void Renderer::draw(Mesh mesh, std::span<glm::mat4x4> const &transform) {
    auto queue = m_manager.wgpu_device().GetQueue();
    auto uniform_buffer_size = transform.size() * sizeof(glm::mat4x4);
    queue.WriteBuffer(m_manager.wgpu_transform_uniform_buffer(), 0, transform.data(), uniform_buffer_size);
    wgpu::CommandEncoderDescriptor command_encoder_descriptor = {.label = "Broccoli.Renderer.Draw.CommandEncoder"};
    wgpu::CommandEncoder command_encoder = m_manager.wgpu_device().CreateCommandEncoder(&command_encoder_descriptor);
    wgpu::RenderPassEncoder rp = new_render_pass(command_encoder, m_target.texture_view, {});
    {
      rp.SetPipeline(m_manager.wgpu_render_pipeline());
      rp.SetBindGroup(0, m_manager.wgpu_bind_group_0());
      rp.SetIndexBuffer(mesh.idx_buffer, wgpu::IndexFormat::Uint32);
      rp.SetVertexBuffer(0, mesh.vtx_buffer);
      rp.DrawIndexed(mesh.idx_count, static_cast<uint32_t>(transform.size()));
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
  void MeshBuilder::triangle(Vtx v1, Vtx v2, Vtx v3) {
    auto e1 = v2.offset - v1.offset;
    auto e2 = v3.offset - v1.offset;
    auto normal = glm::normalize(glm::cross(e1, e2));
    if (glm::dot(normal, normal) <= 1e-9) {
      // Degenerate triangle: normal of length close to 0 means that angle of separation between edges is close to 0.
      return;
    }
    auto unorm_normal = pack_normal_unorm_10x3_1x2(normal);
    auto iv1 = vertex(v1.offset, v1.color, unorm_normal);
    auto iv2 = vertex(v2.offset, v2.color, unorm_normal);
    auto iv3 = vertex(v3.offset, v3.color, unorm_normal);
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
  uint32_t MeshBuilder::vertex(glm::dvec3 offset, glm::dvec4 color, uint32_t packed_normal) {
    auto packed_offset = pack_offset(offset);
    auto packed_color = pack_color(color);
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
  glm::tvec4<uint8_t> MeshBuilder::pack_color(glm::dvec4 color) {
    auto pos_fixpt = glm::round(glm::clamp(color, 0.0, 1.0) * 255.0);
    return {
      static_cast<uint8_t>(pos_fixpt.x),
      static_cast<uint8_t>(pos_fixpt.y),
      static_cast<uint8_t>(pos_fixpt.z),
      static_cast<uint8_t>(pos_fixpt.w),
    };
  }
  uint32_t MeshBuilder::pack_normal_unorm_10x3_1x2(glm::dvec3 normal) {
    auto unsigned_normal = (normal + 1.0) * 0.5;
    auto packed_normal = glm::packUnorm3x10_1x2({unsigned_normal, 0.0});
    return packed_normal;
  }
}
