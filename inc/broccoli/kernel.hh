#pragma once

#include "webgpu/webgpu_cpp.h"
#include "GLFW/glfw3.h"
#include "glm/vec3.hpp"

#include "broccoli/core.hh"

struct GLFWwindow;

namespace broccoli {
	class Kernel {
	private:
		GLFWwindow *m_glfw_window;
    wgpu::Instance m_wgpu_instance;
    wgpu::Surface m_wgpu_surface;
    wgpu::Adapter m_wgpu_adapter;
    wgpu::Device m_wgpu_device;
    wgpu::SwapChain m_wgpu_swapchain;
    glm::dvec3 m_clear_color;
    bool m_is_running;
	public:
    Kernel(const char *caption, int width, int height);
    ~Kernel();
  public:
    void set_clear_color(glm::dvec3 clear_color);
  public:
    void run();
  public:
    void halt();
  private:
    void run_main_loop_iter();
    void dispatch_events();
    void draw();
    void update();
  private:
    static wgpu::Adapter request_adapter(wgpu::Instance instance, wgpu::RequestAdapterOptions const *options);
    static wgpu::Device request_device(wgpu::Adapter adapter, wgpu::DeviceDescriptor const *descriptor);
  private:
    static void on_uncaptured_error(WGPUErrorType error_type, const char *message, void *p_user_data);
    static const char *get_error_type_str(wgpu::ErrorType error_type);
  private:
    void draw_to_texture_view(wgpu::TextureView target_texture_view);
    void draw_with_render_pass_encoder(wgpu::RenderPassEncoder rp_encoder);
	};
}
