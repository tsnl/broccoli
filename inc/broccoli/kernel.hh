#pragma once

#include "broccoli/core.hh"

namespace broccoli {
	class Kernel {
	private:
		void *m_sdl_window;
    bool m_is_running;
	public:
    Kernel(const char *caption, int width, int height);
    ~Kernel();
  public:
    void run();
  public:
    void halt();
  private:
    void dispatch_events();
    void draw();
    void update();
	};
}
