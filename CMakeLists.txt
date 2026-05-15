cmake_minimum_required(VERSION 3.10)
project(main)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -g -m64 -march=native")

add_executable(main
        main.cpp
        image.cpp
        image_io.cpp
)

target_link_libraries(main GL GLEW glut pthread)