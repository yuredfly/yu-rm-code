cmake_minimum_required(VERSION 3.16)
project(week6_yuanshen)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")

# 沿用上周工程的写法：提前创建输出文件夹
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/images")

add_executable(week6_yuanshen
    yuanshen.cpp
    yuanshen_main.cpp
)

# 源码含中文注释（UTF-8）：MSVC 需要 /utf-8
if (MSVC)
    target_compile_options(week6_yuanshen PRIVATE /utf-8)
endif()

target_include_directories(week6_yuanshen PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(week6_yuanshen PRIVATE ${OpenCV_LIBS})

# 代码中使用相对路径 images/yuanshen.png 与 output/xxx.png，
# 因此把 images/ 与 output/ 复制到可执行文件旁边
add_custom_command(TARGET week6_yuanshen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:week6_yuanshen>/output"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/images"
            "$<TARGET_FILE_DIR:week6_yuanshen>/images"
    COMMENT "Copying images/ next to the week6_yuanshen executable"
)
