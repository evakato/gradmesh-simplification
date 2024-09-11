cmake_minimum_required(VERSION 3.0.0)
project(opengl_proj VERSION 0.1.0)

include_directories(libs include libs/imgui libs/imgui/backends)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

file(GLOB SOURCES "src/*.cpp")

add_executable(
    opengl_proj 
    ${SOURCES}
    libs/glad/glad.c
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/backends/imgui_impl_glfw.cpp
    libs/imgui/backends/imgui_impl_opengl3.cpp
)
target_link_libraries(opengl_proj glfw OpenGL::GL)