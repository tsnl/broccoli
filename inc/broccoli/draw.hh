#pragma once

#include "webgpu/webgpu_cpp.h"
#include "glm/vec3.hpp"

namespace broccoli {
  class Engine;
}
namespace broccoli {
  class Renderer;
  class RenderPass3D;
}

namespace broccoli {
  class Renderer {
    friend Engine;
  private:
    wgpu::CommandEncoder m_command_encoder;
    wgpu::TextureView m_texture_view;
    wgpu::Device m_device;
  public:
    Renderer(wgpu::Device device, wgpu::TextureView texture_view);
    ~Renderer();
  private:
    static void initStaticResources(wgpu::Device device);
    static void dropStaticResources();
  public:
    RenderPass3D beginRenderPass3D() const;
    RenderPass3D beginRenderPass3D(glm::dvec3 clear_color) const;
  };
}

namespace broccoli {
  class RenderPass3D {
    friend Renderer;
  private:
    wgpu::CommandEncoder m_command_encoder;
    wgpu::RenderPassEncoder m_render_pass_encoder;
  private:
    RenderPass3D(wgpu::CommandEncoder command_encoder, wgpu::TextureView texture_view, glm::dvec3 clear_color);
  public:
    RenderPass3D() = delete;
    RenderPass3D(RenderPass3D const &other) = delete;
    RenderPass3D(RenderPass3D &&other) = default;
    ~RenderPass3D();
  private:
    static void initStaticResources(wgpu::Device device);
    static void dropStaticResources();
  };
}
