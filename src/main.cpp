#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <random>
#include <chrono>
#include <map>
#include <string>
#include <memory>

#include <cstdlib>
#include <cmath>
#include <cstddef>

#include <docopt/docopt.h>

#ifdef ENABLE_PULSE
#include <pulse/pulseaudio.h>
#include "pulse.h"
#endif


static const char *const USAGE =
  R"(Connecting dots.

    Usage:
      connecting-dots [options]
      connecting-dots (-h | --help)
      connecting-dots --version

    Options:
      -h --help     Show this screen.
      --version     Show version.
      --width=<w>   Width of the window [default: 640].
      --height=<h>  Height of the window [default: 480].
      --amount=<n>  Amount of dots [default: 100].
      --lspeed=<s>  Dots minimum speed [default: 0.0].
      --hspeed=<s>  Dots maximum speed [default: 2.0].
      --bcolor=<c>  Background color [default: 000000].
      --dcolor=<c>  Dot color [default: FFFFFF].
      --lcolor=<c>  Line color [default: FFFFFF].
      --dsize=<s>   Dot size [default: 4.0].
      --lwidth=<w>  Line width [default: 0.05].
      --fade        Lines fade in as the dots gets closer.
      --mouse       Lines connect to the mouse.
      --fullscreen  Make the window fullscreen.
      --trans       Make the window transparent.
      --nodecor     Removes window decoration.
)"
#ifdef ENABLE_PULSE
  "      --audio       React to audio.\n"
#endif
  ;

int screen_width = 0;
int screen_height = 0;

constexpr float max_distance = 200.0F * 200.0F;

struct Point
{
  GLfloat x = 0.0F;
  GLfloat y = 0.0F;
};

class MovingPoint
{
public:
  MovingPoint(Point &_point,
    const GLfloat startx,
    const GLfloat starty,
    const GLfloat _speedx,
    const GLfloat _speedy,
    const bool _move_right,
    const bool _move_down)
    : point(_point), speedx(_speedx), speedy(_speedy), move_right(_move_right),
      move_down(_move_down)
  {
    point.x = startx;
    point.y = starty;
  };

  void update()
  {
    if (move_right) {
      point.x += speedx;
    } else {
      point.x -= speedx;
    }

    if (move_down) {
      point.y += speedy;
    } else {
      point.y -= speedy;
    }

    if (move_right && point.x >= static_cast<float>(screen_width)) { move_right = false; }
    if (!move_right && point.x <= 0.0F) { move_right = true; }

    if (move_down && point.y >= static_cast<float>(screen_height)) { move_down = false; }
    if (!move_down && point.y <= 0.0F) { move_down = true; }
  }

private:
  Point &point;


  GLfloat speedx = 0.0F;
  GLfloat speedy = 0.0F;

  bool move_right = false;
  bool move_down = false;
};

struct rgb
{
  float r = 0.0F;
  float g = 0.0F;
  float b = 0.0F;
};

constexpr rgb to_rgb(const unsigned int in)
{
  return rgb{ static_cast<float>(static_cast<std::byte>(in >> 8U * 0U))
                / static_cast<float>(UINT8_MAX),
    static_cast<float>(static_cast<std::byte>(in >> 8U * 1U)) / static_cast<float>(UINT8_MAX),
    static_cast<float>(static_cast<std::byte>(in >> 8U * 2U)) / static_cast<float>(UINT8_MAX) };
}

void initialize_projection()
{
  glViewport(0.0F, 0.0F, screen_width, screen_height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, screen_width, 0, screen_height, 0, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

[[nodiscard]] constexpr float distance(const Point point1, const Point point2)
{
  const GLfloat xs = point2.x - point1.x;
  const GLfloat ys = point2.y - point1.y;

  return (xs * xs + ys * ys);
}

int main(int argc, char **argv)
{
  const std::map<std::string, docopt::value> args =
    docopt::docopt(USAGE, { argv + 1, argv + argc }, true, "Connecting dots 1.0");

  screen_width = static_cast<int>(args.at("--width").asLong());
  screen_height = static_cast<int>(args.at("--height").asLong());

  const int amount = static_cast<int>(args.at("--amount").asLong());

  const float max_speed = std::stof(args.at("--hspeed").asString());
  const float min_speed = std::stof(args.at("--lspeed").asString());

  const unsigned int background_color =
    static_cast<unsigned int>(std::stoi(args.at("--bcolor").asString(), nullptr, 16));
  const unsigned int dot_color =
    static_cast<unsigned int>(std::stoi(args.at("--dcolor").asString(), nullptr, 16));
  const unsigned int line_color =
    static_cast<unsigned int>(std::stoi(args.at("--lcolor").asString(), nullptr, 16));

  const rgb bc = to_rgb(background_color);
  const rgb dc = to_rgb(dot_color);
  const rgb lc = to_rgb(line_color);


  const GLfloat dot_size = std::stof(args.at("--dsize").asString());
  const GLfloat line_width = std::stof(args.at("--lwidth").asString());

  const bool fade = args.at("--fade").asBool();
  const bool mouse = args.at("--mouse").asBool();

  const bool fullscreen = args.at("--fullscreen").asBool();
  const bool transparent = args.at("--trans").asBool();
  const bool no_decorations = args.at("--nodecor").asBool();

#ifdef ENABLE_PULSE
  const bool audio = args.at("--audio").asBool();

  std::shared_ptr<pa_mainloop> mainloop(pa_mainloop_new(), pa_mainloop_free);
  std::shared_ptr<pa_context> context(
    pa_context_new(pa_mainloop_get_api(mainloop.get()), "Connecting dots"), pa_context_unref);

  if (audio) {
    pa_signal_init(pa_mainloop_get_api(mainloop.get()));

    pa_context_set_state_callback(context.get(), context_state_callback, nullptr);
    pa_context_connect(context.get(), nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr);
  }
#endif


  // Initialize the library
  if (glfwInit() == GLFW_FALSE) { return EXIT_FAILURE; }


  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, transparent ? GLFW_TRUE : GLFW_FALSE);
  glfwWindowHint(GLFW_DECORATED, no_decorations ? GLFW_FALSE : GLFW_TRUE);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

  // glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  // glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);


  // Create a windowed mode window and its OpenGL context
  GLFWwindow *window = glfwCreateWindow(screen_width,
    screen_height,
    "Connecting dots",
    fullscreen ? glfwGetPrimaryMonitor() : nullptr,
    nullptr);

  if (window == nullptr) {
    glfwTerminate();
    return EXIT_FAILURE;
  }

  // Make the window's context current
  glfwMakeContextCurrent(window);

  glfwSetWindowSizeCallback(window, [](GLFWwindow * /* unused */, int width, int height) {
    screen_width = width;
    screen_height = height;

    initialize_projection();
  });

  initialize_projection();


  const std::mt19937::result_type seed =
    static_cast<std::mt19937::result_type>(std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch())
                                             .count());

  std::mt19937 random(seed);
  std::uniform_real_distribution<> distw(0, screen_width);
  std::uniform_real_distribution<> disth(0, screen_height);

  std::uniform_real_distribution<> dists(static_cast<double>(min_speed), static_cast<double>(max_speed));

  std::uniform_int_distribution<> distb(0, 1);


  std::vector<Point> v_points;
  v_points.reserve(static_cast<decltype(v_points)::size_type>(amount));

  std::vector<MovingPoint> v_moving_points;
  v_moving_points.reserve(static_cast<decltype(v_moving_points)::size_type>(amount));

  for (int i = 0; i < amount; i++) {
    MovingPoint point(v_points.emplace_back(),
      static_cast<float>(distw(random)),
      static_cast<float>(disth(random)),
      static_cast<float>(dists(random)),
      static_cast<float>(dists(random)),
      static_cast<bool>(distb(random)),
      static_cast<bool>(distb(random)));

    v_moving_points.push_back(point);
  }

  glEnable(GL_BLEND);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_POINT_SMOOTH);
  glPointSize(dot_size);
  glLineWidth(line_width);
  glClearColor(bc.r, bc.g, bc.b, 0.0F);


  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Loop until the user closes the window
  while (glfwWindowShouldClose(window) == 0) {

    for (auto &m_point : v_moving_points) { m_point.update(); }
    glClear(GL_COLOR_BUFFER_BIT);

    // Render OpenGL here
    glColor4f(dc.r, dc.g, dc.b, 1.0F);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, v_points.data());
    glDrawArrays(GL_POINTS, 0, amount);
    glDisableClientState(GL_VERTEX_ARRAY);

#ifdef ENABLE_PULSE
    const float lev = audio ? level * 1.25F : 1.0F;
#else
    const float lev = 1.0F;
#endif

    const auto col = 1.0F;
    glColor4f(col, col, col, 0.2F);
    glBegin(GL_LINES);
    for (const auto point1 : v_points) {
      if (mouse) {
        double xpos = 0.0;
        double ypos = 0.0;
        glfwGetCursorPos(window, &xpos, &ypos);
        const Point point{ static_cast<float>(xpos),
          static_cast<float>(screen_height) - static_cast<float>(ypos) };
        if (const auto dist = distance(point1, point); dist < max_distance) {
          glVertex2f(point1.x, point1.y);
          glVertex2f(point.x, point.y);
        }
      }


      for (const auto point2 : v_points) {
        if (const auto dist = distance(point1, point2); dist != 0.0F && dist < max_distance) {
          glColor4f(
            lc.r, lc.g, lc.b, (fade ? (max_distance - dist) / (max_distance * 2.0F) : 0.5F) * lev);
          glVertex2f(point1.x, point1.y);
          glVertex2f(point2.x, point2.y);
        }
      }
    }

    glEnd();


    // Swap front and back buffers
    glfwSwapBuffers(window);

    // Poll for and process events
    glfwPollEvents();

#ifdef ENABLE_PULSE
    int retval = 0;
    if (pa_mainloop_iterate(mainloop.get(), 0, &retval) < 0){
      return retval;
    }

    if (audio){
      level -= 0.032F;
    }
#endif
  }

  glfwTerminate();

  return EXIT_SUCCESS;
}
