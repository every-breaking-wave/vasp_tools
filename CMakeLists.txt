cmake_minimum_required(VERSION 3.10)  
  
# 设置项目名称和版本  
project(MyProject VERSION 1.0)  
  
# 设置C++标准  
set(CMAKE_CXX_STANDARD 11)  
set(CMAKE_CXX_STANDARD_REQUIRED True)  
  
# 包含目录  
include_directories(include)  
  
# 添加可执行文件  
add_executable(main src/main.cpp src/remote_server.cpp src/vasp.cpp src/vasp_tool.cpp src/tool.cpp)  
  
# 链接库  
find_package(Boost REQUIRED COMPONENTS system filesystem)  
if(Boost_FOUND)  
    include_directories(${Boost_INCLUDE_DIRS})  
    target_link_libraries(main ${Boost_LIBRARIES} pthread)  
else()  
    message(FATAL_ERROR "Could not find Boost libraries.")  
endif()