cmake_minimum_required(VERSION 3.16)
project(week8_outpost)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/images")

add_executable(week8_outpost
    armor_pipeline.cpp
    armor_visualize.cpp
    param_loader.cpp
    pose_solver.cpp
    frame_output.cpp
    main.cpp
)

# 源码含中文注释（UTF-8）：MSVC 需要 /utf-8
if (MSVC)
    target_compile_options(week8_outpost PRIVATE /utf-8)
endif()

target_include_directories(week8_outpost PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(week8_outpost PRIVATE ${OpenCV_LIBS})

# 把输入素材（视频、参数文件、静态测试图）复制到可执行文件旁边，
# 保证代码里的相对路径在 build/Release 下也能直接跑
add_custom_command(TARGET week8_outpost POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:week8_outpost>/output"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:week8_outpost>/images"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/outpost.avi"
            "$<TARGET_FILE_DIR:week8_outpost>/outpost.avi"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/参数.md"
            "$<TARGET_FILE_DIR:week8_outpost>/参数.md"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/images"
            "$<TARGET_FILE_DIR:week8_outpost>/images"
    COMMENT "Copying outpost.avi / 参数.md / images next to the executable"
)
