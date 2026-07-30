param(
    [string]$BuildDir = "build\sdk_local",
    [string]$SdkPrefix = "build\sdk_local_install",
    [string]$MatrixBuildRoot = "build\sdk_local_matrix",
    [string[]]$Configs = @("Debug", "Release"),
    [string]$Profile = "core_platform_standalone",
    [string]$ArtifactOutputDir = "build\sdk_artifacts",
    [string]$ArchiveName = "",
    [string[]]$Targets = @(
        "CustomLog",
        "GLRuntime",
        "Utility",
        "LicenseVerification",
        "RobotCoreAuthorization",
        "RobotCore",
        "Kinematics",
        "RobotTrajectoryCore",
        "RobotIO",
        "RobotRuntime",
        "RobotInstance",
        "Collision",
        "RobotSDK",
        "AssetCore",
        "CameraCore",
        "RobotPlatformAuthorization",
        "ProjectSimulationSDK",
        "SensorCore",
        "SimulationProject",
        "SimulationRuntime",
        "SensorSimulation",
        "RenderCore",
        "RenderCoreShaderResources",
        "SceneCore",
        "RobotRenderBridge",
        "VisualizationSDK"
    ),
    [string[]]$ConfigureArgs = @(
        "-DUSER_TANGTANG_p15v3_Windows=ON",
        "-DBuildExample=ON"
    ),
    [string]$Generator = "",
    [string]$GeneratorPlatform = "",
    [string]$AbiBaseline = "",
    [switch]$SkipConfigure,
    [switch]$SkipBuild,
    [switch]$SkipMatrix,
    [switch]$SkipAbi,
    [switch]$CreateArchive,
    [switch]$CleanInstall,
    [switch]$CleanMatrix,
    [switch]$AllowDirtyTree
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath {
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $PSScriptRoot "..\$Path"
}

function Invoke-CommandStep {
    param(
        [string]$Title,
        [string]$FilePath,
        [string[]]$Arguments
    )

    Write-Host ""
    Write-Host "== $Title =="
    Write-Host "$FilePath $($Arguments -join ' ')"

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Title failed with exit code $LASTEXITCODE."
    }
}

function Assert-BuildDirectoryWritable {
    param([string]$Path)

    $testDir = Join-Path $Path "CMakeFiles\.smrobot_sdk_write_test_$PID"
    try {
        New-Item -ItemType Directory -Force -Path $testDir | Out-Null
        Remove-Item -LiteralPath $testDir -Force
    } catch {
        throw @"
Build directory is not writable for CMake:
  $Path

CMake must be able to create directories under:
  $Path\CMakeFiles

Try one of these:
  1. Run this script from a normal developer PowerShell, not a restricted shell.
  2. Pass a different writable build directory, for example:
     -BuildDir D:\program\build\rs2026-sdk-local -SdkPrefix D:\program\build\rs2026-sdk-install -MatrixBuildRoot D:\program\build\rs2026-sdk-matrix
  3. Delete or unlock the build directory if another process owns it.

Original error:
  $($_.Exception.Message)
"@
    }
}

function Assert-GeneratedPathSafe {
    param(
        [string]$Path,
        [string]$Purpose
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $repoBuildRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "build"))
    $leaf = [System.IO.Path]::GetFileName($fullPath).ToLowerInvariant()
    $lowerPath = $fullPath.ToLowerInvariant()

    $isUnderRepoBuild = $lowerPath.StartsWith($repoBuildRoot.ToLowerInvariant())
    $looksLikeBuildOutput = $lowerPath.Contains("\build\") -and $leaf.Contains("sdk")

    if (-not ($isUnderRepoBuild -or $looksLikeBuildOutput)) {
        throw @"
Refusing to clean $Purpose because the path does not look like a generated SDK build directory:
  $fullPath

Use a path under the repository build directory or a path containing \build\ with an sdk-like leaf name.
"@
    }

    return $fullPath
}

function Remove-GeneratedDirectory {
    param(
        [string]$Path,
        [string]$Purpose
    )

    $safePath = Assert-GeneratedPathSafe $Path $Purpose
    if (Test-Path -LiteralPath $safePath) {
        Write-Host "Cleaning $Purpose`: $safePath"
        Remove-Item -LiteralPath $safePath -Recurse -Force
    }
}

Set-Location (Resolve-Path (Join-Path $PSScriptRoot ".."))

$repoRoot = (Get-Location).Path
$buildDirAbs = Resolve-RepoPath $BuildDir
$sdkPrefixAbs = Resolve-RepoPath $SdkPrefix
$matrixBuildRootAbs = Resolve-RepoPath $MatrixBuildRoot
$artifactOutputDirAbs = Resolve-RepoPath $ArtifactOutputDir
$configList = $Configs -join ";"
$baselineAbs = ""

if ($AbiBaseline) {
    $baselineAbs = Resolve-RepoPath $AbiBaseline
}

if (-not $AllowDirtyTree) {
    $safeRepoRoot = $repoRoot.Replace("\", "/")
    $gitStatus = & git -c "safe.directory=$safeRepoRoot" status --porcelain
    if ($LASTEXITCODE -ne 0) {
        throw "git status failed."
    }
    if ($gitStatus) {
        throw "Working tree is not clean. Commit or stash changes, or pass -AllowDirtyTree for a local SDK build."
    }
}

if (-not $SkipConfigure) {
    Assert-BuildDirectoryWritable $buildDirAbs

    $configureCommand = @("-S", $repoRoot, "-B", $buildDirAbs)
    if ($Generator) {
        $configureCommand += @("-G", $Generator)
    }
    if ($GeneratorPlatform) {
        $configureCommand += @("-A", $GeneratorPlatform)
    }
    $configureCommand += $ConfigureArgs

    Invoke-CommandStep "Configure SDK build tree" "cmake" $configureCommand
}

if ($CleanInstall) {
    Remove-GeneratedDirectory $sdkPrefixAbs "SDK install prefix"
}

if ($CleanMatrix) {
    Remove-GeneratedDirectory $matrixBuildRootAbs "SDK consumer matrix build root"
}

if (-not $SkipBuild) {
    foreach ($config in $Configs) {
        $buildCommand = @(
            "--build", $buildDirAbs,
            "--config", $config
        )
        if ($Targets.Count -gt 0) {
            $buildCommand += "--target"
            $buildCommand += $Targets
        }

        Invoke-CommandStep "Build SDK targets ($config)" "cmake" $buildCommand
    }
}

$installCommand = @(
    "-DSMROBOT_SDK_PROFILE=$Profile",
    "-DSMROBOT_SDK_PROFILE_BUILD_DIR=$buildDirAbs",
    "-DSMROBOT_SDK_PROFILE_PREFIX=$sdkPrefixAbs",
    "-DSMROBOT_SDK_PROFILE_CONFIGS=$configList",
    "-P", (Resolve-RepoPath "cmake\InstallSmRobotSdkProfile.cmake")
)

Invoke-CommandStep "Install SDK profile" "cmake" $installCommand

if (-not $SkipAbi) {
    $abiCommand = @(
        "-DSMROBOT_SDK_ABI_SDK_PREFIX=$sdkPrefixAbs",
        "-DSMROBOT_SDK_ABI_OUTPUT=$sdkPrefixAbs\SMRobotSDKExportSnapshot.txt"
    )

    if ($baselineAbs -and (Test-Path -LiteralPath $baselineAbs)) {
        $abiCommand += "-DSMROBOT_SDK_ABI_BASELINE=$baselineAbs"
        $abiCommand += "-DSMROBOT_SDK_ABI_REQUIRE_BASELINE=ON"
    } else {
        $abiCommand += "-DSMROBOT_SDK_ABI_REQUIRE_BASELINE=OFF"
    }

    $abiCommand += @(
        "-P", (Resolve-RepoPath "cmake\CheckSmRobotSdkExportSnapshot.cmake")
    )

    Invoke-CommandStep "Check SDK ABI export snapshot" "cmake" $abiCommand
}

if (-not $SkipMatrix) {
    $matrixCommand = @(
        "-DSMROBOT_SDK_MATRIX_SDK_PREFIX=$sdkPrefixAbs",
        "-DSMROBOT_SDK_MATRIX_BUILD_ROOT=$matrixBuildRootAbs",
        "-DSMROBOT_SDK_MATRIX_CONFIGS=$configList"
    )

    if ($Generator) {
        $matrixCommand += "-DSMROBOT_SDK_MATRIX_GENERATOR=$Generator"
    }
    if ($GeneratorPlatform) {
        $matrixCommand += "-DSMROBOT_SDK_MATRIX_GENERATOR_PLATFORM=$GeneratorPlatform"
    }
    if ($baselineAbs -and (Test-Path -LiteralPath $baselineAbs)) {
        $matrixCommand += "-DSMROBOT_SDK_MATRIX_ABI_BASELINE=$baselineAbs"
        $matrixCommand += "-DSMROBOT_SDK_MATRIX_REQUIRE_ABI_BASELINE=ON"
    } else {
        $matrixCommand += "-DSMROBOT_SDK_MATRIX_REQUIRE_ABI_BASELINE=OFF"
    }

    $matrixCommand += @(
        "-P", (Resolve-RepoPath "cmake\RunSmRobotSdkConsumerMatrix.cmake")
    )

    Invoke-CommandStep "Run SDK consumer matrix" "cmake" $matrixCommand
}

if ($CreateArchive) {
    $releaseCommand = @(
        "-DSMROBOT_SDK_RELEASE_SDK_PREFIX=$sdkPrefixAbs",
        "-DSMROBOT_SDK_RELEASE_OUTPUT_DIR=$artifactOutputDirAbs"
    )
    if ($ArchiveName) {
        $releaseCommand += "-DSMROBOT_SDK_RELEASE_ARCHIVE_NAME=$ArchiveName"
    }
    if ($baselineAbs) {
        $releaseCommand += "-DSMROBOT_SDK_RELEASE_EXPORT_BASELINE=$baselineAbs"
    }
    $releaseCommand += @(
        "-DSMROBOT_SDK_RELEASE_REQUIRE_EXPORT_BASELINE=ON",
        "-P", (Resolve-RepoPath "cmake\CreateSmRobotSdkReleaseArtifact.cmake")
    )
    Invoke-CommandStep "Create SDK release artifact" "cmake" $releaseCommand
}

Write-Host ""
Write-Host "SDK build completed."
Write-Host "SDK prefix: $sdkPrefixAbs"
Write-Host "Manifest:   $sdkPrefixAbs\SMRobotSDKManifest.json"
