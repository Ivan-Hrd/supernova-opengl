cmake_minimum_required(VERSION 3.10)
project(main)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -g")

if(APPLE)
    set(MAIN_SRC main_mac.cpp)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -march=native")
    set(MAIN_SRC main.cpp)
endif()

add_executable(main
        ${MAIN_SRC}
        image.cpp
        image_io.cpp
        vertex.shd
        fragment.shd
)

if(APPLE)
    # macOS : OpenGL natif d'Apple (framework) + GLEW + GLFW via Homebrew.
    find_package(OpenGL REQUIRED)

    execute_process(COMMAND brew --prefix glew OUTPUT_VARIABLE GLEW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND brew --prefix glfw OUTPUT_VARIABLE GLFW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)

    target_include_directories(main PRIVATE
            ${GLEW_PREFIX}/include
            ${GLFW_PREFIX}/include
    )
    target_link_libraries(main
            OpenGL::GL
            ${GLEW_PREFIX}/lib/libGLEW.dylib
            ${GLFW_PREFIX}/lib/libglfw.dylib
    )
else()
    target_link_libraries(main GL GLEW glut pthread)
endif()
