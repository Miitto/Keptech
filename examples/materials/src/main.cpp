#include <keptech/app.hpp>
#include <keptech/core/window.hpp>

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

class App : public keptech::App {
public:
  App(keptech::core::window::Window&& window)
      : keptech::App(std::move(window)) {}

  void update() override {
    // Update application logic here
  }
};

int main() {
  keptech::core::window::init();

  {
    keptech::core::window::Window window("Material Editor", WINDOW_WIDTH,
                                         WINDOW_HEIGHT);

    App app(std::move(window));

    keptech::run<App>(app);
  }

  keptech::core::window::shutdown();

  return 0;
}
