# UE 5.8 build pipeline (GitHub Actions)

## What this adds

This repository now has a GitHub Actions workflow that invokes the Unreal Build Tool installed on a Windows self-hosted runner.

It compiles the **JumanjiUE project**. It does not copy the Unreal Engine source code into this repository, install the engine on GitHub-hosted machines, cook game content, or package an APK/installer. Unreal Engine remains a separately installed Epic Games product and must be used under its applicable license.

The project descriptor currently declares `EngineAssociation: "5.8"`; this pipeline intentionally targets **Unreal Engine 5.8**.

## Build jobs

| Job | When it runs | Output |
|---|---|---|
| Windows + Editor | Push to `main` when project/source/config files change; also manual run | Development Editor and Win64 game binaries |
| Android | Manual run only, when `build_android` is enabled | Android Development target binaries |

The workflow deliberately has no `pull_request` trigger. This is a public repository and the build uses a self-hosted machine; untrusted pull-request code must not be allowed to execute on that machine.

## 1. Prepare the Windows build machine

Use a Windows 11 x64 computer that you control. Install:

- Unreal Engine **5.8** using your authorized Epic Games account;
- Visual Studio 2022 with Desktop development with C++, MSVC toolset and Windows SDK components supported by UE 5.8;
- Git for Windows and Git LFS;
- enough free disk space for the engine, working copy, Intermediate files and build outputs.

The runner computer must remain powered on and connected to the internet while a build is running.

## 2. Register a GitHub Actions self-hosted runner

In this repository, open **Settings → Actions → Runners → New self-hosted runner**, select **Windows** and **x64**, and follow GitHub's displayed setup commands.

When configuring the runner, add this custom label:

```text
ue5-8
```

Keep the default `self-hosted`, `windows`, and `x64` labels. Run the runner as a service under a Windows account that can access the Unreal Engine installation and the project workspace.

Do not publish the runner registration token, credentials, or local build-machine details. Do not add a `pull_request` workflow that executes repository code on this runner.

Official GitHub guide: https://docs.github.com/en/actions/how-tos/manage-runners/self-hosted-runners

## 3. Configure the engine path

The build script first reads the GitHub Actions repository variable `UE_ROOT`. When that variable is not set, it uses this default path:

```text
C:\Program Files\Epic Games\UE_5.8
```

To use another installation path, open **Settings → Secrets and variables → Actions → Variables → New repository variable** and create:

- Name: `UE_ROOT`
- Value: the engine root directory, for example `D:\Epic Games\UE_5.8`

This is a path, not a password, so it belongs in **Variables**, not Secrets.

The script expects this file to exist:

```text
<UE_ROOT>\Engine\Build\BatchFiles\Build.bat
```

## 4. Configure Android (optional)

Windows compilation does not require the Android toolchain. To run the Android job, configure Android support for the same UE 5.8 installation.

Epic's UE 5.8 documentation recommends using **Platforms → SDK Management → Android → Install SDK** (Turnkey) from Unreal Editor. The UE 5.8 Android Quick Start lists the corresponding toolchain versions, including SDK 35, NDK r27c, Build-tools 35.0.1 and OpenJDK 21.0.3. After installation, configure the project under **Project Settings → Platforms → Android**, accept the SDK license and set the Android package name.

The runner environment must expose:

- `JAVA_HOME` pointing to the JDK;
- `ANDROID_HOME` or `ANDROID_SDK_ROOT` pointing to the Android SDK.

Verify the Android SDK from the Unreal Editor before starting the CI job. Do not place signing keys or passwords in the repository.

Epic documentation:
- https://dev.epicgames.com/documentation/unreal-engine/android-quick-start
- https://dev.epicgames.com/documentation/unreal-engine/setting-up-an-unreal-engine-project-for-mobile-platforms

## 5. Run the pipeline

### Automatic Windows build

Push a change to `main` in one of the watched paths:

- `Source/**`
- `Config/**`
- `JumanjiUE.uproject`
- `Scripts/**`
- `.github/workflows/ue5-build.yml`

The Windows job compiles the Editor target and the Win64 game target.

### Manual Android build

1. Open the repository's **Actions** tab.
2. Select **Unreal Engine 5.8 - Compile**.
3. Choose **Run workflow**.
4. Enable `build_android`.
5. Start the workflow.

The Android job compiles the game target only. It does **not** produce an installable APK; APK/AAB packaging is a separate BuildCookRun stage that should be added after the project has a configured map and Android packaging settings.

## 6. Run locally

From the repository root in PowerShell 7:

```powershell
# Compile the editor target and Windows game target
.\Scripts\Build-UE5.ps1 -Platform Win64 -BuildEditor

# Compile the Android game target (after Android SDK setup)
.\Scripts\Build-UE5.ps1 -Platform Android
```

To override the engine path for one terminal session:

```powershell
$env:UE_ROOT = "D:\Epic Games\UE_5.8"
.\Scripts\Build-UE5.ps1 -Platform Win64 -BuildEditor
```

Build logs are written to `Saved/BuildLogs/`. GitHub Actions uploads build output as workflow artifacts for 14 days.

## 7. Current limitations

- This is a project compilation pipeline, not a hosted Unreal Engine compiler service.
- A self-hosted runner and a licensed local UE 5.8 installation are required.
- Android builds require the UE 5.8-compatible Android toolchain.
- The Android job is opt-in and does not package an APK/AAB.
- A successful compile does not prove that the full game is playable; maps, assets and runtime testing remain separate acceptance steps.
- The workflow has not yet been executed: it will become runnable after the self-hosted runner is registered and configured.
