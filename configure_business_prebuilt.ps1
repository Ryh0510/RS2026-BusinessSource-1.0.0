param(
    [string]$BuildDir = "build\business_prebuilt",
    [string]$Config = "Release",
    [string]$Generator = "Visual Studio 16 2019",
    [string]$GeneratorPlatform = "x64",
    [string]$UserOption = "USER_LOCAL_14390_Windows",
    [string]$ThirdPartyRoot = "thirdparty",
    [string]$DataRoot = "data",
    [string]$LocalPrebuildRoot = "..\prebuild",
    [switch]$BuildExamples,
    [switch]$Build,
    [string]$BuildTarget = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$prebuiltRoot = Join-Path $repoRoot "PrebuiltPackages"
$thirdpartyRoot = if ([System.IO.Path]::IsPathRooted($ThirdPartyRoot)) { $ThirdPartyRoot } else { Join-Path $repoRoot $ThirdPartyRoot }
$dataRoot = if ([System.IO.Path]::IsPathRooted($DataRoot)) { $DataRoot } else { Join-Path $repoRoot $DataRoot }
$localPrebuildRoot = if ([System.IO.Path]::IsPathRooted($LocalPrebuildRoot)) { $LocalPrebuildRoot } else { Join-Path $repoRoot $LocalPrebuildRoot }
$buildRoot = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }
$buildExampleValue = if ($BuildExamples) { "ON" } else { "OFF" }

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
    "-DLOCAL_PREBUILD_ROOT=$localPrebuildRoot",
    "-DBuildExample=$buildExampleValue",
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
