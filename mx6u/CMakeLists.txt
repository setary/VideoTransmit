cmake_minimum_required(VERSION 3.16.3)

project(VideoTransmit VERSION 1 LANGUAGES C CXX)

link_directories(${CMAKE_SOURCE_DIR}/cyclonedds/lib)
include_directories(${CMAKE_SOURCE_DIR}/cyclonedds/include)

#link_directories(${CMAKE_SOURCE_DIR}/opencv/lib)
#include_directories(${CMAKE_SOURCE_DIR}/opencv/include)

file(GLOB VEDIO_TRANSMIT_SOURCES_C "*.c")
file(GLOB VEDIO_TRANSMIT_SOURCES_CPP "*.cpp")

add_executable(VideoTransmit ${VEDIO_TRANSMIT_SOURCES_C} ${VEDIO_TRANSMIT_SOURCES_CPP})

target_compile_definitions(VideoTransmit PRIVATE
    $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<STREQUAL:"${CMAKE_BUILD_TYPE}","Debug">>:__DEBUG>
    $<$<BOOL:${INTERNAL_DEBUG}>:__INTERNALDEBUG> # Internal debug activated.
)

target_link_libraries(VideoTransmit ddsc
                      opencv_highgui opencv_videoio opencv_core opencv_imgcodecs opencv_imgproc
                      -lpthread -ldl)
