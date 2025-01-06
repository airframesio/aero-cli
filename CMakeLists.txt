cmake_minimum_required(VERSION 3.10)

project(aero-cli)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Concurrent Core)
find_package(PkgConfig)

find_path(ZeroMQ_INCLUDE_DIR NAMES zmq.h PATHS ${PC_ZeroMQ_INCLUDE_DIRS})
find_library(ZeroMQ_LIBRARIES NAMES zmq PATHS ${PC_ZeroMQ_LIBRARY_DIRS})

add_subdirectory(decode)
add_subdirectory(publish)
