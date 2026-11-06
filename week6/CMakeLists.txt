cmake_minimum_required(VERSION 3.16)
project(week6_opencv)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")

# 上周工程已在 CMake 里用 file(MAKE_DIRECTORY ...) 创建输出文件夹，这里沿用
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/images")

add_executable(week6
    week6.cpp
    main.cpp
)

# 源码含中文注释（UTF-8）：MSVC 需要 /utf-8，否则按 GBK 解析会报语法错误
if (MSVC)
    target_compile_options(week6 PRIVATE /utf-8)
endif()

target_include_directories(week6 PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(week6 PRIVATE ${OpenCV_LIBS})

# 代码中使用相对路径 images/input.png 与 output/xxx.png，
# 因此把 images/ 与 output/ 复制到可执行文件旁边，保证相对路径可用
add_custom_command(TARGET week6 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:week6>/output"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/images"
            "$<TARGET_FILE_DIR:week6>/images"
    COMMENT "Copying images/ next to the week6 executable"
)
