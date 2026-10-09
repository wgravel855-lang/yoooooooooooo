cmake_minimum_required(VERSION 3.15)
project(r6_esp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(r6_esp WIN32
    src/main.cpp
    src/offsets.cpp
)

target_link_libraries(r6_esp PRIVATE
    d2d1 dwrite dwmapi psapi user32 gdi32
)

if (MSVC)
    target_compile_options(r6_esp PRIVATE /W3 /EHsc)
endif()
