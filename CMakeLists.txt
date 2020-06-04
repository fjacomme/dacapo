cmake_minimum_required(VERSION 3.10)
include(FetchContent)
include(ExternalProject)
set(CMAKE_CXX_STANDARD 17)

project(dacapo VERSION 0.1.0)

set(Boost_USE_STATIC_LIBS    ON)
set(Boost_USE_MULTITHREADED  ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost REQUIRED COMPONENTS system date_time)

if(MSVC)
  set(COMP_OPTS /Wall 
    /wd4623 /wd4625 /wd4626 /wd5026 /wd5027 # deleted operators
   /wd4514 /wd4820 /wd4996 /wd4710 /wd5045)
else()
  set(COMP_OPTS -Wall -Wextra -pedantic -Werror)
endif()


set(IMGUI_DIR deps/imgui)
set(IMPLOT_DIR deps/implot-master)
add_library(imgui STATIC 
  ${IMGUI_DIR}/imconfig.h 
  ${IMGUI_DIR}/imgui.h 
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
  ${IMPLOT_DIR}/implot.cpp
)
target_include_directories(imgui PUBLIC 
  ${IMGUI_DIR} 
  ${IMGUI_DIR}/misc/cpp
  ${IMPLOT_DIR}
)

if (MSVC)
  set(IMGUI_EXMPL ${IMGUI_DIR}/examples)
  add_library(imgui-impl STATIC 
    ${IMGUI_EXMPL}/imgui_impl_win32.h
    ${IMGUI_EXMPL}/imgui_impl_win32.cpp
    ${IMGUI_EXMPL}/imgui_impl_dx11.h
    ${IMGUI_EXMPL}/imgui_impl_dx11.cpp
  )
  target_include_directories(imgui-impl PUBLIC ${IMGUI_EXMPL})
  target_link_libraries(imgui-impl PUBLIC imgui d3d11.lib d3dcompiler.lib dxgi.lib)
  set(BACKEND_CPP renderer-win32.cpp)
endif()

set(DACAPO_CORE_SRC
  src/soundgen/soundgen.cpp
  src/soundgen/soundgen.hpp
  src/soundgen/synth.cpp
  src/soundgen/synth.hpp
  src/soundgen/sample.cpp
  src/soundgen/sample.hpp
  src/chef/chef.cpp
  src/chef/chef.hpp
  src/chef/ast.cpp
  src/chef/ast.hpp
  src/parser/parser.cpp
  src/parser/parser.hpp
)

add_library(dacapocore STATIC ${DACAPO_CORE_SRC})
target_link_libraries(dacapocore PUBLIC Boost::system Boost::date_time)
target_include_directories(dacapocore PUBLIC src)
message("bl:  ${Boost_LIBRARY_DIRS}")
target_link_directories(dacapocore PUBLIC ${Boost_LIBRARY_DIRS})
target_compile_definitions(dacapocore PUBLIC _WIN32 _WIN32_WINNT=0x0A00)
target_compile_options(dacapocore PUBLIC ${COMP_OPTS})

add_executable(dacapo
  src/main.cpp
  src/app.hpp
  src/app.cpp
  src/ui/ui.cpp
  src/ui/ui.hpp
  src/ui/codeedit.cpp
  src/ui/codeedit.hpp
  src/ui/internal/imgui_codeeditor.cpp
  src/ui/internal/imgui_codeeditor.h
  src/ui/renderer/renderer.hpp
  src/ui/renderer/renderer-win32.cpp
)
target_link_libraries(dacapo imgui-impl dacapocore)
target_compile_definitions(dacapo PRIVATE _WIN32_WINNT=0x0A00)
target_compile_options(dacapo PRIVATE ${COMP_OPTS})


FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.12.1
)
FetchContent_MakeAvailable(Catch2)
set(Catch2_DIR ${Catch2_SOURCE_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${Catch2_DIR}/contrib)

set(DACAPO_TEST_FILES
    tests/main.cpp
    tests/parser.t.cpp
)
add_executable(dacapotests ${DACAPO_TEST_FILES})
add_dependencies(dacapotests Catch2)
target_include_directories(dacapotests PUBLIC src ${Catch2_DIR}/single_include)
target_link_libraries(dacapotests PUBLIC dacapocore)
if(DACAPO_TEST_COVERAGE)
  target_compile_options(dacapotests PUBLIC -O0 -g -fprofile-arcs -ftest-coverage)
  target_link_options(dacapotests PUBLIC -fprofile-arcs -ftest-coverage)
endif()

include(CTest)
include(Catch)
include(ParseAndAddCatchTests)
ParseAndAddCatchTests(dacapotests)