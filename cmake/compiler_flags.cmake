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
# @file compiler_flags.cmake
# @author dhjs0000
# @brief AAM Compiler Flags Configuration
# =============================================================================
# 版本: v0.1.0-alpha.2
# 功能: 跨平台编译器标志统一配置
# 支持: MSVC (Windows), GCC/Clang (Linux/macOS)
# =============================================================================

add_library(aam_compiler_flags INTERFACE)

# =============================================================================
# 通用编译器标志（所有平台）
# =============================================================================
target_compile_features(aam_compiler_flags INTERFACE cxx_std_23)

# =============================================================================
# MSVC 特定配置 (Windows)
# =============================================================================
if(MSVC)
    # 基础警告级别
    target_compile_options(aam_compiler_flags INTERFACE
        /W4           # 最高警告级别
        /WX           # 将警告视为错误
        /permissive-  # 严格标准一致性
        /Zc:__cplusplus  # 正确的 __cplusplus 宏
        /Zc:preprocessor # 标准预处理器
        /Zc:lambda     # 标准 lambda 语义
        /Zc:throwingNew # 标准 new 抛出 bad_alloc
        /EHsc         # 标准 C++ 异常处理
        /MP           # 多处理器编译
        /utf-8        # UTF-8 源代码和执行字符集
    )

    # 调试信息配置
    target_compile_options(aam_compiler_flags INTERFACE
        $<$<CONFIG:Debug>:/Zi /Od /RTC1>
        $<$<CONFIG:Release>:/O2 /Ob2 /Oi /Ot /GL>
        $<$<CONFIG:RelWithDebInfo>:/Zi /O2 /Ob1>
        $<$<CONFIG:MinSizeRel>:/O1 /Ob1 /Os>
    )

    # 链接器优化
    target_link_options(aam_compiler_flags INTERFACE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
    )

    # 安全性增强
    target_compile_options(aam_compiler_flags INTERFACE
        /GS           # 缓冲区安全检查
        /sdl          # 附加安全检查
        /guard:cf     # 控制流保护
    )

    target_link_options(aam_compiler_flags INTERFACE
        /GUARD:CF     # 控制流保护
        /DYNAMICBASE  # ASLR
        /HIGHENTROPYVA # 高熵 ASLR
        /NXCOMPAT     # DEP/NX 兼容
    )

    # 预处理器定义
    target_compile_definitions(aam_compiler_flags INTERFACE
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00  # Windows 10
        $<$<CONFIG:Debug>:_DEBUG DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
    )

    # 禁用特定警告（已知安全且必要的）
    target_compile_options(aam_compiler_flags INTERFACE
        /wd4251  # class needs dll-interface
        /wd4275  # non dll-interface class used as base
        /wd4503  # decorated name length exceeded
        /wd4819  # character encoding issues
    )

# =============================================================================
# GCC/Clang 配置 (Linux/macOS)
# =============================================================================
else()
    # 基础警告级别
    target_compile_options(aam_compiler_flags INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )

    # GCC 特定警告
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(aam_compiler_flags INTERFACE
            -Wmisleading-indentation
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wuseless-cast
        )
        # GCC 需要显式启用概念支持
        target_compile_options(aam_compiler_flags INTERFACE
            -fconcepts
        )
    endif()

    # Clang 特定警告
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(aam_compiler_flags INTERFACE
            -Wmove
            -Wmost
        )
    endif()

    # 控制是否在 Release 配置中使用 -march=native（默认关闭以保证二进制可移植性）
    option(AAM_ENABLE_MARCH_NATIVE "Enable -march=native/-mtune=native for Release builds (may reduce portability)" OFF)

    # 优化配置
    target_compile_options(aam_compiler_flags INTERFACE
        $<$<CONFIG:Debug>:-O0 -g3 -ggdb>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
        $<$<AND:$<CONFIG:Release>,$<BOOL:${AAM_ENABLE_MARCH_NATIVE}>>:-march=native -mtune=native>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g -DNDEBUG>
        $<$<CONFIG:MinSizeRel>:-Os -DNDEBUG>
    )

    # 链接时优化 (LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
    if(IPO_SUPPORTED)
        set_target_properties(aam_compiler_flags PROPERTIES
            INTERFACE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
            INTERFACE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
        )
    endif()

    # 安全性标志
    target_compile_options(aam_compiler_flags INTERFACE
        -fstack-protector-strong
        -fPIE
        -D_FORTIFY_SOURCE=2
    )

    target_link_options(aam_compiler_flags INTERFACE
        -pie
        -Wl,-z,relro,-z,now
        -Wl,-z,noexecstack
    )

    # Sanitizers（仅在 Debug 模式且启用选项时）
    if(AAM_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Address Sanitizer
        target_compile_options(aam_compiler_flags INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(aam_compiler_flags INTERFACE
            -fsanitize=address,undefined
        )
    endif()

    # 性能分析支持
    if(AAM_ENABLE_PROFILING)
        target_compile_options(aam_compiler_flags INTERFACE
            -pg
            -fno-omit-frame-pointer
        )
        target_link_options(aam_compiler_flags INTERFACE
            -pg
        )
    endif()
endif()

# =============================================================================
# CUDA 编译器配置
# =============================================================================
if(AAM_ENABLE_CUDA AND CMAKE_CUDA_COMPILER)
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -std=c++17")
    
    # CUDA 架构配置（支持 Turing 到 Ada Lovelace）
    set(CMAKE_CUDA_ARCHITECTURES 75 80 86 89 90)
    
    # CUDA 编译选项
    target_compile_options(aam_compiler_flags INTERFACE
        $<$<COMPILE_LANGUAGE:CUDA>:
            --expt-relaxed-constexpr
            --expt-extended-lambda
            --use_fast_math
            -Xcompiler=-fPIC
        >
    )

    # CUDA 警告
    target_compile_options(aam_compiler_flags INTERFACE
        $<$<COMPILE_LANGUAGE:CUDA>:-Xptxas=-v>
    )

    # CUDA 调试配置
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(aam_compiler_flags INTERFACE
            $<$<COMPILE_LANGUAGE:CUDA>:-g -G>
        )
    endif()
endif()

# =============================================================================
# 代码覆盖率（GCC/Clang Debug 模式）
# =============================================================================
if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(AAM_ENABLE_COVERAGE "Enable code coverage reporting" OFF)
    if(AAM_ENABLE_COVERAGE)
        target_compile_options(aam_compiler_flags INTERFACE
            --coverage
            -fprofile-arcs
            -ftest-coverage
        )
        target_link_options(aam_compiler_flags INTERFACE
            --coverage
            -fprofile-arcs
            -ftest-coverage
        )
    endif()
endif()

# =============================================================================
# 链接器标志
# =============================================================================
if(NOT MSVC)
    # 增量链接
    target_link_options(aam_compiler_flags INTERFACE
        $<$<CONFIG:Debug>:-Wl,--gdb-index>
    )

    # 死代码消除
    target_link_options(aam_compiler_flags INTERFACE
        $<$<CONFIG:Release>:-Wl,--gc-sections>
    )
endif()

# =============================================================================
# 导出配置
# =============================================================================
set(AAM_COMPILER_FLAGS_CONFIGURED TRUE CACHE BOOL "Compiler flags configured" FORCE)
