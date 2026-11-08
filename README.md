# TSI Launcher Bedrock Edition

An Android companion launcher for the official Minecraft Bedrock app. The application layer is implemented in **C++17** with Android NDK `NativeActivity`, CMake, EGL and OpenGL ES 2.0. There are no Kotlin or Java application source files.

## Current native implementation

- **Application entry point:** Android `NativeActivity` loading `libtsi_bedrock.so`.
- **UI:** C++17 and OpenGL ES 2.0, with a small built-in bitmap font and touch hit targets.
- **Package detection:** JNI calls from C++ into Android `PackageManager` to inspect `com.mojang.minecraftpe`.
- **Version information:** reads the installed version name, version code and last update timestamp.
- **Launch:** C++ requests the official Minecraft launch intent through Android package manager.
- **Store link:** C++ opens the official Google Play listing.
- **Version history:** app-private text history stored in the launcher's internal data directory.
- **ABIs:** `arm64-v8a` and `armeabi-v7a`.
- **Build:** Android Gradle Plugin, CMake 3.22.1 and NDK 29.0.14206865.

## Android Stable and Beta limitations

On Android, the Google Play Beta track updates the same `com.mojang.minecraftpe` package used by the regular app. The installed package does not provide a trustworthy field that tells a third-party launcher which Play testing track the account joined. TSI therefore reports the installed version and launches the official package; it does not invent a Stable/Beta label or promise side-by-side installs.

The Android Preview distribution is not a separate package channel that this launcher can detect and switch to. Channel enrolment and leaving Beta remain managed by Google Play.

## Windows AppX / MSIX is a different platform

Microsoft Store AppX/MSIX manifests, package-family URLs, PowerShell and `Add-AppxPackage` are Windows deployment mechanisms. They cannot install or launch Windows packages on Android. They are not part of this Android APK. A separate Windows launcher target would require its own C++/WinRT implementation and Microsoft Store account/entitlement flow.

## Scope and safety

Minecraft Bedrock is proprietary. TSI does not host, patch, downgrade, unpack or redistribute Minecraft game packages or data. Android's package manager and the official signed application remain responsible for installation and updates.

The version history is local to this TSI installation. It records versions observed by the launcher after an install or update; it cannot recreate versions that were never observed and is not an authoritative Mojang release archive.

## Build

Requirements:

- JDK 17
- Android SDK Platform 36 and Build Tools 36.0.0
- Android NDK 29.0.14206865
- CMake 3.22.1
- Gradle 8.13

Build from the repository root:

    gradle :app:assembleDebug --no-daemon

APK output:

    app/build/outputs/apk/debug/app-debug.apk

GitHub Actions builds and verifies the native library for both configured ABIs on pushes to `main` and manual workflow runs. The uploaded artifact is named `TSI-Launcher-Bedrock-native-debug`.

## Current development limits

This migration supplies a native C++ launcher shell, package detection, official app launch, Google Play hand-off and local version observation. It does not install Windows AppX files, fetch arbitrary Minecraft builds, bypass store licensing, or provide a second Android Minecraft install. Device-level launch and touch testing still needs to be performed on an Android phone after the CI APK builds.
