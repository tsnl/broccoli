#include "broccoli/engine.hh"

#include <iostream>

#include "webgpu/webgpu_cpp.h"
#include "webgpu/webgpu_glfw.h"
#include "dawn/dawn_proc.h"
#include "dawn/native/DawnNative.h"
#include "GLFW/glfw3.h"

//
// Activity:
//

namespace broccoli {
  void Activity::activate() {}
  void Activity::update(double dt_sec) { (void)dt_sec; }
  void Activity::fixed_update(double dt_sec) { (void)dt_sec; }
  void Activity::draw(Renderer &device) { (void)device; }
  void Activity::deactivate() {}
}

//
// Engine
//

namespace broccoli {
  Engine::Engine(const char *caption, int width, int height, double fixed_update_hz)
  : m_glfw_window(nullptr),
    m_wgpu_instance(nullptr),
    m_wgpu_surface(nullptr),
    m_wgpu_adapter(nullptr),
    m_wgpu_device(nullptr),
    m_wgpu_swapchain(nullptr),
    m_activity_stack(),
    m_activity_stack_action_fifo(),
    m_prev_update_timestamp(0.0),
    m_fixed_update_accum_time(0.0),
    m_fixed_update_delta_sec(1.0 / fixed_update_hz),
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
    m_wgpu_device.SetUncapturedErrorCallback(Engine::on_uncaptured_error, nullptr);

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
  Engine::~Engine() {
    m_wgpu_swapchain.Release();
    m_wgpu_surface.Release();
    m_wgpu_adapter.Release();
    m_wgpu_instance.Release();
    glfwDestroyWindow(m_glfw_window);
    glfwTerminate();
  }
}
namespace broccoli {
  void Engine::run() {
    update_activity_stack();
    draw();
    m_prev_update_timestamp = glfwGetTime();
    m_fixed_update_accum_time = 0.0;
    m_is_running = true;
    glfwShowWindow(m_glfw_window);
    while (!glfwWindowShouldClose(m_glfw_window)) {
      dispatch_events();
      update();
      draw();
      update_activity_stack();
    }
  }
}
namespace broccoli {
  void Engine::halt() {
    m_is_running = false;
  }
}
namespace broccoli {
  void Engine::push_activity(Activity::BuildCb activity_build_cb) {
    m_activity_stack_action_fifo.emplace(Activity::StackAction::Push, std::move(activity_build_cb));
  }
  void Engine::swap_activity(Activity::BuildCb activity_build_cb) {
    m_activity_stack_action_fifo.emplace(Activity::StackAction::Swap, std::move(activity_build_cb));
  }
  void Engine::pop_activity() {
    std::optional<Activity::BuildCb> none;
    m_activity_stack_action_fifo.emplace(Activity::StackAction::Pop, std::move(none));
  }
}
namespace broccoli {
  void Engine::dispatch_events() {
    glfwPollEvents();
  }
  void Engine::draw() {
    wgpu::TextureView target_texture_view = m_wgpu_swapchain.GetCurrentTextureView();
    CHECK(target_texture_view != nullptr, "Cannot acquire next swapchain texture view");
    if (!m_activity_stack.empty()) {
      {
        Renderer renderer{m_wgpu_device, target_texture_view};
        m_activity_stack.top()->draw(renderer);
        // 'renderer' goes out of scope here, resulting in the associated command buffer being submitted.
      }
    }
    target_texture_view.Release();
    m_wgpu_swapchain.Present();
  }
  void Engine::update() {
    double curr_update_timestamp = glfwGetTime();
    double dt_sec = curr_update_timestamp - m_prev_update_timestamp;
    m_fixed_update_accum_time += dt_sec;
    if (!m_activity_stack.empty()) {
      m_activity_stack.top()->update(dt_sec);
      while (m_fixed_update_accum_time >= m_fixed_update_delta_sec) {
        m_fixed_update_accum_time -= m_fixed_update_delta_sec;
        m_activity_stack.top()->fixed_update(m_fixed_update_delta_sec);
      }
    }
    m_prev_update_timestamp = curr_update_timestamp;
  }
  void Engine::update_activity_stack() {
    while (!m_activity_stack_action_fifo.empty()) {
      auto [action_type, opt_build_cb] = std::move(m_activity_stack_action_fifo.front());
      m_activity_stack_action_fifo.pop();
      switch (action_type) {
        case Activity::StackAction::Push:
          CHECK(opt_build_cb.has_value(), "expected 'Push' action to have a companion builder callback");
          push_activity_impl(std::move(opt_build_cb.value()));
          break;
        case Activity::StackAction::Swap:
          CHECK(opt_build_cb.has_value(), "expected 'Swap' action to have a companion builder callback");
          swap_activity_impl(std::move(opt_build_cb.value()));
          break;
        case Activity::StackAction::Pop:
          CHECK(!opt_build_cb.has_value(), "expected 'Pop' action to have no companion builder callback");
          pop_activity_impl();
          break;
        default:
          PANIC("Unknown stack action: {}", static_cast<int>(action_type));
          break;
      }
    }
  }
}
namespace broccoli {
  struct AdapterData { WGPUAdapter adapter; bool request_ended; };
  struct DeviceData { WGPUDevice device; bool request_ended; };
  wgpu::Adapter Engine::request_adapter(wgpu::Instance instance, wgpu::RequestAdapterOptions const *options) {
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
  wgpu::Device Engine::request_device(wgpu::Adapter adapter, wgpu::DeviceDescriptor const *descriptor) {
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
  void Engine::on_uncaptured_error(WGPUErrorType error_type, const char *message, void *p_user_data) {
    (void)p_user_data;
    auto error_type_str = get_error_type_str(static_cast<wgpu::ErrorType>(error_type));
    PANIC("Uncaptured device error: type {}\nmessage: {}", error_type_str, message);
  }
  const char *Engine::get_error_type_str(wgpu::ErrorType error_type) {
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
  void Engine::push_activity_impl(Activity::BuildCb build_cb) {
    if (!m_activity_stack.empty()) {
      m_activity_stack.top()->deactivate();
    }
    m_activity_stack.emplace(build_cb());
    m_activity_stack.top()->activate();
  }
  void Engine::swap_activity_impl(Activity::BuildCb build_cb) {
    CHECK(!m_activity_stack.empty(), "Expected activity stack to not be empty on 'swap'");
    m_activity_stack.top()->deactivate();
    {
      auto new_activity = build_cb();
      auto old_activity = std::move(m_activity_stack.top());
      m_activity_stack.pop();
      m_activity_stack.push(std::move(new_activity));
      // 'activity', which now contains the old top activity, goes out of scope here.
    }
    m_activity_stack.top()->activate();
  }
  void Engine::pop_activity_impl() {
    CHECK(!m_activity_stack.empty(), "Expected activity stack to not be empty on 'pop'");
    m_activity_stack.top()->deactivate();
    m_activity_stack.pop();
    m_activity_stack.top()->activate();
  }
}
