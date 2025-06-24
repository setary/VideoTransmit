cmake_minimum_required(VERSION 3.16.3)

project(VideoTransmit VERSION 1 LANGUAGES C CXX)


if(NOT OpenCV_FOUND)
  find_package(OpenCV REQUIRED)
endif()

link_directories(${CMAKE_SOURCE_DIR}/cyclonedds/lib)

file(GLOB VEDIO_TRANSMIT_SOURCES_C "*.c")
file(GLOB VEDIO_TRANSMIT_SOURCES_CPP "*.cpp")

message(STATUS "C file is: ${VEDIO_TRANSMIT_SOURCES_C}")

add_executable(VideoTransmit ${VEDIO_TRANSMIT_SOURCES_C} ${VEDIO_TRANSMIT_SOURCES_CPP})

target_include_directories(
    VideoTransmit 
    PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/cyclonedds/include
    ${CMAKE_SOURCE_DIR}/opencv
    ${CMAKE_SOURCE_DIR}/opencv/include
)

target_compile_definitions(VideoTransmit PRIVATE
    $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<STREQUAL:"${CMAKE_BUILD_TYPE}","Debug">>:__DEBUG>
    $<$<BOOL:${INTERNAL_DEBUG}>:__INTERNALDEBUG> # Internal debug activated.
)

target_link_libraries(VideoTransmit cycloneddsidl ddsc
    ${OpenCV_LIBRARIES}
    -lpthread -ldl ws2_32)