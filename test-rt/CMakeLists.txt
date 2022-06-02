cmake_minimum_required(VERSION 3.0)

project(TestRt)

find_package(Qt5 REQUIRED Core Widgets Gui Charts)
add_definitions(${QT_DEFINITIONS})

set(CMAKE_CXX_FLAGS "-std=c++20 -O2 -g -Wall -Werror -pedantic-errors")
set(CMAKE_EXPORT_COMPILE_COMMANDS
    ON
    CACHE INTERNAL "")
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(SRC main.cpp)

add_executable(main ${GUI_TYPE} ${SRC})
target_link_libraries(main Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Charts
                      rtaudio)
