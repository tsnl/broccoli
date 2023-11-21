#pragma once

#include "webgpu/webgpu_cpp.h"
#include "glm/vec3.hpp"

namespace broccoli {
  class Frame;
  class RenderPass3D;
}

namespace broccoli {
  class Renderer {
  private:
    static wgpu::CommandEncoderDescriptor s_command_encoder_descriptor;
  private:
    wgpu::CommandEncoder m_command_encoder;
    wgpu::TextureView m_texture_view;
    wgpu::Device m_device;
  public:
    Renderer(wgpu::Device device, wgpu::TextureView texture_view);
    ~Renderer();
  public:
    RenderPass3D begin_render_pass_3d() const;
    RenderPass3D begin_render_pass_3d(glm::dvec3 clear_color) const;
  };
}

namespace broccoli {
  class RenderPass3D {
    friend Renderer;
  private:
    wgpu::RenderPassEncoder m_render_pass_encoder;
  public:
    RenderPass3D() = delete;
    RenderPass3D(RenderPass3D const &other) = delete;
    RenderPass3D(RenderPass3D &&other) = default;
    ~RenderPass3D();
  private:
    RenderPass3D(wgpu::CommandEncoder command_encoder, wgpu::TextureView texture_view, glm::dvec3 clear_color);
    static wgpu::RenderPassEncoder new_rp_encoder(
      wgpu::CommandEncoder command_encoder,
      wgpu::TextureView texture_view,
      glm::dvec3 clear_color
    );
  };
}