param(
    [string]$Version = "1.0.0",
    [string]$BuildDir = "build\p0_app_release",
    [string]$StagingRoot = "build\app_release_staging",
    [string]$OutputDir = "build\app_release_artifacts",
    [string]$Config = "Release",
    [string]$UserOption = "USER_TANGTANG_4090_Windows",
    [string]$Generator = "Visual Studio 16 2019",
    [string]$GeneratorPlatform = "x64",
    [string]$LicenseFile = "",
    [string]$DataRoot = "data",
    [string[]]$ConfigureArgs = @(),
    [switch]$IncludeData,
    [switch]$CleanStaging,
    [switch]$SkipConfigure,
    [switch]$SkipBuild,
    [switch]$SkipArchive,
    [switch]$VerifyLaunch,
    [switch]$AllowDirtyTree
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath {
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\$Path"))
}

function Invoke-NativeStep {
    param(
        [string]$Title,
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$WorkingDirectory = ""
    )

    Write-Host ""
    Write-Host "== $Title =="
    Write-Host "$FilePath $($Arguments -join ' ')"

    if ($WorkingDirectory) {
        Push-Location $WorkingDirectory
        try {
            & $FilePath @Arguments
        } finally {
            Pop-Location
        }
    } else {
        & $FilePath @Arguments
    }

    if ($LASTEXITCODE -ne 0) {
        throw "$Title failed with exit code $LASTEXITCODE."
    }
}

function Invoke-RobocopyStep {
    param(
        [string]$Title,
        [string]$Source,
        [string]$Destination,
        [string[]]$ExtraArguments = @()
    )

    Write-Host ""
    Write-Host "== $Title =="
    $arguments = @($Source, $Destination, "/E", "/NFL", "/NDL", "/NJH", "/NJS", "/NP")
    $arguments += $ExtraArguments
    & robocopy @arguments | Out-Host
    if ($LASTEXITCODE -gt 7) {
        throw "$Title failed with robocopy exit code $LASTEXITCODE."
    }
}

function Assert-StagingPathSafe {
    param([string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $repoBuildRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "build"))
    $leaf = [System.IO.Path]::GetFileName($fullPath)
    if (-not $fullPath.StartsWith($repoBuildRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Runtime staging must be under the repository build directory: $fullPath"
    }
    if (-not $leaf.StartsWith("RS2026-Runtime-", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Runtime staging leaf must start with RS2026-Runtime-: $fullPath"
    }
}

function Get-GitText {
    param([string[]]$Arguments)
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $safeRepoRoot = $repoRoot.Replace("\", "/")
    try {
        $output = & git -c "safe.directory=$safeRepoRoot" @Arguments 2>$null
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($exitCode -ne 0) {
        return ""
    }
    return ($output -join "`n").Trim()
}

Set-Location (Resolve-Path (Join-Path $PSScriptRoot ".."))
$repoRoot = (Get-Location).Path
$buildDirAbs = Resolve-RepoPath $BuildDir
$stagingRootAbs = Resolve-RepoPath $StagingRoot
$outputDirAbs = Resolve-RepoPath $OutputDir
$dataRootAbs = Resolve-RepoPath $DataRoot
$packageName = "RS2026-Runtime-$Version-win64"
$packageRoot = Join-Path $stagingRootAbs $packageName

if (-not $AllowDirtyTree) {
    $gitStatus = Get-GitText @("status", "--porcelain")
    if ($gitStatus) {
        throw "Working tree is not clean. Commit or stash changes, or pass -AllowDirtyTree for local verification."
    }
}

if (-not $SkipConfigure) {
    $configure = @(
        "-S", $repoRoot,
        "-B", $buildDirAbs,
        "-D$UserOption=ON",
        "-DBuildExample=OFF",
        "-DBuildTests=OFF",
        "-DBuildRegression=OFF",
        "-DRS2026_LICENSE_ENFORCEMENT_MODE=Enforce"
    )
    if ($Generator) {
        $configure += @("-G", $Generator)
    }
    if ($GeneratorPlatform) {
        $configure += @("-A", $GeneratorPlatform)
    }
    $configure += $ConfigureArgs
    Invoke-NativeStep "Configure application release build" "cmake" $configure
}

if (-not $SkipBuild) {
    Invoke-NativeStep "Build application release targets" "cmake" @(
        "--build", $buildDirAbs,
        "--config", $Config,
        "--target", "RobotQtViewer", "RenderCoreShaderResources"
    )
}

$binaryRoot = Join-Path $buildDirAbs "$Config\bin"
if (-not (Test-Path -LiteralPath $binaryRoot)) {
    throw "Application binary directory does not exist: $binaryRoot"
}

if (Test-Path -LiteralPath $packageRoot) {
    if (-not $CleanStaging) {
        throw "Runtime staging already exists: $packageRoot. Pass -CleanStaging to replace it."
    }
    Assert-StagingPathSafe $packageRoot
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
New-Item -ItemType Directory -Force -Path $outputDirAbs | Out-Null

Invoke-RobocopyStep `
    -Title "Copy application runtime binaries" `
    -Source $binaryRoot `
    -Destination $packageRoot `
    -ExtraArguments @(
        "/XD", (Join-Path $binaryRoot "data"),
        "/XF", "*.pdb", "*.ilk", "*.exp"
    )

Invoke-RobocopyStep `
    -Title "Copy application configuration" `
    -Source (Join-Path $repoRoot "config") `
    -Destination (Join-Path $packageRoot "config")

if ($IncludeData) {
    if (-not (Test-Path -LiteralPath $dataRootAbs)) {
        throw "Data root does not exist: $dataRootAbs"
    }
    Invoke-RobocopyStep `
        -Title "Copy application data" `
        -Source $dataRootAbs `
        -Destination (Join-Path $packageRoot "data")
}

$licenseName = ""
if ($LicenseFile) {
    $licenseAbs = Resolve-RepoPath $LicenseFile
    if (-not (Test-Path -LiteralPath $licenseAbs -PathType Leaf)) {
        throw "License file does not exist: $licenseAbs"
    }
    if ([System.IO.Path]::GetExtension($licenseAbs) -ine ".LIC") {
        throw "Only an explicitly selected .LIC file may be copied into the runtime package: $licenseAbs"
    }
    $licenseDir = Join-Path $packageRoot "license"
    New-Item -ItemType Directory -Force -Path $licenseDir | Out-Null
    Copy-Item -LiteralPath $licenseAbs -Destination $licenseDir -Force
    $licenseName = [System.IO.Path]::GetFileName($licenseAbs)
}

$viewerExe = Get-ChildItem -LiteralPath $packageRoot -Filter "RobotQtViewer*.exe" -File |
    Select-Object -First 1
if ($null -eq $viewerExe) {
    throw "RobotQtViewer executable is missing from runtime staging: $packageRoot"
}

$qwindows = Join-Path $packageRoot "platforms\qwindows.dll"
if (-not (Test-Path -LiteralPath $qwindows)) {
    throw "Qt platform plugin is missing from runtime staging: $qwindows"
}

$shaderResourceDll = Get-ChildItem -LiteralPath $packageRoot -Filter "RenderCoreShaderResources*.dll" -File |
    Select-Object -First 1
if ($null -eq $shaderResourceDll) {
    throw "RenderCoreShaderResources DLL is missing from runtime staging: $packageRoot"
}

$readme = @"
RS2026 Runtime Bundle $Version

Start: $($viewerExe.Name)

Requirements:
- Windows x64
- Microsoft Visual C++ runtime compatible with the configured Visual Studio toolchain
- A valid RS2026 license under the executable directory or its license subdirectory

Runtime roots:
- SMROBOT_APP_ROOT overrides the application root
- SMROBOT_CONFIG_ROOT overrides the configuration root
- SMROBOT_DATA_ROOT points directly to the external data directory

Project asset paths remain portable as data/..., and are resolved against the local
data directory first. For example, data/Spray420/model.urdf resolves under
<application-root>/data/Spray420/model.urdf by default.

This bundle does not include the full data/model library unless it was generated with -IncludeData.
"@
[System.IO.File]::WriteAllText(
    (Join-Path $packageRoot "README.txt"),
    ($readme -replace "`r?`n", "`r`n"),
    [System.Text.UTF8Encoding]::new($false))

$sourceCommit = Get-GitText @("rev-parse", "HEAD")
$sourceBranch = Get-GitText @("rev-parse", "--abbrev-ref", "HEAD")
$fileRecords = Get-ChildItem -LiteralPath $packageRoot -Recurse -File |
    Sort-Object FullName |
    ForEach-Object {
        [ordered]@{
            path = $_.FullName.Substring($packageRoot.Length).TrimStart("\").Replace("\", "/")
            size = $_.Length
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
        }
    }
$manifest = [ordered]@{
    schema = "RS2026RuntimeManifest.v1"
    version = $Version
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    sourceCommit = $sourceCommit
    sourceBranch = $sourceBranch
    config = $Config
    executable = $viewerExe.Name
    licenseFile = $licenseName
    dataMode = if ($IncludeData) { "embedded" } else { "external" }
    dataRootContract = "data-directory"
    files = $fileRecords
}
$manifestPath = Join-Path $packageRoot "runtime-manifest.json"
($manifest | ConvertTo-Json -Depth 8) | Set-Content -Encoding UTF8 -LiteralPath $manifestPath

if ($VerifyLaunch) {
    if (-not $licenseName) {
        throw "-VerifyLaunch requires -LicenseFile because release builds enforce authorization."
    }

    $previousAppRoot = $env:SMROBOT_APP_ROOT
    $previousConfigRoot = $env:SMROBOT_CONFIG_ROOT
    $previousDataRoot = $env:SMROBOT_DATA_ROOT
    try {
        $env:SMROBOT_APP_ROOT = $packageRoot
        $env:SMROBOT_CONFIG_ROOT = Join-Path $packageRoot "config"
        $env:SMROBOT_DATA_ROOT = if ($IncludeData) { Join-Path $packageRoot "data" } else { Join-Path $packageRoot "data-unavailable" }
        $process = Start-Process `
            -FilePath $viewerExe.FullName `
            -ArgumentList @("--smoke-exit-ms", "800") `
            -WorkingDirectory $packageRoot `
            -WindowStyle Hidden `
            -Wait `
            -PassThru
        if ($process.ExitCode -ne 0) {
            throw "Runtime launch smoke failed with exit code $($process.ExitCode)."
        }
    } finally {
        $env:SMROBOT_APP_ROOT = $previousAppRoot
        $env:SMROBOT_CONFIG_ROOT = $previousConfigRoot
        $env:SMROBOT_DATA_ROOT = $previousDataRoot
    }
}

if (-not $SkipArchive) {
    $archivePath = Join-Path $outputDirAbs "$packageName.zip"
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }
    Invoke-NativeStep `
        -Title "Create application runtime archive" `
        -FilePath "cmake" `
        -Arguments @("-E", "tar", "cf", $archivePath, "--format=zip", $packageName) `
        -WorkingDirectory $stagingRootAbs

    $archiveHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archivePath).Hash.ToLowerInvariant()
    [System.IO.File]::WriteAllText(
        "$archivePath.sha256",
        "$archiveHash  $([System.IO.Path]::GetFileName($archivePath))`r`n",
        [System.Text.UTF8Encoding]::new($false))
    Write-Host "Archive: $archivePath"
    Write-Host "SHA256:  $archiveHash"
}

Write-Host ""
Write-Host "Application runtime package generated:"
Write-Host "  $packageRoot"
