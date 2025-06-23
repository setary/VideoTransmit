# CMakeList.txt: nat_server 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

project ("nat_server")

# 将源代码添加到此项目的可执行文件。
add_executable (nat_server "main.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET nat_server PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(nat_server PRIVATE ws2_32)

# TODO: 如有需要，请添加测试并安装目标。
