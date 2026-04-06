# ============================================================================
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
# ============================================================================
# @file build.ps1
# @author dhjs0000
# @brief AAM 一键编译脚本 - 工业级构建系统
# ============================================================================
# 版本: v1.0.0
# 功能: 完整的构建流程自动化，包含环境检查、依赖管理、编译执行
# 支持: Windows 11 + PowerShell 7.5+
# ============================================================================

<#
.SYNOPSIS
    Arknights Auto Machine (AAM) 一键编译脚本

.DESCRIPTION
    提供完整的 AAM 项目构建流程自动化，包括：
    - 环境预检（CMake、vcpkg、编译器）
    - 依赖安装与管理
    - 配置生成（CMake Configure）
    - 项目编译
    - 测试执行（可选）
    - 安装打包（可选）

    时间复杂度: O(n) 其中 n 为源文件数量
    空间复杂度: O(1) 额外空间

.PARAMETER Configuration
    构建配置类型，可选: Debug, Release, RelWithDebInfo, MinSizeRel
    默认值: Release

.PARAMETER BuildDirectory
    构建输出目录
    默认值: build

.PARAMETER InstallDirectory
    安装目标目录
    默认值: install

.PARAMETER Triplet
    vcpkg triplet，如 x64-windows, x64-windows-static
    默认值: 自动检测

.PARAMETER SkipDeps
    跳过依赖检查与安装

.PARAMETER SkipConfigure
    跳过 CMake 配置阶段

.PARAMETER SkipBuild
    跳过编译阶段

.PARAMETER RunTests
    编译后执行测试

.PARAMETER Clean
    清理构建目录后重新构建

.PARAMETER ParallelJobs
    并行编译任务数
    默认值: 逻辑处理器数量

.PARAMETER Detailed
    启用详细输出

.PARAMETER UseNinja
    使用 Ninja 生成器（默认使用 Visual Studio 2022）

.PARAMETER EnableCUDA
    启用 CUDA 支持

.PARAMETER EnableStaticLink
    启用静态链接运行时库

.PARAMETER VcpkgRoot
    指定 vcpkg 根目录

.OUTPUTS
    System.Int32
    返回码: 0=成功, 非0=失败

.EXAMPLE
    .\scripts\build.ps1
    使用默认配置执行完整构建流程

.EXAMPLE
    .\scripts\build.ps1 -Configuration Debug -RunTests
    以 Debug 模式构建并执行测试

.EXAMPLE
    .\scripts\build.ps1 -Clean -EnableCUDA
    清理后重新构建，启用 CUDA 支持

.EXAMPLE
    .\scripts\build.ps1 -Configuration Release -ParallelJobs 16
    使用 16 个并行任务进行 Release 构建

.NOTES
    依赖关系:
    - CMake 3.25+
    - Visual Studio 2022 (Windows) 或 GCC 13+ (Linux)
    - vcpkg (依赖管理)
    - Git (子模块管理)

    线程安全: 本脚本设计为单线程执行，不支持并发调用

    性能特征:
    - 首次运行需要下载并编译依赖，耗时较长（10-30分钟）
    - 增量构建通常在 1-5 分钟内完成
    - 使用 SSD 可显著提升编译速度
#>

[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter()]
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string]$BuildDirectory = 'build',

    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string]$InstallDirectory = 'install',

    [Parameter()]
    [string]$Triplet = '',

    [Parameter()]
    [switch]$SkipDeps,

    [Parameter()]
    [switch]$SkipConfigure,

    [Parameter()]
    [switch]$SkipBuild,

    [Parameter()]
    [switch]$RunTests,

    [Parameter()]
    [switch]$Clean,

    [Parameter()]
    [ValidateRange(1, 256)]
    [int]$ParallelJobs = 0,

    [Parameter()]
    [switch]$Detailed,

    [Parameter()]
    [switch]$UseNinja,

    [Parameter()]
    [switch]$EnableCUDA,

    [Parameter()]
    [switch]$EnableStaticLink,

    [Parameter()]
    [string]$VcpkgRoot = ''
)

# ============================================================================
# 全局常量定义
# ============================================================================

# 脚本版本
$script:ScriptVersion = '1.0.0'

# 最小版本要求
$script:RequiredCMakeVersion = [Version]'3.25.0'
$script:RequiredPowerShellVersion = [Version]'7.0.0'

# 颜色定义（用于输出美化）
$script:Colors = @{
    Success = 'Green'
    Error = 'Red'
    Warning = 'Yellow'
    Info = 'Cyan'
    Debug = 'Gray'
    Default = 'White'
}

# 构建状态跟踪
$script:BuildState = @{
    StartTime = $null
    StageStartTime = $null
    CurrentStage = ''
    Errors = @()
    Warnings = @()
    Metrics = @{}
}

# ============================================================================
# 日志与输出函数
# ============================================================================

<#
.SYNOPSIS
    输出带时间戳的日志消息

.DESCRIPTION
    根据消息级别使用不同颜色输出，支持详细模式控制

.PARAMETER Message
    要输出的消息

.PARAMETER Level
    消息级别: Info, Success, Warning, Error, Debug

.PARAMETER NoNewline
    不换行输出
#>
function Write-BuildLog {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true, Position = 0)]
        [string]$Message,

        [Parameter()]
        [ValidateSet('Info', 'Success', 'Warning', 'Error', 'Debug')]
        [string]$Level = 'Info',

        [Parameter()]
        [switch]$NoNewline
    )

    $timestamp = Get-Date -Format 'HH:mm:ss'
    $color = $script:Colors[$Level]

    # Debug 级别仅在 Detailed 模式下输出
    if ($Level -eq 'Debug' -and -not $Detailed) {
        return
    }

    $prefix = switch ($Level) {
        'Success' { '[✓]' }
        'Error' { '[✗]' }
        'Warning' { '[!]' }
        'Debug' { '[D]' }
        default { '[*]' }
    }

    $output = "[$timestamp] $prefix $Message"

    if ($NoNewline) {
        Write-Host $output -NoNewline -ForegroundColor $color
    }
    else {
        Write-Host $output -ForegroundColor $color
    }

    # 记录到状态跟踪
    switch ($Level) {
        'Error' { $script:BuildState.Errors += $Message }
        'Warning' { $script:BuildState.Warnings += $Message }
    }
}

<#
.SYNOPSIS
    输出阶段开始标记

.PARAMETER StageName
    阶段名称
#>
function Write-StageHeader {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$StageName
    )

    $script:BuildState.CurrentStage = $StageName
    $script:BuildState.StageStartTime = Get-Date

    Write-Host ''
    Write-Host ('=' * 70) -ForegroundColor $script:Colors.Info
    Write-BuildLog "开始: $StageName" -Level Info
    Write-Host ('=' * 70) -ForegroundColor $script:Colors.Info
}

<#
.SYNOPSIS
    输出阶段完成标记
#>
function Write-StageFooter {
    [CmdletBinding()]
    param()

    $duration = (Get-Date) - $script:BuildState.StageStartTime
    $durationStr = '{0:mm\:ss\.fff}' -f $duration

    Write-Host ('-' * 70) -ForegroundColor $script:Colors.Info
    Write-BuildLog "完成: $($script:BuildState.CurrentStage) (耗时: $durationStr)" -Level Success
    Write-Host ('=' * 70) -ForegroundColor $script:Colors.Info

    # 记录指标
    $script:BuildState.Metrics[$script:BuildState.CurrentStage] = $duration
}

<#
.SYNOPSIS
    输出构建摘要
#>
function Write-BuildSummary {
    [CmdletBinding()]
    param()

    $totalDuration = (Get-Date) - $script:BuildState.StartTime

    Write-Host ''
    Write-Host ('#' * 70) -ForegroundColor $script:Colors.Info
    Write-Host '#' -NoNewline -ForegroundColor $script:Colors.Info
    Write-Host '                    AAM 构建摘要' -NoNewline -ForegroundColor $script:Colors.Default
    Write-Host '                              #' -ForegroundColor $script:Colors.Info
    Write-Host ('#' * 70) -ForegroundColor $script:Colors.Info

    Write-Host ''
    Write-Host '阶段耗时:' -ForegroundColor $script:Colors.Info
    foreach ($stage in $script:BuildState.Metrics.Keys) {
        $duration = $script:BuildState.Metrics[$stage]
        $durationStr = '{0:mm\:ss\.fff}' -f $duration
        Write-Host "  - $stage`: $durationStr" -ForegroundColor $script:Colors.Default
    }

    Write-Host ''
    Write-Host "总耗时: {0:mm\:ss\.fff}" -f $totalDuration -ForegroundColor $script:Colors.Info

    if ($script:BuildState.Warnings.Count -gt 0) {
        Write-Host ''
        Write-Host "警告数: $($script:BuildState.Warnings.Count)" -ForegroundColor $script:Colors.Warning
    }

    if ($script:BuildState.Errors.Count -gt 0) {
        Write-Host ''
        Write-Host "错误数: $($script:BuildState.Errors.Count)" -ForegroundColor $script:Colors.Error
        foreach ($error in $script:BuildState.Errors) {
            Write-Host "  - $error" -ForegroundColor $script:Colors.Error
        }
    }

    Write-Host ''
    if ($script:BuildState.Errors.Count -eq 0) {
        Write-Host '构建成功完成! ✓' -ForegroundColor $script:Colors.Success
    }
    else {
        Write-Host '构建失败，请检查上述错误。' -ForegroundColor $script:Colors.Error
    }
    Write-Host ('#' * 70) -ForegroundColor $script:Colors.Info
}

# ============================================================================
# 环境检查函数
# ============================================================================

<#
.SYNOPSIS
    检查 PowerShell 版本

.OUTPUTS
    System.Boolean
    版本符合要求返回 $true，否则返回 $false
#>
function Test-PowerShellVersion {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-BuildLog '检查 PowerShell 版本...' -Level Debug

    $currentVersion = $PSVersionTable.PSVersion
    Write-BuildLog "当前版本: $currentVersion" -Level Debug

    if ($currentVersion -lt $script:RequiredPowerShellVersion) {
        Write-BuildLog "PowerShell 版本过低。需要: $($script:RequiredPowerShellVersion)+, 当前: $currentVersion" -Level Error
        Write-BuildLog '请升级到 PowerShell 7.0 或更高版本: https://aka.ms/powershell' -Level Error
        return $false
    }

    Write-BuildLog "PowerShell 版本检查通过 ($currentVersion)" -Level Success
    return $true
}

<#
.SYNOPSIS
    检查 CMake 安装与版本

.OUTPUTS
    System.Boolean
    CMake 可用且版本符合要求返回 $true，否则返回 $false
#>
function Test-CMakeInstallation {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-BuildLog '检查 CMake 安装...' -Level Debug

    $cmakePath = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmakePath) {
        Write-BuildLog 'CMake 未找到。请安装 CMake 3.25 或更高版本。' -Level Error
        Write-BuildLog '下载地址: https://cmake.org/download/' -Level Error
        return $false
    }

    try {
        $versionOutput = cmake --version 2>&1 | Select-Object -First 1
        if ($versionOutput -match 'cmake version (\d+\.\d+\.\d+)') {
            $currentVersion = [Version]$matches[1]
            Write-BuildLog "找到 CMake $currentVersion 位于: $($cmakePath.Source)" -Level Debug

            if ($currentVersion -lt $script:RequiredCMakeVersion) {
                Write-BuildLog "CMake 版本过低。需要: $($script:RequiredCMakeVersion)+, 当前: $currentVersion" -Level Error
                return $false
            }

            Write-BuildLog "CMake 版本检查通过 ($currentVersion)" -Level Success
            return $true
        }
        else {
            Write-BuildLog '无法解析 CMake 版本输出' -Level Error
            return $false
        }
    }
    catch {
        Write-BuildLog "检查 CMake 版本时出错: $_" -Level Error
        return $false
    }
}

<#
.SYNOPSIS
    检查 Visual Studio 2022 安装

.OUTPUTS
    System.Boolean
    找到可用的 Visual Studio 2022 返回 $true，否则返回 $false
#>
function Test-VisualStudioInstallation {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-BuildLog '检查 Visual Studio 2022 安装...' -Level Debug

    # 检查 vswhere 工具
    $vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

    if (-not (Test-Path $vsWherePath)) {
        Write-BuildLog 'vswhere.exe 未找到，尝试直接查找 MSBuild...' -Level Warning

        # 备选方案：直接查找 MSBuild
        $msbuildPaths = @(
            "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
            "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
            "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
            "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
        )

        foreach ($path in $msbuildPaths) {
            if (Test-Path $path) {
                Write-BuildLog "找到 MSBuild: $path" -Level Success
                return $true
            }
        }

        Write-BuildLog 'Visual Studio 2022 未找到。请安装 Visual Studio 2022。' -Level Error
        Write-BuildLog '需要组件: 使用 C++ 的桌面开发' -Level Error
        return $false
    }

    try {
        $vsInfo = & $vsWherePath -latest -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath -format value 2>$null

        if ($vsInfo) {
            Write-BuildLog "找到 Visual Studio 2022: $vsInfo" -Level Success
            return $true
        }
        else {
            Write-BuildLog 'Visual Studio 2022 (v17.0+) 未找到或未安装 C++ 工作负载' -Level Error
            Write-BuildLog '请安装 Visual Studio 2022 并确保选择"使用 C++ 的桌面开发"工作负载' -Level Error
            return $false
        }
    }
    catch {
        Write-BuildLog "检查 Visual Studio 时出错: $_" -Level Error
        return $false
    }
}

<#
.SYNOPSIS
    检查并配置 vcpkg

.OUTPUTS
    System.Boolean
    vcpkg 可用返回 $true，否则返回 $false
#>
function Test-VcpkgConfiguration {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-BuildLog '检查 vcpkg 配置...' -Level Debug

    # 检查用户指定的 VCPKG_ROOT
    if ($VcpkgRoot -and (Test-Path $VcpkgRoot)) {
        $env:VCPKG_ROOT = $VcpkgRoot
        Write-BuildLog "使用指定的 vcpkg 路径: $VcpkgRoot" -Level Debug
    }

    # 检查环境变量
    if ($env:VCPKG_ROOT -and (Test-Path $env:VCPKG_ROOT)) {
        $vcpkgExe = Join-Path $env:VCPKG_ROOT 'vcpkg.exe'
        if (Test-Path $vcpkgExe) {
            Write-BuildLog "找到 vcpkg: $vcpkgExe" -Level Success

            # 检查版本
            try {
                $versionOutput = & $vcpkgExe version 2>&1 | Select-Object -First 1
                Write-BuildLog "vcpkg 版本: $versionOutput" -Level Debug
            }
            catch {
                Write-BuildLog "无法获取 vcpkg 版本: $_" -Level Warning
            }

            return $true
        }
    }

    # 检查 PATH 中的 vcpkg
    $vcpkgInPath = Get-Command vcpkg -ErrorAction SilentlyContinue
    if ($vcpkgInPath) {
        $vcpkgExe = $vcpkgInPath.Source
        $env:VCPKG_ROOT = Split-Path $vcpkgExe -Parent
        Write-BuildLog "找到 vcpkg (PATH): $vcpkgExe" -Level Success

        # 检查版本
        try {
            $versionOutput = & $vcpkgExe version 2>&1 | Select-Object -First 1
            Write-BuildLog "vcpkg 版本: $versionOutput" -Level Debug
        }
        catch {
            Write-BuildLog "无法获取 vcpkg 版本: $_" -Level Warning
        }

        return $true
    }

    # 检查项目内的 vcpkg
    $projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
    $projectVcpkg = Join-Path $projectRoot 'vcpkg' 'vcpkg.exe'

    if (Test-Path $projectVcpkg) {
        $env:VCPKG_ROOT = Split-Path $projectVcpkg -Parent
        Write-BuildLog "使用项目内 vcpkg: $projectVcpkg" -Level Success
        return $true
    }

    # 检查常见安装位置
    $commonPaths = @(
        'C:\vcpkg'
        'C:\Tools\vcpkg'
        "$env:USERPROFILE\vcpkg"
        "$env:LOCALAPPDATA\vcpkg"
    )

    foreach ($path in $commonPaths) {
        $vcpkgExe = Join-Path $path 'vcpkg.exe'
        if (Test-Path $vcpkgExe) {
            $env:VCPKG_ROOT = $path
            Write-BuildLog "找到 vcpkg: $vcpkgExe" -Level Success
            return $true
        }
    }

    Write-BuildLog 'vcpkg 未找到。将尝试使用项目内 vcpkg 子模块。' -Level Warning
    return Initialize-VcpkgSubmodule
}

<#
.SYNOPSIS
    初始化 vcpkg 子模块

.OUTPUTS
    System.Boolean
    初始化成功返回 $true，否则返回 $false
#>
function Initialize-VcpkgSubmodule {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-BuildLog '初始化 vcpkg 子模块...' -Level Debug

    $projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
    $vcpkgDir = Join-Path $projectRoot 'vcpkg'

    # 检查是否已克隆
    if (Test-Path (Join-Path $vcpkgDir 'vcpkg.exe')) {
        Write-BuildLog 'vcpkg 子模块已存在' -Level Success
        $env:VCPKG_ROOT = $vcpkgDir
        return $true
    }

    # 检查 .vcpkg-root 标记文件
    if (-not (Test-Path (Join-Path $vcpkgDir '.vcpkg-root'))) {
        Write-BuildLog 'vcpkg 目录不存在，尝试从 Git 克隆...' -Level Warning

        try {
            Push-Location $projectRoot

            # 初始化子模块
            Write-BuildLog '执行: git submodule update --init --recursive' -Level Debug
            $output = git submodule update --init --recursive 2>&1

            if ($LASTEXITCODE -ne 0) {
                Write-BuildLog "子模块初始化失败: $output" -Level Error

                # 备选：直接克隆
                Write-BuildLog '尝试直接克隆 vcpkg...' -Level Warning
                Remove-Item $vcpkgDir -Recurse -Force -ErrorAction SilentlyContinue

                $cloneOutput = git clone https://github.com/microsoft/vcpkg.git $vcpkgDir 2>&1
                if ($LASTEXITCODE -ne 0) {
                    Write-BuildLog "克隆失败: $cloneOutput" -Level Error
                    return $false
                }
            }

            Pop-Location
        }
        catch {
            Write-BuildLog "初始化 vcpkg 时出错: $_" -Level Error
            return $false
        }
    }

    # 引导 vcpkg
    if (-not (Test-Path (Join-Path $vcpkgDir 'vcpkg.exe'))) {
        Write-BuildLog '引导 vcpkg (首次运行，可能需要几分钟)...' -Level Info

        try {
            Push-Location $vcpkgDir

            $bootstrapOutput = .\bootstrap-vcpkg.bat 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-BuildLog "vcpkg 引导失败: $bootstrapOutput" -Level Error
                Pop-Location
                return $false
            }

            Pop-Location
        }
        catch {
            Write-BuildLog "引导 vcpkg 时出错: $_" -Level Error
            return $false
        }
    }

    $env:VCPKG_ROOT = $vcpkgDir
    Write-BuildLog 'vcpkg 初始化完成' -Level Success
    return $true
}

<#
.SYNOPSIS
    检查项目依赖是否已安装

.OUTPUTS
    System.Boolean
    所有依赖可用返回 $true，否则返回 $false
#>
function Test-DependenciesInstalled {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-BuildLog '检查项目依赖...' -Level Debug

    $projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

    # 确定 triplet
    if (-not $Triplet) {
        $Triplet = 'x64-windows'
        Write-BuildLog "使用默认 triplet: $Triplet" -Level Debug
    }

    $vcpkgInstalledDir = Join-Path $projectRoot "vcpkg_installed\$Triplet"

    if (-not (Test-Path $vcpkgInstalledDir)) {
        Write-BuildLog "依赖未安装。期望目录: $vcpkgInstalledDir" -Level Warning
        return $false
    }

    # 检查关键依赖
    $requiredPackages = @(
        'spdlog',
        'fmt',
        'protobuf',
        'grpc',
        'opencv4',
        'zeromq'
    )

    $includeDir = Join-Path $vcpkgInstalledDir 'include'
    $missingPackages = @()

    foreach ($package in $requiredPackages) {
        $found = $false

        switch ($package) {
            'spdlog' { $found = Test-Path (Join-Path $includeDir 'spdlog' 'spdlog.h') }
            'fmt' { $found = Test-Path (Join-Path $includeDir 'fmt' 'format.h') }
            'protobuf' { $found = Test-Path (Join-Path $includeDir 'google' 'protobuf' 'message.h') }
            'grpc' { $found = Test-Path (Join-Path $includeDir 'grpcpp' 'grpcpp.h') }
            'opencv4' { $found = Test-Path (Join-Path $includeDir 'opencv2' 'opencv.hpp') }
            'zeromq' { $found = Test-Path (Join-Path $includeDir 'zmq.h') }
        }

        if (-not $found) {
            $missingPackages += $package
        }
    }

    if ($missingPackages.Count -gt 0) {
        Write-BuildLog "缺少依赖包: $($missingPackages -join ', ')" -Level Warning
        return $false
    }

    Write-BuildLog '所有依赖已安装' -Level Success
    return $true
}

<#
.SYNOPSIS
    安装项目依赖

.OUTPUTS
    System.Boolean
    安装成功返回 $true，否则返回 $false
#>
function Install-Dependencies {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-BuildLog '开始安装依赖...' -Level Info

    $projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
    $vcpkgJson = Join-Path $projectRoot 'vcpkg.json'

    if (-not (Test-Path $vcpkgJson)) {
        Write-BuildLog "vcpkg.json 未找到: $vcpkgJson" -Level Error
        return $false
    }

    # 确定 triplet
    if (-not $Triplet) {
        $Triplet = 'x64-windows'
    }

    $vcpkgExe = Join-Path $env:VCPKG_ROOT 'vcpkg.exe'

    try {
        Push-Location $projectRoot

        # 安装依赖
        Write-BuildLog "执行: vcpkg install --triplet=$Triplet" -Level Info

        $installArgs = @(
            'install',
            "--triplet=$Triplet",
            '--recurse'
        )

        if ($Detailed) {
            $installArgs += '--debug'
        }

        & $vcpkgExe @installArgs

        if ($LASTEXITCODE -ne 0) {
            Write-BuildLog '依赖安装失败' -Level Error
            Pop-Location
            return $false
        }

        Pop-Location
    }
    catch {
        Write-BuildLog "安装依赖时出错: $_" -Level Error
        return $false
    }

    Write-BuildLog '依赖安装完成' -Level Success
    return $true
}

# ============================================================================
# 构建阶段函数
# ============================================================================

<#
.SYNOPSIS
    清理构建目录

.OUTPUTS
    System.Boolean
    清理成功返回 $true，否则返回 $false
#>
function Clear-BuildDirectory {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-BuildLog "清理构建目录: $BuildDirectory" -Level Info

    try {
        if (Test-Path $BuildDirectory) {
            Remove-Item $BuildDirectory -Recurse -Force
            Write-BuildLog '构建目录已清理' -Level Success
        }
        else {
            Write-BuildLog '构建目录不存在，无需清理' -Level Debug
        }
        return $true
    }
    catch {
        Write-BuildLog "清理构建目录失败: $_" -Level Error
        return $false
    }
}

<#
.SYNOPSIS
    执行 CMake 配置

.OUTPUTS
    System.Boolean
    配置成功返回 $true，否则返回 $false
#>
function Invoke-CMakeConfigure {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-BuildLog '开始 CMake 配置...' -Level Info

    $projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

    # 确保构建目录存在
    if (-not (Test-Path $BuildDirectory)) {
        New-Item $BuildDirectory -ItemType Directory -Force | Out-Null
    }

    # 确定生成器
    $generator = if ($UseNinja) { 'Ninja' } else { 'Visual Studio 17 2022' }

    # 构建 CMake 参数
    $cmakeArgs = @(
        '-B', $BuildDirectory,
        '-S', $projectRoot,
        '-G', $generator
    )

    # 平台参数（仅 Visual Studio 生成器需要）
    if (-not $UseNinja) {
        $cmakeArgs += '-A', 'x64'
    }

    # 构建类型
    $cmakeArgs += "-DCMAKE_BUILD_TYPE=$Configuration"

    # 安装目录
    $installPath = Resolve-Path (Join-Path $projectRoot $InstallDirectory) -ErrorAction SilentlyContinue
    if (-not $installPath) {
        $installPath = Join-Path $projectRoot $InstallDirectory
    }
    $cmakeArgs += "-DCMAKE_INSTALL_PREFIX=$installPath"

    # 选项
    if ($EnableCUDA) {
        $cmakeArgs += '-DAAM_ENABLE_CUDA=ON'
    }

    if ($EnableStaticLink) {
        $cmakeArgs += '-DAAM_STATIC_LINK=ON'
    }

    if ($RunTests) {
        $cmakeArgs += '-DAAM_BUILD_TESTS=ON'
    }
    else {
        $cmakeArgs += '-DAAM_BUILD_TESTS=OFF'
    }

    # vcpkg 工具链
    if ($env:VCPKG_ROOT) {
        $toolchainFile = Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'
        if (Test-Path $toolchainFile) {
            $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
        }
    }

    # Triplet
    if ($Triplet) {
        $cmakeArgs += "-DAAM_VCPKG_TRIPLET=$Triplet"
    }

    # 详细输出
    if ($Detailed) {
        $cmakeArgs += '--debug-output'
    }

    Write-BuildLog "CMake 参数: $($cmakeArgs -join ' ')" -Level Debug

    try {
        $output = cmake @cmakeArgs 2>&1
        $exitCode = $LASTEXITCODE

        if ($Detailed -or $exitCode -ne 0) {
            $output | ForEach-Object { Write-BuildLog $_ -Level Debug }
        }

        if ($exitCode -ne 0) {
            Write-BuildLog "CMake 配置失败 (退出码: $exitCode)" -Level Error
            return $false
        }

        Write-BuildLog 'CMake 配置完成' -Level Success
        return $true
    }
    catch {
        Write-BuildLog "CMake 配置时出错: $_" -Level Error
        return $false
    }
}

<#
.SYNOPSIS
    执行 CMake 构建

.OUTPUTS
    System.Boolean
    构建成功返回 $true，否则返回 $false
#>
function Invoke-CMakeBuild {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-BuildLog '开始编译项目...' -Level Info

    # 确定并行任务数
    $jobs = if ($ParallelJobs -gt 0) { $ParallelJobs } else { $env:NUMBER_OF_PROCESSORS }

    Write-BuildLog "使用 $jobs 个并行任务" -Level Debug

    $cmakeArgs = @(
        '--build', $BuildDirectory,
        '--config', $Configuration,
        '--parallel', $jobs
    )

    if ($Detailed) {
        $cmakeArgs += '--verbose'
    }

    try {
        $output = cmake @cmakeArgs 2>&1
        $exitCode = $LASTEXITCODE

        if ($Detailed -or $exitCode -ne 0) {
            $output | ForEach-Object { Write-BuildLog $_ -Level Debug }
        }

        if ($exitCode -ne 0) {
            Write-BuildLog "编译失败 (退出码: $exitCode)" -Level Error
            return $false
        }

        Write-BuildLog '编译完成' -Level Success
        return $true
    }
    catch {
        Write-BuildLog "编译时出错: $_" -Level Error
        return $false
    }
}

<#
.SYNOPSIS
    执行测试

.OUTPUTS
    System.Boolean
    测试通过返回 $true，否则返回 $false
#>
function Invoke-CTest {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-BuildLog '开始执行测试...' -Level Info

    $ctestArgs = @(
        '--test-dir', $BuildDirectory,
        '-C', $Configuration,
        '--output-on-failure'
    )

    if ($Detailed) {
        $ctestArgs += '-V'
    }
    else {
        $ctestArgs += '--progress'
    }

    try {
        $output = ctest @ctestArgs 2>&1
        $exitCode = $LASTEXITCODE

        if ($Detailed -or $exitCode -ne 0) {
            $output | ForEach-Object { Write-BuildLog $_ -Level Debug }
        }

        if ($exitCode -ne 0) {
            Write-BuildLog "测试失败 (退出码: $exitCode)" -Level Warning
            return $false
        }

        Write-BuildLog '测试通过' -Level Success
        return $true
    }
    catch {
        Write-BuildLog "执行测试时出错: $_" -Level Error
        return $false
    }
}

<#
.SYNOPSIS
    执行安装

.OUTPUTS
    System.Boolean
    安装成功返回 $true，否则返回 $false
#>
function Invoke-CMakeInstall {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-BuildLog '开始安装...' -Level Info

    $cmakeArgs = @(
        '--install', $BuildDirectory,
        '--config', $Configuration
    )

    try {
        $output = cmake @cmakeArgs 2>&1
        $exitCode = $LASTEXITCODE

        if ($Detailed -or $exitCode -ne 0) {
            $output | ForEach-Object { Write-BuildLog $_ -Level Debug }
        }

        if ($exitCode -ne 0) {
            Write-BuildLog "安装失败 (退出码: $exitCode)" -Level Error
            return $false
        }

        Write-BuildLog "安装完成，目标目录: $InstallDirectory" -Level Success
        return $true
    }
    catch {
        Write-BuildLog "安装时出错: $_" -Level Error
        return $false
    }
}

# ============================================================================
# 主函数
# ============================================================================

<#
.SYNOPSIS
    主构建流程

.DESCRIPTION
    执行完整的 AAM 构建流程，包括环境检查、依赖安装、配置、编译、测试和安装

.OUTPUTS
    System.Int32
    0 表示成功，非0 表示失败
#>
function Invoke-Build {
    [CmdletBinding()]
    [OutputType([int])]
    param()

    $script:BuildState.StartTime = Get-Date

    # 显示欢迎信息
    Write-Host ''
    Write-Host '╔══════════════════════════════════════════════════════════════════════╗' -ForegroundColor $script:Colors.Info
    Write-Host '║         Arknights Auto Machine (AAM) 构建系统                        ║' -ForegroundColor $script:Colors.Info
    Write-Host '║         版本: v1.0.0 | PowerShell 7+ | Windows 11                    ║' -ForegroundColor $script:Colors.Info
    Write-Host '╚══════════════════════════════════════════════════════════════════════╝' -ForegroundColor $script:Colors.Info
    Write-Host ''

    Write-BuildLog "配置: $Configuration | 构建目录: $BuildDirectory | 并行任务: $(if ($ParallelJobs -gt 0) { $ParallelJobs } else { '自动' })" -Level Info

    # ============================================================================
    # 阶段 1: 环境预检
    # ============================================================================
    Write-StageHeader -StageName '环境预检'

    # 检查 PowerShell 版本
    if (-not (Test-PowerShellVersion)) {
        return 1
    }

    # 检查 CMake
    if (-not (Test-CMakeInstallation)) {
        return 1
    }

    # 检查 Visual Studio（仅当不使用 Ninja 时）
    if (-not $UseNinja) {
        if (-not (Test-VisualStudioInstallation)) {
            return 1
        }
    }

    Write-StageFooter

    # ============================================================================
    # 阶段 2: 依赖管理
    # ============================================================================
    if (-not $SkipDeps) {
        Write-StageHeader -StageName '依赖管理'

        # 检查 vcpkg
        if (-not (Test-VcpkgConfiguration)) {
            Write-BuildLog 'vcpkg 配置失败' -Level Error
            return 1
        }

        # 检查并安装依赖
        if (-not (Test-DependenciesInstalled)) {
            if (-not (Install-Dependencies)) {
                Write-BuildLog '依赖安装失败' -Level Error
                return 1
            }
        }

        Write-StageFooter
    }
    else {
        Write-BuildLog '跳过依赖检查' -Level Warning
    }

    # ============================================================================
    # 阶段 3: 清理（如果请求）
    # ============================================================================
    if ($Clean) {
        Write-StageHeader -StageName '清理构建目录'

        if (-not (Clear-BuildDirectory)) {
            return 1
        }

        Write-StageFooter
    }

    # ============================================================================
    # 阶段 4: CMake 配置
    # ============================================================================
    if (-not $SkipConfigure) {
        Write-StageHeader -StageName 'CMake 配置'

        if (-not (Invoke-CMakeConfigure)) {
            return 1
        }

        Write-StageFooter
    }
    else {
        Write-BuildLog '跳过 CMake 配置' -Level Warning
    }

    # ============================================================================
    # 阶段 5: 编译
    # ============================================================================
    if (-not $SkipBuild) {
        Write-StageHeader -StageName '编译项目'

        if (-not (Invoke-CMakeBuild)) {
            return 1
        }

        Write-StageFooter
    }
    else {
        Write-BuildLog '跳过编译' -Level Warning
    }

    # ============================================================================
    # 阶段 6: 测试
    # ============================================================================
    if ($RunTests -and -not $SkipBuild) {
        Write-StageHeader -StageName '执行测试'

        Invoke-CTest | Out-Null  # 测试失败不中断构建流程

        Write-StageFooter
    }

    # ============================================================================
    # 阶段 7: 安装
    # ============================================================================
    if (-not $SkipBuild) {
        Write-StageHeader -StageName '安装'

        if (-not (Invoke-CMakeInstall)) {
            return 1
        }

        Write-StageFooter
    }

    # ============================================================================
    # 构建摘要
    # ============================================================================
    Write-BuildSummary

    return 0
}

# ============================================================================
# 脚本入口点
# ============================================================================

# 设置错误操作偏好
$ErrorActionPreference = 'Stop'

# 设置执行目录为项目根目录
$projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Push-Location $projectRoot

try {
    # 执行主构建流程
    $exitCode = Invoke-Build

    # 退出
    exit $exitCode
}
catch {
    Write-BuildLog "未处理的异常: $_" -Level Error
    Write-BuildLog $_.ScriptStackTrace -Level Debug
    exit 1
}
finally {
    Pop-Location
}
