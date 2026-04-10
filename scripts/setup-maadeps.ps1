#!/usr/bin/env pwsh
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
# @file setup-maadeps.ps1
# @brief MaaDeps 依赖自动下载和提取脚本
# ==========================================================================
# 版本: v0.2.0-alpha.2
# 功能: 自动下载并提取 MaaFramework 所需的 MaaDeps 依赖
# 依赖: PowerShell 7+, 7z (用于解压 tar.xz)
# 支持平台: Windows x64
# ==========================================================================

<#
.SYNOPSIS
    自动下载并配置 MaaFramework 的 MaaDeps 依赖

.DESCRIPTION
    该脚本会自动从 MaaXYZ/MaaDeps 仓库下载预编译的依赖包，
    并提取到正确的位置以供 MaaFramework 构建使用。

.PARAMETER Triplet
    目标平台三元组，默认为 x64-windows

.PARAMETER Force
    强制重新下载，即使文件已存在

.EXAMPLE
    .\setup-maadeps.ps1
    下载并配置默认的 x64-windows 依赖

.EXAMPLE
    .\setup-maadeps.ps1 -Triplet "x64-windows" -Force
    强制重新下载依赖

.NOTES
    - 需要 PowerShell 7 或更高版本
    - 需要 7-Zip 安装并添加到 PATH
    - 需要网络连接访问 GitHub
#>

[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [Parameter()]
    [ValidateSet("x64-windows", "x64-windows-static", "x86-windows")]
    [string]$Triplet = "x64-windows",

    [Parameter()]
    [switch]$Force
)

# 错误处理设置
$ErrorActionPreference = "Stop"

# ==========================================================================
# 常量定义
# ==========================================================================

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Resolve-Path (Join-Path $ScriptDir "..")
$MaaUtilsDir = Join-Path $ProjectRoot "third_party\maafw\source\MaaUtils"
$MaaDepsDir = Join-Path $MaaUtilsDir "MaaDeps"
$TarballDir = Join-Path $MaaDepsDir "tarball"

# MaaDeps 版本号 - 需要与 GitHub Releases 保持一致
$MaaDepsVersion = "v2.12.1"

# GitHub 原始 URL 格式 (使用具体版本号而非 latest)
$BaseUrl = "https://github.com/MaaXYZ/MaaDeps/releases/download/$MaaDepsVersion"
$RuntimeFile = "MaaDeps-$Triplet-runtime.tar.xz"
$DevelFile = "MaaDeps-$Triplet-devel.tar.xz"

# 中国大陆 CDN 加速镜像 - 按速度排序 (EdgeOne 和 Fastly 通常最快)
# 注意：gh-proxy 需要使用具体版本号 URL 格式
$CdnUrl = "https://edgeone.gh-proxy.org/https://github.com/MaaXYZ/MaaDeps/releases/download/$MaaDepsVersion"

# 备用 CDN 镜像列表 (按推荐优先级排序)
$FallbackCdns = @(
    # EdgeOne - 全球加速，通常最快
    "https://edgeone.gh-proxy.org/https://github.com/MaaXYZ/MaaDeps/releases/download/$MaaDepsVersion",
    # Fastly CDN
    "https://cdn.gh-proxy.org/https://github.com/MaaXYZ/MaaDeps/releases/download/$MaaDepsVersion",
    # Cloudflare 主站
    "https://gh-proxy.org/https://github.com/MaaXYZ/MaaDeps/releases/download/$MaaDepsVersion",
    # 香港节点 (国内线路优化)
    "https://hk.gh-proxy.org/https://github.com/MaaXYZ/MaaDeps/releases/download/$MaaDepsVersion",
    # 其他备用镜像
    "https://mirror.ghproxy.com/https://github.com/MaaXYZ/MaaDeps/releases/download/$MaaDepsVersion",
    "https://github.moeyy.xyz/https://github.com/MaaXYZ/MaaDeps/releases/download/$MaaDepsVersion"
)

# ==========================================================================
# 辅助函数
# ==========================================================================

function Test-7Zip {
    <#
    .SYNOPSIS
        检查系统中是否安装了 7-Zip
    #>
    $7zPaths = @(
        "${env:ProgramFiles}\7-Zip\7z.exe",
        "${env:ProgramFiles(x86)}\7-Zip\7z.exe",
        (Get-Command 7z -ErrorAction SilentlyContinue).Source
    )

    foreach ($path in $7zPaths) {
        if ($path -and (Test-Path $path)) {
            return $path
        }
    }

    return $null
}

function Test-Command {
    <#
    .SYNOPSIS
        检查命令是否可用
    #>
    param([string]$Command)
    return [bool](Get-Command $Command -ErrorAction SilentlyContinue)
}

function Get-LatestMaaDepsVersion {
    <#
    .SYNOPSIS
        通过 GitHub API 获取 MaaDeps 最新版本号
    .DESCRIPTION
        调用 GitHub Releases API 获取最新版本标签
    .OUTPUTS
        返回版本号字符串 (如 "v2.12.1")
    #>
    param(
        [Parameter()]
        [int]$TimeoutSec = 10
    )

    $apiUrl = "https://api.github.com/repos/MaaXYZ/MaaDeps/releases/latest"

    try {
        Write-Status "Fetching latest MaaDeps version from GitHub API..." "Info"
        Write-Status "API URL: $apiUrl" "Info"

        $response = Invoke-RestMethod -Uri $apiUrl -Method GET -TimeoutSec $TimeoutSec -UseBasicParsing

        if ($response.tag_name) {
            Write-Status "Latest version: $($response.tag_name)" "Success"
            return $response.tag_name
        } else {
            throw "Could not find tag_name in API response"
        }
    }
    catch {
        Write-Status "Failed to fetch version from API: $_" "Warning"
        Write-Status "Falling back to hardcoded version" "Warning"
        return "v2.12.1"  # 默认版本
    }
}

function Test-ChinaMainland {
    <#
    .SYNOPSIS
        检测是否位于中国大陆地区
    .DESCRIPTION
        通过检查系统时区、语言设置和地理位置来推断是否位于中国大陆
    #>
    try {
        # 检查时区
        $timeZone = Get-TimeZone
        $chinaTimeZones = @(
            "China Standard Time",
            "Asia/Shanghai",
            "Asia/Hong_Kong",
            "Asia/Taipei"
        )

        if ($chinaTimeZones -contains $timeZone.Id -or $timeZone.StandardName -match "China|Beijing|Shanghai") {
            return $true
        }

        # 检查系统区域设置
        $culture = Get-Culture
        $region = Get-WinSystemLocale -ErrorAction SilentlyContinue

        if ($culture.Name -match "^zh-(CN|Hans)" -or $region.Name -match "^zh-(CN|Hans)") {
            return $true
        }

        # 检查地理位置（Windows 10+）
        try {
            $geoInfo = Get-WinHomeLocation -ErrorAction SilentlyContinue
            if ($geoInfo.GeoId -eq 45) {  # 45 是中国大陆的 GeoID
                return $true
            }
        }
        catch {
            # 忽略地理位置检查错误
        }

        return $false
    }
    catch {
        # 如果检测失败，默认返回 false
        return $false
    }
}

function Write-Status {
    <#
    .SYNOPSIS
        输出带颜色的状态信息
    #>
    param(
        [Parameter(Mandatory=$true)]
        [string]$Message,

        [Parameter()]
        [ValidateSet("Info", "Success", "Warning", "Error")]
        [string]$Level = "Info"
    )

    $colors = @{
        "Info"    = "Cyan"
        "Success" = "Green"
        "Warning" = "Yellow"
        "Error"   = "Red"
    }

    $prefix = @{
        "Info"    = "[INFO]"
        "Success" = "[OK]"
        "Warning" = "[WARN]"
        "Error"   = "[ERR]"
    }

    Write-Host "$($prefix[$Level]) $Message" -ForegroundColor $colors[$Level]
}

function Invoke-DownloadFile {
    <#
    .SYNOPSIS
        下载文件，支持 aria2c 多线程高速下载
    .DESCRIPTION
        优先使用 aria2c 进行多线程下载，如果不存在则提示用户选择
    #>
    param(
        [Parameter(Mandatory=$true)]
        [string]$Url,

        [Parameter(Mandatory=$true)]
        [string]$OutFile,

        [Parameter()]
        [switch]$Force,

        [Parameter()]
        [int]$TimeoutSec = 300
    )

    if ((Test-Path $OutFile) -and -not $Force) {
        Write-Status "File already exists: $(Split-Path $OutFile -Leaf)" "Info"
        return
    }

    # 显示生成的下载 URL
    Write-Status "Download URL: $Url" "Info"
    Write-Status "Target file: $(Split-Path $OutFile -Leaf)" "Info"

    try {
        # 创建目录
        $outDir = Split-Path -Parent $OutFile
        if (-not (Test-Path $outDir)) {
            New-Item -ItemType Directory -Path $outDir -Force | Out-Null
        }

        # 删除已存在的临时文件
        if (Test-Path $OutFile) {
            Remove-Item $OutFile -Force -ErrorAction SilentlyContinue
        }

        # 优先使用 aria2c (多线程下载器，速度最快)
        $aria2Path = Get-Command aria2c -ErrorAction SilentlyContinue
        if ($aria2Path) {
            # 根据 CPU 核心数计算最优线程数
            # aria2c 限制：max-connection-per-server 最大为 16
            $cpuCores = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors
            $maxConnections = 16  # aria2c 最大允许 16 连接/服务器
            $splitCount = [Math]::Min(64, [Math]::Max(16, $cpuCores * 2))  # 分片数：每核心 2 个，最少 16，最多 64
            $chunkSize = "1M"  # 1MB 分片大小

            Write-Status "Using aria2c for multi-threaded high-speed download..." "Info"
            Write-Status "CPU Cores: $cpuCores logical processors" "Info"
            Write-Status "Connections: $maxConnections per server (max allowed)" "Info"
            Write-Status "Splits: $splitCount x $chunkSize chunks" "Info"

            $startTime = Get-Date

            # 获取文件名和目录（使用相对路径避免 aria2c 路径问题）
            $fileName = Split-Path $OutFile -Leaf
            $outDir = Split-Path -Parent $OutFile

            # 切换到目标目录再执行 aria2c（避免绝对路径问题）
            Push-Location $outDir
            try {
                # 使用 aria2c 下载，最大 16 连接，高分片数
                # -x 16: 每个服务器最大 16 连接 (aria2c 上限)
                # -s N: 分片数为 N (可以大于 16)
                # -k 1M: 每个分片 1MB
                # -j 1: 同时下载 1 个文件
                # --max-connection-per-server=16: 单服务器最大连接数 (aria2c 上限)
                # --min-split-size=1M: 最小分片 1MB
                # --max-download-limit=0: 不限速
                $aria2Args = @(
                    "-x", $maxConnections,
                    "-s", $splitCount,
                    "-k", $chunkSize,
                    "-j", "1",
                    "--max-connection-per-server=$maxConnections",
                    "--min-split-size=$chunkSize",
                    "--max-download-limit=0",
                    "--file-allocation=trunc",
                    "--timeout=60",
                    "--connect-timeout=30",
                    "--max-tries=3",
                    "--retry-wait=5",
                    "--user-agent=Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                    "-o", $fileName,
                    $Url
                )

                & aria2c @aria2Args
            }
            finally {
                Pop-Location
            }

            if ($LASTEXITCODE -eq 0) {
                $totalTime = (Get-Date) - $startTime
                $fileSizeMB = [math]::Round((Get-Item $OutFile).Length / 1MB, 2)
                $avgSpeed = [math]::Round($fileSizeMB / $totalTime.TotalSeconds, 2)
                Write-Status "Download completed: $(Split-Path $OutFile -Leaf) (${fileSizeMB}MB in $($totalTime.ToString('mm\:ss')), avg: ${avgSpeed}MB/s)" "Success"
                return
            } else {
                throw "aria2c exited with code $LASTEXITCODE"
            }
        }

        # aria2c 不存在，提示用户选择
        Write-Status "========================================" "Warning"
        Write-Status "aria2c not found!" "Warning"
        Write-Status "For best download speed (2+ MB/s), please install aria2c:" "Warning"
        Write-Status "  winget install aria2" "Info"
        Write-Status "  or download from: https://github.com/aria2/aria2/releases" "Info"
        Write-Host ""
        Write-Status "Download options:" "Warning"
        Write-Status "  [1] Use curl (slow, ~100KB/s, may take 10+ minutes)" "Info"
        Write-Status "  [2] Manual download (fast, use browser/IDM/aria2c)" "Info"
        Write-Status "  [3] Cancel" "Info"
        Write-Status "========================================" "Warning"

        $choice = Read-Host "Please select an option (1/2/3)"

        switch ($choice) {
            "1" {
                # 使用 curl 下载
                $curlPath = Get-Command curl -ErrorAction SilentlyContinue
                if ($curlPath -and $curlPath.Source -notmatch "PowerShell") {
                    Write-Status "Using curl for download (this will be slow)..." "Warning"

                    $startTime = Get-Date

                    $curlArgs = @(
                        "-L",
                        "-o", $OutFile,
                        "--progress-bar",
                        "--max-time", $TimeoutSec,
                        "--connect-timeout", 30,
                        "-H", "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                        $Url
                    )

                    & curl @curlArgs

                    if ($LASTEXITCODE -eq 0) {
                        $totalTime = (Get-Date) - $startTime
                        $fileSizeMB = [math]::Round((Get-Item $OutFile).Length / 1MB, 2)
                        $avgSpeed = [math]::Round($fileSizeMB / $totalTime.TotalSeconds, 2)
                        Write-Status "Download completed: $(Split-Path $OutFile -Leaf) (${fileSizeMB}MB in $($totalTime.ToString('mm\:ss')), avg: ${avgSpeed}MB/s)" "Success"
                        return
                    } else {
                        throw "curl exited with code $LASTEXITCODE"
                    }
                } else {
                    throw "curl not found"
                }
            }
            "2" {
                # 手动下载模式
                $fileName = Split-Path $OutFile -Leaf
                Write-Host ""
                Write-Status "========================================" "Warning"
                Write-Status "MANUAL DOWNLOAD MODE" "Warning"
                Write-Status "========================================" "Warning"
                Write-Status "Please manually download the following file:" "Warning"
                Write-Host ""
                Write-Status "File: $fileName" "Info"
                Write-Status "URL:  $Url" "Info"
                Write-Host ""
                Write-Status "Save to: $OutFile" "Info"
                Write-Host ""
                Write-Status "Recommended tools for fast download:" "Info"
                Write-Status "  - Browser (Chrome/Edge with Fastly CDN: 2-5 MB/s)" "Info"
                Write-Status "  - IDM (Internet Download Manager)" "Info"
                Write-Status "  - aria2c: aria2c -x 16 -s 16 -k 1M -o '$fileName' '$Url'" "Info"
                Write-Status "========================================" "Warning"

                # 打开浏览器
                Start-Process $Url

                # 等待用户确认
                Write-Host ""
                $confirm = Read-Host "Press Enter after you have downloaded the file to '$OutFile' (or type 'skip' to cancel)"

                if ($confirm -eq "skip") {
                    throw "User cancelled manual download"
                }

                # 检查文件是否存在
                if (Test-Path $OutFile) {
                    $fileSizeMB = [math]::Round((Get-Item $OutFile).Length / 1MB, 2)
                    Write-Status "File found: $fileName (${fileSizeMB}MB)" "Success"
                    return
                } else {
                    throw "File not found at $OutFile. Please ensure the file is downloaded to the correct location."
                }
            }
            default {
                throw "User cancelled download"
            }
        }
    }
    catch {
        Write-Status "Failed to download: $_" "Error"
        if (Test-Path $OutFile) {
            Remove-Item $OutFile -Force -ErrorAction SilentlyContinue
        }
        throw
    }
}

function Expand-TarXz {
    <#
    .SYNOPSIS
        使用 7-Zip 解压 tar.xz 文件
    #>
    param(
        [Parameter(Mandatory=$true)]
        [string]$Archive,

        [Parameter(Mandatory=$true)]
        [string]$Destination,

        [Parameter(Mandatory=$true)]
        [string]$SevenZipPath
    )

    Write-Status "Extracting: $(Split-Path $Archive -Leaf)" "Info"

    try {
        # 先解压 .xz 得到 .tar
        $tarFile = $Archive -replace '\.xz$',''

        # 使用 7-Zip 解压 xz
        & $SevenZipPath x $Archive "-o$(Split-Path $tarFile -Parent)" -y | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Failed to extract xz archive" }

        # 解压 tar
        & $SevenZipPath x $tarFile "-o$Destination" -y | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Failed to extract tar archive" }

        # 清理临时 tar 文件
        Remove-Item $tarFile -Force -ErrorAction SilentlyContinue

        Write-Status "Extraction completed" "Success"
    }
    catch {
        Write-Status "Failed to extract: $_" "Error"
        throw
    }
}

# ==========================================================================
# 主逻辑
# ==========================================================================

function Main {
    Write-Status "MaaDeps Setup Script" "Info"
    Write-Status "Target triplet: $Triplet" "Info"
    Write-Status "Project root: $ProjectRoot" "Info"

    # 检查 PowerShell 版本
    if ($PSVersionTable.PSVersion.Major -lt 7) {
        Write-Status "This script requires PowerShell 7 or higher" "Error"
        exit 1
    }

    # 检查 7-Zip
    $sevenZip = Test-7Zip
    if (-not $sevenZip) {
        Write-Status "7-Zip not found. Please install 7-Zip and add it to PATH" "Error"
        Write-Status "Download from: https://www.7-zip.org/" "Info"
        exit 1
    }
    Write-Status "7-Zip found: $sevenZip" "Success"

    # 通过 GitHub API 获取最新版本号
    $latestVersion = Get-LatestMaaDepsVersion
    Write-Status "Using MaaDeps version: $latestVersion" "Info"

    # 动态构建 URL (使用获取到的版本号)
    # 注意：Fastly CDN (cdn.gh-proxy.org) 实测速度最快，设为首选
    $dynamicBaseUrl = "https://github.com/MaaXYZ/MaaDeps/releases/download/$latestVersion"
    $dynamicCdnUrl = "https://cdn.gh-proxy.org/https://github.com/MaaXYZ/MaaDeps/releases/download/$latestVersion"
    $dynamicFallbackCdns = @(
        # Fastly CDN - 实测最快，首选
        "https://cdn.gh-proxy.org/https://github.com/MaaXYZ/MaaDeps/releases/download/$latestVersion",
        # EdgeOne - 全球加速
        "https://edgeone.gh-proxy.org/https://github.com/MaaXYZ/MaaDeps/releases/download/$latestVersion",
        # Cloudflare 主站
        "https://gh-proxy.org/https://github.com/MaaXYZ/MaaDeps/releases/download/$latestVersion",
        # 香港节点
        "https://hk.gh-proxy.org/https://github.com/MaaXYZ/MaaDeps/releases/download/$latestVersion",
        # 其他备用镜像
        "https://mirror.ghproxy.com/https://github.com/MaaXYZ/MaaDeps/releases/download/$latestVersion",
        "https://github.moeyy.xyz/https://github.com/MaaXYZ/MaaDeps/releases/download/$latestVersion"
    )

    # 检测是否位于中国大陆，自动使用 CDN 加速
    $isChina = Test-ChinaMainland
    $downloadBaseUrl = if ($isChina) {
        Write-Status "Detected China mainland location, using CDN acceleration" "Info"
        Write-Status "Primary CDN: $dynamicCdnUrl" "Info"
        $dynamicCdnUrl
    } else {
        Write-Status "Using direct GitHub download" "Info"
        Write-Status "Source URL: $dynamicBaseUrl" "Info"
        $dynamicBaseUrl
    }

    # 创建目录结构
    @($MaaDepsDir, $TarballDir) | ForEach-Object {
        if (-not (Test-Path $_)) {
            New-Item -ItemType Directory -Path $_ -Force | Out-Null
            Write-Status "Created directory: $_" "Info"
        }
    }

    # 下载文件
    $runtimeUrl = "$downloadBaseUrl/$RuntimeFile"
    $develUrl = "$downloadBaseUrl/$DevelFile"
    $runtimeLocal = Join-Path $TarballDir $RuntimeFile
    $develLocal = Join-Path $TarballDir $DevelFile

    # 尝试下载，支持 CDN 回退
    $downloadSuccess = $false
    $urlsToTry = @($downloadBaseUrl)

    # 如果在中国大陆，添加备用 CDN
    if ($isChina) {
        foreach ($fallbackUrl in $dynamicFallbackCdns) {
            if ($fallbackUrl -ne $dynamicCdnUrl) {
                $urlsToTry += $fallbackUrl
            }
        }
        # 最后尝试 GitHub 直连
        $urlsToTry += $dynamicBaseUrl
    }

    $lastError = $null
    foreach ($baseUrl in $urlsToTry) {
        $runtimeUrl = "$baseUrl/$RuntimeFile"
        $develUrl = "$baseUrl/$DevelFile"

        if ($baseUrl -ne $downloadBaseUrl) {
            Write-Status "Trying alternative source: $baseUrl" "Warning"
        }

        try {
            Invoke-DownloadFile -Url $runtimeUrl -OutFile $runtimeLocal -Force:$Force
            Invoke-DownloadFile -Url $develUrl -OutFile $develLocal -Force:$Force
            $downloadSuccess = $true
            break
        }
        catch {
            $lastError = $_
            Write-Status "Download from current source failed, trying next..." "Warning"
            continue
        }
    }

    if (-not $downloadSuccess) {
        Write-Status "All download sources failed" "Error"
        Write-Status "Last error: $lastError" "Error"

        # 尝试备用方案：使用 Python 脚本
        Write-Status "Trying Python download script..." "Warning"

        $pythonScript = Join-Path $MaaUtilsDir "tools\maadeps_download.py"
        if (Test-Path $pythonScript) {
            Push-Location $MaaUtilsDir
            try {
                python $pythonScript
                $downloadSuccess = $true
            }
            finally {
                Pop-Location
            }
        }
        else {
            Write-Status "Python download script not found" "Error"
        }
    }

    if (-not $downloadSuccess) {
        Write-Status "Failed to download required files from all sources" "Error"
        exit 1
    }

    # 检查文件是否下载成功
    if (-not (Test-Path $runtimeLocal) -or -not (Test-Path $develLocal)) {
        Write-Status "Required files not found in $TarballDir" "Error"
        Write-Status "Please manually download:" "Info"
        Write-Status "  - $RuntimeFile" "Info"
        Write-Status "  - $DevelFile" "Info"
        Write-Status "from: https://github.com/MaaXYZ/MaaDeps/releases/latest" "Info"
        exit 1
    }

    # 提取文件
    $extractScript = Join-Path $MaaUtilsDir "tools\maadeps-extract.py"

    if (Test-Path $extractScript) {
        # 使用 MaaUtils 提供的 Python 脚本提取
        Write-Status "Using maadeps-extract.py for extraction..." "Info"
        Write-Status "Extract script: $extractScript" "Info"

        try {
            Push-Location $MaaUtilsDir
            try {
                python $extractScript
                if ($LASTEXITCODE -ne 0) {
                    throw "maadeps-extract.py exited with code $LASTEXITCODE"
                }
                Write-Status "Extraction completed using Python script" "Success"
            }
            finally {
                Pop-Location
            }
        }
        catch {
            Write-Status "Python extraction failed, falling back to 7-Zip: $_" "Warning"

            # 回退到 7-Zip 手动提取
            try {
                Expand-TarXz -Archive $runtimeLocal -Destination $MaaDepsDir -SevenZipPath $sevenZip
                Expand-TarXz -Archive $develLocal -Destination $MaaDepsDir -SevenZipPath $sevenZip
            }
            catch {
                Write-Status "7-Zip extraction also failed: $_" "Error"
                exit 1
            }
        }
    }
    else {
        # 使用 7-Zip 手动提取
        Write-Status "maadeps-extract.py not found, using 7-Zip..." "Warning"

        try {
            Expand-TarXz -Archive $runtimeLocal -Destination $MaaDepsDir -SevenZipPath $sevenZip
            Expand-TarXz -Archive $develLocal -Destination $MaaDepsDir -SevenZipPath $sevenZip
        }
        catch {
            Write-Status "Extraction failed: $_" "Error"
            exit 1
        }
    }

    Write-Host ""
    Write-Status "MaaDeps setup completed successfully!" "Success"
    Write-Status "Location: $MaaDepsDir" "Info"
    Write-Host ""
    Write-Status "You can now build the project with:" "Info"
    Write-Status "  cmake -B build -S ." "Info"
}

# 执行主函数
Main
