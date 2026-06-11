#Requires -Version 5.1
<#
.SYNOPSIS
    Deploy WhYModem for Windows using windeployqt.

.DESCRIPTION
    Copies the Release executable, gathers Qt and compiler runtime dependencies,
    and optionally creates a ZIP archive for distribution.

.PARAMETER BuildDir
    Directory containing the Release WhYModem.exe.
    When omitted, the newest Release build under ./build is used.

.PARAMETER QtBin
    Path to the Qt bin directory that contains windeployqt.exe.
    When omitted, the script tries to infer it from the build directory name.

.PARAMETER DistDir
    Output directory for the deployed package.
    Defaults to ./dist/WhYModem-<version>-win64-<toolchain>.

.PARAMETER Version
    Package version used in the default output directory and ZIP file name.
    Defaults to the version from CMakeLists.txt.

.PARAMETER Zip
    Create a ZIP archive after deployment.

.PARAMETER NoZip
    Skip ZIP creation even if -Zip is not explicitly passed.
    By default, ZIP is created unless -NoZip is specified.

.EXAMPLE
    .\packaging\deploy-windows.ps1

.EXAMPLE
    .\packaging\deploy-windows.ps1 -BuildDir ".\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release" -Zip

.EXAMPLE
    .\packaging\deploy-windows.ps1 -QtBin "C:\Qt\6.11.1\mingw_64\bin" -NoZip
#>
[CmdletBinding()]
param(
    [string]$BuildDir = "",
    [string]$QtBin = "",
    [string]$DistDir = "",
    [string]$Version = "",
    [switch]$Zip,
    [switch]$NoZip
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Get-ProjectVersion {
    param([string]$Root)

    $cmakeLists = Join-Path $Root "CMakeLists.txt"
    $content = Get-Content $cmakeLists -Raw
    if ($content -match 'project\(WhYModem VERSION ([0-9.]+)') {
        return $Matches[1]
    }

    throw "Unable to read project version from CMakeLists.txt."
}

function Find-ReleaseExecutable {
    param([string]$Root)

    $buildRoot = Join-Path $Root "build"
    if (-not (Test-Path $buildRoot)) {
        throw "Build directory not found: $buildRoot. Build the Release target first."
    }

    $candidates = Get-ChildItem $buildRoot -Recurse -Filter "WhYModem.exe" -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.FullName -match 'Release' -and
            $_.FullName -notmatch 'Debug|\.qt|_autogen|CMakeFiles'
        } |
        Sort-Object LastWriteTime -Descending

    if (-not $candidates) {
        throw "No Release WhYModem.exe found under $buildRoot."
    }

    return $candidates[0]
}

function Get-ToolchainName {
    param([string]$PathValue)

    if ($PathValue -match 'MinGW|mingw') {
        return "mingw"
    }
    if ($PathValue -match 'MSVC|msvc') {
        return "msvc"
    }

    return "qt"
}

function Find-QtBinDirectory {
    param(
        [string]$HintPath,
        [string]$Toolchain
    )

    if ($env:QT_BIN -and (Test-Path (Join-Path $env:QT_BIN "windeployqt.exe"))) {
        return (Resolve-Path $env:QT_BIN).Path
    }

    $qtRoot = if ($env:QTDIR) { $env:QTDIR } else { "C:\Qt" }
    if (-not (Test-Path $qtRoot)) {
        throw "Qt installation not found. Set -QtBin or the QT_BIN environment variable."
    }

    $versionDirs = Get-ChildItem $qtRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^[0-9]+\.[0-9]+\.[0-9]+$' } |
        Sort-Object { [version]$_.Name } -Descending

    foreach ($versionDir in $versionDirs) {
        $kits = switch ($Toolchain) {
            "mingw" { @("mingw_64") }
            "msvc" { Get-ChildItem $versionDir.FullName -Directory | Where-Object { $_.Name -match '^msvc.*_64$' } | Select-Object -ExpandProperty Name }
            default { @("mingw_64") + @(Get-ChildItem $versionDir.FullName -Directory | Where-Object { $_.Name -match '^msvc.*_64$' } | Select-Object -ExpandProperty Name) }
        }

        foreach ($kit in $kits) {
            $candidate = Join-Path $versionDir.FullName (Join-Path $kit "bin")
            if (Test-Path (Join-Path $candidate "windeployqt.exe")) {
                return (Resolve-Path $candidate).Path
            }
        }
    }

    throw "windeployqt.exe not found. Set -QtBin or the QT_BIN environment variable."
}

function Stop-RunningWhYModem {
    param([string]$ExecutablePath)

    Get-Process -Name "WhYModem" -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.Path -eq $ExecutablePath) {
            Stop-Process -Id $_.Id -Force
        }
    }
}

if (-not $Version) {
    $Version = Get-ProjectVersion -Root $ProjectRoot
}

$releaseExe = if ($BuildDir) {
    $candidate = Join-Path (Resolve-Path $BuildDir).Path "WhYModem.exe"
    if (-not (Test-Path $candidate)) {
        throw "WhYModem.exe not found in $BuildDir."
    }
    Get-Item $candidate
} else {
    Find-ReleaseExecutable -Root $ProjectRoot
}

$resolvedBuildDir = $releaseExe.Directory.FullName
$toolchain = Get-ToolchainName -PathValue $resolvedBuildDir

if (-not $QtBin) {
    $QtBin = Find-QtBinDirectory -HintPath $resolvedBuildDir -Toolchain $toolchain
} else {
    $QtBin = (Resolve-Path $QtBin).Path
}

$windeployqt = Join-Path $QtBin "windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "windeployqt.exe not found: $windeployqt"
}

if (-not $DistDir) {
    $DistDir = Join-Path $ProjectRoot "dist\WhYModem-$Version-win64-$toolchain"
} else {
    $resolvedDistDir = Resolve-Path -Path $DistDir -ErrorAction SilentlyContinue
    if ($resolvedDistDir) {
        $DistDir = $resolvedDistDir.Path
    } else {
        $DistDir = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $DistDir))
    }
}

$shouldCreateZip = $Zip -or -not $NoZip

Write-Host "Project root : $ProjectRoot"
Write-Host "Version      : $Version"
Write-Host "Build dir    : $resolvedBuildDir"
Write-Host "Qt bin       : $QtBin"
Write-Host "Deploy dir   : $DistDir"
Write-Host "Toolchain    : $toolchain"
Write-Host ""

if (Test-Path $DistDir) {
    Remove-Item $DistDir -Recurse -Force
}
New-Item -ItemType Directory -Path $DistDir -Force | Out-Null

Copy-Item $releaseExe.FullName (Join-Path $DistDir "WhYModem.exe")

$windeployqtArgs = @(
    "--release",
    "--serialport",
    "--no-translations",
    (Join-Path $DistDir "WhYModem.exe")
)

Write-Host "Running windeployqt..."
& $windeployqt @windeployqtArgs
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE."
}

if ($toolchain -eq "mingw") {
    $mingwRoots = @(
        $env:MINGW_PREFIX,
        "C:\Qt\Tools\mingw1310_64",
        "C:\Qt\Tools\mingw1120_64"
    ) | Where-Object { $_ }

    $mingwBin = $null
    foreach ($root in $mingwRoots) {
        $candidate = Join-Path $root "bin"
        if (Test-Path (Join-Path $candidate "libstdc++-6.dll")) {
            $mingwBin = $candidate
            break
        }
    }

    if ($mingwBin) {
        foreach ($dll in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
            $source = Join-Path $mingwBin $dll
            if (Test-Path $source) {
                Copy-Item $source $DistDir -Force
            }
        }
    }
}

$deployedExe = Join-Path $DistDir "WhYModem.exe"
$proc = Start-Process -FilePath $deployedExe -PassThru
Start-Sleep -Seconds 2
if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force
    Write-Host "Smoke test   : passed"
} else {
    throw "Deployed executable exited early with code $($proc.ExitCode)."
}

$folderSizeMb = [math]::Round(
    ((Get-ChildItem $DistDir -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB),
    2
)
Write-Host "Deploy size  : $folderSizeMb MB"
Write-Host ""

if ($shouldCreateZip) {
    Stop-RunningWhYModem -ExecutablePath $deployedExe
    Start-Sleep -Milliseconds 500

    $zipPath = "$DistDir.zip"
    if (Test-Path $zipPath) {
        Remove-Item $zipPath -Force
    }

    Compress-Archive -Path (Join-Path $DistDir "*") -DestinationPath $zipPath -CompressionLevel Optimal
    $zipSizeMb = [math]::Round(((Get-Item $zipPath).Length / 1MB), 2)
    Write-Host "ZIP created  : $zipPath ($zipSizeMb MB)"
}

Write-Host ""
Write-Host "Deployment completed."
Write-Host "Distribute the folder or ZIP under dist/ to other Windows users."
