#include "broccoli/draw.hh"

#include "broccoli/core.hh"

//
// Static helpers:
//

namespace broccoli {
  static const wgpu::TextureFormat SWAPCHAIN_TEXTURE_FORMAT = wgpu::TextureFormat::BGRA8Unorm;
}
namespace broccoli {
  static wgpu::CommandEncoderDescriptor s_command_encoder_descriptor = {
    .nextInChain = nullptr, 
    .label = "Broccoli.Renderer.CommandEncoder",
  };
}
namespace broccoli {
  static const char *R3D_SHADER_FILEPATH = "res/r3d.wgsl";
  static const char *R3D_SHADER_VS_ENTRY_POINT_NAME = "vs_main";
  static const char *R3D_SHADER_FS_ENTRY_POINT_NAME = "fs_main";
  static wgpu::PipelineLayout s_r3d_render_pipeline_layout = nullptr;
  static wgpu::RenderPipeline s_r3d_render_pipeline = nullptr;
  static wgpu::ShaderModule s_r3d_shader_module = nullptr;
}
namespace broccoli {
  static wgpu::RenderPassEncoder new_rp_encoder(
    wgpu::CommandEncoder command_encoder, 
    wgpu::TextureView texture_view,
    glm::dvec3 clear_color
  ) {
    wgpu::RenderPassColorAttachment rp_surface_color_attachment = {
      .view = texture_view,
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue = wgpu::Color{.r=clear_color.x, .g=clear_color.y, .b=clear_color.z, .a=1.0},
    };
    wgpu::RenderPassDescriptor rp_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.RenderPass3D.WGPURenderPassEncoder",
      .colorAttachmentCount = 1,
      .colorAttachments = &rp_surface_color_attachment,
      .depthStencilAttachment = nullptr,
      .timestampWriteCount = 0,
      .timestampWrites = nullptr,
    };
    return command_encoder.BeginRenderPass(&rp_descriptor);
  }
}

//
// Interface:
//

namespace broccoli {
  Renderer::Renderer(wgpu::Device device, wgpu::TextureView texture_view)
  : m_command_encoder(device.CreateCommandEncoder(&s_command_encoder_descriptor)),
    m_texture_view(texture_view),
    m_device(device)
  {}
  Renderer::~Renderer() {
    auto command_buffer = m_command_encoder.Finish();
    m_device.GetQueue().Submit(1, &command_buffer);
  }
}
namespace broccoli {
  void Renderer::initStaticResources(wgpu::Device device) {
    RenderPass3D::initStaticResources(device);
  }
  void Renderer::dropStaticResources() {
    RenderPass3D::dropStaticResources();
  }
}
namespace broccoli {
  RenderPass3D Renderer::beginRenderPass3D() const {
    return {m_command_encoder, m_texture_view, glm::dvec3{1.0, 0.0, 1.0}};
  }
  RenderPass3D Renderer::beginRenderPass3D(glm::dvec3 clear_color) const {
    return {m_command_encoder, m_texture_view, clear_color};
  }
}

namespace broccoli {
  RenderPass3D::RenderPass3D(
    wgpu::CommandEncoder command_encoder, 
    wgpu::TextureView texture_view,
    glm::dvec3 clear_color
  )
  : m_command_encoder(command_encoder),
    m_render_pass_encoder(new_rp_encoder(command_encoder, texture_view, clear_color))
  {
    m_render_pass_encoder.SetPipeline(s_r3d_render_pipeline);
    m_render_pass_encoder.Draw(3);
  }
  RenderPass3D::~RenderPass3D() {
    m_render_pass_encoder.End();
  }
}
namespace broccoli {
  void RenderPass3D::initStaticResources(wgpu::Device device) {
    std::string shader_text = readTextFile(R3D_SHADER_FILEPATH);
    
    wgpu::ShaderModuleWGSLDescriptor shader_module_wgsl_descriptor;
    shader_module_wgsl_descriptor.nextInChain = nullptr;
    shader_module_wgsl_descriptor.code = shader_text.c_str();
    wgpu::ShaderModuleDescriptor shader_module_descriptor = {
      .nextInChain = &shader_module_wgsl_descriptor,
      .label = "Broccoli.RenderPass3D.ShaderModule",
    };
    s_r3d_shader_module = device.CreateShaderModule(&shader_module_descriptor);

    wgpu::PipelineLayoutDescriptor render_pipeline_layout_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.RenderPass3D.RenderPipelineLayout",
      .bindGroupLayoutCount = 0,
      .bindGroupLayouts = nullptr,
    };
    s_r3d_render_pipeline_layout = device.CreatePipelineLayout(&render_pipeline_layout_descriptor);

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
    wgpu::VertexState vertex_state = {
      .nextInChain = nullptr,
      .module = s_r3d_shader_module,
      .entryPoint = R3D_SHADER_VS_ENTRY_POINT_NAME,
      .constantCount = 0,
      .constants = nullptr,
      .bufferCount = 0,
      .buffers = nullptr,
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
    //   .stencilFront = wgpu::StencilFaceState{},
    //   .stencilBack = wgpu::StencilFaceState{},
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
      .module = s_r3d_shader_module,
      .entryPoint = R3D_SHADER_FS_ENTRY_POINT_NAME,
      .constantCount = 0,
      .constants = nullptr,
      .targetCount = 1,
      .targets = &color_target
    };
    wgpu::RenderPipelineDescriptor render_pipeline_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.RenderPass3D.RenderPipeline",
      .layout = s_r3d_render_pipeline_layout,
      .vertex = vertex_state,
      .primitive = primitive_state,
      // .depthStencil = &depth_stencil_state,
      .depthStencil = nullptr,
      .multisample = multisample_state,
      .fragment = &fragment_state,
    };
    s_r3d_render_pipeline = device.CreateRenderPipeline(&render_pipeline_descriptor);
  }
  void RenderPass3D::dropStaticResources() {
    s_r3d_render_pipeline.Release();
    s_r3d_render_pipeline_layout.Release();
    s_r3d_shader_module.Release();
  }
}
