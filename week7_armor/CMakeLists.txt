cmake_minimum_required(VERSION 3.16)
project(week7_armor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")

# 提前创建输出目录，避免 imwrite 因目录不存在而失败
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/images")

add_executable(week7_armor
    armor_pipeline.cpp
    armor_visualize.cpp
    main.cpp
)

# 源码含中文注释（UTF-8）：MSVC 需要 /utf-8，否则按 GBK 解析会报语法错误
if (MSVC)
    target_compile_options(week7_armor PRIVATE /utf-8)
endif()

target_include_directories(week7_armor PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(week7_armor PRIVATE ${OpenCV_LIBS})

# 代码里用相对路径 images/outpost_frame.png 与 output/xxx.png，
# 因此把 images/ 复制到可执行文件旁边，保证相对路径可用
add_custom_command(TARGET week7_armor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:week7_armor>/output"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/images"
            "$<TARGET_FILE_DIR:week7_armor>/images"
    COMMENT "Copying images/ next to the week7_armor executable"
)
