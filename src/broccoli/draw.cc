#include "broccoli/draw.hh"

#include <deque>

#include "broccoli/core.hh"

//
// Constants:
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
}
namespace broccoli {
  static const uint64_t R3D_VERTEX_BUFFER_CAPACITY = 1 << 16;
  static const uint64_t R3D_INDEX_BUFFER_CAPACITY = 1 << 20;
}
namespace broccoli {
  static wgpu::PipelineLayout s_r3d_render_pipeline_layout = nullptr;
  static wgpu::RenderPipeline s_r3d_render_pipeline = nullptr;
  static wgpu::ShaderModule s_r3d_shader_module = nullptr;
}

//
// Static helpers:
//

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
namespace broccoli {
  class BufferPool {
  private:
    std::deque<wgpu::Buffer> m_buffer_pool;
    wgpu::BufferDescriptor m_buffer_descriptor;
  public:
    BufferPool(const char *label, wgpu::BufferUsage buffer_usage, uint64_t buffer_size);
    ~BufferPool();
  public:
    void clear();
    wgpu::Buffer allocate(wgpu::Device device);
    void release(wgpu::Buffer buffer);
  };
  BufferPool::BufferPool(const char *label, wgpu::BufferUsage buffer_usage, uint64_t buffer_size)
  : m_buffer_pool(),
    m_buffer_descriptor{.label=label, .usage=buffer_usage, .size=buffer_size, .mappedAtCreation=true}
  {}
  BufferPool::~BufferPool() {
    clear();
  }
  void BufferPool::clear() {
    for (auto buf: m_buffer_pool) {
      buf.Release();
    }
    m_buffer_pool.clear();
  }
  wgpu::Buffer BufferPool::allocate(wgpu::Device device) {
    if (m_buffer_pool.empty()) {
      return device.CreateBuffer(&m_buffer_descriptor);
    }
    auto res = m_buffer_pool.front();
    m_buffer_pool.pop_front();
    return res;
  }
  void BufferPool::release(wgpu::Buffer buffer) {
    m_buffer_pool.push_back(buffer);
  }
}
namespace broccoli {
  class VertexMapPool {
  private:
    std::deque<robin_hood::unordered_map<Vertex, uint16_t>> m_vertex_idx_map_pool;
  public:
    VertexMapPool() = default;
  public:
    robin_hood::unordered_map<Vertex, uint16_t> allocate();
    void release(robin_hood::unordered_map<Vertex, uint16_t> hash_map);
  };
  robin_hood::unordered_map<Vertex, uint16_t> VertexMapPool::allocate() {
    if (!m_vertex_idx_map_pool.empty()) {
      auto res = std::move(m_vertex_idx_map_pool.front());
      m_vertex_idx_map_pool.pop_front();
      return res;
    } else {
      robin_hood::unordered_map<Vertex, uint16_t> out_map;
      out_map.reserve(R3D_VERTEX_BUFFER_CAPACITY);
      return out_map;
    }
  }
  void VertexMapPool::release(robin_hood::unordered_map<Vertex, uint16_t> hash_map) {
    m_vertex_idx_map_pool.emplace_back(std::move(hash_map));
  }
}
namespace broccoli {
  class RenderPass3DResourcePool {
  private:
    BufferPool m_vertex_buffer_pool;
    BufferPool m_index_buffer_pool;
    VertexMapPool m_vertex_compression_pool;
    wgpu::Device m_last_device;
  public:
    RenderPass3DResourcePool();
    ~RenderPass3DResourcePool() = default;
  public:
    RenderPass3D::PooledResources allocate(wgpu::Device device);
    void release(RenderPass3D::PooledResources buffers);
  };
  RenderPass3DResourcePool::RenderPass3DResourcePool()
  : m_vertex_buffer_pool("Broccoli.RenderPass3D.BufferPool.Vertex", wgpu::BufferUsage::Vertex | wgpu::BufferUsage::MapWrite, R3D_VERTEX_BUFFER_CAPACITY * sizeof(Vertex)),
    m_index_buffer_pool("Broccoli.RenderPass3D.BufferPool.Index", wgpu::BufferUsage::Index | wgpu::BufferUsage::MapWrite, R3D_INDEX_BUFFER_CAPACITY * sizeof(uint16_t)),
    m_vertex_compression_pool(),
    m_last_device(nullptr)
  {}
  RenderPass3D::PooledResources RenderPass3DResourcePool::allocate(wgpu::Device device) {
    if (m_last_device.Get() != device.Get()) {
      m_last_device = device;
      m_vertex_buffer_pool.clear();
      m_index_buffer_pool.clear();
    }
    auto vertex_buffer = m_vertex_buffer_pool.allocate(device);
    auto vertex_compression_map = m_vertex_compression_pool.allocate();
    auto index_buffer = m_index_buffer_pool.allocate(device);
    return RenderPass3D::PooledResources{.vtx_map=vertex_compression_map, .vtx=vertex_buffer, .idx=index_buffer};
  }
  void RenderPass3DResourcePool::release(RenderPass3D::PooledResources buffers) {
    m_vertex_buffer_pool.release(buffers.vtx);
    m_index_buffer_pool.release(buffers.idx);
    m_vertex_compression_pool.release(std::move(buffers.vtx_map));
  }
}
namespace broccoli {
  static RenderPass3DResourcePool *s_r3d_resource_pool;
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
    return {m_device, m_command_encoder, m_texture_view, glm::dvec3{1.0, 0.0, 1.0}};
  }
  RenderPass3D Renderer::beginRenderPass3D(glm::dvec3 clear_color) const {
    return {m_device, m_command_encoder, m_texture_view, clear_color};
  }
}

namespace broccoli {
  RenderPass3D::RenderPass3D(
    wgpu::Device device,
    wgpu::CommandEncoder command_encoder, 
    wgpu::TextureView texture_view,
    glm::dvec3 clear_color
  )
  : m_command_encoder(command_encoder),
    m_render_pass_encoder(new_rp_encoder(command_encoder, texture_view, clear_color)),
    m_pooled_resources(s_r3d_resource_pool->allocate(device)),
    m_vtx_buf_data(reinterpret_cast<Vertex*>(m_pooled_resources.vtx.GetMappedRange()), R3D_VERTEX_BUFFER_CAPACITY),
    m_idx_buf_data(reinterpret_cast<uint16_t*>(m_pooled_resources.idx.GetMappedRange()), R3D_INDEX_BUFFER_CAPACITY)
  {
    m_render_pass_encoder.SetPipeline(s_r3d_render_pipeline);
  }
  RenderPass3D::~RenderPass3D() {
    m_render_pass_encoder.Draw(3);
    m_render_pass_encoder.End();
    s_r3d_resource_pool->release(std::move(m_pooled_resources));
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
      .module = s_r3d_shader_module,
      .entryPoint = R3D_SHADER_VS_ENTRY_POINT_NAME,
      .constantCount = 0,
      .constants = nullptr,
      .bufferCount = 1,
      .buffers = &vertex_buffer_layout,
    };
    wgpu::PrimitiveState primitive_state = {
      .nextInChain = nullptr,
      .topology = wgpu::PrimitiveTopology::TriangleList,
      .stripIndexFormat = wgpu::IndexFormat::Uint16,
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

    s_r3d_resource_pool = new RenderPass3DResourcePool;
  }
  void RenderPass3D::dropStaticResources() {
    delete s_r3d_resource_pool;
    s_r3d_render_pipeline.Release();
    s_r3d_render_pipeline_layout.Release();
    s_r3d_shader_module.Release();
  }
}
