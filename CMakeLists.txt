cmake_minimum_required(VERSION 3.10)

# set the project name
project(GLAudioVisApp)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# include everything under 'include'
include_directories(include)

# recursively get cpp files
file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.cpp)

# executable
add_executable(GLAudioVisApp ${sources})
target_compile_options(GLAudioVisApp PRIVATE -O3 -Wall -Wextra -Werror)
# target_compile_options(GLAudioVisApp PRIVATE -Wall -Wextra -Werror)

# libpthread
find_package(Threads REQUIRED)

# find SDL2 libraries from the system
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
message(STATUS "SDL2 includes from ${SDL2_INCLUDE_DIRS}")

# find OpenGL libraries from the system
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

# find PulseAudio
find_package(PulseAudio REQUIRED)

find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})
message(STATUS "GLEW includes from ${GLEW_INCLUDE_DIRS}")

# include everything under 'libs', which contains libraries supplied by the project
include_directories(libs)

# add fftw
#find_package(fftw3 REQUIRED) # this fails because of a bug in fedora?

add_subdirectory(libs/fftw-3.3.8 EXCLUDE_FROM_ALL bench) # bench doesn't link in subdir build???

#include_directories(libs/fftw-3.3.8/api)

# add fmt
add_subdirectory(libs/fmt)
include_directories(libs/fmt/include)

# whatever trevor
add_library(imgui STATIC
	libs/imgui/imconfig.h
	libs/imgui/imgui.cpp
	libs/imgui/imgui_demo.cpp
	libs/imgui/imgui_draw.cpp
	libs/imgui/imgui.h
	libs/imgui/imgui_impl_opengl3.cpp
	libs/imgui/imgui_impl_opengl3.h
	libs/imgui/imgui_impl_sdl.cpp
	libs/imgui/imgui_impl_sdl.h
	libs/imgui/imgui_internal.h
	libs/imgui/imgui_widgets.cpp
	libs/imgui/imstb_rectpack.h
	libs/imgui/imstb_textedit.h
	libs/imgui/imstb_truetype.h
)

# link our executable against external libraries
target_link_libraries(GLAudioVisApp Threads::Threads pulse pulse-simple fftw3 fmt imgui ${SDL2_LIBRARIES} ${GLEW_LIBRARIES} ${OPENGL_LIBRARY})
