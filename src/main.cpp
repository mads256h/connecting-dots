#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <random>

#include <cmath>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

int screen_width = SCREEN_WIDTH;
int screen_height = SCREEN_HEIGHT;

const float max_distance = 200.0f;

struct Point
{
  GLfloat x = 0.0f;
  GLfloat y = 0.0f;
};

class MovingPoint
{
public:
  MovingPoint(Point &_point, const GLfloat startx, const GLfloat starty, const float _speedx, const float _speedy, const bool _move_right, const bool _move_down) : point(_point), speedx(_speedx), speedy(_speedy), move_right(_move_right), move_down(_move_down)
  {
    point.x = startx;
    point.y = starty;
  };

  void update()
  {
    if (move_right)
      point.x += speedx;
    else
      point.x -= speedx;

    if (move_down)
      point.y += speedy;
    else
      point.y -= speedy;

    if (move_right && point.x >= static_cast<float>(screen_width))
      move_right = false;
    if (!move_right && point.x <= 0.0f)
      move_right = true;

    if (move_down && point.y >= static_cast<float>(screen_height))
      move_down = false;
    if (!move_down && point.y <= 0.0f)
      move_down = true;
  }

private:
  Point &point;


  float speedx = 0.0f;
  float speedy = 0.0f;

  bool move_right = false;
  bool move_down = false;
};

float distance(const Point point1, const Point point2)
{
  float xs = 0.0f;
  float ys = 0.0f;

  xs = point2.x - point1.x;
  xs = xs * xs;

  ys = point2.y - point1.y;
  ys = ys * ys;

  return std::sqrt(xs + ys);
}

int main(void)
{
  GLFWwindow *window;

  // Initialize the library
  if (!glfwInit()) {
    return -1;
  }


  glfwWindowHint(GLFW_SAMPLES, 4);
  //glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  //glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);


  // Create a windowed mode window and its OpenGL context
  window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Connecting dots", NULL, NULL);

  if (!window) {
    glfwTerminate();
    return -1;
  }

  // Make the window's context current
  glfwMakeContextCurrent(window);

  glfwSetWindowSizeCallback(window, [](GLFWwindow *, int width, int height) {
    screen_width = width;
    screen_height = height;
    glViewport(0.0f, 0.0f, screen_width, screen_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, screen_width, 0, screen_height, 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
  });

  glViewport(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, SCREEN_WIDTH, 0, SCREEN_HEIGHT, 0, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  const int amount = 100;

  std::mt19937 random;
  std::uniform_real_distribution<> distw(0, screen_width);
  std::uniform_real_distribution<> disth(0, screen_height);

  std::uniform_real_distribution<> dists(0, 2);

  std::uniform_int_distribution<> distb(0, 1);


  std::vector<Point> v_points;
  v_points.reserve(amount);

  std::vector<MovingPoint> v_moving_points;
  v_moving_points.reserve(amount);

  for (int i = 0; i < amount; i++) {
    MovingPoint point(v_points.emplace_back(), static_cast<float>(distw(random)), static_cast<float>(disth(random)), static_cast<float>(dists(random)), static_cast<float>(dists(random)), distb(random), distb(random));

    v_moving_points.push_back(point);
  }

  glEnable(GL_BLEND);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_POINT_SMOOTH);
  glLineWidth(0.05f);
  glPointSize(4);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Loop until the user closes the window
  while (!glfwWindowShouldClose(window)) {

    for (auto &m_point : v_moving_points) {
      m_point.update();
    }
    glClear(GL_COLOR_BUFFER_BIT);

    // Render OpenGL here
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, v_points.data());
    glDrawArrays(GL_POINTS, 0, amount);
    glDisableClientState(GL_VERTEX_ARRAY);

    const auto col = 1.0f;
    glColor4f(col, col, col, 0.2f);
    for (auto point1 : v_points) {
      double xpos, ypos;
      glfwGetCursorPos(window, &xpos, &ypos);
      Point point{ static_cast<float>(xpos), static_cast<float>(screen_height) - static_cast<float>(ypos) };
      if (auto dist = distance(point1, point); dist < max_distance) {
        glBegin(GL_LINES);
        glVertex2f(point1.x, point1.y);
        glVertex2f(point.x, point.y);
        glEnd();
      }


      for (auto point2 : v_points) {
        if (auto dist = distance(point1, point2); dist != 0.0f && dist < max_distance) {
          glColor4f(1.0f, 1.0f, 1.0f, (max_distance - dist) / (max_distance * 2.0f));
          glBegin(GL_LINES);
          glVertex2f(point1.x, point1.y);
          glVertex2f(point2.x, point2.y);
          glEnd();
        }
      }
    }


    // Swap front and back buffers
    glfwSwapBuffers(window);

    // Poll for and process events
    glfwPollEvents();
  }

  glfwTerminate();

  return 0;
}
