#include <keptech/app.hpp>

#include <keptech/core/window.hpp>
#include <keptech/gui.h>
#include <keptech/vulkan/renderer.hpp>
#include <spdlog/spdlog.h>

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

template <class R>
  requires(keptech::core::renderer::Renderer<R>)
class App : public keptech::App {
public:
  App(keptech::core::window::Window&& window, R* renderer)
      : keptech::App(std::move(window)), renderer(renderer) {}

  void update() override {
    {
      keptech::gui::Frame frame("Demo");
      frame.text("Material Editor Example");
    }
  }

  void onEvent(const keptech::core::window::Window::Event& event) override {
    // Handle window events here
  }

private:
  R* renderer;
};

int main() {
  keptech::core::window::init();

  {
    keptech::core::window::Window window("Material Editor", WINDOW_WIDTH,
                                         WINDOW_HEIGHT);

    auto renderer_res =
        keptech::vkh::Renderer::create("Material Editor", window);
    if (!renderer_res) {
      SPDLOG_CRITICAL("Failed to create Vulkan renderer: {}",
                      renderer_res.error());
      return -1;
    }

    auto renderer = std::move(renderer_res.value());

    App app(std::move(window), &renderer);

    SPDLOG_INFO("Starting Material Editor");
    keptech::run(app, renderer);
  }

  keptech::core::window::shutdown();

  return 0;
}
