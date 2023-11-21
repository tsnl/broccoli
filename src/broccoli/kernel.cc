#include "broccoli/kernel.hh"

#include "webgpu/webgpu_cpp.h"
#include "webgpu/webgpu_glfw.h"
#include "dawn/dawn_proc.h"
#include "dawn/native/DawnNative.h"
#include "GLFW/glfw3.h"

namespace broccoli {
  Kernel::Kernel(const char *caption, int width, int height)
  : m_glfw_window(nullptr),
    m_wgpu_instance(nullptr),
    m_wgpu_surface(nullptr),
    m_wgpu_adapter(nullptr),
    m_wgpu_device(nullptr),
    m_wgpu_swapchain(nullptr),
    m_clear_color(0.0, 0.0, 0.0),
    m_is_running(false)
  {
    CHECK(glfwInit(), "Failed to initialize GLFW");

    dawnProcSetProcs(&dawn::native::GetProcs());

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_glfw_window = glfwCreateWindow(width, height, caption, nullptr, nullptr);
    CHECK(m_glfw_window != nullptr, "Failed to create a window with GLFW");

    int framebuffer_width, framebuffer_height;
    glfwGetFramebufferSize(m_glfw_window, &framebuffer_width, &framebuffer_height);

    wgpu::InstanceDescriptor instance_descriptor = {.nextInChain = nullptr};
    m_wgpu_instance = wgpu::CreateInstance(&instance_descriptor);
    CHECK(m_wgpu_instance != nullptr, "Failed to create a WebGPU instance");

    m_wgpu_surface = wgpu::glfw::CreateSurfaceForWindow(m_wgpu_instance, m_glfw_window);
    CHECK(m_wgpu_surface != nullptr, "Failed to create a WebGPU surface");

    wgpu::RequestAdapterOptions adapter_opts = {.compatibleSurface=m_wgpu_surface};
    m_wgpu_adapter = request_adapter(m_wgpu_instance, &adapter_opts);

    wgpu::DeviceDescriptor device_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Kernel.DeviceDescriptor",
      .requiredFeaturesCount = 0,
      .requiredLimits = nullptr,
      .defaultQueue = {
        .nextInChain = nullptr,
        .label = "Broccoli.Kernel.DefaultQueue",
      },
    };
    m_wgpu_device = request_device(m_wgpu_adapter, &device_descriptor);
    m_wgpu_device.SetUncapturedErrorCallback(Kernel::on_uncaptured_error, nullptr);

    wgpu::SwapChainDescriptor swapchain_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.Kernel.SwapchainDescriptor",
      .usage = wgpu::TextureUsage::RenderAttachment,
      .format = wgpu::TextureFormat::BGRA8Unorm,
      .width = static_cast<uint32_t>(framebuffer_width),
      .height = static_cast<uint32_t>(framebuffer_height),
      .presentMode = wgpu::PresentMode::Mailbox,
    };
    m_wgpu_swapchain = m_wgpu_device.CreateSwapChain(m_wgpu_surface, &swapchain_descriptor);
  }
  Kernel::~Kernel() {
    m_wgpu_swapchain.Release();
    m_wgpu_surface.Release();
    m_wgpu_adapter.Release();
    m_wgpu_instance.Release();
    glfwDestroyWindow(m_glfw_window);
    glfwTerminate();
  }
}
namespace broccoli {
  void Kernel::set_clear_color(glm::dvec3 clear_color) {
    m_clear_color = clear_color;
  }
}
namespace broccoli {
  void Kernel::run() {
    m_is_running = true;
    run_main_loop_iter();
    glfwShowWindow(m_glfw_window);
    while (!glfwWindowShouldClose(m_glfw_window)) {
      run_main_loop_iter();
    }
  }
  void Kernel::run_main_loop_iter() {
    dispatch_events();
    update();
    draw();
  }
}
namespace broccoli {
  void Kernel::halt() {
    m_is_running = false;
  }
}
namespace broccoli {
  void Kernel::dispatch_events() {
    glfwPollEvents();
  }
  void Kernel::draw() {
    wgpu::TextureView target_texture_view = m_wgpu_swapchain.GetCurrentTextureView();
    CHECK(target_texture_view != nullptr, "Cannot acquire next swapchain texture view");
    draw_to_texture_view(target_texture_view);
    target_texture_view.Release();
    m_wgpu_swapchain.Present();
  }
  void Kernel::update() {
  }
}
namespace broccoli {
  struct AdapterData { WGPUAdapter adapter; bool request_ended; };
  struct DeviceData { WGPUDevice device; bool request_ended; };
  wgpu::Adapter Kernel::request_adapter(wgpu::Instance instance, wgpu::RequestAdapterOptions const *options) {
    AdapterData adapter_data{nullptr, false};
    auto const on_adapter_request_ended = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, void *p_user_data) {
      auto &data = *reinterpret_cast<AdapterData*>(p_user_data);
      CHECK(
        status == WGPURequestAdapterStatus_Success, 
        [message] () { return fmt::format("Could not get WebGPU adapter: {}", message); }
      );
      data.adapter = adapter;
      data.request_ended = true;
    };
    instance.RequestAdapter(options, on_adapter_request_ended, reinterpret_cast<void*>(&adapter_data));
    CHECK(adapter_data.request_ended, "expected async call to wgpuInstanceRequestAdapter to actually be sync");
    return static_cast<wgpu::Adapter>(adapter_data.adapter);
  }
  wgpu::Device Kernel::request_device(wgpu::Adapter adapter, wgpu::DeviceDescriptor const *descriptor) {
    DeviceData device_data;
    auto const on_device_request_ended = [](WGPURequestDeviceStatus status, WGPUDevice device, char const *message, void *p_user_data) {
      DeviceData& data = *reinterpret_cast<DeviceData*>(p_user_data);
      CHECK(
        status == WGPURequestDeviceStatus_Success, 
        [message] () { return fmt::format("Could not get WebGPU device: {}", message); }
      );
      data.device = device;
      data.request_ended = true;
    };
    adapter.RequestDevice(descriptor, on_device_request_ended, reinterpret_cast<void*>(&device_data));
    CHECK(device_data.request_ended, "expected async call to wgpuAdapterRequestDevice to actually be sync");
    return device_data.device;
  }
}
namespace broccoli {
  void Kernel::on_uncaptured_error(WGPUErrorType error_type, const char *message, void *p_user_data) {
    (void)p_user_data;
    auto error_type_str = get_error_type_str(static_cast<wgpu::ErrorType>(error_type));
    PANIC("Uncaptured device error: type {}\nmessage: {}", error_type_str, message);
  }
  const char *Kernel::get_error_type_str(wgpu::ErrorType error_type) {
    switch (error_type) {
      case wgpu::ErrorType::NoError: 
        return "NoError";
      case wgpu::ErrorType::Validation: 
        return "Validation";
      case wgpu::ErrorType::OutOfMemory: 
        return "OutOfMemory";
      case wgpu::ErrorType::Internal: 
        return "Internal";
      case wgpu::ErrorType::Unknown: 
        return "Unknown";
      case wgpu::ErrorType::DeviceLost: 
        return "DeviceLost";
      default:
        return "<NotImplemented>"; 
    }
  }
}
namespace broccoli {
  void Kernel::draw_to_texture_view(wgpu::TextureView target_texture_view) {
    wgpu::CommandEncoderDescriptor command_encoder_descriptor = {.nextInChain = nullptr, .label = "Broccoli.CEDraw"};
    wgpu::CommandEncoder command_encoder = m_wgpu_device.CreateCommandEncoder(&command_encoder_descriptor);

    wgpu::RenderPassColorAttachment rp_surface_color_attachment = {
      .view = target_texture_view,
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue = wgpu::Color{.r=m_clear_color.x, .g=m_clear_color.y, .b=m_clear_color.z, .a=1.0},
    };
    wgpu::RenderPassDescriptor rp_descriptor = {
      .nextInChain = nullptr,
      .label = "Broccoli.RPDraw",
      .colorAttachmentCount = 1,
      .colorAttachments = &rp_surface_color_attachment,
      .depthStencilAttachment = nullptr,
      .timestampWriteCount = 0,
      .timestampWrites = nullptr,
    };
    wgpu::RenderPassEncoder rp_encoder = command_encoder.BeginRenderPass(&rp_descriptor);
    {
      draw_with_render_pass_encoder(rp_encoder);
    }
    rp_encoder.End();
    wgpu::CommandBuffer draw_command_buffer = command_encoder.Finish();
    m_wgpu_device.GetQueue().Submit(1, &draw_command_buffer);
  }
  void Kernel::draw_with_render_pass_encoder(wgpu::RenderPassEncoder rp_encoder) {
    // TODO: implement this.
    (void)rp_encoder;
  }
}
