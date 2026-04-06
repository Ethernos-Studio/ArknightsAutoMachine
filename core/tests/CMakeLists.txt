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
# @file CMakeLists.txt
# @brief Core module test suite configuration
# @version 0.1.0-alpha.3
# =============================================================================

# 查找 GTest
find_package(GTest CONFIG REQUIRED)

# 测试可执行文件
set(CORE_TEST_SOURCES
    test_main.cpp
    test_memory_pool.cpp
    test_timer.cpp
    test_frame_buffer.cpp
)

# 创建测试可执行文件
add_executable(aam_core_tests ${CORE_TEST_SOURCES})

# 链接依赖
target_link_libraries(aam_core_tests
    PRIVATE
        aam_core
        GTest::gtest
        GTest::gtest_main
)

# 包含目录
target_include_directories(aam_core_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/core/include
)

# 添加测试
add_test(NAME CoreTests COMMAND aam_core_tests)

# 设置测试属性
set_target_properties(aam_core_tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

# ============================================================================
# 跨平台运行时依赖处理
# ============================================================================
if(WIN32)
    # Windows: 复制项目特定的运行时 DLL
    # 注意：系统 DLL (如 VC++ Redistributable) 和通用库由 vcpkg/系统提供
    # 只复制 AAM 项目生成的和第三方预编译的 DLL
    set(AAM_RUNTIME_DLLS
        # MaaFramework 组件
        MaaFramework.dll
        MaaToolkit.dll
        MaaUtils.dll
        MaaAdbControlUnit.dll
        MaaWin32ControlUnit.dll
        MaaCustomControlUnit.dll
        MaaRecordControlUnit.dll
        MaaReplayControlUnit.dll
        MaaGamepadControlUnit.dll
        # ONNX Runtime 和 OCR
        fastdeploy_ppocr_maa.dll
        onnxruntime_maa.dll
        DirectML.dll
        ViGEmClient.dll
        # OpenCV MAA 特定版本
        opencv_world4_maa.dll
    )
    
    # 为每个 DLL 添加复制命令
    foreach(dll ${AAM_RUNTIME_DLLS})
        add_custom_command(TARGET aam_core_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/${dll}
                $<TARGET_FILE_DIR:aam_core_tests>
            COMMENT "Copying ${dll} for aam_core_tests"
        )
    endforeach()
    
    # 复制 vcpkg 提供的运行时 DLL
    set(VCPKG_DLLS
        # Core libraries
        ${VCPKG_INSTALLED_DIR}/bin/fmt.dll
        ${VCPKG_INSTALLED_DIR}/bin/spdlog.dll
        ${VCPKG_INSTALLED_DIR}/bin/libprotobuf.dll
        ${VCPKG_INSTALLED_DIR}/bin/libprotobuf-lite.dll
        ${VCPKG_INSTALLED_DIR}/bin/zlib1.dll
        ${VCPKG_INSTALLED_DIR}/bin/libzmq-mt-4_3_5.dll
        # Abseil (protobuf dependency)
        ${VCPKG_INSTALLED_DIR}/bin/abseil_dll.dll
        # OpenCV
        ${VCPKG_INSTALLED_DIR}/bin/opencv_core4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_imgproc4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_imgcodecs4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_highgui4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_videoio4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_flann4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_features2d4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_video4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_photo4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_calib3d4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_dnn4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_ml4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_objdetect4.dll
        ${VCPKG_INSTALLED_DIR}/bin/opencv_stitching4.dll
        # Image codecs
        ${VCPKG_INSTALLED_DIR}/bin/libpng16.dll
        ${VCPKG_INSTALLED_DIR}/bin/jpeg62.dll
        ${VCPKG_INSTALLED_DIR}/bin/tiff.dll
        ${VCPKG_INSTALLED_DIR}/bin/libwebp.dll
        ${VCPKG_INSTALLED_DIR}/bin/libsharpyuv.dll
        # Compression
        ${VCPKG_INSTALLED_DIR}/bin/liblzma.dll
    )
    
    foreach(dll ${VCPKG_DLLS})
        if(EXISTS ${dll})
            add_custom_command(TARGET aam_core_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${dll} $<TARGET_FILE_DIR:aam_core_tests>
            )
        endif()
    endforeach()
    
    # 复制 GTest DLL
    add_custom_command(TARGET aam_core_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${VCPKG_INSTALLED_DIR}/bin/gtest.dll
            ${VCPKG_INSTALLED_DIR}/bin/gtest_main.dll
            $<TARGET_FILE_DIR:aam_core_tests>
    )
    
elseif(APPLE)
    # macOS: 设置 RPATH 以便找到动态库
    set_target_properties(aam_core_tests PROPERTIES
        INSTALL_RPATH "@loader_path;@loader_path/../lib;@loader_path/../../bin"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
else()
    # Linux: 设置 RPATH 以便找到动态库
    set_target_properties(aam_core_tests PROPERTIES
        INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib:$ORIGIN/../../bin"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()
