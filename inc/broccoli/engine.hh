#pragma once

#include <stack>
#include <queue>
#include <utility>
#include <functional>
#include <memory>

#include "webgpu/webgpu_cpp.h"
#include "GLFW/glfw3.h"

#include "broccoli/core.hh"
#include "broccoli/draw.hh"

struct GLFWwindow;

namespace broccoli {
  class Engine;
  class Activity;
}

namespace broccoli {
  class Activity {
  public:
    enum class StackAction { Push, Pop, Swap };
    using BuildCb = std::function<std::unique_ptr<Activity>()>;
  protected:
    Activity() = default;
  public:
    virtual ~Activity() = default;
  public:
    virtual void activate();
    virtual void update(double dt_sec);
    virtual void fixed_update(double dt_sec);
    virtual void draw(Renderer &device);
    virtual void deactivate();
  };
}

namespace broccoli {
	class Engine {
	private:
		GLFWwindow *m_glfw_window;
    wgpu::Instance m_wgpu_instance;
    wgpu::Surface m_wgpu_surface;
    wgpu::Adapter m_wgpu_adapter;
    wgpu::Device m_wgpu_device;
    wgpu::SwapChain m_wgpu_swapchain;
    std::stack<std::unique_ptr<Activity>> m_activity_stack;
    std::queue<std::pair<Activity::StackAction, std::optional<Activity::BuildCb>>> m_activity_stack_action_fifo;
    double m_prev_update_timestamp;
    double m_fixed_update_accum_time;
    double m_fixed_update_delta_sec;
    bool m_is_running;
	public:
    Engine(const char *caption, int width, int height, double fixed_update_hz = 120.0);
    ~Engine();
  public:
    void run();
  public:
    void halt();
  public:
    void push_activity(Activity::BuildCb activity);
    void swap_activity(Activity::BuildCb activity);
    void pop_activity();
  private:
    void dispatch_events();
    void draw();
    void update();
    void update_activity_stack();
  private:
    static wgpu::Adapter request_adapter(wgpu::Instance instance, wgpu::RequestAdapterOptions const *options);
    static wgpu::Device request_device(wgpu::Adapter adapter, wgpu::DeviceDescriptor const *descriptor);
  private:
    static void on_uncaptured_error(WGPUErrorType error_type, const char *message, void *p_user_data);
    static const char *get_error_type_str(wgpu::ErrorType error_type);
  private:
    void push_activity_impl(Activity::BuildCb build_cb);
    void swap_activity_impl(Activity::BuildCb build_cb);
    void pop_activity_impl();
	};
}
