cmake_minimum_required(VERSION 3.16)
project(opencv_basic)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")

# 创建输出文件夹
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")

add_definitions(-DOUTPUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/output")

add_executable(opencv_basic
    image_basic.cpp
    main.cpp
)

# 源码含中文注释（UTF-8）：MSVC 需要 /utf-8，否则按 GBK 解析会报语法错误
if (MSVC)
    target_compile_options(opencv_basic PRIVATE /utf-8)
endif()

target_include_directories(opencv_basic PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(opencv_basic PRIVATE ${OpenCV_LIBS})

# 把输入图片自动拷贝到可执行文件同一层目录
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/input.jpg")
    add_custom_command(TARGET opencv_basic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/input.jpg"
        "$<TARGET_FILE_DIR:opencv_basic>/input.jpg"
        COMMENT "Copying input.jpg next to the executable"
    )
endif()

# ================================================================
# 第 6 周：预处理、阈值与轮廓目标检测
# 代码中使用相对路径 images/input.png 与 output/xxx.png，
# 因此需要保证"可执行文件所在目录"下存在 images/ 与 output/
# ================================================================
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/images")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")

add_executable(week6
    week6.cpp
    main_week6.cpp
)

if (MSVC)
    target_compile_options(week6 PRIVATE /utf-8)
endif()

target_include_directories(week6 PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(week6 PRIVATE ${OpenCV_LIBS})

# 生成后把 images/ 与 output/ 复制到可执行文件旁边，保证相对路径可用
add_custom_command(TARGET week6 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:week6>/output"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/images"
            "$<TARGET_FILE_DIR:week6>/images"
    COMMENT "Copying images/ next to the week6 executable"
)