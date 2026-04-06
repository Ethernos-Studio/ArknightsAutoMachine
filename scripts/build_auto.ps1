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
# @file build_auto.ps1
# @brief AAM 一键构建脚本 - 自动清理、配置和构建
# @version 0.1.0-alpha.3
# =============================================================================

<#
.SYNOPSIS
    AAM 项目一键构建脚本
.DESCRIPTION
    自动执行以下步骤：
    1. 清理旧的构建目录
    2. 运行 CMake 配置
    3. 执行编译构建
    4. 运行测试（可选）
.PARAMETER Config
    构建配置类型，可选值：Debug, Release, RelWithDebInfo, MinSizeRel
    默认值：Release
.PARAMETER Jobs
    并行编译作业数
    默认值：8
.PARAMETER Clean
    强制清理构建目录后重新构建
.PARAMETER Test
    构建完成后运行测试
.PARAMETER Verbose
    显示详细输出
.EXAMPLE
    .\scripts\build_auto.ps1
    使用默认配置（Release）构建项目
.EXAMPLE
    .\scripts\build_auto.ps1 -Config Debug -Test
    以 Debug 模式构建并运行测试
.EXAMPLE
    .\scripts\build_auto.ps1 -Clean -ShowVerbose
    强制清理后重新构建，显示详细输出
.OUTPUTS
    构建成功返回 0，失败返回非零错误码
.NOTES
    要求：PowerShell 7+, Visual Studio 2022, CMake 3.20+
    依赖：vcpkg 包管理器（已安装在 vcpkg_installed 目录）
#>

[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",

    [Parameter()]
    [ValidateRange(1, 32)]
    [int]$Jobs = 8,

    [Parameter()]
    [switch]$Clean,

    [Parameter()]
    [switch]$Test,

    [Parameter()]
    [switch]$ShowVerbose
)

# =============================================================================
# 常量定义
# =============================================================================
$PROJECT_ROOT = Split-Path -Parent $PSScriptRoot
$BUILD_DIR = Join-Path $PROJECT_ROOT "build"
$VCPKG_DIR = Join-Path $PROJECT_ROOT "vcpkg_installed" "x64-windows"

# 颜色定义
$COLOR_INFO = "Cyan"
$COLOR_SUCCESS = "Green"
$COLOR_WARNING = "Yellow"
$COLOR_ERROR = "Red"

# =============================================================================
# 辅助函数
# =============================================================================

<#
.SYNOPSIS
    打印带颜色的信息消息
#>
function Write-Info {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor $COLOR_INFO
}

<#
.SYNOPSIS
    打印带颜色的成功消息
#>
function Write-Success {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor $COLOR_SUCCESS
}

<#
.SYNOPSIS
    打印带颜色的警告消息
#>
function Write-Warning {
    param([string]$Message)
    Write-Host "[WARNING] $Message" -ForegroundColor $COLOR_WARNING
}

<#
.SYNOPSIS
    打印带颜色的错误消息
#>
function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor $COLOR_ERROR
}

<#
.SYNOPSIS
    执行命令并处理错误
.DESCRIPTION
    执行外部命令，如果失败则输出错误并退出
#>
function Invoke-BuildCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,

        [Parameter()]
        [string]$Description = "执行命令"
    )

    Write-Info $Description
    Write-Info "执行: $Command"

    try {
        if ($ShowVerbose) {
            Invoke-Expression $Command | ForEach-Object { Write-Host $_ }
        }
        else {
            $output = Invoke-Expression $Command 2>&1
            if ($LASTEXITCODE -ne 0) {
                throw "命令执行失败，退出码: $LASTEXITCODE"
            }
        }

        if ($LASTEXITCODE -ne 0) {
            throw "命令执行失败，退出码: $LASTEXITCODE"
        }

        return $true
    }
    catch {
        Write-Error "$Description 失败: $_"
        if ($output) {
            Write-Error "输出: $output"
        }
        return $false
    }
}

<#
.SYNOPSIS
    清理构建目录
#>
function Clear-BuildDirectory {
    Write-Info "清理构建目录: $BUILD_DIR"

    if (Test-Path $BUILD_DIR) {
        try {
            Remove-Item -Path $BUILD_DIR -Recurse -Force -ErrorAction Stop
            Write-Success "构建目录已清理"
        }
        catch {
            Write-Error "清理构建目录失败: $_"
            exit 1
        }
    }
    else {
        Write-Info "构建目录不存在，无需清理"
    }
}

<#
.SYNOPSIS
    验证构建环境
#>
function Test-BuildEnvironment {
    Write-Info "验证构建环境..."

    # 检查 CMake
    $cmakeVersion = cmake --version 2>$null | Select-Object -First 1
    if (-not $cmakeVersion) {
        Write-Error "CMake 未找到，请确保已安装 CMake 3.20+"
        exit 1
    }
    Write-Info "CMake 版本: $cmakeVersion"

    # 检查 Visual Studio
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsVersion = & $vsWhere -latest -property displayName 2>$null
        if ($vsVersion) {
            Write-Info "Visual Studio: $vsVersion"
        }
    }

    # 检查 vcpkg 包
    if (Test-Path $VCPKG_DIR) {
        Write-Info "vcpkg 包目录: $VCPKG_DIR"
    }
    else {
        Write-Warning "vcpkg 包目录不存在: $VCPKG_DIR"
    }

    Write-Success "环境验证完成"
}

<#
.SYNOPSIS
    运行 CMake 配置
#>
function Invoke-CMakeConfiguration {
    [CmdletBinding()]
    param()

    Write-Info "运行 CMake 配置..."
    Write-Info "构建类型: $Config"

    # 确保构建目录存在
    if (-not (Test-Path $BUILD_DIR)) {
        New-Item -ItemType Directory -Path $BUILD_DIR -Force | Out-Null
    }

    $cmakeArgs = @(
        "-B `"$BUILD_DIR`""
        "-S `"$PROJECT_ROOT`""
        "-G `"Visual Studio 17 2022`""
        "-A x64"
        "-DCMAKE_BUILD_TYPE=$Config"
        "-DCMAKE_TOOLCHAIN_FILE=`"$PROJECT_ROOT\vcpkg_installed\x64-windows\share\vcpkg\vcpkg.toolchain.cmake`""
    )

    if ($ShowVerbose) {
        $cmakeArgs += "--debug-output"
    }

    $cmakeCommand = "cmake $($cmakeArgs -join ' ')"

    if (-not (Invoke-BuildCommand -Command $cmakeCommand -Description "CMake 配置")) {
        exit 1
    }

    Write-Success "CMake 配置完成"
}

<#
.SYNOPSIS
    执行编译构建
#>
function Invoke-Build {
    [CmdletBinding()]
    param()

    Write-Info "开始编译构建..."
    Write-Info "配置: $Config"
    Write-Info "并行作业数: $Jobs"

    $buildArgs = @(
        "--build `"$BUILD_DIR`""
        "--config $Config"
        "--parallel $Jobs"
    )

    if ($ShowVerbose) {
        $buildArgs += "--verbose"
    }

    $buildCommand = "cmake $($buildArgs -join ' ')"

    if (-not (Invoke-BuildCommand -Command $buildCommand -Description "编译构建")) {
        exit 1
    }

    Write-Success "编译构建完成"
}

<#
.SYNOPSIS
    运行测试
#>
function Invoke-Tests {
    Write-Info "运行测试..."

    $testExecutable = Join-Path $BUILD_DIR "core\tests" $Config "aam_core_tests.exe"

    if (-not (Test-Path $testExecutable)) {
        Write-Warning "测试可执行文件不存在: $testExecutable"
        return
    }

    # 设置 PATH 以包含 vcpkg DLL
    $vcpkgBinDir = Join-Path $PROJECT_ROOT "vcpkg_installed" "x64-windows" "bin"
    $env:PATH = "$env:PATH;$vcpkgBinDir"

    $testCommand = "& `"$testExecutable`""

    if (-not (Invoke-BuildCommand -Command $testCommand -Description "运行测试")) {
        Write-Error "测试失败"
        exit 1
    }

    Write-Success "测试通过"
}

<#
.SYNOPSIS
    显示构建摘要
#>
function Show-BuildSummary {
    Write-Host ""
    Write-Host "=============================================================================" -ForegroundColor $COLOR_INFO
    Write-Host "                        AAM 构建摘要" -ForegroundColor $COLOR_INFO
    Write-Host "=============================================================================" -ForegroundColor $COLOR_INFO
    Write-Host ""
    Write-Host "  项目根目录: $PROJECT_ROOT"
    Write-Host "  构建目录:   $BUILD_DIR"
    Write-Host "  构建配置:   $Config"
    Write-Host ""

    # 列出生成的文件
    $outputDirs = @(
        (Join-Path $BUILD_DIR $Config),
        (Join-Path $BUILD_DIR "examples" $Config),
        (Join-Path $BUILD_DIR "core\tests" $Config)
    )

    $allExecutables = @()
    $allLibraries = @()

    foreach ($outputDir in $outputDirs) {
        if (Test-Path $outputDir) {
            $allExecutables += Get-ChildItem -Path $outputDir -Filter "*.exe" -ErrorAction SilentlyContinue
            $allLibraries += Get-ChildItem -Path $outputDir -Filter "*.lib" -ErrorAction SilentlyContinue
        }
    }

    if ($allExecutables) {
        Write-Host "  生成的可执行文件:" -ForegroundColor $COLOR_SUCCESS
        foreach ($exe in $allExecutables) {
            Write-Host "    - $($exe.Name)" -ForegroundColor $COLOR_SUCCESS
        }
    }

    if ($allLibraries) {
        Write-Host "  生成的库文件:" -ForegroundColor $COLOR_SUCCESS
        foreach ($lib in $allLibraries) {
            Write-Host "    - $($lib.Name)" -ForegroundColor $COLOR_SUCCESS
        }
    }

    Write-Host ""
    Write-Host "=============================================================================" -ForegroundColor $COLOR_INFO
    Write-Host ""
}

# =============================================================================
# 主执行流程
# =============================================================================

function Main {
    Write-Host ""
    Write-Host "=============================================================================" -ForegroundColor $COLOR_INFO
    Write-Host "           Arknights Auto Machine (AAM) 一键构建脚本" -ForegroundColor $COLOR_INFO
    Write-Host "=============================================================================" -ForegroundColor $COLOR_INFO
    Write-Host ""
    Write-Host "  构建配置: $Config"
    Write-Host "  并行作业: $Jobs"
    Write-Host "  清理模式: $Clean"
    Write-Host "  运行测试: $Test"
    Write-Host "  详细输出: $ShowVerbose"
    Write-Host ""
    Write-Host "=============================================================================" -ForegroundColor $COLOR_INFO
    Write-Host ""

    # 验证环境
    Test-BuildEnvironment

    # 清理（如果需要）
    if ($Clean -or -not (Test-Path $BUILD_DIR)) {
        Clear-BuildDirectory
    }

    # CMake 配置
    Invoke-CMakeConfiguration

    # 编译构建
    Invoke-Build

    # 运行测试（如果需要）
    if ($Test) {
        Invoke-Tests
    }

    # 显示摘要
    Show-BuildSummary

    Write-Success "构建流程全部完成！"
    exit 0
}

# 执行主函数
Main
