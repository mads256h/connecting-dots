# Generic test that uses conan libs
add_executable(connecting-dots main.cpp)
target_link_libraries(
  connecting-dots
  PRIVATE project_options
          project_warnings
	  glfw
	  GLEW
	  GL
	  docopt)

