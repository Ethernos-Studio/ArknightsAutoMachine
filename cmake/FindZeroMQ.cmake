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
# FindZeroMQ.cmake - ZeroMQ Library Discovery Module
# =============================================================================
# 版本: v0.1.0-alpha.2
# 功能: 跨平台 ZeroMQ 库查找模块
# 输出变量:
#   ZeroMQ_FOUND          - 是否找到 ZeroMQ
#   ZeroMQ_INCLUDE_DIRS   - 头文件路径
#   ZeroMQ_LIBRARIES      - 库文件路径
#   ZeroMQ_VERSION        - 版本号
# 目标:
#   libzmq-static         - 静态库目标
#   cppzmq-static         - C++ 封装静态库目标
# =============================================================================

include(FindPackageHandleStandardArgs)

# =============================================================================
# 版本配置
# =============================================================================
set(ZeroMQ_MINIMUM_VERSION "4.3.4")

# =============================================================================
# 查找路径配置
# =============================================================================
# vcpkg 路径
if(DEFINED ENV{VCPKG_ROOT})
    list(APPEND ZeroMQ_SEARCH_PATHS
        "$ENV{VCPKG_ROOT}/installed/x64-windows"
        "$ENV{VCPKG_ROOT}/installed/x64-windows-static"
        "$ENV{VCPKG_ROOT}/installed/x64-linux"
        "$ENV{VCPKG_ROOT}/installed/x64-osx"
        "$ENV{VCPKG_ROOT}/installed/arm64-osx"
    )
endif()

# 系统默认路径
list(APPEND ZeroMQ_SEARCH_PATHS
    "${CMAKE_PREFIX_PATH}"
    "${CMAKE_INSTALL_PREFIX}"
    "/usr/local"
    "/usr"
    "/opt/local"
    "/opt"
)

# Windows 特定路径
if(WIN32)
    list(APPEND ZeroMQ_SEARCH_PATHS
        "C:/Program Files/ZeroMQ"
        "C:/Program Files (x86)/ZeroMQ"
        "C:/vcpkg/installed/x64-windows"
        "C:/vcpkg/installed/x64-windows-static"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zeromq"
    )
endif()

# =============================================================================
# 查找头文件
# =============================================================================
find_path(ZeroMQ_INCLUDE_DIR
    NAMES zmq.h
    PATHS ${ZeroMQ_SEARCH_PATHS}
    PATH_SUFFIXES
        include
        zeromq/include
        zmq/include
    DOC "ZeroMQ header file path"
)

# =============================================================================
# 查找库文件
# =============================================================================
if(WIN32)
    # Windows: 查找 .lib 文件
    # 注意：使用通用名称而非硬编码版本号，确保兼容性
    find_library(ZeroMQ_LIBRARY_RELEASE
        NAMES libzmq-mt-s libzmq-mt libzmq libzmq-static zmq
        PATHS ${ZeroMQ_SEARCH_PATHS}
        PATH_SUFFIXES
            lib
            lib/x64
            zeromq/lib
        DOC "ZeroMQ release library"
    )

    find_library(ZeroMQ_LIBRARY_DEBUG
        NAMES libzmq-mt-sgd libzmq-mt-gd libzmqd libzmq-staticd zmqd
        PATHS ${ZeroMQ_SEARCH_PATHS}
        PATH_SUFFIXES
            lib
            lib/x64
            zeromq/lib
        DOC "ZeroMQ debug library"
    )
    
    # 选择库
    if(ZeroMQ_LIBRARY_RELEASE AND ZeroMQ_LIBRARY_DEBUG)
        set(ZeroMQ_LIBRARY
            optimized ${ZeroMQ_LIBRARY_RELEASE}
            debug ${ZeroMQ_LIBRARY_DEBUG}
        )
    elseif(ZeroMQ_LIBRARY_RELEASE)
        set(ZeroMQ_LIBRARY ${ZeroMQ_LIBRARY_RELEASE})
    elseif(ZeroMQ_LIBRARY_DEBUG)
        set(ZeroMQ_LIBRARY ${ZeroMQ_LIBRARY_DEBUG})
    endif()
else()
    # Linux/macOS: 查找 .a 或 .so
    find_library(ZeroMQ_LIBRARY
        NAMES zmq libzmq.a libzmq.so
        PATHS ${ZeroMQ_SEARCH_PATHS}
        PATH_SUFFIXES
            lib
            lib/x86_64-linux-gnu
            lib/aarch64-linux-gnu
        DOC "ZeroMQ library"
    )
endif()

# =============================================================================
# 查找 C++ 封装库 (cppzmq)
# =============================================================================
find_path(CPPZMQ_INCLUDE_DIR
    NAMES zmq.hpp
    PATHS ${ZeroMQ_SEARCH_PATHS}
    PATH_SUFFIXES
        include
        cppzmq/include
    DOC "cppzmq header file path"
)

# =============================================================================
# 提取版本信息
# =============================================================================
if(ZeroMQ_INCLUDE_DIR AND EXISTS "${ZeroMQ_INCLUDE_DIR}/zmq.h")
    file(STRINGS "${ZeroMQ_INCLUDE_DIR}/zmq.h" ZMQ_VERSION_MAJOR_LINE
        REGEX "^#define ZMQ_VERSION_MAJOR [0-9]+$")
    file(STRINGS "${ZeroMQ_INCLUDE_DIR}/zmq.h" ZMQ_VERSION_MINOR_LINE
        REGEX "^#define ZMQ_VERSION_MINOR [0-9]+$")
    file(STRINGS "${ZeroMQ_INCLUDE_DIR}/zmq.h" ZMQ_VERSION_PATCH_LINE
        REGEX "^#define ZMQ_VERSION_PATCH [0-9]+$")
    
    string(REGEX REPLACE "^#define ZMQ_VERSION_MAJOR ([0-9]+)$" "\\1"
        ZMQ_VERSION_MAJOR "${ZMQ_VERSION_MAJOR_LINE}")
    string(REGEX REPLACE "^#define ZMQ_VERSION_MINOR ([0-9]+)$" "\\1"
        ZMQ_VERSION_MINOR "${ZMQ_VERSION_MINOR_LINE}")
    string(REGEX REPLACE "^#define ZMQ_VERSION_PATCH ([0-9]+)$" "\\1"
        ZMQ_VERSION_PATCH "${ZMQ_VERSION_PATCH_LINE}")
    
    set(ZeroMQ_VERSION "${ZMQ_VERSION_MAJOR}.${ZMQ_VERSION_MINOR}.${ZMQ_VERSION_PATCH}")
else()
    set(ZeroMQ_VERSION "0.0.0")
endif()

# =============================================================================
# 标准查找处理
# =============================================================================
find_package_handle_standard_args(ZeroMQ
    REQUIRED_VARS
        ZeroMQ_LIBRARY
        ZeroMQ_INCLUDE_DIR
    VERSION_VAR ZeroMQ_VERSION
    HANDLE_VERSION_RANGE
)

# ---------------------------------------------------------------------------
# 强制最低版本检查
# ---------------------------------------------------------------------------
if(ZeroMQ_FOUND AND ZeroMQ_VERSION VERSION_LESS ZeroMQ_MINIMUM_VERSION)
    set(ZeroMQ_FOUND FALSE)
    set(_ZeroMQ_version_error
        "Found ZeroMQ version ${ZeroMQ_VERSION}, but at least ${ZeroMQ_MINIMUM_VERSION} is required")

    # 清除变量，避免调用者意外使用不支持的版本
    unset(ZeroMQ_LIBRARY)
    unset(ZeroMQ_INCLUDE_DIR)
    unset(ZeroMQ_LIBRARIES)
    unset(ZeroMQ_INCLUDE_DIRS)

    if(NOT ZeroMQ_FIND_QUIETLY)
        if(ZeroMQ_FIND_REQUIRED)
            message(FATAL_ERROR "${_ZeroMQ_version_error}")
        else()
            message(STATUS "${_ZeroMQ_version_error}")
        endif()
    endif()
endif()

# =============================================================================
# 创建导入目标
# =============================================================================
# 现代 CMake 导入目标：ZeroMQ::ZeroMQ
if(ZeroMQ_FOUND AND NOT TARGET ZeroMQ::ZeroMQ)
    add_library(ZeroMQ::ZeroMQ UNKNOWN IMPORTED GLOBAL)

    # 检测是否为静态库（Windows: .lib, Unix: .a）
    set(_ZMQ_IS_STATIC FALSE)
    if(WIN32)
        # Windows 下无法直接区分静态/动态 .lib，默认假设为静态
        set(_ZMQ_IS_STATIC TRUE)
    else()
        # Unix 下检查文件后缀
        if(ZeroMQ_LIBRARY MATCHES "\\.a$")
            set(_ZMQ_IS_STATIC TRUE)
        endif()
    endif()

    set_target_properties(ZeroMQ::ZeroMQ PROPERTIES
        IMPORTED_LOCATION "${ZeroMQ_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ZeroMQ_INCLUDE_DIR}"
        IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    )

    # 仅在确认为静态库时定义 ZMQ_STATIC
    if(_ZMQ_IS_STATIC)
        set_property(TARGET ZeroMQ::ZeroMQ PROPERTY
            INTERFACE_COMPILE_DEFINITIONS "ZMQ_STATIC")
    endif()

    # Windows 特定链接依赖
    if(WIN32)
        set_property(TARGET ZeroMQ::ZeroMQ APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES
                ws2_32
                iphlpapi
                rpcrt4
        )
    else()
        # Linux/macOS 可能需要 pthread
        find_package(Threads REQUIRED)
        set_property(TARGET ZeroMQ::ZeroMQ APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES
                Threads::Threads
        )
    endif()

    # 区分 Debug/Release
    if(WIN32 AND ZeroMQ_LIBRARY_RELEASE AND ZeroMQ_LIBRARY_DEBUG)
        set_target_properties(ZeroMQ::ZeroMQ PROPERTIES
            IMPORTED_LOCATION_RELEASE "${ZeroMQ_LIBRARY_RELEASE}"
            IMPORTED_LOCATION_DEBUG "${ZeroMQ_LIBRARY_DEBUG}"
        )
    endif()
endif()

# 向后兼容：libzmq-static 目标
if(ZeroMQ_FOUND AND NOT TARGET libzmq-static)
    add_library(libzmq-static STATIC IMPORTED GLOBAL)

    # 检测是否为静态库（Windows: .lib, Unix: .a）
    set(_ZMQ_IS_STATIC FALSE)
    if(WIN32)
        # Windows 下无法直接区分静态/动态 .lib，默认假设为静态
        set(_ZMQ_IS_STATIC TRUE)
    else()
        # Unix 下检查文件后缀
        if(ZeroMQ_LIBRARY MATCHES "\\.a$")
            set(_ZMQ_IS_STATIC TRUE)
        endif()
    endif()

    set_target_properties(libzmq-static PROPERTIES
        IMPORTED_LOCATION "${ZeroMQ_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ZeroMQ_INCLUDE_DIR}"
    )

    # 仅在确认为静态库时定义 ZMQ_STATIC
    if(_ZMQ_IS_STATIC)
        set_property(TARGET libzmq-static PROPERTY
            INTERFACE_COMPILE_DEFINITIONS "ZMQ_STATIC")
    endif()
    
    # Windows 特定链接依赖
    if(WIN32)
        set_property(TARGET libzmq-static APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES
                ws2_32
                iphlpapi
                rpcrt4
        )
    else()
        # Linux/macOS 可能需要 pthread
        find_package(Threads REQUIRED)
        set_property(TARGET libzmq-static APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES
                Threads::Threads
        )
    endif()
    
    # 区分 Debug/Release
    if(WIN32 AND ZeroMQ_LIBRARY_RELEASE AND ZeroMQ_LIBRARY_DEBUG)
        set_target_properties(libzmq-static PROPERTIES
            IMPORTED_LOCATION_RELEASE "${ZeroMQ_LIBRARY_RELEASE}"
            IMPORTED_LOCATION_DEBUG "${ZeroMQ_LIBRARY_DEBUG}"
        )
    endif()

    # 链接到现代目标
    if(TARGET ZeroMQ::ZeroMQ)
        set_property(TARGET libzmq-static PROPERTY
            INTERFACE_LINK_LIBRARIES ZeroMQ::ZeroMQ)
    endif()
endif()

# cppzmq 目标
if(CPPZMQ_INCLUDE_DIR AND NOT TARGET cppzmq-static)
    add_library(cppzmq-static INTERFACE IMPORTED GLOBAL)
    set_target_properties(cppzmq-static PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CPPZMQ_INCLUDE_DIR}"
    )

    # 优先链接到现代目标 ZeroMQ::ZeroMQ
    if(TARGET ZeroMQ::ZeroMQ)
        set_property(TARGET cppzmq-static PROPERTY
            INTERFACE_LINK_LIBRARIES ZeroMQ::ZeroMQ)
    elseif(TARGET libzmq-static)
        set_property(TARGET cppzmq-static PROPERTY
            INTERFACE_LINK_LIBRARIES libzmq-static)
    endif()
endif()

# =============================================================================
# 设置输出变量
# =============================================================================
if(ZeroMQ_FOUND)
    set(ZeroMQ_INCLUDE_DIRS ${ZeroMQ_INCLUDE_DIR})
    if(CPPZMQ_INCLUDE_DIR)
        list(APPEND ZeroMQ_INCLUDE_DIRS ${CPPZMQ_INCLUDE_DIR})
    endif()
    list(REMOVE_DUPLICATES ZeroMQ_INCLUDE_DIRS)
    
    set(ZeroMQ_LIBRARIES ${ZeroMQ_LIBRARY})
endif()

# =============================================================================
# 标记高级变量
# =============================================================================
mark_as_advanced(
    ZeroMQ_INCLUDE_DIR
    ZeroMQ_LIBRARY
    ZeroMQ_LIBRARY_RELEASE
    ZeroMQ_LIBRARY_DEBUG
    CPPZMQ_INCLUDE_DIR
)

# =============================================================================
# 诊断输出
# =============================================================================
if(ZeroMQ_FOUND AND NOT ZeroMQ_FIND_QUIETLY)
    message(STATUS "ZeroMQ found:")
    message(STATUS "  Version: ${ZeroMQ_VERSION}")
    message(STATUS "  Includes: ${ZeroMQ_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${ZeroMQ_LIBRARIES}")
    if(CPPZMQ_INCLUDE_DIR)
        message(STATUS "  cppzmq: Found at ${CPPZMQ_INCLUDE_DIR}")
    else()
        message(WARNING "  cppzmq: Not found (header-only C++ binding)")
    endif()
endif()
