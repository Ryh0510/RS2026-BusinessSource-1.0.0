param(
    [string]$Version = "1.0.0",
    [string]$OutputRoot = "build\phase1_release",
    [string]$UserOption = "USER_TANGTANG_4090_Windows",
    [string]$Generator = "Visual Studio 16 2019",
    [string]$GeneratorPlatform = "x64",
    [string]$LicenseFile = "",
    [string]$ThirdPartyRoot = "thirdparty",
    [string]$DataRoot = "data",
    [string]$SdkPrefix = "build\phase1_sdk_install",
    [switch]$IncludeRuntimeData,
    [switch]$IncludeSdk,
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
$outputRootAbs = Resolve-RepoPath $OutputRoot
$thirdPartyRootAbs = Resolve-RepoPath $ThirdPartyRoot
$dataRootAbs = Resolve-RepoPath $DataRoot
$sdkPrefixAbs = Resolve-RepoPath $SdkPrefix
$licenseAbs = if ($LicenseFile) { Resolve-RepoPath $LicenseFile } else { "" }

if (-not $AllowDirtyTree) {
    $gitStatus = Get-GitText @("status", "--porcelain")
    if ($gitStatus) {
        throw "Formal phase-one release requires a clean working tree. Commit or stash changes, or pass -AllowDirtyTree for local verification."
    }
}

if (-not (Test-Path -LiteralPath $thirdPartyRootAbs -PathType Container)) {
    throw "Third-party root does not exist: $thirdPartyRootAbs"
}
if (-not (Test-Path -LiteralPath $dataRootAbs -PathType Container)) {
    throw "Data root does not exist: $dataRootAbs"
}
if ($licenseAbs -and -not (Test-Path -LiteralPath $licenseAbs -PathType Leaf)) {
    throw "License file does not exist: $licenseAbs"
}
if (-not $IncludeSdk -and -not (Test-Path -LiteralPath $sdkPrefixAbs -PathType Container)) {
    throw "The default release reuses an existing Common/Core/Platform PreBuild, but the SDK prefix does not exist: $sdkPrefixAbs. Pass -IncludeSdk to build it, or provide -SdkPrefix."
}

New-Item -ItemType Directory -Force -Path $outputRootAbs | Out-Null

$sdkBuildDir = Join-Path $repoRoot "build\phase1_sdk"
$sdkMatrixRoot = Join-Path $repoRoot "build\phase1_sdk_matrix"
$sdkArchiveName = "SMRobotCorePlatformPreBuild-$Version-vs2019-x64.zip"

if ($IncludeSdk) {
    $sdkArgs = @{
        BuildDir = $sdkBuildDir
        SdkPrefix = $sdkPrefixAbs
        MatrixBuildRoot = $sdkMatrixRoot
        Configs = @("Debug", "Release")
        Profile = "core_platform_standalone"
        ArtifactOutputDir = $outputRootAbs
        ArchiveName = $sdkArchiveName
        ConfigureArgs = @("-D$UserOption=ON", "-DBuildExample=OFF")
        Generator = $Generator
        GeneratorPlatform = $GeneratorPlatform
        AbiBaseline = (Join-Path $repoRoot "cmake\sdk_abi_baselines\SMRobotSDKExportSnapshot.v3.txt")
        CreateArchive = $true
        CleanInstall = $true
        CleanMatrix = $true
        AllowDirtyTree = [bool]$AllowDirtyTree
    }
    & (Join-Path $PSScriptRoot "build_sdk.ps1") @sdkArgs
} else {
    Write-Host "Reusing existing Common/Core/Platform PreBuild without rebuilding: $sdkPrefixAbs"
}

$businessArgs = @{
    PackageName = "RS2026-BusinessSource"
    BusinessVersion = $Version
    StagingRoot = "build\phase1_business_staging"
    OutputDir = $outputRootAbs
    SdkPrefix = $sdkPrefixAbs
    ThirdPartyRootHint = "thirdparty"
    DataRootHint = "data"
    LicenseFile = $licenseAbs
    Generator = $Generator
    GeneratorPlatform = $GeneratorPlatform
    UserOption = $UserOption
    VerifyConfigs = @("Debug", "Release")
    VerifyBuildRoot = "build\phase1_business_verify"
    VerifyBuildTarget = "RobotQtViewer"
    CleanStaging = $true
    IncludeThirdParty = $true
    IncludeSdkThirdParty = $true
    VerifyBuild = $true
    AllowDirtyTree = [bool]$AllowDirtyTree
}
& (Join-Path $PSScriptRoot "package_business_source.ps1") @businessArgs

$runtimeArgs = @{
    Version = $Version
    BuildDir = "build\phase1_app_release"
    StagingRoot = "build\phase1_app_staging"
    OutputDir = $outputRootAbs
    UserOption = $UserOption
    Generator = $Generator
    GeneratorPlatform = $GeneratorPlatform
    LicenseFile = $licenseAbs
    DataRoot = $dataRootAbs
    CleanStaging = $true
    IncludeData = [bool]$IncludeRuntimeData
    VerifyLaunch = [bool]$licenseAbs
    AllowDirtyTree = [bool]$AllowDirtyTree
}
& (Join-Path $PSScriptRoot "package_app_release.ps1") @runtimeArgs

$artifactPaths = @(
    (Join-Path $outputRootAbs "RS2026-BusinessSource-$Version.zip"),
    (Join-Path $outputRootAbs "RS2026-Runtime-$Version-win64.zip")
)
if ($IncludeSdk) {
    $artifactPaths += Join-Path $outputRootAbs $sdkArchiveName
}
$artifacts = $artifactPaths |
    ForEach-Object {
        if (-not (Test-Path -LiteralPath $_ -PathType Leaf)) {
            throw "Expected release artifact is missing: $_"
        }
        $artifact = Get-Item -LiteralPath $_
        [ordered]@{
            file = $artifact.Name
            size = $artifact.Length
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $artifact.FullName).Hash.ToLowerInvariant()
        }
    }

$releaseManifest = [ordered]@{
    schema = "RS2026Phase1ReleaseManifest.v1"
    version = $Version
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    sourceCommit = Get-GitText @("rev-parse", "HEAD")
    sourceBranch = Get-GitText @("rev-parse", "--abbrev-ref", "HEAD")
    configs = @("Debug", "Release")
    sdkProfile = "core_platform_standalone"
    sdkArtifactIncluded = [bool]$IncludeSdk
    sdkPrefix = $sdkPrefixAbs
    thirdpartyRoot = $thirdPartyRootAbs
    dataRoot = $dataRootAbs
    runtimeDataEmbedded = [bool]$IncludeRuntimeData
    artifacts = $artifacts
}
$releaseManifestPath = Join-Path $outputRootAbs "release-manifest.json"
($releaseManifest | ConvertTo-Json -Depth 8) | Set-Content -Encoding UTF8 -LiteralPath $releaseManifestPath

$hashLines = $artifacts | ForEach-Object { "$($_.sha256)  $($_.file)" }
[System.IO.File]::WriteAllText(
    (Join-Path $outputRootAbs "SHA256SUMS.txt"),
    (($hashLines -join "`r`n") + "`r`n"),
    [System.Text.UTF8Encoding]::new($false))

$verificationReport = @"
RS2026 phase-one release verification
version=$Version
source_commit=$($releaseManifest.sourceCommit)
sdk_archive=$(if ($IncludeSdk) { "passed" } else { "not-requested" })
sdk_consumer_matrix=$(if ($IncludeSdk) { "passed" } else { "not-run-reused-prefix" })
business_source_core_platform_absent=passed
business_source_robotqtviewer_debug_release=passed
runtime_bundle_created=passed
runtime_launch_smoke=$(if ($licenseAbs) { "passed" } else { "not-run-no-license" })
"@
[System.IO.File]::WriteAllText(
    (Join-Path $outputRootAbs "verification-report.txt"),
    ($verificationReport -replace "`r?`n", "`r`n"),
    [System.Text.UTF8Encoding]::new($false))

Write-Host ""
Write-Host "Phase-one release completed:"
Write-Host "  $outputRootAbs"
