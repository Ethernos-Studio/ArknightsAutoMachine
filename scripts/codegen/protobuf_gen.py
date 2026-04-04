#!/usr/bin/env python3
# -*- coding: utf-8 -*-
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
# @file protobuf_gen.py
# @author dhjs0000
# @brief Protobuf 代码生成脚本
# =============================================================================
# 版本: v0.1.0-alpha.3
# 功能: 生成 C++ 和 Python 的 Protobuf/gRPC 代码
# 用法: python scripts/codegen/protobuf_gen.py [--proto-dir DIR] [--output-dir DIR]
# =============================================================================

import argparse
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Tuple


# =============================================================================
# 常量定义
# =============================================================================
DEFAULT_PROTO_DIR = Path("proto")
DEFAULT_OUTPUT_DIR = Path("generated")
CPP_OUTPUT_SUBDIR = "cpp"
PYTHON_OUTPUT_SUBDIR = "python"

# 支持的生成语言
SUPPORTED_LANGUAGES = ["cpp", "python"]

# Protobuf 编译器命令
PROTOC_CMD = "protoc"
GRPC_CPP_PLUGIN = "grpc_cpp_plugin"
GRPC_PYTHON_PLUGIN = "grpc_python_plugin"


# =============================================================================
# 异常定义
# =============================================================================
class ProtobufGenError(Exception):
    """Protobuf 代码生成错误"""
    pass


class ProtocNotFoundError(ProtobufGenError):
    """protoc 编译器未找到"""
    pass


class ProtoFileError(ProtobufGenError):
    """.proto 文件处理错误"""
    pass


# =============================================================================
# 辅助函数
# =============================================================================
def find_protoc() -> Path:
    """
    查找 protoc 编译器

    Returns:
        Path: protoc 可执行文件路径

    Raises:
        ProtocNotFoundError: 未找到 protoc
    """
    # 首先尝试从环境变量查找
    protoc_path = shutil.which(PROTOC_CMD)
    if protoc_path:
        return Path(protoc_path)

    # 尝试常见安装路径
    common_paths = [
        Path("/usr/local/bin/protoc"),
        Path("/usr/bin/protoc"),
        Path("C:/Program Files/protoc/bin/protoc.exe"),
        Path("C:/vcpkg/installed/x64-windows/tools/protobuf/protoc.exe"),
        Path.home() / "vcpkg/installed/x64-linux/tools/protobuf/protoc",
    ]

    for path in common_paths:
        if path.exists():
            return path

    raise ProtocNotFoundError(
        "未找到 protoc 编译器。请安装 Protocol Buffers 编译器:\n"
        "  - Windows: vcpkg install protobuf\n"
        "  - Ubuntu/Debian: sudo apt-get install protobuf-compiler\n"
        "  - macOS: brew install protobuf"
    )


def find_grpc_plugin(plugin_name: str) -> Optional[Path]:
    """
    查找 gRPC 插件

    Args:
        plugin_name: 插件名称 (grpc_cpp_plugin 或 grpc_python_plugin)

    Returns:
        Optional[Path]: 插件路径，未找到返回 None
    """
    plugin_path = shutil.which(plugin_name)
    if plugin_path:
        return Path(plugin_path)

    # 尝试常见安装路径
    common_paths = [
        Path(f"/usr/local/bin/{plugin_name}"),
        Path(f"/usr/bin/{plugin_name}"),
        Path(f"C:/Program Files/grpc/bin/{plugin_name}.exe"),
        Path(f"C:/vcpkg/installed/x64-windows/tools/grpc/{plugin_name}.exe"),
        Path.home() / f"vcpkg/installed/x64-linux/tools/grpc/{plugin_name}",
    ]

    for path in common_paths:
        if path.exists():
            return path

    return None


def collect_proto_files(proto_dir: Path) -> List[Path]:
    """
    收集所有 .proto 文件

    Args:
        proto_dir: proto 文件根目录

    Returns:
        List[Path]: .proto 文件路径列表

    Raises:
        ProtoFileError: proto 目录不存在或无 .proto 文件
    """
    if not proto_dir.exists():
        raise ProtoFileError(f"Proto 目录不存在: {proto_dir}")

    proto_files = list(proto_dir.rglob("*.proto"))

    if not proto_files:
        raise ProtoFileError(f"在 {proto_dir} 中未找到 .proto 文件")

    return sorted(proto_files)


def generate_cpp_code(
    proto_files: List[Path],
    proto_dir: Path,
    output_dir: Path,
    verbose: bool = False
) -> Tuple[int, int]:
    """
    生成 C++ 代码

    Args:
        proto_files: .proto 文件列表
        proto_dir: proto 文件根目录
        output_dir: 输出目录
        verbose: 是否输出详细信息

    Returns:
        Tuple[int, int]: (成功数量, 失败数量)
    """
    protoc = find_protoc()
    cpp_out = output_dir / CPP_OUTPUT_SUBDIR
    cpp_out.mkdir(parents=True, exist_ok=True)

    grpc_plugin = find_grpc_plugin(GRPC_CPP_PLUGIN)
    if grpc_plugin:
        grpc_out = cpp_out / "grpc"
        grpc_out.mkdir(parents=True, exist_ok=True)

    success_count = 0
    fail_count = 0

    for proto_file in proto_files:
        if verbose:
            print(f"  生成 C++: {proto_file.relative_to(proto_dir)}")

        # 构建 protoc 命令
        cmd = [
            str(protoc),
            f"--proto_path={proto_dir}",
            f"--cpp_out={cpp_out}",
        ]

        # 添加 gRPC 插件（如果可用）
        if grpc_plugin and _is_service_proto(proto_file):
            cmd.extend([
                f"--grpc_out={grpc_out}",
                f"--plugin=protoc-gen-grpc={grpc_plugin}",
            ])

        cmd.append(str(proto_file))

        try:
            subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=True
            )
            success_count += 1
        except subprocess.CalledProcessError as e:
            print(f"  错误: 生成 {proto_file.name} 失败")
            print(f"    {e.stderr}")
            fail_count += 1

    return success_count, fail_count


def generate_python_code(
    proto_files: List[Path],
    proto_dir: Path,
    output_dir: Path,
    verbose: bool = False
) -> Tuple[int, int]:
    """
    生成 Python 代码

    Args:
        proto_files: .proto 文件列表
        proto_dir: proto 文件根目录
        output_dir: 输出目录
        verbose: 是否输出详细信息

    Returns:
        Tuple[int, int]: (成功数量, 失败数量)
    """
    protoc = find_protoc()
    py_out = output_dir / PYTHON_OUTPUT_SUBDIR
    py_out.mkdir(parents=True, exist_ok=True)

    grpc_plugin = find_grpc_plugin(GRPC_PYTHON_PLUGIN)

    success_count = 0
    fail_count = 0

    for proto_file in proto_files:
        if verbose:
            print(f"  生成 Python: {proto_file.relative_to(proto_dir)}")

        # 构建 protoc 命令
        cmd = [
            str(protoc),
            f"--proto_path={proto_dir}",
            f"--python_out={py_out}",
            f"--pyi_out={py_out}",  # 生成类型存根文件
        ]

        # 添加 gRPC 插件（如果可用）
        if grpc_plugin and _is_service_proto(proto_file):
            cmd.extend([
                f"--grpc_python_out={py_out}",
                f"--plugin=protoc-gen-grpc_python={grpc_plugin}",
            ])

        cmd.append(str(proto_file))

        try:
            subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=True
            )
            success_count += 1
        except subprocess.CalledProcessError as e:
            print(f"  错误: 生成 {proto_file.name} 失败")
            print(f"    {e.stderr}")
            fail_count += 1

    return success_count, fail_count


def _is_service_proto(proto_file: Path) -> bool:
    """
    检查 .proto 文件是否包含服务定义

    Args:
        proto_file: .proto 文件路径

    Returns:
        bool: 是否包含服务定义
    """
    try:
        content = proto_file.read_text(encoding='utf-8')
        return 'service ' in content
    except Exception:
        return False


def create_init_files(output_dir: Path, verbose: bool = False) -> None:
    """
    为 Python 包创建 __init__.py 文件

    Args:
        output_dir: Python 输出目录
        verbose: 是否输出详细信息
    """
    py_out = output_dir / PYTHON_OUTPUT_SUBDIR

    if not py_out.exists():
        return

    # 为每个子目录创建 __init__.py
    for subdir in py_out.rglob("*"):
        if subdir.is_dir():
            init_file = subdir / "__init__.py"
            if not init_file.exists():
                init_file.touch()
                if verbose:
                    print(f"  创建: {init_file.relative_to(output_dir)}")


def clean_generated(output_dir: Path, verbose: bool = False) -> None:
    """
    清理生成的代码

    Args:
        output_dir: 输出目录
        verbose: 是否输出详细信息
    """
    if output_dir.exists():
        if verbose:
            print(f"清理目录: {output_dir}")
        shutil.rmtree(output_dir)


# =============================================================================
# 主函数
# =============================================================================
def main() -> int:
    """
    主入口函数

    Returns:
        int: 退出码 (0=成功, 1=失败)
    """
    parser = argparse.ArgumentParser(
        description="AAM Protobuf 代码生成工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s                          # 使用默认配置生成
  %(prog)s --clean                  # 清理生成的代码
  %(prog)s --languages cpp          # 仅生成 C++ 代码
  %(prog)s --verbose                # 详细输出
  %(prog)s --proto-dir ./proto --output-dir ./gen
        """
    )

    parser.add_argument(
        "--proto-dir",
        type=Path,
        default=DEFAULT_PROTO_DIR,
        help=f"proto 文件目录 (默认: {DEFAULT_PROTO_DIR})"
    )

    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help=f"输出目录 (默认: {DEFAULT_OUTPUT_DIR})"
    )

    parser.add_argument(
        "--languages",
        nargs="+",
        choices=SUPPORTED_LANGUAGES,
        default=SUPPORTED_LANGUAGES,
        help=f"生成语言 (默认: {' '.join(SUPPORTED_LANGUAGES)})"
    )

    parser.add_argument(
        "--clean",
        action="store_true",
        help="清理生成的代码后退出"
    )

    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="详细输出"
    )

    parser.add_argument(
        "--version",
        action="version",
        version="%(prog)s v0.1.0-alpha.3"
    )

    args = parser.parse_args()

    # 清理模式
    if args.clean:
        clean_generated(args.output_dir, args.verbose)
        print("✅ 已清理生成的代码")
        return 0

    print("=" * 60)
    print("AAM Protobuf 代码生成工具")
    print(f"版本: v0.1.0-alpha.3")
    print("=" * 60)

    try:
        # 检查 protoc
        protoc = find_protoc()
        print(f"✅ 找到 protoc: {protoc}")

        # 检查 gRPC 插件
        cpp_plugin = find_grpc_plugin(GRPC_CPP_PLUGIN)
        py_plugin = find_grpc_plugin(GRPC_PYTHON_PLUGIN)

        if cpp_plugin:
            print(f"✅ 找到 gRPC C++ 插件: {cpp_plugin}")
        else:
            print("⚠️  未找到 gRPC C++ 插件，将跳过 gRPC 代码生成")

        if py_plugin:
            print(f"✅ 找到 gRPC Python 插件: {py_plugin}")
        else:
            print("⚠️  未找到 gRPC Python 插件，将跳过 gRPC 代码生成")

        # 收集 .proto 文件
        print(f"\n📁 扫描 proto 目录: {args.proto_dir}")
        proto_files = collect_proto_files(args.proto_dir)
        print(f"✅ 找到 {len(proto_files)} 个 .proto 文件")

        if args.verbose:
            for f in proto_files:
                print(f"  - {f.relative_to(args.proto_dir)}")

        # 创建输出目录
        args.output_dir.mkdir(parents=True, exist_ok=True)
        print(f"\n📁 输出目录: {args.output_dir}")

        # 生成代码
        total_success = 0
        total_fail = 0

        if "cpp" in args.languages:
            print("\n🔨 生成 C++ 代码...")
            success, fail = generate_cpp_code(
                proto_files,
                args.proto_dir,
                args.output_dir,
                args.verbose
            )
            total_success += success
            total_fail += fail
            print(f"   成功: {success}, 失败: {fail}")

        if "python" in args.languages:
            print("\n🐍 生成 Python 代码...")
            success, fail = generate_python_code(
                proto_files,
                args.proto_dir,
                args.output_dir,
                args.verbose
            )
            total_success += success
            total_fail += fail
            print(f"   成功: {success}, 失败: {fail}")

            # 创建 Python 包初始化文件
            print("\n📦 创建 Python 包结构...")
            create_init_files(args.output_dir, args.verbose)

        # 总结
        print("\n" + "=" * 60)
        if total_fail == 0:
            print(f"✅ 代码生成完成！总计: {total_success} 个文件")
            print(f"\n生成文件位置:")
            if "cpp" in args.languages:
                print(f"  C++: {args.output_dir / CPP_OUTPUT_SUBDIR}")
            if "python" in args.languages:
                print(f"  Python: {args.output_dir / PYTHON_OUTPUT_SUBDIR}")
            return 0
        else:
            print(f"❌ 代码生成失败！成功: {total_success}, 失败: {total_fail}")
            return 1

    except ProtobufGenError as e:
        print(f"\n❌ 错误: {e}")
        return 1
    except KeyboardInterrupt:
        print("\n\n⚠️  用户中断")
        return 130
    except Exception as e:
        print(f"\n❌ 未预期错误: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
