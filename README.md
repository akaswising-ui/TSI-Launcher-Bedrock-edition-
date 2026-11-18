# TSI Launcher Bedrock Edition

An Android companion launcher for the official Minecraft Bedrock app. The application layer is implemented in **C++17** with Android NDK `NativeActivity`, CMake, EGL and OpenGL ES 2.0. There are no Kotlin or Java application source files.

## Native mobile launcher

- **Application entry point:** Android `NativeActivity` loading `libtsi_bedrock.so`.
- **Rendering:** C++17 and OpenGL ES 2.0, rounded panels and a compact bitmap typeface.
- **Navigation:** Home, Versions and Settings screens with eased tab transitions and animated panel movement.
- **Touch feedback:** iPhone-inspired press-down scaling, spring-back on release, and an expanding touch ripple on all launcher buttons, tabs and version-history rows. Touch animations are rendered only while a finger is held or a short animation is active; the app does not run an idle 60 FPS loop.
- **Package detection:** JNI calls into Android `PackageManager` to inspect `com.mojang.minecraftpe`.
- **Version information:** installed version name, full 64-bit version code on Android 9+, and last update timestamp.
- **Version library:** locally records the latest 20 installed versions observed by this launcher; the four newest entries are shown in the UI.
- **Launch:** starts the official installed Minecraft package through Android's package manager.
- **Store and Beta controls:** opens the official Google Play listing so the user can manage the Minecraft Beta enrollment.
- **ABIs:** `arm64-v8a` and `armeabi-v7a`.
- **App version:** 0.4.0.

## Touch animation

The custom OpenGL ES interface handles Android touch down, move, up and cancel events. Interactive elements shrink slightly while pressed, spring back after release, and show a short expanding ripple centered at the touch point. Page transitions animate the panels and labels together. The render loop is active only while these effects are running and blocks while idle.

## Android version-management limits

The Versions page is a **local history**, not a download catalogue or installer. It can show only versions this launcher has actually observed after it was installed. It cannot recreate versions that were never recorded.

The standard Android Minecraft app uses the package `com.mojang.minecraftpe`. Google Play manages that installation and updates it in place. This launcher does not claim to install arbitrary old APKs, keep multiple official Bedrock versions side by side, downgrade the app, or bypass the Play Store license and package-signature checks.

Mojang's Android test channel is **Minecraft Beta**, enrolled through the official Google Play listing. Joining Beta replaces the normal Minecraft client; it is not a second independently installed Android client. The Android package API does not expose a reliable flag proving whether a Google Play account is enrolled in Beta. TSI therefore opens the official Play listing for channel changes and refreshes the detected version when the app regains focus.

Back up worlds before changing channels because Beta replaces the normal client and test builds can be unstable.

Official guidance:
- https://help.minecraft.net/hc/en-us/articles/4423653831821
- https://help.minecraft.net/hc/en-us/articles/360040841471-How-Do-I-Join-and-Leave-the-Minecraft-Beta

## Windows launchers are a different platform

The Windows Bedrock Launcher works with Windows Store package formats and Windows deployment APIs. AppX/MSIX packages, PowerShell and Windows package registration cannot be used to install or launch Windows game packages on Android. A matching Android interface is possible, but the Windows installation backend is not portable to an Android APK.

## Scope and safety

Minecraft Bedrock is proprietary. TSI does not host, patch, unpack, or redistribute Minecraft game packages or data. Android's package manager and the official signed application remain responsible for game installation and updates.

The local version history is stored in the launcher's private app data directory and is capped at 20 entries. It is not an authoritative Mojang release archive.

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

GitHub Actions builds and verifies the native library for both configured ABIs on pull requests to `main`, pushes to `main`, and manual workflow runs. The uploaded artifact is named `TSI-Launcher-Bedrock-native-debug`.

Device-level launch, touch-target and animation testing still needs to be performed on an Android phone after the CI APK builds.
