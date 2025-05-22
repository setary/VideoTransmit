cmake_minimum_required(VERSION 3.16.3)

project(VideoTransmit VERSION 1 LANGUAGES CXX)

#set(fastcdr_DIR ${CMAKE_SOURCE_DIR}/Fast-DDS/fastcdr/lib)
link_directories(${CMAKE_SOURCE_DIR}/Fast-DDS/fastcdr/lib)
#if(NOT fastcdr_FOUND)
#    find_package(fastcdr REQUIRED)
#endif()
include_directories(${CMAKE_SOURCE_DIR}/Fast-DDS/fastcdr/include)

#set(fastrtps_DIR ${CMAKE_SOURCE_DIR}/Fast-DDS/fastrtps/lib)
link_directories(${CMAKE_SOURCE_DIR}/Fast-DDS/fastrtps/lib)
#if(NOT fastrtps_FOUND)
#    find_package(fastrtps REQUIRED)
#endif()
include_directories(${CMAKE_SOURCE_DIR}/Fast-DDS/fastrtps/include)

#set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv/lib)
link_directories(${CMAKE_SOURCE_DIR}/opencv/lib)
#if(NOT OpenCV_FOUND)
#  find_package(OpenCV REQUIRED)
#endif()
include_directories(${CMAKE_SOURCE_DIR}/opencv/include)

file(GLOB VEDIO_TRANSMIT_SOURCES_CXX "*.cxx")
file(GLOB VEDIO_TRANSMIT_SOURCES_CPP "*.cpp")

add_executable(VideoTransmit ${VEDIO_TRANSMIT_SOURCES_CXX} ${VEDIO_TRANSMIT_SOURCES_CPP})

target_compile_definitions(VideoTransmit PRIVATE
    $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<STREQUAL:"${CMAKE_BUILD_TYPE}","Debug">>:__DEBUG>
    $<$<BOOL:${INTERNAL_DEBUG}>:__INTERNALDEBUG> # Internal debug activated.
)

# target_link_libraries(VideoTransmit fastrtps fastcdr OpenCV -lpthread -ldl)
target_link_libraries(VideoTransmit fastrtps fastcdr
                      opencv_highgui opencv_videoio opencv_core opencv_imgcodecs opencv_imgproc
                      -lpthread -ldl)
