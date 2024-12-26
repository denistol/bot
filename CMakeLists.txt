cmake_minimum_required(VERSION 3.10)

project(MyProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)

add_executable(bot "src/main.cpp")

set(OpenCV_DIR "C:/opencv/build")
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})
    target_link_libraries(bot ${OpenCV_LIBS})
endif()

target_link_libraries(bot user32)
