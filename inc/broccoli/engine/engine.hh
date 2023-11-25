#pragma once

#include <stack>
#include <queue>
#include <utility>
#include <functional>
#include <memory>

#include "webgpu/webgpu_cpp.h"
#include "GLFW/glfw3.h"

#include "core.hh"
#include "render.hh"

struct GLFWwindow;

namespace broccoli {
  class Engine;
  class Glfw;
  class Activity;
}

namespace broccoli {
  class Activity {
  public:
    enum class StackAction { Push, Pop, Swap };
    using BuildCb = std::function<std::unique_ptr<Activity>(Engine &engine)>;
  private:
    Engine &m_engine;
  protected:
    Activity(Engine &engine);
  public:
    Activity() = delete;
    Activity(Activity const &other) = delete;
    Activity(Activity &&other) = default;
  public:
    virtual ~Activity() = default;
  public:
    virtual void activate();
    virtual void update(double dt_sec);
    virtual void fixedUpdate(double dt_sec);
    virtual void draw(RenderFrame &frame);
    virtual void deactivate();
  public:
    Engine &engine();
  };
}

namespace broccoli {
  class Glfw {
  private:
    GLFWwindow *m_window;
  public:
    Glfw(glm::ivec2 size, const char *caption, bool fullscreen);
    ~Glfw();
  public:
    GLFWwindow *window() const;
  };
}

namespace broccoli {
	class Engine {
	private:
		Glfw m_glfw;
    wgpu::Instance m_wgpu_instance;
    wgpu::Surface m_wgpu_surface;
    wgpu::Adapter m_wgpu_adapter;
    wgpu::Device m_wgpu_device;
    wgpu::SwapChain m_wgpu_swapchain;
    glm::ivec2 m_framebuffer_size;
    std::stack<std::unique_ptr<Activity>> m_activity_stack;
    std::queue<std::pair<Activity::StackAction, std::optional<Activity::BuildCb>>> m_activity_stack_action_fifo;
    double m_prev_update_timestamp_sec;
    double m_curr_update_timestamp_sec;
    double m_curr_update_dt_sec;
    double m_fixed_update_accum_time;
    double m_fixed_update_delta_sec;
    std::unique_ptr<RenderManager> m_renderer;
    bool m_is_running;
	public:
    Engine(glm::ivec2 size, const char *caption, bool fullscreen = false, double fixed_update_hz = 120.0);
    ~Engine() = default;
  public:
    void run();
    void halt();
  public:
    void pushActivity(Activity::BuildCb activity);
    void swapActivity(Activity::BuildCb activity);
    void popActivity();
  public:
    MeshBuilder createMeshBuilder();
    MeshFactory createMeshFactory();
  public:
    double currUpdateTimestampSec() const;
    double currUpdateDtSec() const;
  private:
    void beginFrame();
    void endFrame();
    void dispatchEvents();
    void draw();
    void update();
    void updateActivityStack();
  private:
    static wgpu::Adapter requestAdapter(wgpu::Instance instance, wgpu::RequestAdapterOptions const *options);
    static wgpu::Device requestDevice(wgpu::Adapter adapter, wgpu::DeviceDescriptor const *descriptor);
  private:
    static void onUncapturedWgpuError(WGPUErrorType error_type, const char *message, void *p_user_data);
    static void onWgpuLog(WGPULoggingType logging_type, const char *message, void *p_user_data);
    static const char *getErrorTypeStr(WGPUErrorType error_type);
    static const char *getLoggingTypeStr(WGPULoggingType error_type);
  private:
    void pushActivityImpl(Activity::BuildCb build_cb);
    void swapActivityImpl(Activity::BuildCb build_cb);
    void popActivityImpl();
	};
}
