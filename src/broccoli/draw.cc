#include "broccoli/draw.hh"

#include <iostream>

namespace broccoli {
  wgpu::CommandEncoderDescriptor Renderer::s_command_encoder_descriptor = {
    .nextInChain = nullptr, 
    .label = "Broccoli.Renderer.CommandEncoder",
  };
  Renderer::Renderer(wgpu::Device device, wgpu::TextureView texture_view)
  : m_command_encoder(device.CreateCommandEncoder(&s_command_encoder_descriptor)),
    m_texture_view(texture_view),
    m_device(device)
  {}
  Renderer::~Renderer() {
    auto command_buffer = m_command_encoder.Finish();
    m_device.GetQueue().Submit(1, &command_buffer);
  }
  RenderPass3D Renderer::begin_render_pass_3d() const {
    return {m_command_encoder, m_texture_view, glm::dvec3{1.0, 0.0, 1.0}};
  }
  RenderPass3D Renderer::begin_render_pass_3d(glm::dvec3 clear_color) const {
    return {m_command_encoder, m_texture_view, clear_color};
  }
}

namespace broccoli {
  wgpu::RenderPassEncoder RenderPass3D::new_rp_encoder(
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
  RenderPass3D::RenderPass3D(
    wgpu::CommandEncoder command_encoder, 
    wgpu::TextureView texture_view,
    glm::dvec3 clear_color
  )
  : m_render_pass_encoder(new_rp_encoder(command_encoder, texture_view, clear_color))
  {}
  RenderPass3D::~RenderPass3D() {
    m_render_pass_encoder.End();
  }
}
