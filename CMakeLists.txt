# =============================================================================
# Copyright (C) 2026 Ethernos Studio
# This file is part of Arknights Auto Machine (AAM).
#
# AAM is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# AAM is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with AAM. If not, see <https://www.gnu.org/licenses/>.
# =============================================================================
# Arknights Auto Machine (AAM) - Root CMake Configuration
# =============================================================================
# 版本: v0.1.0-alpha.2
# 目标: C++23 标准，aam_core 静态库构建
# 作者: dhjs0000
# =============================================================================

cmake_minimum_required(VERSION 3.25)

# =============================================================================
# vcpkg 工具链配置（必须在 project() 之前）
# =============================================================================
# 注意：如果用户通过命令行传递了 CMAKE_TOOLCHAIN_FILE，则优先使用用户指定的
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
        message(STATUS "Auto-configured vcpkg toolchain: $ENV{VCPKG_ROOT}")
    endif()
endif()

# =============================================================================
# 项目定义与版本管理
# =============================================================================
project(
    AAM
    VERSION 0.1.0
    DESCRIPTION "Arknights Auto Machine - Industrial-grade Game Automation Framework"
    HOMEPAGE_URL "https://github.com/AAM-Team/Arknights-Auto-Machine"
    LANGUAGES CXX C
)

# =============================================================================
# C++ 标准配置
# =============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 导出编译命令数据库（用于 clangd 等工具）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
        ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

# =============================================================================
# 构建类型与选项
# =============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

# 构建选项
option(AAM_BUILD_TESTS "Build test suite" ON)
option(AAM_BUILD_EXAMPLES "Build example applications" ON)
option(AAM_ENABLE_CUDA "Enable CUDA support for GPU acceleration" ON)
option(AAM_ENABLE_PROFILING "Enable performance profiling instrumentation" OFF)
option(AAM_ENABLE_SANITIZERS "Enable address/thread sanitizers in Debug mode" ON)
option(AAM_USE_VCPKG "Use vcpkg for dependency management" ON)
option(AAM_STATIC_LINK "Static link runtime libraries" OFF)
option(AAM_ENABLE_STATIC_ANALYSIS "Enable static analysis with clang-tidy" ON)

# =============================================================================
# 静态分析集成
# =============================================================================
if(AAM_ENABLE_STATIC_ANALYSIS)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        message(STATUS "Static analysis enabled: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy not found, static analysis disabled")
    endif()
endif()

# =============================================================================
# 平台检测与配置
# =============================================================================
if(WIN32)
    set(AAM_PLATFORM_WINDOWS TRUE)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)
    # Windows 特定：启用 UTF-8 编码
    add_compile_options(/utf-8)
elseif(UNIX AND NOT APPLE)
    set(AAM_PLATFORM_LINUX TRUE)
elseif(APPLE)
    set(AAM_PLATFORM_MACOS TRUE)
endif()

# 架构检测
if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
    set(AAM_ARCH_X64 TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
    set(AAM_ARCH_ARM64 TRUE)
endif()

# =============================================================================
# 编译器标志配置
# =============================================================================
include(cmake/compiler_flags.cmake)

# =============================================================================
# 依赖管理
# =============================================================================
# 查找包路径（自定义 Find 模块）
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# =============================================================================
# 第三方依赖查找
# =============================================================================
# vcpkg 配置优先策略：优先使用 vcpkg 提供的 CMake Config 模式
if(AAM_USE_VCPKG AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
    message(STATUS "Using vcpkg toolchain: $ENV{VCPKG_ROOT}")
endif()
      add_library(ZeroMQ::ZeroMQ ALIAS ZeroMQ::libzmq)
    endif()
endif()

find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(ZeroMQ REQUIRED)
find_package(cppzmq CONFIG REQUIRED)

# CUDA 配置（可选）
if(AAM_ENABLE_CUDA)
    include(CheckLanguage)
    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        set(CMAKE_CUDA_STANDARD 17)
        set(CMAKE_CUDA_STANDARD_REQUIRED ON)
        find_package(CUDAToolkit REQUIRED)
        set(AAM_CUDA_ENABLED TRUE)
        message(STATUS "CUDA support enabled: ${CMAKE_CUDA_COMPILER_VERSION}")
    else()
        message(WARNING "CUDA compiler not found, GPU acceleration disabled")
        set(AAM_CUDA_ENABLED FALSE)
    endif()
else()
    set(AAM_CUDA_ENABLED FALSE)
endif()

# =============================================================================
# 代码生成配置（Protobuf）
# =============================================================================
set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_BINARY_DIR})

# 收集所有 .proto 文件
file(GLOB_RECURSE PROTO_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto"
)

# Protobuf 生成目标
foreach(proto_file ${PROTO_FILES})
    get_filename_component(proto_name ${proto_file} NAME_WE)
    get_filename_component(proto_dir ${proto_file} DIRECTORY)
    
    set(proto_srcs "${PROTO_BINARY_DIR}/${proto_name}.pb.cc")
    set(proto_hdrs "${PROTO_BINARY_DIR}/${proto_name}.pb.h")
    set(grpc_srcs "${PROTO_BINARY_DIR}/${proto_name}.grpc.pb.cc")
    set(grpc_hdrs "${PROTO_BINARY_DIR}/${proto_name}.grpc.pb.h")
    
    add_custom_command(
        OUTPUT ${proto_srcs} ${proto_hdrs} ${grpc_srcs} ${grpc_hdrs}
        COMMAND protobuf::protoc
        ARGS 
            --grpc_out=${PROTO_BINARY_DIR}
            --cpp_out=${PROTO_BINARY_DIR}
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
            ${proto_file}
        DEPENDS ${proto_file}
        COMMENT "Generating Protobuf and gRPC code for ${proto_name}"
        VERBATIM
    )
    
    list(APPEND PROTO_GENERATED_SOURCES ${proto_srcs} ${grpc_srcs})
    list(APPEND PROTO_GENERATED_HEADERS ${proto_hdrs} ${grpc_hdrs})
endforeach()

add_custom_target(aam_proto_generation 
    DEPENDS ${PROTO_GENERATED_SOURCES} ${PROTO_GENERATED_HEADERS}
)

# =============================================================================
# 核心库目标: aam_core
# =============================================================================
# 收集核心库源文件
file(GLOB_RECURSE AAM_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/core/src/*.cpp"
)

file(GLOB_RECURSE AAM_CORE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/include/*.h"
)

# 创建静态库
add_library(aam_core STATIC
    ${AAM_CORE_SOURCES}
    ${PROTO_GENERATED_SOURCES}
)

add_dependencies(aam_core aam_proto_generation)

# 目标属性配置
target_include_directories(aam_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
        $<BUILD_INTERFACE:${PROTO_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core/src
)

target_link_libraries(aam_core
    PUBLIC
        Threads::Threads
        protobuf::libprotobuf
        gRPC::grpc++
        spdlog::spdlog
        fmt::fmt
        yaml-cpp
        nlohmann_json::nlohmann_json
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        Eigen3::Eigen
        ZeroMQ::ZeroMQ
        cppzmq-static
    PRIVATE
        aam_compiler_flags
)

# CUDA 链接（如果启用）
if(AAM_CUDA_ENABLED)
    target_link_libraries(aam_core
        PUBLIC
            CUDA::cudart
            CUDA::cuda_driver
    )
    target_include_directories(aam_core
        PUBLIC
            ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES}
    )
endif()

# 编译特性
target_compile_features(aam_core PUBLIC cxx_std_23)

# 预处理器定义
target_compile_definitions(aam_core
    PUBLIC
        AAM_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        AAM_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        AAM_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        AAM_VERSION_STRING="${PROJECT_VERSION}"
        $<$<BOOL:${AAM_CUDA_ENABLED}>:AAM_ENABLE_CUDA>
        $<$<BOOL:${AAM_PLATFORM_WINDOWS}>:AAM_PLATFORM_WINDOWS>
        $<$<BOOL:${AAM_PLATFORM_LINUX}>:AAM_PLATFORM_LINUX>
        $<$<BOOL:${AAM_PLATFORM_MACOS}>:AAM_PLATFORM_MACOS>
)

# 编译器特定选项已由 aam_compiler_flags 提供，此处无需重复设置

# 静态链接配置
if(AAM_STATIC_LINK AND MSVC)
    set_property(TARGET aam_core PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# =============================================================================
# 测试配置
# =============================================================================
if(AAM_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(core/tests)
endif()

# =============================================================================
# 示例配置
# =============================================================================
if(AAM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# =============================================================================
# Bridge 模块配置（v0.7.0-beta 预留）
# =============================================================================
# add_subdirectory(bridge/src)  # 将在 v0.7.0 启用

# =============================================================================
# GUI 模块配置（v0.9.0-beta 预留）
# =============================================================================
# add_subdirectory(gui/qt)  # 将在 v0.9.0 启用

# =============================================================================
# 安装配置
# =============================================================================
include(GNUInstallDirs)

install(TARGETS aam_core
    EXPORT aam-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT aam-targets
    FILE aam-targets.cmake
    NAMESPACE aam::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aam
)

install(DIRECTORY core/include/aam
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/aam-config.cmake.in"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aam
    RENAME aam-config.cmake
)

# =============================================================================
# 打包配置
# =============================================================================
set(CPACK_PACKAGE_NAME "ArknightsAutoMachine")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;DragNDrop")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
endif()

include(CPack)

# =============================================================================
# 构建信息输出
# =============================================================================
message(STATUS "")
message(STATUS "=============================================================================")
message(STATUS "Arknights Auto Machine (AAM) Build Configuration")
message(STATUS "=============================================================================")
message(STATUS "Version:        ${PROJECT_VERSION}")
message(STATUS "Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "CUDA Enabled:   ${AAM_CUDA_ENABLED}")
message(STATUS "Tests:          ${AAM_BUILD_TESTS}")
message(STATUS "Examples:       ${AAM_BUILD_EXAMPLES}")
message(STATUS "Platform:       ${CMAKE_SYSTEM_NAME}")
message(STATUS "Architecture:   ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "=============================================================================")
message(STATUS "")
