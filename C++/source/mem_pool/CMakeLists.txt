cmake_minimum_required(VERSION 3.10)
project(MemoryPool)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# 创建内存池库
add_library(memory_pool_lib memory_pool.cpp)
target_include_directories(memory_pool_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 创建可执行文件
add_executable(memory_pool_demo example.cpp)
target_link_libraries(memory_pool_demo PRIVATE memory_pool_lib pthread)

# 设置输出目录
set_target_properties(memory_pool_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 添加编译选项
if(MSVC)
    target_compile_options(memory_pool_demo PRIVATE /W4)
else()
    target_compile_options(memory_pool_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()
