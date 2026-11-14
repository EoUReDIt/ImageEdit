cmake_minimum_required(VERSION 3.16)
project(streambench CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Vulkan headers + volk (loads vulkan-1.dll / libvulkan.so at run time), so no
# Vulkan SDK is needed to build. The shader is committed as SPIR-V in
# shaders/qmatmul.spv.h; regenerate with:
#   glslangValidator -V --vn qmatmul_spv -o shaders/qmatmul.spv.h shaders/qmatmul.comp
include(FetchContent)
FetchContent_Declare(vulkan_headers
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
  GIT_TAG v1.3.290 GIT_SHALLOW TRUE SOURCE_SUBDIR _none)
FetchContent_Declare(volk
  GIT_REPOSITORY https://github.com/zeux/volk.git
  GIT_TAG 1.3.295 GIT_SHALLOW TRUE SOURCE_SUBDIR _none)
FetchContent_MakeAvailable(vulkan_headers volk)

add_executable(streambench main.cpp ${volk_SOURCE_DIR}/volk.c)
target_include_directories(streambench PRIVATE
  ${vulkan_headers_SOURCE_DIR}/include ${volk_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Threads REQUIRED)
target_link_libraries(streambench PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if(MSVC)
  target_compile_options(streambench PRIVATE /W3)
  set_property(TARGET streambench PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
else()
  target_compile_options(streambench PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()
