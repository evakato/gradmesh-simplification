cmake_minimum_required(VERSION 3.12)
project(gms VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(libs include libs/stb libs/imgui libs/imgui/backends libs/ssim/include libs/flip/src/cpp libs/glm)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

file(GLOB SOURCES "src/*.cpp")

add_executable(
    gms 
    ${SOURCES}
    libs/glad/glad.c
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/backends/imgui_impl_glfw.cpp
    libs/imgui/backends/imgui_impl_opengl3.cpp
    libs/ssim/src/ssim.cpp
    libs/ssim/src/ssim-openmp.cpp
    libs/ssim/src/ssim_avx.cpp
    libs/ssim/src/ssim_avx512.cpp
    libs/ssim/src/ssim_sse.cpp
    libs/ssim/src/ssim_fma.cpp
)

link_directories(/usr/lib64)
target_link_libraries(gms glfw OpenGL::GL)

# Enable OpenMP support
find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    target_link_libraries(gms OpenMP::OpenMP_CXX)
    target_compile_definitions(gms PRIVATE RMGR_SSIM_USE_OPENMP)  # Define macro for OpenMP usage
    target_compile_options(gms PRIVATE ${OpenMP_CXX_FLAGS})  # Add OpenMP compile flags
endif()