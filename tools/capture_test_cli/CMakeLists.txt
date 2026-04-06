# ==========================================================================
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
# ==========================================================================
# @file CMakeLists.txt
# @brief 捕获测试 CLI 工具构建配置
# @version 0.2.0-alpha.2
# ==========================================================================

set(AAM_CAPTURE_TEST_CLI_SOURCES
    main.cpp
    cli_parser.cpp
    cli_parser.hpp
    commands/list_devices.cpp
    commands/list_devices.hpp
    commands/capture_test.cpp
    commands/capture_test.hpp
    commands/benchmark.cpp
    commands/benchmark.hpp
    commands/compare.cpp
    commands/compare.hpp
    utils/frame_saver.cpp
    utils/frame_saver.hpp
    utils/statistics.cpp
    utils/statistics.hpp
)

add_executable(aam_capture_test_cli ${AAM_CAPTURE_TEST_CLI_SOURCES})

target_include_directories(aam_capture_test_cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
        $<$<BOOL:${AAM_MAAFW_ENABLED}>:${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/maafw/include>
)

target_compile_definitions(aam_capture_test_cli
    PRIVATE
        $<$<BOOL:${AAM_MAAFW_ENABLED}>:AAM_MAAFW_ENABLED>
)

target_link_libraries(aam_capture_test_cli
    PRIVATE
        aam_core
        cxxopts::cxxopts
        spdlog::spdlog
        fmt::fmt
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        opencv_highgui
        $<$<BOOL:${AAM_MAAFW_ENABLED}>:MaaFramework>
        $<$<BOOL:${AAM_MAAFW_ENABLED}>:MaaToolkit>
)

# Windows 特定链接
if(WIN32)
    target_link_libraries(aam_capture_test_cli
        PRIVATE
            dwmapi
            psapi
            d3d11
            dxgi
            dxguid
    )
endif()

set_target_properties(aam_capture_test_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME "aam-capture-test"
)

# 安装规则
install(TARGETS aam_capture_test_cli
    RUNTIME DESTINATION bin
)

# Windows: 复制 vcpkg DLL 到输出目录
if(WIN32)
    # 使用 file(GLOB) 获取所有 DLL 文件
    file(GLOB VCPKG_DLLS
        # FFmpeg
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/avcodec-62.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/avformat-62.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/avutil-60.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/swscale-9.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/swresample-6.dll
        # OpenCV
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_core4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_imgproc4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_imgcodecs4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_highgui4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_videoio4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_flann4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_features2d4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_video4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_photo4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_calib3d4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_dnn4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_ml4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_objdetect4.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/opencv_stitching4.dll
        # Core libraries
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/fmt.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/spdlog.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libprotobuf.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libprotobuf-lite.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/zlib1.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libzmq-mt-4_3_5.dll
        # Image codecs
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libpng16.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/jpeg62.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/tiff.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libwebp.dll
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libsharpyuv.dll
        # Abseil (protobuf dependency)
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/abseil_dll.dll
        # Compression
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/liblzma.dll
    )
    
    foreach(DLL ${VCPKG_DLLS})
        if(EXISTS ${DLL})
            add_custom_command(TARGET aam_capture_test_cli POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLL} $<TARGET_FILE_DIR:aam_capture_test_cli>
            )
        endif()
    endforeach()
    
    add_custom_command(TARGET aam_capture_test_cli POST_BUILD
        COMMENT "Copied vcpkg DLLs to output directory"
    )
    
    # 复制 MaaFramework DLL
    set(AAM_RUNTIME_DLLS
        MaaFramework.dll
        MaaToolkit.dll
        MaaUtils.dll
        MaaAdbControlUnit.dll
        MaaWin32ControlUnit.dll
        MaaCustomControlUnit.dll
        MaaRecordControlUnit.dll
        MaaReplayControlUnit.dll
        MaaGamepadControlUnit.dll
        fastdeploy_ppocr_maa.dll
        onnxruntime_maa.dll
        DirectML.dll
        ViGEmClient.dll
        opencv_world4_maa.dll
    )
    
    foreach(dll ${AAM_RUNTIME_DLLS})
        add_custom_command(TARGET aam_capture_test_cli POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/${dll}
                $<TARGET_FILE_DIR:aam_capture_test_cli>
            COMMENT "Copying ${dll} for aam_capture_test_cli"
        )
    endforeach()
endif()
