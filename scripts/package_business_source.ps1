param(
    [string]$PackageName = "SprayBusinessDev",
    [string]$BusinessVersion = "0.1.0",
    [string]$StagingRoot = "build\business_source_staging",
    [string]$OutputDir = "build\business_source_artifacts",
    [string]$SdkPrefix = "build\sdk_local_install",
    [string]$PrebuiltDirName = "PrebuiltPackages",
    [string]$ThirdPartyRootHint = "thirdparty",
    [string]$DataRootHint = "data",
    [string[]]$RemoveSourceDirs = @("SMRobotCore", "SMRobotPlatform"),
    [string[]]$ExcludeDirs = @(
        ".git",
        ".vs",
        ".idea",
        ".codex",
        ".agents",
        ".planning",
        "build",
        "data",
        "PrebuiltPackages",
        "PackagesInstallation",
        "cmake_bk",
        "cmake_upgrade",
        "archive",
        "archives",
        "generated_assets",
        "log",
        "license",
        "thirdparty"
    ),
    [string[]]$ExcludeFiles = @(".git", "*.user", "*.suo", "*.tmp", "*.log"),
    [string]$Generator = "Visual Studio 16 2019",
    [string]$GeneratorPlatform = "",
    [string]$Config = "Release",
    [string]$UserOption = "USER_TANGTANG_4090_Windows",
    [string[]]$VerifyConfigs = @("Debug", "Release"),
    [string]$VerifyBuildRoot = "build\business_source_verify",
    [string]$VerifyBuildTarget = "",
    [string]$LicenseFile = "",
    [switch]$IncludeThirdParty,
    [switch]$IncludeData,
    [switch]$IncludeSdkThirdParty,
    [switch]$CleanStaging,
    [switch]$SkipArchive,
    [switch]$VerifyConfigure,
    [switch]$VerifyBuild,
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

function Invoke-ProcessStep {
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
        [string[]]$ExcludeDirectories = @(),
        [string[]]$ExcludeFilePatterns = @()
    )

    Write-Host ""
    Write-Host "== $Title =="
    $args = @($Source, $Destination, "/E", "/NFL", "/NDL", "/NJH", "/NJS", "/NP")
    if ($ExcludeDirectories.Count -gt 0) {
        $args += "/XD"
        $args += $ExcludeDirectories
    }
    if ($ExcludeFilePatterns.Count -gt 0) {
        $args += "/XF"
        $args += $ExcludeFilePatterns
    }
    Write-Host "robocopy $($args -join ' ')"
    & robocopy @args | Out-Host
    $result = $LASTEXITCODE
    if ($result -gt 7) {
        throw "$Title failed with robocopy exit code $result."
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

function Get-FileHashText {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        return ""
    }
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
}

function Get-FirstExistingPath {
    param([string[]]$Paths)

    foreach ($candidate in $Paths) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    return $Paths[0]
}

function Assert-StagingPathSafe {
    param([string]$Path)
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $repoRootFull = [System.IO.Path]::GetFullPath($repoRoot)
    $repoBuildRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "build"))
    $leaf = [System.IO.Path]::GetFileName($fullPath)

    if ($fullPath -eq $repoRootFull) {
        throw "Refusing to clean repository root: $fullPath"
    }
    if (-not $leaf.StartsWith($PackageName, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean staging path because its leaf does not start with package name '$PackageName': $fullPath"
    }
    if (-not $fullPath.StartsWith($repoBuildRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Host "Staging path is outside repository build dir; explicit -CleanStaging was required: $fullPath"
    }
}

function Remove-StagingDirectory {
    param([string]$Path)

    Assert-StagingPathSafe $Path

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $parent = Split-Path -Parent $fullPath
    $leaf = [System.IO.Path]::GetFileName($fullPath)
    $deletePath = Join-Path $parent "$leaf.deleting-$([System.DateTime]::UtcNow.ToString("yyyyMMddHHmmss"))"

    Move-Item -LiteralPath $fullPath -Destination $deletePath

    for ($attempt = 1; $attempt -le 3; ++$attempt) {
        try {
            Get-ChildItem -LiteralPath $deletePath -Force -Recurse -ErrorAction SilentlyContinue |
                ForEach-Object { $_.Attributes = "Normal" }
            Remove-Item -LiteralPath $deletePath -Recurse -Force -ErrorAction Stop
            return
        } catch {
            if ($attempt -eq 3) {
                Write-Warning "Moved old staging package to '$deletePath', but could not delete it: $($_.Exception.Message)"
                return
            }
            Start-Sleep -Milliseconds (300 * $attempt)
        }
    }
}

function Remove-StagedBuildDirectory {
    param([string]$PackageRoot)

    $buildPath = Join-Path $PackageRoot "build"
    if (-not (Test-Path -LiteralPath $buildPath)) {
        return
    }

    $packageParent = Split-Path -Parent $PackageRoot
    $packageLeaf = [System.IO.Path]::GetFileName($PackageRoot)
    $deletePath = Join-Path $packageParent "$packageLeaf.build-deleting-$([System.DateTime]::UtcNow.ToString("yyyyMMddHHmmss"))"

    Write-Host "Moving staged build directory out of archive root: build"
    Move-Item -LiteralPath $buildPath -Destination $deletePath

    for ($attempt = 1; $attempt -le 3; ++$attempt) {
        try {
            Get-ChildItem -LiteralPath $deletePath -Force -Recurse -ErrorAction SilentlyContinue |
                ForEach-Object {
                    try {
                        $_.Attributes = "Normal"
                    } catch {
                    }
                }
            Remove-Item -LiteralPath $deletePath -Recurse -Force -ErrorAction Stop
            return
        } catch {
            if ($attempt -eq 3) {
                Write-Warning "Moved staged build directory to '$deletePath', but could not delete it: $($_.Exception.Message)"
                return
            }
            Start-Sleep -Milliseconds (300 * $attempt)
        }
    }
}

function Assert-SdkPrefixReady {
    param([string]$Path)

    $requiredFiles = @(
        "Common\lib\cmake\CommonConfig.cmake",
        "SMRobotCore\lib\cmake\SMRobotCoreConfig.cmake",
        "SMRobotPlatform\lib\cmake\SMRobotPlatformConfig.cmake",
        "SMRobotSDKManifest.json"
    )

    foreach ($required in $requiredFiles) {
        $candidate = Join-Path $Path $required
        if (-not (Test-Path -LiteralPath $candidate)) {
            throw "SDK prefix is incomplete; missing $required under $Path"
        }
    }
}

function Reset-VerifyBuildDirectory {
    param([string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $repoBuildRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "build"))
    $repoBuildPrefix = $repoBuildRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($repoBuildPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Verification build root must be under the repository build directory: $fullPath"
    }
    if (Test-Path -LiteralPath $fullPath) {
        Remove-Item -LiteralPath $fullPath -Recurse -Force
    }
}

function Assert-BusinessPackageSafe {
    param(
        [string]$PackageRoot,
        [string]$AllowedLicensePath = ""
    )

    foreach ($forbiddenDirectory in @("SMRobotCore", "SMRobotPlatform", ".git")) {
        $candidate = Join-Path $PackageRoot $forbiddenDirectory
        if (Test-Path -LiteralPath $candidate) {
            throw "Business source package contains forbidden directory: $candidate"
        }
    }

    $allowedLicenseFull = ""
    if ($AllowedLicensePath) {
        $allowedLicenseFull = [System.IO.Path]::GetFullPath($AllowedLicensePath)
    }

    $sensitiveExtensions = @(".authn", ".pfx", ".pem", ".key", ".keymgmt", ".lic")
    $sensitiveFiles = Get-ChildItem -LiteralPath $PackageRoot -Recurse -File |
        Where-Object { $sensitiveExtensions -contains $_.Extension.ToLowerInvariant() }

    $unexpected = @()
    foreach ($file in $sensitiveFiles) {
        if ($allowedLicenseFull -and
            [System.IO.Path]::GetFullPath($file.FullName).Equals(
                $allowedLicenseFull,
                [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }
        $unexpected += $file.FullName
    }

    if ($unexpected.Count -gt 0) {
        throw "Business source package contains unexpected authorization or key material:`n$($unexpected -join "`n")"
    }
}

function Write-BusinessCMakeDefaults {
    param(
        [string]$PackageRoot,
        [string]$PrebuiltRootRelative,
        [string]$ThirdPartyRootHint
    )

    $defaultsPath = Join-Path $PackageRoot "cmake\BusinessSourcePackageDefaults.cmake"
    $prebuiltRoot = $PrebuiltRootRelative.Replace("\", "/")
    $thirdpartyRoot = $ThirdPartyRootHint.Replace("\", "/")
    if (-not [System.IO.Path]::IsPathRooted($PrebuiltRootRelative)) {
        $prebuiltRoot = '${_smrobot_business_source_root}/' + $prebuiltRoot
    }
    if (-not [System.IO.Path]::IsPathRooted($ThirdPartyRootHint)) {
        $thirdpartyRoot = '${_smrobot_business_source_root}/' + $thirdpartyRoot
    }

    $defaults = @'
# Generated by package_business_source.ps1. This file exists only in Business Source packages.
get_filename_component(_smrobot_business_source_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT SMROBOT_PREBUILT_PACKAGE_ROOT)
    set(SMROBOT_PREBUILT_PACKAGE_ROOT "__PREBUILT_ROOT__" CACHE PATH
        "Business Source Common/Core/Platform prebuilt package root.")
endif()
if(NOT SMROBOT_PREBUILT_THIRDPARTY_ROOT)
    set(SMROBOT_PREBUILT_THIRDPARTY_ROOT "__THIRDPARTY_ROOT__" CACHE PATH
        "Business Source prebuilt third-party root.")
endif()
if(NOT SMROBOT_THIRDPARTY_ROOT)
    set(SMROBOT_THIRDPARTY_ROOT "__THIRDPARTY_ROOT__" CACHE PATH
        "Business Source third-party dependency root.")
endif()

# Common, Core and Platform are consumed from the exported SDK in a Business Source package.
set(UsingPrebuilt_Common ON CACHE BOOL "Use prebuilt Common package." FORCE)
set(UsingPrebuilt_SMRobotCore ON CACHE BOOL "Use prebuilt SMRobotCore package." FORCE)
set(UsingPrebuilt_SMRobotPlatform ON CACHE BOOL "Use prebuilt SMRobotPlatform package." FORCE)

message(STATUS "Business Source defaults enabled; Common/Core/Platform will be loaded from ${SMROBOT_PREBUILT_PACKAGE_ROOT}.")
unset(_smrobot_business_source_root)
'@
    $defaults = $defaults.Replace("__PREBUILT_ROOT__", $prebuiltRoot)
    $defaults = $defaults.Replace("__THIRDPARTY_ROOT__", $thirdpartyRoot)
    [System.IO.File]::WriteAllText(
        $defaultsPath,
        ($defaults -replace "`r?`n", "`r`n"),
        [System.Text.UTF8Encoding]::new($false)
    )
}

function Write-ConfigureHelper {
    param(
        [string]$PackageRoot,
        [string]$PrebuiltRootRelative,
        [string]$ThirdPartyRootHint,
        [string]$DataRootHint,
        [string]$DefaultUserOption
    )

    $helperPath = Join-Path $PackageRoot "configure_business_prebuilt.ps1"
    $helper = @'
param(
    [string]$BuildDir = "build\business_prebuilt",
    [string]$Config = "Release",
    [string]$Generator = "Visual Studio 16 2019",
    [string]$GeneratorPlatform = "",
    [string]$UserOption = "__USER_OPTION__",
    [string]$ThirdPartyRoot = "__THIRDPARTY_ROOT_HINT__",
    [string]$DataRoot = "__DATA_ROOT_HINT__",
    [switch]$Build,
    [string]$BuildTarget = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$prebuiltRoot = Join-Path $repoRoot "__PREBUILT_ROOT__"
$thirdpartyRoot = if ([System.IO.Path]::IsPathRooted($ThirdPartyRoot)) { $ThirdPartyRoot } else { Join-Path $repoRoot $ThirdPartyRoot }
$dataRoot = if ([System.IO.Path]::IsPathRooted($DataRoot)) { $DataRoot } else { Join-Path $repoRoot $DataRoot }
$buildRoot = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }

function Invoke-NativeStep {
    param(
        [string]$Title,
        [string]$FilePath,
        [string[]]$Arguments
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $FilePath @Arguments
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($LASTEXITCODE -ne 0) {
        throw "$Title failed with exit code $LASTEXITCODE."
    }
}

$configure = @(
    "-S", $repoRoot,
    "-B", $buildRoot,
    "-D$UserOption=ON",
    "-DSMROBOT_PREBUILT_PACKAGE_ROOT=$prebuiltRoot",
    "-DSMROBOT_PREBUILT_THIRDPARTY_ROOT=$thirdpartyRoot",
    "-DSMROBOT_THIRDPARTY_ROOT=$thirdpartyRoot",
    "-DSMROBOT_DATA_ROOT=$dataRoot",
    "-DUsingPrebuilt_Common=ON",
    "-DUsingPrebuilt_SMRobotCore=ON",
    "-DUsingPrebuilt_SMRobotPlatform=ON"
)
if ($Generator) {
    $configure += @("-G", $Generator)
}
if ($GeneratorPlatform) {
    $configure += @("-A", $GeneratorPlatform)
}

Invoke-NativeStep -Title "Configure" -FilePath "cmake" -Arguments $configure

if ($Build) {
    $buildArgs = @("--build", $buildRoot, "--config", $Config)
    if ($BuildTarget) {
        $buildArgs += @("--target", $BuildTarget)
    }
    Invoke-NativeStep -Title "Build" -FilePath "cmake" -Arguments $buildArgs
}
'@
    $helper = $helper.Replace("__PREBUILT_ROOT__", $PrebuiltRootRelative.Replace("\", "\\"))
    $helper = $helper.Replace("__THIRDPARTY_ROOT_HINT__", $ThirdPartyRootHint.Replace("\", "\\"))
    $helper = $helper.Replace("__DATA_ROOT_HINT__", $DataRootHint.Replace("\", "\\"))
    $helper = $helper.Replace("__USER_OPTION__", $DefaultUserOption)
    [System.IO.File]::WriteAllText($helperPath, ($helper -replace "`r?`n", "`r`n"), [System.Text.UTF8Encoding]::new($false))
}

Set-Location (Resolve-Path (Join-Path $PSScriptRoot ".."))
$repoRoot = (Get-Location).Path
$stagingRootAbs = Resolve-RepoPath $StagingRoot
$outputDirAbs = Resolve-RepoPath $OutputDir
$sdkPrefixAbs = Resolve-RepoPath $SdkPrefix
$packageId = "$PackageName-$BusinessVersion"
$packageRoot = Join-Path $stagingRootAbs $packageId
$prebuiltRoot = Join-Path $packageRoot $PrebuiltDirName

if (-not (Test-Path -LiteralPath $sdkPrefixAbs)) {
    throw "SDK prefix does not exist: $sdkPrefixAbs"
}
Assert-SdkPrefixReady $sdkPrefixAbs

if (-not $AllowDirtyTree) {
    $gitStatus = Get-GitText @("status", "--porcelain")
    if ($gitStatus) {
        throw "Working tree is not clean. Commit or stash changes, or pass -AllowDirtyTree for a local package build."
    }
}

if (Test-Path -LiteralPath $packageRoot) {
    if (-not $CleanStaging) {
        throw "Staging package already exists: $packageRoot. Pass -CleanStaging to replace it."
    }
    Remove-StagingDirectory $packageRoot
}

New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
New-Item -ItemType Directory -Force -Path $outputDirAbs | Out-Null

$effectiveExcludeDirs = @($ExcludeDirs)
if ($IncludeThirdParty) {
    $effectiveExcludeDirs = @($effectiveExcludeDirs | Where-Object { $_ -ne "thirdparty" })
}
if ($IncludeData) {
    $effectiveExcludeDirs = @($effectiveExcludeDirs | Where-Object { $_ -ne "data" })
}

Invoke-RobocopyStep `
    -Title "Copy source tree to staging" `
    -Source $repoRoot `
    -Destination $packageRoot `
    -ExcludeDirectories $effectiveExcludeDirs `
    -ExcludeFilePatterns $ExcludeFiles

foreach ($dir in $RemoveSourceDirs) {
    $target = Join-Path $packageRoot $dir
    if (Test-Path -LiteralPath $target) {
        Write-Host "Removing source directory from package: $dir"
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

$sdkExcludeDirs = @()
if (-not $IncludeSdkThirdParty) {
    $sdkExcludeDirs += "thirdparty"
}
Invoke-RobocopyStep `
    -Title "Copy Common/Core/Platform SDK to package prebuilt root" `
    -Source $sdkPrefixAbs `
    -Destination $prebuiltRoot `
    -ExcludeDirectories $sdkExcludeDirs

$stagedLicensePath = ""
if ($LicenseFile) {
    $licenseAbs = Resolve-RepoPath $LicenseFile
    if (-not (Test-Path -LiteralPath $licenseAbs -PathType Leaf)) {
        throw "License file does not exist: $licenseAbs"
    }
    if ([System.IO.Path]::GetExtension($licenseAbs) -ine ".LIC") {
        throw "Only an explicitly selected .LIC file may be copied into a business source package: $licenseAbs"
    }
    $stagedLicenseDir = Join-Path $packageRoot "license"
    New-Item -ItemType Directory -Force -Path $stagedLicenseDir | Out-Null
    Copy-Item -LiteralPath $licenseAbs -Destination $stagedLicenseDir -Force
    $stagedLicensePath = Join-Path $stagedLicenseDir ([System.IO.Path]::GetFileName($licenseAbs))
}

$sourceCommit = Get-GitText @("rev-parse", "HEAD")
$sourceBranch = Get-GitText @("rev-parse", "--abbrev-ref", "HEAD")
$dirtyText = Get-GitText @("status", "--porcelain")
$thirdpartyTree = Get-GitText @("ls-tree", "HEAD", "thirdparty")
$thirdpartyCommit = ""
if ($thirdpartyTree -match "commit\s+([0-9a-fA-F]+)") {
    $thirdpartyCommit = $Matches[1]
}
$dataTree = Get-GitText @("ls-tree", "HEAD", "data")
$dataCommit = ""
if ($dataTree -match "commit\s+([0-9a-fA-F]+)") {
    $dataCommit = $Matches[1]
}
if (-not $dataCommit -and (Test-Path -LiteralPath (Join-Path $repoRoot "data\.git"))) {
    $dataCommit = Get-GitText @("-C", (Join-Path $repoRoot "data"), "rev-parse", "HEAD")
}

$sdkManifestPath = Join-Path $sdkPrefixAbs "SMRobotSDKManifest.json"
$sdkSnapshotPath = Get-FirstExistingPath @(
    (Join-Path $sdkPrefixAbs "SMRobotSDKExportSnapshot.txt"),
    (Join-Path $sdkPrefixAbs "SMRobotSDKExportSnapshot")
)
$lock = [ordered]@{
    businessPackage = $PackageName
    businessVersion = $BusinessVersion
    sourceCommit = $sourceCommit
    sourceBranch = $sourceBranch
    sourceDirty = [bool]$dirtyText
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    removedSourceDirs = $RemoveSourceDirs
    sdk = [ordered]@{
        name = "SMRobotSDK"
        pathHint = $PrebuiltDirName
        sourcePrefix = $sdkPrefixAbs
        manifestSha256 = Get-FileHashText $sdkManifestPath
        exportSnapshotSha256 = Get-FileHashText $sdkSnapshotPath
        copiedThirdParty = [bool]$IncludeSdkThirdParty
    }
    thirdparty = [ordered]@{
        name = "SMRobotThirdParty"
        mode = if ($IncludeThirdParty) { "copied-source-submodule" } else { "external-source-submodule" }
        pathHint = $ThirdPartyRootHint
        gitCommit = $thirdpartyCommit
    }
    data = [ordered]@{
        name = "SMRobotData"
        mode = if ($IncludeData) { "copied-source-submodule" } else { "external-source-submodule" }
        pathHint = $DataRootHint
        gitCommit = $dataCommit
    }
    cmake = [ordered]@{
        prebuiltPackageRoot = $PrebuiltDirName
        prebuiltThirdPartyRoot = $ThirdPartyRootHint
        thirdPartyRoot = $ThirdPartyRootHint
        dataRoot = $DataRootHint
        usingPrebuilt = @("Common", "SMRobotCore", "SMRobotPlatform")
    }
}

$lockPath = Join-Path $packageRoot "sdk.lock.json"
($lock | ConvertTo-Json -Depth 8) | Set-Content -Encoding UTF8 -LiteralPath $lockPath

Write-BusinessCMakeDefaults `
    -PackageRoot $packageRoot `
    -PrebuiltRootRelative $PrebuiltDirName `
    -ThirdPartyRootHint $ThirdPartyRootHint

Write-ConfigureHelper `
    -PackageRoot $packageRoot `
    -PrebuiltRootRelative $PrebuiltDirName `
    -ThirdPartyRootHint $ThirdPartyRootHint `
    -DataRootHint $DataRootHint `
    -DefaultUserOption $UserOption

Assert-BusinessPackageSafe `
    -PackageRoot $packageRoot `
    -AllowedLicensePath $stagedLicensePath

if ($VerifyBuild) {
    $VerifyConfigure = $true
}

if ($VerifyConfigure) {
    $verifyBuildDir = Resolve-RepoPath $VerifyBuildRoot
    Reset-VerifyBuildDirectory $verifyBuildDir
    $verifyThirdPartyRoot = if ([System.IO.Path]::IsPathRooted($ThirdPartyRootHint)) { $ThirdPartyRootHint } else { Join-Path $packageRoot $ThirdPartyRootHint }
    $verifyDataRoot = if ([System.IO.Path]::IsPathRooted($DataRootHint)) { $DataRootHint } else { Join-Path $packageRoot $DataRootHint }
    $configureArgs = @(
        "-S", $packageRoot,
        "-B", $verifyBuildDir,
        "-D$UserOption=ON",
        "-DSMROBOT_PREBUILT_PACKAGE_ROOT=$prebuiltRoot",
        "-DSMROBOT_PREBUILT_THIRDPARTY_ROOT=$verifyThirdPartyRoot",
        "-DSMROBOT_THIRDPARTY_ROOT=$verifyThirdPartyRoot",
        "-DSMROBOT_DATA_ROOT=$verifyDataRoot",
        "-DUsingPrebuilt_Common=ON",
        "-DUsingPrebuilt_SMRobotCore=ON",
        "-DUsingPrebuilt_SMRobotPlatform=ON"
    )
    if ($Generator) {
        $configureArgs += @("-G", $Generator)
    }
    if ($GeneratorPlatform) {
        $configureArgs += @("-A", $GeneratorPlatform)
    }
    Invoke-ProcessStep "Verify staged package configure" "cmake" $configureArgs

    if ($VerifyBuild) {
        foreach ($verifyConfig in $VerifyConfigs) {
            $buildArgs = @("--build", $verifyBuildDir, "--config", $verifyConfig)
            if ($VerifyBuildTarget) {
                $buildArgs += @("--target", $VerifyBuildTarget)
            }
            Invoke-ProcessStep "Verify staged package build ($verifyConfig)" "cmake" $buildArgs
        }
    }
}

$stagedBuildDir = Join-Path $packageRoot "build"
if (Test-Path -LiteralPath $stagedBuildDir) {
    Remove-StagedBuildDirectory $packageRoot
}

if (-not $SkipArchive) {
    $archivePath = Join-Path $outputDirAbs "$packageId.zip"
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }
    Invoke-ProcessStep `
        -Title "Create business source package archive" `
        -FilePath "cmake" `
        -Arguments @("-E", "tar", "cf", $archivePath, "--format=zip", $packageId) `
        -WorkingDirectory $stagingRootAbs

    $archiveHash = Get-FileHashText $archivePath
    [System.IO.File]::WriteAllText(
        "$archivePath.sha256",
        "$archiveHash  $([System.IO.Path]::GetFileName($archivePath))`r`n",
        [System.Text.UTF8Encoding]::new($false))
    Write-Host "Archive: $archivePath"
    Write-Host "SHA256:  $archiveHash"
}

Write-Host ""
Write-Host "Business source package generated:"
Write-Host "  $packageRoot"
Write-Host "Lock file:"
Write-Host "  $lockPath"
