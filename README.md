# TSI Launcher Bedrock Edition

An Android companion launcher for the official Minecraft Bedrock app. The application layer is implemented in **C++17** with Android NDK `NativeActivity`, CMake, EGL and OpenGL ES 2.0. There are no Kotlin or Java application source files.

## Current native implementation

- **Application entry point:** Android `NativeActivity` loading `libtsi_bedrock.so`.
- **UI:** C++17 and OpenGL ES 2.0, with a small built-in bitmap font and touch hit targets.
- **Package detection:** JNI calls from C++ into Android `PackageManager` to inspect `com.mojang.minecraftpe`.
- **Version information:** reads the installed version name, full 64-bit version code on Android 9+, and last update timestamp.
- **Launch:** C++ requests the official Minecraft launch intent through Android package manager.
- **Store link:** C++ opens the official Google Play listing.
- **Version history:** app-private version history capped at the latest 20 observed installations/updates.
- **ABIs:** `arm64-v8a` and `armeabi-v7a`.
- **Build:** Android Gradle Plugin, CMake 3.22.1 and NDK 29.0.14206865.

## Android Stable and Beta behaviour

Mojang's Android test channel is **Minecraft Beta**, enrolled through the official Google Play listing. Joining Beta replaces the normal Minecraft client; Stable and Beta are not two independently installed Android copies. The installed Android package remains `com.mojang.minecraftpe`.

Minecraft Preview is not the Android distribution channel. The launcher therefore does not pretend that a separate Android Preview package exists. The Android PackageManager exposes the installed package version, but it does not provide a reliable flag proving whether the user's Google Play account is enrolled in Beta.

The native UI provides a **Manage Beta in Play** action that opens the official Minecraft Play listing. The user completes the Join Beta or Leave Beta action in Google Play. TSI refreshes the installed version when the app regains focus; it does not silently change the user's Play testing enrollment. Back up worlds before changing channels because Beta replaces the normal client and test builds can be unstable.

Official guidance:
- https://help.minecraft.net/hc/en-us/articles/4423653831821
- https://help.minecraft.net/hc/en-us/articles/360040841471-How-Do-I-Join-and-Leave-the-Minecraft-Beta

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

This migration supplies a native C++ launcher shell, package detection, official app launch, Google Play hand-off and local version history, Google Play Beta hand-off, full-width version-code support and event-driven rendering to reduce idle battery use. It does not install Windows AppX files, fetch arbitrary Minecraft builds, bypass store licensing, or provide a second Android Minecraft install. Device-level launch and touch testing still needs to be performed on an Android phone after the CI APK builds.
