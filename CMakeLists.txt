cmake_minimum_required(VERSION 3.15)
project(NarakaMapTool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# 定义源文件路径
set(SOURCES
    src/main.cpp
    src/control_panel.cpp
    src/overlay_window.cpp
    src/map_status_detector.cpp
    src/config_manager.cpp
)

if(MSVC)
    # 添加 WIN32 参数，让程序作为窗口程序启动而非控制台程序
    add_executable(NarakaMapTool WIN32 ${SOURCES})
else()
    add_executable(NarakaMapTool ${SOURCES})
endif()

target_link_libraries(NarakaMapTool PRIVATE 
    Qt6::Widgets 
    Qt6::QWindowsIntegrationPlugin 
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    gdiplus gdi32 user32 shlwapi
)

# 设置资源拷贝（可选：方便开发调试）
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# ================= 打包与安装配置 =================

# 强制指定只生成二进制包，不生成源码包
set(CPACK_BINARY_ZIP "ON")
set(CPACK_SOURCE_GENERATOR "") # 禁用源码包生成

# 设置包的基本信息
set(CPACK_PACKAGE_NAME "NarakaMapTool")
set(CPACK_PACKAGE_VENDOR "StarStudio")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_GENERATOR "ZIP")

# 1. 指定可执行文件
install(TARGETS NarakaMapTool
        RUNTIME DESTINATION .
)

# 2. 指定资源文件夹 (注意：文件夹路径末尾建议不加斜杠)
# 这样会将 resources 文件夹整个拷贝到安装目录
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources" 
        DESTINATION .
        PATTERN ".vs" EXCLUDE   # 显式排除可能冲突的目录
        PATTERN "build" EXCLUDE
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/map" 
        DESTINATION .
)

include(CPack)