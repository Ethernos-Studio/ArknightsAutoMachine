$script:vcpkgPath = "C:\Dev\vcpkg"
function Install-WingetDepsSoftware {
    $softwareList = @(
        "Microsoft.VisualStudio.2022.BuildTools",
        "Kitware.CMake",
        "Python.Python.3.11",
        "Git.Git"
    )
    foreach ($software in $softwareList) {
        Write-Output "Checking if $software is installed..."
        $installed = winget list --id "$software" -e | Select-String "$software"
        if ($installed) {
            Write-Output "$software is already installed."
        } else {
            Write-Output "$software is not installed. Installing..."
            winget install --id "$software" -e --silent --accept-package-agreements
        }
    }
}
function Get-VcpkgRepository {
    $vcpkgPath =$script:vcpkgPath
    if (Test-Path $vcpkgPath) {
        Write-Output "vcpkg is already cloned."
    } else {
        Write-Output "Cloning vcpkg repository..."
        git clone https://github.com/Microsoft/vcpkg.git $vcpkgPath
    }
}
function Install-VCPKG {
    $vcpkgPath = $script:vcpkgPath
    if (Test-Path "$vcpkgPath\vcpkg.exe") {
        Write-Output "vcpkg is already installed."
    } else {
        Write-Output "Installing vcpkg..."
        & "$vcpkgPath\bootstrap-vcpkg.bat"
    }
}
function Install-VCPKG-Dependency {
    $vcpkgExe = "$script:vcpkgPath\vcpkg.exe"
    $jsonPath = Join-Path $PSScriptRoot "..\..\vcpkg.json"  

    if (-not (Test-Path $jsonPath)) {
        Write-Error "vcpkg.json not found at $jsonPath"
        return
    }


    $json = Get-Content $jsonPath -Raw | ConvertFrom-Json
    $depsList = $json.dependencies

    if ($depsList.Count -eq 0) {
        Write-Output "No dependencies listed in vcpkg.json"
        return
    }

    foreach ($dep in $depsList) {
        Write-Output "Installing $dep via vcpkg..."
        & $vcpkgExe install $dep
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Failed to install $dep"
        }
    }
}
function Get-Admin{
    $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")
    if(-NOT $isAdmin) {
        Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$($MyInvocation.MyCommand.Path)`""
        exit
    }
}
function  Install-Poetry {
    $MaxWhileAttempts = 5
    while ($MaxWhileAttempts > 0) {
        Write-Output "Checking if Poetry is installed..."
        try {
            poetry --version | Out-Null
            Write-Output "Poetry is already installed."
            break
        }
        catch {
            $MaxWhileAttempts--
            Write-Output "Poetry is not installed. Installing..."
            python.exe -m pip install  poetry
        }
    }
}
try {
    winget --version | Out-Null
    Write-Output "Winget is available."
}
catch {
    Write-Output "Winget is not installed."
    Write-Output "Please install Winget from the Microsoft Store and run this script again."
    Pause
    exit 1
}
Install-WingetDepsSoftware
Get-VcpkgRepository
Install-VCPKG
& "$script:vcpkgPath\vcpkg.exe" integrate install
Install-VCPKG-Dependency
Install-Poetry