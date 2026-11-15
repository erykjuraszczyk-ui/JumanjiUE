#requires -Version 7.0
<#
.SYNOPSIS
    Compiles the JumanjiUE project with the locally installed Unreal Engine 5.8 toolchain.

.DESCRIPTION
    This script invokes UnrealBuildTool through Engine/Build/BatchFiles/Build.bat.
    It compiles project targets only; it does not cook assets or package an APK/installer.

.EXAMPLE
    .\Scripts\Build-UE5.ps1 -Platform Win64 -BuildEditor

.EXAMPLE
    .\Scripts\Build-UE5.ps1 -Platform Android
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Win64", "Android")]
    [string]$Platform,

    [ValidateSet("Development", "Shipping", "DebugGame")]
    [string]$Configuration = "Development",

    [string]$EngineRoot = $env:UE_ROOT,

    [switch]$BuildEditor
)

$ErrorActionPreference = "Stop"

$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ProjectPath = Join-Path $RepositoryRoot "JumanjiUE.uproject"
$ProjectName = "JumanjiUE"

if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf)) {
    throw "Project descriptor not found: $ProjectPath"
}

$ProjectDescriptor = Get-Content -LiteralPath $ProjectPath -Raw | ConvertFrom-Json
if ([string]$ProjectDescriptor.EngineAssociation -ne "5.8") {
    throw "This pipeline is configured for Unreal Engine 5.8. JumanjiUE.uproject currently declares EngineAssociation='$($ProjectDescriptor.EngineAssociation)'."
}

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $EngineRoot = Join-Path $env:ProgramFiles "Epic Games\UE_5.8"
}

$EngineRoot = [Environment]::ExpandEnvironmentVariables($EngineRoot.Trim().Trim('"'))
$BuildTool = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"

if (-not (Test-Path -LiteralPath $BuildTool -PathType Leaf)) {
    throw @"
UnrealBuildTool launcher was not found:
  $BuildTool

Install Unreal Engine 5.8 on this runner or set the GitHub Actions repository variable UE_ROOT
to the engine installation directory, for example:
  D:\Epic Games\UE_5.8
"@
}

if ($Platform -eq "Android") {
    if ([string]::IsNullOrWhiteSpace($env:JAVA_HOME)) {
        throw "JAVA_HOME is not set. Install/configure the Android toolchain supported by UE 5.8 (Turnkey recommends JDK 21)."
    }

    if (-not (Test-Path -LiteralPath (Join-Path $env:JAVA_HOME "bin\java.exe") -PathType Leaf)) {
        throw "JAVA_HOME does not point to a valid JDK: $env:JAVA_HOME"
    }

    $AndroidSdk = $env:ANDROID_HOME
    if ([string]::IsNullOrWhiteSpace($AndroidSdk)) {
        $AndroidSdk = $env:ANDROID_SDK_ROOT
    }

    if ([string]::IsNullOrWhiteSpace($AndroidSdk) -or -not (Test-Path -LiteralPath $AndroidSdk -PathType Container)) {
        throw "ANDROID_HOME or ANDROID_SDK_ROOT must point to the installed Android SDK. Configure the UE 5.8 Android toolchain with Turnkey."
    }

    if ($BuildEditor) {
        throw "The Unreal Editor target is not an Android target. Run Android builds without -BuildEditor."
    }
}

$LogDirectory = Join-Path $RepositoryRoot "Saved\BuildLogs"
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
$Timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$TranscriptPath = Join-Path $LogDirectory "Build-$Platform-$Configuration-$Timestamp.log"
$TranscriptStarted = $false

try {
    Start-Transcript -Path $TranscriptPath -Force | Out-Null
    $TranscriptStarted = $true

    Write-Host "============================================================"
    Write-Host " JUMANJI UE5.8 BUILD"
    Write-Host " Project       : $ProjectPath"
    Write-Host " Engine        : $EngineRoot"
    Write-Host " Platform      : $Platform"
    Write-Host " Configuration : $Configuration"
    Write-Host "============================================================"

    $Targets = [System.Collections.Generic.List[string]]::new()
    if ($Platform -eq "Win64" -and $BuildEditor) {
        $Targets.Add("JumanjiUEEditor")
    }
    $Targets.Add($ProjectName)

    foreach ($Target in $Targets) {
        Write-Host ""
        Write-Host "Building target: $Target ($Platform / $Configuration)"

        $BuildArguments = @(
            $Target,
            $Platform,
            $Configuration,
            "-Project=$ProjectPath",
            "-WaitMutex",
            "-NoHotReloadFromIDE"
        )

        & $BuildTool @BuildArguments
        if ($LASTEXITCODE -ne 0) {
            throw "UnrealBuildTool failed for target '$Target' with exit code $LASTEXITCODE."
        }
    }

    Write-Host ""
    Write-Host "Build completed successfully."
    Write-Host "Build log: $TranscriptPath"
}
finally {
    if ($TranscriptStarted) {
        Stop-Transcript | Out-Null
    }
}
