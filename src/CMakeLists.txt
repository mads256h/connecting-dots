option(ENABLE_PULSE "Enable audio response with pulseaudio" OFF)

if(ENABLE_PULSE)
add_definitions(-DENABLE_PULSE)
add_executable(connecting-dots main.cpp pulse.cpp)
target_link_libraries(
  connecting-dots
  PRIVATE project_options
          project_warnings
	  glfw
	  GLEW
	  GL
	  docopt
	  pulse)
else()
add_executable(connecting-dots main.cpp)
target_link_libraries(
  connecting-dots
  PRIVATE project_options
          project_warnings
	  glfw
	  GLEW
	  GL
	  docopt)
endif()
