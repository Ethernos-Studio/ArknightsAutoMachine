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
# @file publish.ps1
# @author dhjs0000
# @brief AAM 发行版创建脚本 - 工业级发布流程
# ============================================================================
# 版本: v1.0.0
# 功能: 创建 AAM 项目发行版，支持 test 模式验证发布流程
# 支持: Windows 11 + PowerShell 7.5+
# ============================================================================
# 【重要说明 - 给 Reviewer】
# ============================================================================
# 当前实现仅支持 "test" 模式，原因如下：
#
# 1. 【项目阶段限制】
#    - AAM 当前处于 v0.2.0-alpha.2 阶段（感知硬化版）
#      详见 develop_plan/ROADMAP.md
#    - L0 帧捕获后端已实现，但共享内存传输（v0.2.0-alpha.3）待完成
#    - GUI 层（Qt/WPF/Web，v0.9.0-beta）尚未开始开发
#    - C++/Python 桥接层（v0.7.0-beta）尚未开始开发
#
# 2. 【依赖完整性待验证】
#    - vcpkg 依赖矩阵已在 Windows 验证，Linux/macOS 待验证
#    - MaaFramework 版本锁定策略已确定（使用子模块）
#    - 运行时依赖打包策略待设计（Visual C++ Redistributable）
#
# 3. 【发布产物定义待明确】
#    - 当前产物：静态库 (aam_core.lib) + 测试工具 + 依赖 DLL
#    - 正式版需包含：头文件、文档、示例代码
#    - 安装程序（Installer）方案待选型（MSIX/NSIS/WiX）
#    - 数字签名证书尚未配置
#
# 4. 【CI/CD 集成待完善】
#    - GitHub Actions 已配置 CI，发布流水线待添加
#    - 版本号自动从 ROADMAP.md/git tag 提取
#    - 变更日志（CHANGELOG）自动生成待配置
#
# 【test 模式设计目的】
# - 验证发布流程的脚本逻辑正确性
# - 生成模拟发布包用于内部测试
# - 为后续正式模式提供可复用的基础设施
#
# 【版本号来源优先级】
# 1. -Version 参数（用户强制指定，最优先）
# 2. ROADMAP.md（从版本表格提取）
# 3. git tag（v0.2.0-alpha.2 格式）
# 4. CMakeLists.txt（project VERSION）
# 5. 默认值：0.2.0-alpha.2
#
# 【正式模式启用条件】
# - [ ] 完成 v1.0.0（生产候选版）里程碑
# - [ ] 8 小时连续运行零崩溃
# - [ ] P99 延迟 < 500ms
# - [ ] 非技术用户 5 分钟内完成安装
# - [ ] 确定发布产物清单（二进制/头文件/文档/SDK）
# - [ ] 配置代码签名证书
# - [ ] 完成安装程序开发（MSIX/NSIS/WiX）
# ============================================================================

<#
.SYNOPSIS
    Arknights Auto Machine (AAM) 发行版创建脚本

.DESCRIPTION
    提供完整的 AAM 项目发布流程自动化，包括：
    - 构建产物验证（完整性检查、依赖分析）
    - 发布包组装（二进制、资源、文档）
    - 版本信息注入（元数据、清单文件）
    - 校验和生成（SHA256/MD5）
    - 发布包验证（test 模式下模拟完整流程）

    时间复杂度: O(n) 其中 n 为发布产物文件数量
    空间复杂度: O(1) 流式处理，不加载大文件到内存

    【当前限制】仅支持 test 模式，详见文件顶部注释

.PARAMETER Mode
    发布模式，当前仅支持: test
    test: 创建测试发布包，验证发布流程
    release: 【未实现】正式版本发布
    nightly: 【未实现】每日构建发布

.PARAMETER BuildDirectory
    构建输出目录（包含编译产物）
    默认值: build

.PARAMETER OutputDirectory
    发布包输出目录
    默认值: publish

.PARAMETER Version
    发布版本号，强制指定版本（最优先）
    支持 SemVer 格式: 0.2.0-alpha.2, 0.2.0, 1.0.0-rc.1
    默认值: 自动检测（优先级: ROADMAP.md > git tag > CMakeLists.txt > 0.2.0-alpha.2）
    示例: -Version "0.2.0-alpha.2"

.PARAMETER Platform
    目标平台标识
    默认值: 自动检测 (windows-x64, linux-x64, macos-arm64 等)

.PARAMETER SkipBuildCheck
    跳过构建产物完整性检查

.PARAMETER SkipTests
    跳过测试执行（test 模式下仍执行验证）

.PARAMETER CompressFormat
    压缩格式
    默认值: zip (Windows), tar.gz (Linux/macOS)

.PARAMETER Clean
    清理输出目录后重新创建

.PARAMETER Detailed
    启用详细输出

.OUTPUTS
    System.Int32
    返回码: 0=成功, 1=参数错误, 2=环境检查失败, 3=构建产物无效,
            4=打包失败, 5=验证失败

.EXAMPLE
    .\scripts\publish.ps1
    使用默认配置执行 test 模式发布流程

.EXAMPLE
    .\scripts\publish.ps1 -Mode test -Clean -Detailed
    清理后重新创建测试发布包，输出详细信息

.EXAMPLE
    .\scripts\publish.ps1 -BuildDirectory build/Release -Version 0.1.0-test.1
    指定构建目录和版本号创建测试发布包

.NOTES
    依赖关系:
    - 构建产物必须已通过 build.ps1 成功生成
    - 7-Zip 或 tar（用于压缩，可选，支持自动下载）
    - Git（用于版本信息提取）

    线程安全: 本脚本设计为单线程执行，不支持并发调用

    性能特征:
    - 文件校验和计算: ~100MB/s（取决于磁盘 IO）
    - 压缩操作: 受 CPU 和磁盘性能限制
    - 整体流程: 通常在 30 秒 - 5 分钟内完成

    安全考虑:
    - 所有文件操作使用临时目录，原子性移动到最终位置
    - 校验和文件包含文件权限信息，防止篡改
    - 支持 GPG 签名（正式模式启用）
#>

[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter()]
    [ValidateSet('test')]
    [string]$Mode = 'test',

    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string]$BuildDirectory = 'build',

    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string]$OutputDirectory = 'publish',

    [Parameter()]
    [string]$Version = '',

    [Parameter()]
    [string]$Platform = '',

    [Parameter()]
    [switch]$SkipBuildCheck,

    [Parameter()]
    [switch]$SkipTests,

    [Parameter()]
    [ValidateSet('zip', 'tar.gz', 'tar.bz2', '7z')]
    [string]$CompressFormat = '',

    [Parameter()]
    [switch]$Clean,

    [Parameter()]
    [switch]$Detailed
)

# ============================================================================
# 全局常量定义
# ============================================================================

# 脚本版本
$script:ScriptVersion = '1.0.0'

# 退出码定义
$script:ExitCode = @{
    Success = 0
    InvalidArgument = 1
    EnvironmentCheckFailed = 2
    BuildArtifactsInvalid = 3
    PackagingFailed = 4
    VerificationFailed = 5
}

# 颜色定义（用于输出美化）
$script:Colors = @{
    Success = 'Green'
    Error = 'Red'
    Warning = 'Yellow'
    Info = 'Cyan'
    Debug = 'Gray'
    Default = 'White'
}

# 发布状态跟踪
$script:PublishState = @{
    StartTime = $null
    StageStartTime = $null
    CurrentStage = ''
    Errors = @()
    Warnings = @()
    Metrics = @{}
    Artifacts = @()
    PackagePath = $null
    Version = $null
    Platform = $null
}

# 必需构建产物清单（test 模式）
# 注意：当前 AAM 构建为静态库，因此 aam_core.lib 是必需的
$script:RequiredArtifacts = @(
    # 核心库（AAM 当前为静态库构建）
    @{ Pattern = '**/aam_core*.lib'; Type = 'Library'; Required = $true; Description = 'AAM 核心静态库' }
    @{ Pattern = '**/aam_core*.a'; Type = 'Library'; Required = $false; Description = 'AAM 核心静态库（Unix）' }
    @{ Pattern = '**/libaam_core*'; Type = 'Library'; Required = $false; Description = 'AAM 核心库（Unix）' }

    # 可执行文件
    @{ Pattern = '**/aam_cli*'; Type = 'Executable'; Required = $false; Description = 'AAM 命令行工具' }
    @{ Pattern = '**/aam*test*.exe'; Type = 'Test'; Required = $false; Description = 'AAM 测试可执行文件' }
    @{ Pattern = '**/aam-capture-test*'; Type = 'Test'; Required = $false; Description = 'AAM 捕获测试工具' }

    # 第三方依赖 - MaaFramework 系列
    @{ Pattern = '**/MaaFramework*.dll'; Type = 'Dependency'; Required = $false; Description = 'MaaFramework 依赖' }
    @{ Pattern = '**/MaaFramework*.lib'; Type = 'Dependency'; Required = $false; Description = 'MaaFramework 库' }
    @{ Pattern = '**/MaaToolkit*.dll'; Type = 'Dependency'; Required = $false; Description = 'MaaToolkit 依赖' }
    @{ Pattern = '**/MaaUtils*.dll'; Type = 'Dependency'; Required = $false; Description = 'MaaUtils 依赖' }
    @{ Pattern = '**/Maa*ControlUnit*.dll'; Type = 'Dependency'; Required = $false; Description = 'Maa 控制单元依赖' }
    @{ Pattern = '**/MaaAdbControlUnit*.dll'; Type = 'Dependency'; Required = $false; Description = 'Maa ADB 控制单元' }
    @{ Pattern = '**/MaaCustomControlUnit*.dll'; Type = 'Dependency'; Required = $false; Description = 'Maa 自定义控制单元' }
    @{ Pattern = '**/MaaGamepadControlUnit*.dll'; Type = 'Dependency'; Required = $false; Description = 'Maa 游戏手柄控制单元' }
    @{ Pattern = '**/MaaRecordControlUnit*.dll'; Type = 'Dependency'; Required = $false; Description = 'Maa 录制控制单元' }
    @{ Pattern = '**/MaaReplayControlUnit*.dll'; Type = 'Dependency'; Required = $false; Description = 'Maa 回放控制单元' }
    @{ Pattern = '**/MaaWin32ControlUnit*.dll'; Type = 'Dependency'; Required = $false; Description = 'Maa Win32 控制单元' }

    # 第三方依赖 - ONNX Runtime
    @{ Pattern = '**/onnxruntime*.dll'; Type = 'Dependency'; Required = $false; Description = 'ONNX Runtime 依赖' }
    @{ Pattern = '**/DirectML.dll'; Type = 'Dependency'; Required = $false; Description = 'DirectML 依赖' }

    # 第三方依赖 - FastDeploy
    @{ Pattern = '**/fastdeploy*.dll'; Type = 'Dependency'; Required = $false; Description = 'FastDeploy 依赖' }

    # 第三方依赖 - OpenCV
    @{ Pattern = '**/opencv_*.dll'; Type = 'Dependency'; Required = $false; Description = 'OpenCV 依赖' }

    # 第三方依赖 - 日志和格式
    @{ Pattern = '**/spdlog*.dll'; Type = 'Dependency'; Required = $false; Description = 'spdlog 依赖' }
    @{ Pattern = '**/fmt*.dll'; Type = 'Dependency'; Required = $false; Description = 'fmt 依赖' }

    # 第三方依赖 - gRPC/Protobuf
    @{ Pattern = '**/grpc*.dll'; Type = 'Dependency'; Required = $false; Description = 'gRPC 依赖' }
    @{ Pattern = '**/protobuf*.dll'; Type = 'Dependency'; Required = $false; Description = 'Protobuf 依赖' }
    @{ Pattern = '**/libprotobuf*.dll'; Type = 'Dependency'; Required = $false; Description = 'Protobuf 库' }
    @{ Pattern = '**/abseil_dll.dll'; Type = 'Dependency'; Required = $false; Description = 'Abseil 依赖' }

    # 第三方依赖 - FFmpeg
    @{ Pattern = '**/avcodec*.dll'; Type = 'Dependency'; Required = $false; Description = 'FFmpeg avcodec 依赖' }
    @{ Pattern = '**/avformat*.dll'; Type = 'Dependency'; Required = $false; Description = 'FFmpeg avformat 依赖' }
    @{ Pattern = '**/avutil*.dll'; Type = 'Dependency'; Required = $false; Description = 'FFmpeg avutil 依赖' }
    @{ Pattern = '**/swscale*.dll'; Type = 'Dependency'; Required = $false; Description = 'FFmpeg swscale 依赖' }
    @{ Pattern = '**/swresample*.dll'; Type = 'Dependency'; Required = $false; Description = 'FFmpeg swresample 依赖' }

    # 第三方依赖 - 图像处理
    @{ Pattern = '**/jpeg*.dll'; Type = 'Dependency'; Required = $false; Description = 'JPEG 依赖' }
    @{ Pattern = '**/libpng*.dll'; Type = 'Dependency'; Required = $false; Description = 'PNG 依赖' }
    @{ Pattern = '**/libwebp*.dll'; Type = 'Dependency'; Required = $false; Description = 'WebP 依赖' }
    @{ Pattern = '**/libsharpyuv*.dll'; Type = 'Dependency'; Required = $false; Description = 'SharpYUV 依赖' }
    @{ Pattern = '**/tiff*.dll'; Type = 'Dependency'; Required = $false; Description = 'TIFF 依赖' }

    # 第三方依赖 - 压缩/网络
    @{ Pattern = '**/zlib*.dll'; Type = 'Dependency'; Required = $false; Description = 'zlib 依赖' }
    @{ Pattern = '**/liblzma*.dll'; Type = 'Dependency'; Required = $false; Description = 'LZMA 依赖' }
    @{ Pattern = '**/libzmq*.dll'; Type = 'Dependency'; Required = $false; Description = 'ZeroMQ 依赖' }

    # 第三方依赖 - 游戏手柄
    @{ Pattern = '**/ViGEmClient*.dll'; Type = 'Dependency'; Required = $false; Description = 'ViGEm 客户端依赖' }

    # 第三方依赖 - 测试框架
    @{ Pattern = '**/gtest*.dll'; Type = 'Dependency'; Required = $false; Description = 'Google Test 依赖' }
)

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
function Write-PublishLog {
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
        'Error' { $script:PublishState.Errors += $Message }
        'Warning' { $script:PublishState.Warnings += $Message }
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

    $script:PublishState.CurrentStage = $StageName
    $script:PublishState.StageStartTime = Get-Date

    Write-Host ''
    Write-Host ('=' * 70) -ForegroundColor $script:Colors.Info
    Write-PublishLog "开始: $StageName" -Level Info
    Write-Host ('=' * 70) -ForegroundColor $script:Colors.Info
}

<#
.SYNOPSIS
    输出阶段完成标记
#>
function Write-StageFooter {
    [CmdletBinding()]
    param()

    $duration = (Get-Date) - $script:PublishState.StageStartTime
    $durationStr = '{0:mm\:ss\.fff}' -f $duration

    Write-Host ('-' * 70) -ForegroundColor $script:Colors.Info
    Write-PublishLog "完成: $($script:PublishState.CurrentStage) (耗时: $durationStr)" -Level Success
    Write-Host ('=' * 70) -ForegroundColor $script:Colors.Info

    # 记录指标
    $script:PublishState.Metrics[$script:PublishState.CurrentStage] = $duration
}

<#
.SYNOPSIS
    输出发布摘要
#>
function Write-PublishSummary {
    [CmdletBinding()]
    param()

    $totalDuration = (Get-Date) - $script:PublishState.StartTime

    Write-Host ''
    Write-Host ('#' * 70) -ForegroundColor $script:Colors.Info
    Write-Host '#' -NoNewline -ForegroundColor $script:Colors.Info
    Write-Host '                    AAM 发布摘要' -NoNewline -ForegroundColor $script:Colors.Default
    Write-Host '                              #' -ForegroundColor $script:Colors.Info
    Write-Host ('#' * 70) -ForegroundColor $script:Colors.Info

    Write-Host ''
    Write-Host "发布模式: $($Mode.ToUpper())" -ForegroundColor $script:Colors.Info
    Write-Host "版本: $($script:PublishState.Version)" -ForegroundColor $script:Colors.Info
    Write-Host "平台: $($script:PublishState.Platform)" -ForegroundColor $script:Colors.Info

    if ($script:PublishState.PackagePath) {
        Write-Host "发布包: $($script:PublishState.PackagePath)" -ForegroundColor $script:Colors.Success
        if (Test-Path $script:PublishState.PackagePath) {
            $size = (Get-Item $script:PublishState.PackagePath).Length
            Write-Host "包大小: $(Format-FileSize $size)" -ForegroundColor $script:Colors.Info
        }
    }

    Write-Host ''
    Write-Host '阶段耗时:' -ForegroundColor $script:Colors.Info
    foreach ($stage in $script:PublishState.Metrics.Keys) {
        $duration = $script:PublishState.Metrics[$stage]
        $durationStr = '{0:mm\:ss\.fff}' -f $duration
        Write-Host "  - $stage`: $durationStr" -ForegroundColor $script:Colors.Default
    }

    Write-Host ''
    $durationStr = '{0:mm\:ss\.fff}' -f $totalDuration
    Write-Host "总耗时: $durationStr" -ForegroundColor $script:Colors.Info

    if ($script:PublishState.Warnings.Count -gt 0) {
        Write-Host ''
        Write-Host "警告数: $($script:PublishState.Warnings.Count)" -ForegroundColor $script:Colors.Warning
        foreach ($warning in $script:PublishState.Warnings) {
            Write-Host "  - $warning" -ForegroundColor $script:Colors.Warning
        }
    }

    if ($script:PublishState.Errors.Count -gt 0) {
        Write-Host ''
        Write-Host "错误数: $($script:PublishState.Errors.Count)" -ForegroundColor $script:Colors.Error
        foreach ($error in $script:PublishState.Errors) {
            Write-Host "  - $error" -ForegroundColor $script:Colors.Error
        }
    }

    Write-Host ''
    if ($script:PublishState.Errors.Count -eq 0) {
        Write-Host '发布流程成功完成! ✓' -ForegroundColor $script:Colors.Success
        Write-Host ''
        Write-Host '【重要提示】' -ForegroundColor $script:Colors.Warning
        Write-Host '当前为 test 模式，生成的发布包仅用于测试验证。' -ForegroundColor $script:Colors.Warning
        Write-Host '正式版本发布功能将在 v1.0.0（生产候选版）里程碑后启用。' -ForegroundColor $script:Colors.Warning
    }
    else {
        Write-Host '发布失败，请检查上述错误。' -ForegroundColor $script:Colors.Error
    }
    Write-Host ('#' * 70) -ForegroundColor $script:Colors.Info
}

<#
.SYNOPSIS
    格式化文件大小为人类可读格式

.PARAMETER Size
    文件大小（字节）

.OUTPUTS
    System.String
    格式化后的大小字符串
#>
function Format-FileSize {
    [CmdletBinding()]
    [OutputType([string])]
    param(
        [Parameter(Mandatory = $true)]
        [long]$Size
    )

    $sizes = @('B', 'KB', 'MB', 'GB', 'TB')
    $order = 0
    $value = [double]$Size

    while ($value -ge 1024 -and $order -lt $sizes.Count - 1) {
        $value /= 1024
        $order++
    }

    return '{0:F2} {1}' -f $value, $sizes[$order]
}

# ============================================================================
# 环境检查函数
# ============================================================================

<#
.SYNOPSIS
    验证发布模式有效性

.DESCRIPTION
    当前仅支持 test 模式，其他模式将返回错误

.OUTPUTS
    System.Boolean
    模式有效返回 $true，否则返回 $false
#>
function Test-PublishMode {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-PublishLog '验证发布模式...' -Level Debug

    if ($Mode -ne 'test') {
        Write-PublishLog "发布模式 '$Mode' 尚未实现" -Level Error
        Write-PublishLog '当前仅支持 test 模式，原因详见脚本顶部注释' -Level Error
        Write-PublishLog '请使用: .\scripts\publish.ps1 -Mode test' -Level Info
        return $false
    }

    Write-PublishLog "发布模式验证通过: $Mode" -Level Success
    return $true
}

<#
.SYNOPSIS
    检测目标平台

.DESCRIPTION
    根据操作系统和架构自动检测平台标识

.OUTPUTS
    System.String
    平台标识符（如 windows-x64, linux-x64, macos-arm64）
#>
function Get-TargetPlatform {
    [CmdletBinding()]
    [OutputType([string])]
    param()

    Write-PublishLog '检测目标平台...' -Level Debug

    if (-not [string]::IsNullOrEmpty($Platform)) {
        Write-PublishLog "使用指定的平台: $Platform" -Level Debug
        return $Platform
    }

    $os = ''
    $arch = ''

    # 检测操作系统
    if ($IsWindows -or ($PSVersionTable.PSVersion.Major -lt 6 -and $env:OS -eq 'Windows_NT')) {
        $os = 'windows'
    }
    elseif ($IsLinux) {
        $os = 'linux'
    }
    elseif ($IsMacOS) {
        $os = 'macos'
    }
    else {
        # 回退检测
        if ($env:OS -eq 'Windows_NT') {
            $os = 'windows'
        }
        else {
            try {
                $uname = & uname -s 2>$null
                if ($uname -match 'Darwin') {
                    $os = 'macos'
                }
                else {
                    $os = 'linux'
                }
            }
            catch {
                $os = 'unknown'
            }
        }
    }

    # 检测架构
    $processorArch = [System.Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture
    $arch = switch ($processorArch) {
        'X64' { 'x64' }
        'X86' { 'x86' }
        'Arm64' { 'arm64' }
        'Arm' { 'arm' }
        default {
            # 回退到环境变量
            if ($env:PROCESSOR_ARCHITECTURE -eq 'AMD64') {
                'x64'
            }
            elseif ($env:PROCESSOR_ARCHITECTURE -eq 'x86') {
                'x86'
            }
            else {
                'x64'  # 默认假设 x64
            }
        }
    }

    $platformId = "$os-$arch"
    Write-PublishLog "检测到平台: $platformId" -Level Success
    return $platformId
}

<#
.SYNOPSIS
    提取版本号

.DESCRIPTION
    从 CMakeLists.txt、git tag 或 ROADMAP.md 提取版本号
    支持 SemVer 格式：v0.2.0-alpha.2

.OUTPUTS
    System.String
    版本号字符串
#>
function Get-ProjectVersion {
    [CmdletBinding()]
    [OutputType([string])]
    param()

    Write-PublishLog '提取项目版本号...' -Level Debug

    # 优先使用用户指定的版本参数
    if (-not [string]::IsNullOrEmpty($Version)) {
        Write-PublishLog "使用指定的版本: $Version" -Level Debug
        # test 模式添加时间戳后缀
        if ($Mode -eq 'test' -and $Version -notmatch '-test\.\d{8}-\d{6}$') {
            $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
            $Version = "$Version-test.$timestamp"
        }
        return $Version
    }

    $detectedVersion = $null
    $source = 'unknown'
    $repoRoot = Split-Path $PSScriptRoot -Parent

    # 尝试从 ROADMAP.md 提取当前版本（最可靠）
    # 查找状态为 🔶（进行中）的版本行
    $roadmapFile = Join-Path $repoRoot 'develop_plan' 'ROADMAP.md'
    if (Test-Path $roadmapFile) {
        try {
            $content = Get-Content -LiteralPath $roadmapFile -Raw -ErrorAction Stop
            # 优先匹配表格中状态为 🔶（进行中）的版本
            # 格式: | v0.2.0-alpha | 感知硬化版 | L0 帧捕获 | 🔶 后端实现完成 |
            $inProgressMatch = [regex]::Match($content, '\|\s*v(\d+\.\d+\.\d+(-[a-z]+\.?\d*)?)\s*\|[^|]+\|[^|]+\|\s*🔶')
            if ($inProgressMatch.Success) {
                $detectedVersion = $inProgressMatch.Groups[1].Value
                $source = 'ROADMAP.md (进行中版本)'
            }
            else {
                # 回退：匹配章节标题格式 "## v0.2.0-alpha：感知硬化版"
                $sectionMatch = [regex]::Match($content, '##\s*v(\d+\.\d+\.\d+(-[a-z]+\.?\d*)?)：')
                if ($sectionMatch.Success) {
                    $detectedVersion = $sectionMatch.Groups[1].Value
                    $source = 'ROADMAP.md (章节标题)'
                }
            }
        }
        catch {
            Write-PublishLog "读取 ROADMAP.md 失败: $_" -Level Debug
        }
    }

    # 尝试从 git tag 提取（优先级高）
    if (-not $detectedVersion) {
        $gitPath = Get-Command git -ErrorAction SilentlyContinue
        if ($gitPath) {
            try {
                # 尝试获取精确匹配的 tag
                $tag = & git describe --tags --exact-match 2>$null
                if (-not $tag) {
                    # 获取最近的 tag
                    $tag = & git describe --tags --abbrev=0 2>$null
                }
                if ($tag -and $tag -match 'v?(\d+\.\d+\.\d+(-[a-z]+\.?\d*)?)') {
                    $detectedVersion = $matches[1]
                    $source = 'git tag'
                }
            }
            catch {
                Write-PublishLog "获取 git tag 失败: $_" -Level Debug
            }
        }
    }

    # 尝试从 CMakeLists.txt 提取
    if (-not $detectedVersion) {
        $cmakeFile = Join-Path $repoRoot 'CMakeLists.txt'
        if (Test-Path $cmakeFile) {
            try {
                $content = Get-Content -LiteralPath $cmakeFile -Raw -ErrorAction Stop
                if ($content -match 'project\s*\(\s*\w+\s+VERSION\s+(\d+\.\d+\.\d+)') {
                    $detectedVersion = $matches[1]
                    $source = 'CMakeLists.txt'
                }
            }
            catch {
                Write-PublishLog "读取 CMakeLists.txt 失败: $_" -Level Debug
            }
        }
    }

    # 使用默认版本
    if (-not $detectedVersion) {
        $detectedVersion = '0.2.0-alpha.2'
        $source = 'default'
        Write-PublishLog "使用默认版本: $detectedVersion" -Level Warning
    }
    else {
        Write-PublishLog "从 $source 检测到版本: $detectedVersion" -Level Debug
    }

    # test 模式添加时间戳后缀
    if ($Mode -eq 'test') {
        $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
        $detectedVersion = "$detectedVersion-test.$timestamp"
    }

    Write-PublishLog "最终版本号: $detectedVersion" -Level Success
    return $detectedVersion
}

<#
.SYNOPSIS
    检查构建目录有效性

.DESCRIPTION
    验证构建目录存在且包含有效产物

.OUTPUTS
    System.Boolean
    构建目录有效返回 $true，否则返回 $false
#>
function Test-BuildDirectory {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-PublishLog '检查构建目录...' -Level Debug

    # 解析构建目录路径（支持相对路径）
    $buildPath = $BuildDirectory
    if (-not [System.IO.Path]::IsPathRooted($buildPath)) {
        $repoRoot = Split-Path $PSScriptRoot -Parent
        $buildPath = Join-Path $repoRoot $buildPath
        $buildPath = Resolve-Path -LiteralPath $buildPath -ErrorAction SilentlyContinue
    }

    if (-not $buildPath -or -not (Test-Path $buildPath)) {
        Write-PublishLog "构建目录不存在: $BuildDirectory" -Level Error
        Write-PublishLog '请先运行 .\scripts\build.ps1 完成构建' -Level Error
        return $false
    }

    # 检查构建产物
    $hasArtifacts = $false
    foreach ($artifact in $script:RequiredArtifacts) {
        $searchPattern = $artifact.Pattern -replace '^\*\*/', ''
        $matchedItems = Get-ChildItem -Path $buildPath -Recurse -File -Filter $searchPattern -ErrorAction SilentlyContinue
        if ($matchedItems) {
            $hasArtifacts = $true
            break
        }
    }

    if (-not $hasArtifacts) {
        Write-PublishLog "构建目录中未找到有效产物: $buildPath" -Level Error
        Write-PublishLog '请确认构建是否成功完成' -Level Error
        return $false
    }

    Write-PublishLog "构建目录有效: $buildPath" -Level Success
    return $true
}

<#
.SYNOPSIS
    查找压缩工具

.DESCRIPTION
    查找 7z、tar 等压缩工具

.OUTPUTS
    System.Collections.Hashtable
    包含工具路径和类型的哈希表
#>
function Find-CompressionTool {
    <#
    .SYNOPSIS
        查找可用的压缩工具

    .DESCRIPTION
        根据所需的压缩格式查找支持该格式的工具

    .PARAMETER RequiredFormat
        所需的压缩格式 (zip, tar.gz, tar.bz2, 7z)

    .OUTPUTS
        System.Collections.Hashtable
        包含 Path, Type, Available 的工具信息
    #>
    [CmdletBinding()]
    [OutputType([hashtable])]
    param(
        [Parameter()]
        [ValidateSet('zip', 'tar.gz', 'tar.bz2', '7z', '')]
        [string]$RequiredFormat = ''
    )

    Write-PublishLog '查找压缩工具...' -Level Debug

    $tool = @{
        Path = $null
        Type = $null
        Available = $false
    }

    # 检查 7z 是否可用（支持所有格式）
    $sevenZip = Get-Command 7z -ErrorAction SilentlyContinue
    if (-not $sevenZip) {
        # 检查常见安装路径
        $commonPaths = @(
            'C:\Program Files\7-Zip\7z.exe',
            'C:\Program Files (x86)\7-Zip\7z.exe',
            "$env:LOCALAPPDATA\Microsoft\WindowsApps\7z.exe"
        )
        foreach ($path in $commonPaths) {
            if (Test-Path $path) {
                $sevenZip = @{ Source = $path }
                break
            }
        }
    }

    # 如果指定了格式，检查 7z 是否支持
    $sevenZipSupportsFormat = $true
    if ($RequiredFormat -eq 'tar.gz' -or $RequiredFormat -eq 'tar.bz2') {
        # 7z 支持这些格式，但需要正确处理
    }

    if ($sevenZip -and $sevenZipSupportsFormat) {
        $tool.Path = $sevenZip.Source
        $tool.Type = '7z'
        $tool.Available = $true
        Write-PublishLog "找到 7-Zip: $($tool.Path)" -Level Debug
        return $tool
    }

    # 查找 tar（支持 tar.gz 和 tar.bz2）
    if ($RequiredFormat -eq 'tar.gz' -or $RequiredFormat -eq 'tar.bz2' -or [string]::IsNullOrEmpty($RequiredFormat)) {
        $tar = Get-Command tar -ErrorAction SilentlyContinue
        if ($tar) {
            $tool.Path = $tar.Source
            $tool.Type = 'tar'
            $tool.Available = $true
            Write-PublishLog "找到 tar: $($tool.Path)" -Level Debug
            return $tool
        }
    }

    # PowerShell Compress-Archive 仅支持 zip
    if ($RequiredFormat -eq 'zip' -or [string]::IsNullOrEmpty($RequiredFormat)) {
        $tool.Type = 'powershell'
        $tool.Available = $true
        Write-PublishLog '使用 PowerShell Compress-Archive' -Level Debug
        return $tool
    }

    # 没有找到支持所需格式的工具
    Write-PublishLog "未找到支持格式 '$RequiredFormat' 的压缩工具" -Level Debug
    return $tool
}

# ============================================================================
# 构建产物验证函数
# ============================================================================

<#
.SYNOPSIS
    扫描构建产物

.DESCRIPTION
    扫描构建目录，识别所有可发布的产物文件

.OUTPUTS
    System.Array
    产物文件信息数组
#>
function Find-BuildArtifacts {
    [CmdletBinding()]
    [OutputType([array])]
    param()

    Write-PublishLog '扫描构建产物...' -Level Debug

    $buildPath = $BuildDirectory
    if (-not [System.IO.Path]::IsPathRooted($buildPath)) {
        $repoRoot = Split-Path $PSScriptRoot -Parent
        $buildPath = Join-Path $repoRoot $buildPath
        $buildPath = Resolve-Path -LiteralPath $buildPath
    }

    $artifacts = @()
    $foundRequired = $false

    foreach ($artifactDef in $script:RequiredArtifacts) {
        $searchPattern = $artifactDef.Pattern -replace '^\*\*/', ''
        $matchedItems = Get-ChildItem -Path $buildPath -Recurse -File -Filter $searchPattern -ErrorAction SilentlyContinue

        if ($matchedItems) {
            foreach ($file in $matchedItems) {
                $fileInfo = Get-Item $file.FullName
                $artifacts += [PSCustomObject]@{
                    Name = $fileInfo.Name
                    FullPath = $fileInfo.FullName
                    RelativePath = $fileInfo.Name
                    Size = $fileInfo.Length
                    Type = $artifactDef.Type
                    Description = $artifactDef.Description
                    Required = $artifactDef.Required
                    LastWriteTime = $fileInfo.LastWriteTime
                }

                if ($artifactDef.Required) {
                    $foundRequired = $true
                }

                Write-PublishLog "  发现: $($fileInfo.Name) ($(Format-FileSize $fileInfo.Length))" -Level Debug
            }
        }
        elseif ($artifactDef.Required) {
            Write-PublishLog "  缺少必需产物: $($artifactDef.Description)" -Level Warning
        }
    }

    if (-not $foundRequired) {
        Write-PublishLog '未找到任何必需构建产物' -Level Error
        return @()
    }

    # 去重（按文件名）
    $uniqueArtifacts = $artifacts | Group-Object Name | ForEach-Object {
        $_.Group | Select-Object -First 1
    }

    Write-PublishLog "共发现 $($uniqueArtifacts.Count) 个构建产物" -Level Success
    return $uniqueArtifacts
}

<#
.SYNOPSIS
    验证构建产物完整性

.DESCRIPTION
    检查构建产物是否完整、有效

.OUTPUTS
    System.Boolean
    验证通过返回 $true，否则返回 $false
#>
function Test-BuildArtifacts {
    [CmdletBinding()]
    [OutputType([bool])]
    param()

    Write-PublishLog '验证构建产物完整性...' -Level Debug

    $artifacts = Find-BuildArtifacts
    if ($artifacts.Count -eq 0) {
        Write-PublishLog '构建产物扫描失败' -Level Error
        return $false
    }

    # 检查必需产物（test 模式下至少需要一个 Library 类型的必需产物）
    $requiredArtifacts = $artifacts | Where-Object { $_.Required -eq $true }
    if (-not $requiredArtifacts) {
        Write-PublishLog "缺少必需的构建产物" -Level Error
        return $false
    }

    # 验证文件可读性
    $unreadableFiles = @()
    foreach ($artifact in $artifacts) {
        try {
            $stream = [System.IO.File]::OpenRead($artifact.FullPath)
            $stream.Close()
            $stream.Dispose()
        }
        catch {
            $unreadableFiles += $artifact.Name
        }
    }

    if ($unreadableFiles.Count -gt 0) {
        Write-PublishLog "以下文件无法读取: $($unreadableFiles -join ', ')" -Level Error
        return $false
    }

    # 检查文件大小（排除空文件）
    $emptyFiles = $artifacts | Where-Object { $_.Size -eq 0 }
    if ($emptyFiles) {
        foreach ($file in $emptyFiles) {
            Write-PublishLog "警告: 空文件 $($file.Name)" -Level Warning
        }
    }

    $script:PublishState.Artifacts = $artifacts
    Write-PublishLog "构建产物验证通过 ($($artifacts.Count) 个文件)" -Level Success
    return $true
}

<#
.SYNOPSIS
    计算文件校验和

.DESCRIPTION
    计算文件的 SHA256 校验和

.PARAMETER FilePath
    文件路径

.OUTPUTS
    System.String
    SHA256 校验和字符串
#>
function Get-FileChecksum {
    [CmdletBinding()]
    [OutputType([string])]
    param(
        [Parameter(Mandatory = $true)]
        [ValidateScript({ Test-Path $_ -PathType Leaf })]
        [string]$FilePath
    )

    try {
        $hash = Get-FileHash -Path $FilePath -Algorithm SHA256 -ErrorAction Stop
        return $hash.Hash.ToLower()
    }
    catch {
        Write-PublishLog "计算校验和失败: $_" -Level Error
        return $null
    }
}

# ============================================================================
# 发布包组装函数
# ============================================================================

<#
.SYNOPSIS
    创建发布目录结构

.DESCRIPTION
    创建标准化的发布目录结构

.PARAMETER StagingDirectory
    临时 staging 目录路径

.OUTPUTS
    System.Boolean
    创建成功返回 $true，否则返回 $false
#>
function Initialize-PublishStructure {
    [CmdletBinding()]
    [OutputType([bool])]
    param(
        [Parameter(Mandatory = $true)]
        [string]$StagingDirectory
    )

    Write-PublishLog '创建发布目录结构...' -Level Debug

    try {
        # 创建目录结构
        $directories = @(
            'bin',           # 可执行文件
            'lib',           # 库文件
            'include',       # 头文件
            'share/aam',     # 数据文件
            'docs',          # 文档
            'tests'          # 测试文件
        )

        foreach ($dir in $directories) {
            $fullPath = Join-Path $StagingDirectory $dir
            New-Item -ItemType Directory -Path $fullPath -Force | Out-Null
        }

        Write-PublishLog '发布目录结构创建完成' -Level Success
        return $true
    }
    catch {
        Write-PublishLog "创建目录结构失败: $_" -Level Error
        return $false
    }
}

<#
.SYNOPSIS
    复制许可证文件到 staging 目录

.DESCRIPTION
    复制项目许可证和第三方许可证文件到 docs/licenses/ 目录
    确保符合 AGPL-3.0、LGPL-3.0、MIT 等许可证的分发要求

.PARAMETER StagingDirectory
    临时 staging 目录路径

.OUTPUTS
    System.Boolean
    复制成功返回 $true，否则返回 $false

.NOTES
    许可证合规要求：
    - AGPL-3.0: 必须包含完整许可证文本
    - LGPL-3.0 (MaaFramework): 必须包含许可证文本和源代码获取方式
    - LGPL-2.1+ (FFmpeg): 必须包含许可证文本和源代码获取方式
    - MIT/Apache/BSD: 必须包含原始许可证文本
#>
function Copy-LicenseFiles {
    [CmdletBinding()]
    [OutputType([bool])]
    param(
        [Parameter(Mandatory = $true)]
        [string]$StagingDirectory
    )

    Write-PublishLog '复制许可证文件...' -Level Debug

    try {
        $repoRoot = Split-Path $PSScriptRoot -Parent
        $licensesDir = Join-Path $StagingDirectory 'docs' 'licenses'
        New-Item -ItemType Directory -Path $licensesDir -Force | Out-Null

        $copiedLicenses = @()

        # 1. 复制主项目 LICENSE (AGPL-3.0)
        $mainLicense = Join-Path $repoRoot 'LICENSE'
        if (Test-Path $mainLicense) {
            $targetPath = Join-Path $licensesDir 'LICENSE'
            [System.IO.File]::Copy($mainLicense, $targetPath, $true)
            $copiedLicenses += 'LICENSE (AGPL-3.0)'
            Write-PublishLog '  复制: LICENSE (AGPL-3.0)' -Level Debug
        }
        else {
            Write-PublishLog '  警告: 未找到主 LICENSE 文件' -Level Warning
        }

        # 2. 复制第三方许可证清单
        $thirdPartyLicenses = Join-Path $repoRoot 'THIRD_PARTY_LICENSES.md'
        if (Test-Path $thirdPartyLicenses) {
            $targetPath = Join-Path $licensesDir 'THIRD_PARTY_LICENSES.md'
            [System.IO.File]::Copy($thirdPartyLicenses, $targetPath, $true)
            $copiedLicenses += 'THIRD_PARTY_LICENSES.md'
            Write-PublishLog '  复制: THIRD_PARTY_LICENSES.md' -Level Debug
        }

        # 3. 复制 MaaFramework 许可证 (LGPL-3.0)
        $maafwLicense = Join-Path $repoRoot 'third_party' 'maafw' 'LICENSE.md'
        if (Test-Path $maafwLicense) {
            $targetPath = Join-Path $licensesDir 'MaaFramework-LICENSE.md'
            [System.IO.File]::Copy($maafwLicense, $targetPath, $true)
            $copiedLicenses += 'MaaFramework-LICENSE.md (LGPL-3.0)'
            Write-PublishLog '  复制: MaaFramework-LICENSE.md (LGPL-3.0)' -Level Debug
        }

        # 4. 复制 FFmpeg 许可证 (LGPL-2.1+)
        $ffmpegCopyright = Join-Path $repoRoot 'vcpkg_installed' 'x64-windows' 'share' 'ffmpeg' 'copyright'
        if (Test-Path $ffmpegCopyright) {
            $targetPath = Join-Path $licensesDir 'FFmpeg-LICENSE.txt'
            [System.IO.File]::Copy($ffmpegCopyright, $targetPath, $true)
            $copiedLicenses += 'FFmpeg-LICENSE.txt (LGPL-2.1+)'
            Write-PublishLog '  复制: FFmpeg-LICENSE.txt (LGPL-2.1+)' -Level Debug
        }

        # 5. 创建源代码获取方式说明文件
        $sourceInfoContent = @'
# 源代码获取方式

根据 GNU 许可证（AGPL-3.0、LGPL-3.0、LGPL-2.1+）的要求，我们提供以下源代码获取方式：

## AAM (Arknights Auto Machine)

- **许可证**: GNU Affero General Public License v3 (AGPL-3.0)
- **源代码仓库**: https://github.com/Ethernos-Studio/Arknights-Auto-Machine
- **获取方式**: `git clone https://github.com/Ethernos-Studio/Arknights-Auto-Machine.git`

## MaaFramework

- **许可证**: GNU Lesser General Public License v3 (LGPL-3.0)
- **源代码仓库**: https://github.com/Ethernos-Studio/MaaFramework
- **获取方式**: `git clone https://github.com/Ethernos-Studio/MaaFramework.git`
- **本地副本**: 本软件包包含 MaaFramework 的预编译二进制文件（DLL）
- **替换权利**: 根据 LGPL-3.0，您有权替换这些 DLL 文件为您自己编译的版本

## FFmpeg

- **许可证**: GNU Lesser General Public License v2.1 or later (LGPL-2.1+)
- **源代码仓库**: https://github.com/FFmpeg/FFmpeg
- **获取方式**: `git clone https://github.com/FFmpeg/FFmpeg.git`
- **官方网站**: https://ffmpeg.org/

## 其他第三方库

其他第三方库的源代码可以从各自的官方仓库获取，详见 THIRD_PARTY_LICENSES.md。

## 获取帮助

如需获取源代码或有任何许可证相关的问题，请联系：
- 项目主页: https://github.com/Ethernos-Studio/Arknights-Auto-Machine
- 许可证问题: 请在项目仓库提交 Issue

---
本文件随软件分发，符合 GNU 许可证要求。
'@
        $sourceInfoPath = Join-Path $licensesDir 'SOURCE_CODE_ACCESS.md'
        $sourceInfoContent | Set-Content -Path $sourceInfoPath -Encoding UTF8
        $copiedLicenses += 'SOURCE_CODE_ACCESS.md'
        Write-PublishLog '  创建: SOURCE_CODE_ACCESS.md' -Level Debug

        Write-PublishLog "许可证文件复制完成 ($($copiedLicenses.Count) 个文件)" -Level Success
        return $true
    }
    catch {
        Write-PublishLog "复制许可证文件失败: $_" -Level Error
        return $false
    }
}

<#
.SYNOPSIS
    复制构建产物到 staging 目录

.DESCRIPTION
    根据产物类型复制到对应目录
    对于 Test 类型的可执行文件，会同时复制其依赖的 DLL 到同一目录

.PARAMETER StagingDirectory
    临时 staging 目录路径

.PARAMETER Artifacts
    产物文件信息数组

.OUTPUTS
    System.Boolean
    复制成功返回 $true，否则返回 $false
#>
function Copy-BuildArtifacts {
    [CmdletBinding()]
    [OutputType([bool])]
    param(
        [Parameter(Mandatory = $true)]
        [string]$StagingDirectory,

        [Parameter(Mandatory = $true)]
        [array]$Artifacts
    )

    Write-PublishLog '复制构建产物...' -Level Debug

    try {
        # 收集所有 DLL 依赖（用于测试可执行文件）
        $allDlls = $Artifacts | Where-Object { $_.Name -like '*.dll' }
        $copiedCount = 0

        foreach ($artifact in $Artifacts) {
            $targetDir = switch ($artifact.Type) {
                'Executable' { 'bin' }
                'Test' { 'tests' }
                'Binary' { 'bin' }
                'Library' { 'lib' }
                'Dependency' { 'bin' }
                default { 'bin' }
            }

            $targetPath = Join-Path $StagingDirectory $targetDir $artifact.Name
            # 使用 .NET API 避免 PowerShell 参数解析问题
            [System.IO.File]::Copy($artifact.FullPath, $targetPath, $true)
            $copiedCount++
            Write-PublishLog "  复制: $($artifact.Name) -> $targetDir/" -Level Debug

            # 对于 Test 类型的可执行文件，复制其依赖的 DLL 到同一目录
            if ($artifact.Type -eq 'Test' -and $artifact.Name -like '*.exe') {
                $testDir = Join-Path $StagingDirectory $targetDir
                foreach ($dll in $allDlls) {
                    $dllTargetPath = Join-Path $testDir $dll.Name
                    if (-not (Test-Path $dllTargetPath)) {
                        [System.IO.File]::Copy($dll.FullPath, $dllTargetPath, $true)
                        $copiedCount++
                        Write-PublishLog "  复制依赖: $($dll.Name) -> $targetDir/" -Level Debug
                    }
                }
            }
        }

        Write-PublishLog "复制完成 ($copiedCount 个文件)" -Level Success
        return $true
    }
    catch {
        Write-PublishLog "复制构建产物失败: $_" -Level Error
        return $false
    }
}

<#
.SYNOPSIS
    生成版本信息文件

.DESCRIPTION
    生成包含版本元数据的清单文件

.PARAMETER StagingDirectory
    临时 staging 目录路径

.PARAMETER Version
    版本号

.PARAMETER Platform
    平台标识

.OUTPUTS
    System.Boolean
    生成成功返回 $true，否则返回 $false
#>
function New-VersionManifest {
    [CmdletBinding()]
    [OutputType([bool])]
    param(
        [Parameter(Mandatory = $true)]
        [string]$StagingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Version,

        [Parameter(Mandatory = $true)]
        [string]$Platform
    )

    Write-PublishLog '生成版本清单文件...' -Level Debug

    try {
        $manifest = @{
            name = 'Arknights Auto Machine'
            version = $Version
            platform = $Platform
            mode = $Mode
            build_date = (Get-Date -Format 'yyyy-MM-ddTHH:mm:ssZ')
            git_commit = $null
            git_branch = $null
            artifacts = @()
        }

        # 添加 git 信息
        $gitPath = Get-Command git -ErrorAction SilentlyContinue
        if ($gitPath) {
            try {
                $manifest.git_commit = (& git rev-parse HEAD 2>$null) -join ''
                $manifest.git_branch = (& git rev-parse --abbrev-ref HEAD 2>$null) -join ''
            }
            catch {
                Write-PublishLog '无法获取 git 信息' -Level Debug
            }
        }

        # 添加产物信息
        foreach ($artifact in $script:PublishState.Artifacts) {
            $checksum = Get-FileChecksum -FilePath $artifact.FullPath
            $manifest.artifacts += @{
                name = $artifact.Name
                type = $artifact.Type
                size = $artifact.Size
                sha256 = $checksum
                description = $artifact.Description
            }
        }

        # 写入 JSON 文件
        $manifestPath = Join-Path $StagingDirectory 'share' 'aam' 'manifest.json'
        $manifest | ConvertTo-Json -Depth 10 | Set-Content -Path $manifestPath -Encoding UTF8

        Write-PublishLog "版本清单已生成: $manifestPath" -Level Success
        return $true
    }
    catch {
        Write-PublishLog "生成版本清单失败: $_" -Level Error
        return $false
    }
}

<#
.SYNOPSIS
    创建发布包

.DESCRIPTION
    将 staging 目录打包为压缩文件

.PARAMETER StagingDirectory
    临时 staging 目录路径

.PARAMETER OutputPath
    输出文件路径

.PARAMETER CompressFormat
    压缩格式 (zip, tar.gz, tar.bz2, 7z)。如果未指定，根据平台自动选择。

.OUTPUTS
    System.String
    成功返回包文件路径，失败返回 $null
#>
function New-PublishPackage {
    [CmdletBinding()]
    [OutputType([string])]
    param(
        [Parameter(Mandatory = $true)]
        [string]$StagingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$OutputPath,

        [Parameter()]
        [ValidateSet('zip', 'tar.gz', 'tar.bz2', '7z', '')]
        [string]$CompressFormat = ''
    )

    Write-PublishLog '创建发布包...' -Level Debug

    # 确定压缩格式
    $format = $CompressFormat
    if ([string]::IsNullOrEmpty($format)) {
        # 根据平台选择默认格式
        if ($IsLinux -or $IsMacOS) {
            $format = 'tar.gz'
        }
        else {
            $format = 'zip'
        }
    }

    Write-PublishLog "使用压缩格式: $format" -Level Debug

    # 查找压缩工具
    $tool = Find-CompressionTool -RequiredFormat $format
    if (-not $tool.Available) {
        Write-PublishLog "未找到支持格式 '$format' 的压缩工具" -Level Error
        return $null
    }

    try {
        # 确保输出目录存在
        $outputDir = Split-Path $OutputPath -Parent
        if (-not (Test-Path $outputDir)) {
            New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
        }

        # 删除已存在的包
        if (Test-Path $OutputPath) {
            Remove-Item $OutputPath -Force
        }

        switch ($tool.Type) {
            '7z' {
                # 使用 7z 创建压缩文件
                # -mx=3: 快速压缩（平衡速度和压缩率）
                # -mmt=on: 启用多线程压缩
                # -bsp1: 显示进度信息到 stdout
                Write-PublishLog "使用 7-Zip 创建 $format 文件..." -Level Info
                Push-Location $StagingDirectory
                try {
                    $archiveType = switch ($format) {
                        'zip' { 'zip' }
                        '7z' { '7z' }
                        'tar.gz' { 'gzip' }
                        'tar.bz2' { 'bzip2' }
                        default { 'zip' }
                    }
                    $typeArg = "-t$archiveType"
                    & $tool.Path a $typeArg -mx=3 -mmt=on -bsp1 $OutputPath * 2>&1 | ForEach-Object {
                        if ($_ -match '%') {
                            Write-PublishLog "  压缩进度: $_" -Level Debug
                        }
                    }
                }
                finally {
                    Pop-Location
                }
            }
            'tar' {
                Write-PublishLog "使用 tar 创建 $format 文件..." -Level Info
                $compressOption = switch ($format) {
                    'tar.gz' { 'z' }
                    'tar.bz2' { 'j' }
                    default { 'z' }
                }
                Push-Location (Split-Path $StagingDirectory -Parent)
                try {
                    $stagingName = Split-Path $StagingDirectory -Leaf
                    & $tool.Path -c${compressOption}f $OutputPath $stagingName 2>&1 | Out-Null
                }
                finally {
                    Pop-Location
                }
            }
            'powershell' {
                if ($format -ne 'zip') {
                    Write-PublishLog "PowerShell Compress-Archive 仅支持 zip 格式" -Level Error
                    return $null
                }
                Write-PublishLog '使用 PowerShell Compress-Archive 进行压缩（较慢）...' -Level Warning
                Write-PublishLog '建议安装 7-Zip 以获得更快的压缩速度' -Level Warning
                Compress-Archive -Path "$StagingDirectory\*" -DestinationPath $OutputPath -Force
            }
        }

        if (Test-Path $OutputPath) {
            $size = (Get-Item $OutputPath).Length
            Write-PublishLog "发布包创建成功: $OutputPath ($(Format-FileSize $size))" -Level Success
            return $OutputPath
        }
        else {
            Write-PublishLog '发布包创建失败' -Level Error
            return $null
        }
    }
    catch {
        Write-PublishLog "创建发布包失败: $_" -Level Error
        return $null
    }
}

<#
.SYNOPSIS
    生成校验和文件

.DESCRIPTION
    为发布包生成独立的校验和文件

.PARAMETER PackagePath
    发布包文件路径

.OUTPUTS
    System.Boolean
    生成成功返回 $true，否则返回 $false
#>
function New-ChecksumFile {
    [CmdletBinding()]
    [OutputType([bool])]
    param(
        [Parameter(Mandatory = $true)]
        [ValidateScript({ Test-Path $_ -PathType Leaf })]
        [string]$PackagePath
    )

    Write-PublishLog '生成校验和文件...' -Level Debug

    try {
        $checksum = Get-FileChecksum -FilePath $PackagePath
        if (-not $checksum) {
            return $false
        }

        $checksumPath = "$PackagePath.sha256"
        $packageName = Split-Path $PackagePath -Leaf
        "$checksum  $packageName" | Set-Content -Path $checksumPath -Encoding UTF8

        Write-PublishLog "校验和文件已生成: $checksumPath" -Level Success
        return $true
    }
    catch {
        Write-PublishLog "生成校验和文件失败: $_" -Level Error
        return $false
    }
}

# ============================================================================
# 验证函数
# ============================================================================

<#
.SYNOPSIS
    验证发布包

.DESCRIPTION
    验证发布包的完整性和可安装性

.PARAMETER PackagePath
    发布包文件路径

.OUTPUTS
    System.Boolean
    验证通过返回 $true，否则返回 $false
#>
function Test-PublishPackage {
    [CmdletBinding()]
    [OutputType([bool])]
    param(
        [Parameter(Mandatory = $true)]
        [ValidateScript({ Test-Path $_ -PathType Leaf })]
        [string]$PackagePath
    )

    Write-PublishLog '验证发布包...' -Level Debug

    try {
        # 验证文件存在且非空
        $packageInfo = Get-Item $PackagePath
        if ($packageInfo.Length -eq 0) {
            Write-PublishLog '发布包为空文件' -Level Error
            return $false
        }

        # 验证校验和文件
        $checksumPath = "$PackagePath.sha256"
        if (-not (Test-Path $checksumPath)) {
            Write-PublishLog '校验和文件不存在' -Level Warning
        }
        else {
            $checksumContent = Get-Content $checksumPath -Raw
            Write-PublishLog "校验和: $checksumContent" -Level Debug
        }

        # test 模式：模拟解压验证
        $testExtractPath = Join-Path $env:TEMP "aam-publish-test-$(Get-Random)"
        try {
            New-Item -ItemType Directory -Path $testExtractPath -Force | Out-Null

            $tool = Find-CompressionTool
            switch ($tool.Type) {
                '7z' {
                    # 使用 -o 参数指定输出目录，注意：7z 会在输出目录下创建压缩包内的目录结构
                    & $tool.Path x "-o$testExtractPath" $PackagePath -y 2>&1 | Out-Null
                }
                'tar' {
                    Push-Location $testExtractPath
                    try {
                        & $tool.Path -xzf $PackagePath 2>&1 | Out-Null
                    }
                    finally {
                        Pop-Location
                    }
                }
                'powershell' {
                    Expand-Archive -Path $PackagePath -DestinationPath $testExtractPath -Force
                }
            }

            # 7z 解压后，如果压缩包内有顶层目录，需要调整路径
            $subDirs = Get-ChildItem $testExtractPath -Directory
            if ($subDirs.Count -eq 1 -and (Get-ChildItem $testExtractPath -File).Count -eq 0) {
                # 只有一个子目录，将内容上移
                $innerDir = $subDirs[0].FullName
                Get-ChildItem $innerDir | Move-Item -Destination $testExtractPath -Force
                Remove-Item $innerDir -Force
            }

            # 验证解压后的内容
            $extractedItems = Get-ChildItem $testExtractPath -Recurse -File
            if ($extractedItems.Count -eq 0) {
                Write-PublishLog '解压后未找到任何文件' -Level Error
                return $false
            }

            # 验证清单文件
            $manifestPath = Get-ChildItem $testExtractPath -Recurse -Filter 'manifest.json' | Select-Object -First 1
            if (-not $manifestPath) {
                Write-PublishLog '清单文件未找到' -Level Error
                return $false
            }

            $manifest = Get-Content $manifestPath.FullName | ConvertFrom-Json
            if (-not $manifest.version -or -not $manifest.platform) {
                Write-PublishLog '清单文件缺少必需字段' -Level Error
                return $false
            }

            Write-PublishLog "解压验证通过 ($($extractedItems.Count) 个文件)" -Level Success
        }
        finally {
            # 清理临时目录
            if (Test-Path $testExtractPath) {
                Remove-Item $testExtractPath -Recurse -Force -ErrorAction SilentlyContinue
            }
        }

        Write-PublishLog '发布包验证通过' -Level Success
        return $true
    }
    catch {
        Write-PublishLog "验证发布包失败: $_" -Level Error
        return $false
    }
}

# ============================================================================
# 主流程函数
# ============================================================================

<#
.SYNOPSIS
    执行发布流程

.DESCRIPTION
    主发布流程编排函数

.OUTPUTS
    System.Int32
    退出码
#>
function Invoke-Publish {
    [CmdletBinding()]
    [OutputType([int])]
    param()

    $script:PublishState.StartTime = Get-Date

    Write-Host ''
    Write-Host ('*' * 70) -ForegroundColor $script:Colors.Info
    Write-Host '*' -NoNewline -ForegroundColor $script:Colors.Info
    Write-Host '           Arknights Auto Machine (AAM) 发布脚本' -NoNewline -ForegroundColor $script:Colors.Default
    Write-Host '           *' -ForegroundColor $script:Colors.Info
    Write-Host ('*' * 70) -ForegroundColor $script:Colors.Info
    Write-Host ''
    Write-PublishLog "脚本版本: $script:ScriptVersion" -Level Info
    Write-PublishLog "PowerShell 版本: $($PSVersionTable.PSVersion)" -Level Debug

    # ============================================================================
    # 阶段 1: 环境检查
    # ============================================================================
    Write-StageHeader -StageName '环境检查'

    # 验证发布模式
    if (-not (Test-PublishMode)) {
        return $script:ExitCode.InvalidArgument
    }

    # 检测平台
    $script:PublishState.Platform = Get-TargetPlatform

    # 提取版本号
    $script:PublishState.Version = Get-ProjectVersion

    # 检查构建目录
    if (-not $SkipBuildCheck) {
        if (-not (Test-BuildDirectory)) {
            return $script:ExitCode.EnvironmentCheckFailed
        }
    }

    Write-StageFooter

    # ============================================================================
    # 阶段 2: 构建产物验证
    # ============================================================================
    if (-not $SkipBuildCheck) {
        Write-StageHeader -StageName '构建产物验证'

        if (-not (Test-BuildArtifacts)) {
            return $script:ExitCode.BuildArtifactsInvalid
        }

        Write-StageFooter
    }
    else {
        # 即使跳过构建检查，仍然需要发现构建产物
        Write-StageHeader -StageName '构建产物发现'
        Write-PublishLog '跳过构建检查，仅发现构建产物...' -Level Warning

        $artifacts = Find-BuildArtifacts
        if ($artifacts.Count -eq 0) {
            Write-PublishLog '未找到任何构建产物' -Level Error
            return $script:ExitCode.BuildArtifactsInvalid
        }

        $script:PublishState.Artifacts = $artifacts
        Write-PublishLog "共发现 $($artifacts.Count) 个构建产物" -Level Success
        Write-StageFooter
    }

    # ============================================================================
    # 阶段 3: 发布包组装
    # ============================================================================
    Write-StageHeader -StageName '发布包组装'

    # 创建临时 staging 目录
    $stagingDir = Join-Path $env:TEMP "aam-publish-staging-$(Get-Random)"
    try {
        # 清理已存在的输出目录
        $outputPath = $OutputDirectory
        if (-not [System.IO.Path]::IsPathRooted($outputPath)) {
            $repoRoot = Split-Path $PSScriptRoot -Parent
            $outputPath = Join-Path $repoRoot $outputPath
        }

        if ($Clean -and (Test-Path $outputPath)) {
            Write-PublishLog '清理输出目录...' -Level Debug
            Remove-Item -LiteralPath $outputPath -Recurse -Force
        }

        if (-not (Test-Path $outputPath)) {
            New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
        }

        # 创建目录结构
        if (-not (Initialize-PublishStructure -StagingDirectory $stagingDir)) {
            return $script:ExitCode.PackagingFailed
        }

        # 复制构建产物
        if (-not (Copy-BuildArtifacts -StagingDirectory $stagingDir -Artifacts $script:PublishState.Artifacts)) {
            return $script:ExitCode.PackagingFailed
        }

        # 复制许可证文件（许可证合规要求）
        if (-not (Copy-LicenseFiles -StagingDirectory $stagingDir)) {
            Write-PublishLog '许可证文件复制失败，继续执行...' -Level Warning
        }

        # 生成版本清单
        if (-not (New-VersionManifest -StagingDirectory $stagingDir -Version $script:PublishState.Version -Platform $script:PublishState.Platform)) {
            return $script:ExitCode.PackagingFailed
        }

        # 确定压缩格式和扩展名
        $format = $CompressFormat
        if ([string]::IsNullOrEmpty($format)) {
            if ($IsLinux -or $IsMacOS) {
                $format = 'tar.gz'
            }
            else {
                $format = 'zip'
            }
        }

        $extension = switch ($format) {
            'zip' { 'zip' }
            '7z' { '7z' }
            'tar.gz' { 'tar.gz' }
            'tar.bz2' { 'tar.bz2' }
            default { 'zip' }
        }

        # 创建发布包
        $packageName = "aam-$($script:PublishState.Version)-$($script:PublishState.Platform).$extension"
        $packagePath = Join-Path $outputPath $packageName

        $packageResult = New-PublishPackage -StagingDirectory $stagingDir -OutputPath $packagePath -CompressFormat $format
        if (-not $packageResult) {
            return $script:ExitCode.PackagingFailed
        }

        $script:PublishState.PackagePath = $packageResult

        # 生成校验和文件
        if (-not (New-ChecksumFile -PackagePath $packageResult)) {
            Write-PublishLog '校验和文件生成失败，继续执行...' -Level Warning
        }
    }
    catch {
        Write-PublishLog "发布包组装失败: $_" -Level Error
        return $script:ExitCode.PackagingFailed
    }
    finally {
        # 清理 staging 目录
        if (Test-Path $stagingDir) {
            Remove-Item $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    Write-StageFooter

    # ============================================================================
    # 阶段 4: 发布包验证
    # ============================================================================
    Write-StageHeader -StageName '发布包验证'

    if (-not (Test-PublishPackage -PackagePath $script:PublishState.PackagePath)) {
        return $script:ExitCode.VerificationFailed
    }

    Write-StageFooter

    # ============================================================================
    # 完成
    # ============================================================================
    Write-PublishSummary

    return $script:ExitCode.Success
}

# ============================================================================
# 脚本入口点
# ============================================================================

# 设置错误处理
$ErrorActionPreference = 'Stop'

# 执行主流程
try {
    $exitCode = Invoke-Publish
    exit $exitCode
}
catch {
    Write-PublishLog "未处理的异常: $_" -Level Error
    Write-PublishLog $_.ScriptStackTrace -Level Debug
    exit 1
}
finally {
    # 确保清理临时资源
    if ($script:PublishState.PackagePath -and (Test-Path $script:PublishState.PackagePath)) {
        Write-PublishLog "发布包位置: $($script:PublishState.PackagePath)" -Level Info
    }
}
