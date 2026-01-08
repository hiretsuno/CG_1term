cmake_minimum_required(VERSION 3.31)
project(Lab_03)

set(CMAKE_CXX_STANDARD 17)
add_executable(renderer
        src/main.cpp
        src/tgaimage.cpp
        src/mesh.cpp
        src/our_gl.cpp
        src/math.cpp

)

include_directories(third_party)
