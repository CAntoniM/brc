cmake_minimum_required(VERSION 3.20)

project(brc LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 20)

FIND_PACKAGE( Boost 1.40 COMPONENTS atomic REQUIRED )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

add_executable(${PROJECT_NAME} src/brc.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC inc)

target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ${Boost_LIBRARIES})

find_package(Java REQUIRED)
include(UseJava)

add_jar(generator src/CreateMeasurements.java)
