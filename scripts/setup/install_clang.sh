#!/bin/bash
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
# @file install_clang.sh
# @author dhjs0000
# @brief 安装指定版本的 Clang/LLVM（用于 CI 和本地开发）
# =============================================================================
# 用法: ./install_clang.sh <version>
# 示例: ./install_clang.sh 16
# =============================================================================

set -euo pipefail

# 检查参数
if [ $# -ne 1 ]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 16"
    exit 1
fi

CLANG_VERSION="$1"

echo "Installing Clang/LLVM ${CLANG_VERSION}..."

# 更新包列表
sudo apt-get update

# 安装必要的依赖
sudo apt-get install -y wget lsb-release gnupg software-properties-common

# 添加 LLVM 官方 GPG key
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/llvm-snapshot.asc

# 添加 LLVM 仓库
sudo add-apt-repository "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-${CLANG_VERSION} main"

# 更新包列表以包含新添加的仓库
sudo apt-get update

# 安装 Clang/LLVM 及其工具
sudo apt-get install -y \
    clang-${CLANG_VERSION} \
    clang++-${CLANG_VERSION} \
    lldb-${CLANG_VERSION} \
    lld-${CLANG_VERSION} \
    llvm-${CLANG_VERSION}-dev \
    libclang-${CLANG_VERSION}-dev

# 创建符号链接（可选）
if [ ! -f /usr/bin/clang ]; then
    sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-${CLANG_VERSION} 100
    sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-${CLANG_VERSION} 100
fi

echo "Clang/LLVM ${CLANG_VERSION} installed successfully!"
echo "Version: $(clang-${CLANG_VERSION} --version | head -n 1)"
