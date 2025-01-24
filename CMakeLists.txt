cmake_minimum_required(VERSION 3.10)

project(aero-cli)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Werror")

find_package(Qt6 COMPONENTS Concurrent Core Multimedia Network)
find_package(PkgConfig)

pkg_check_modules(ZeroMQ REQUIRED libzmq)

set(COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common/)
find_file(COMMON_NOTIFIER_SOURCE_FILE notifier.cpp ${COMMON_INCLUDE_DIR})
find_file(COMMON_LOGGER_SOURCE_FILE logger.cpp ${COMMON_INCLUDE_DIR})

add_subdirectory(decode)
add_subdirectory(publish)
