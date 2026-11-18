# TSI Launcher Bedrock Edition

An Android companion launcher for the official Minecraft Bedrock app. The application layer is implemented in **C++17** with Android NDK `NativeActivity`, CMake, EGL and OpenGL ES 2.0. There are no Kotlin or Java application source files.

## Launcher interface (0.5.0)

The interface has been reworked around the supplied wide desktop-launcher reference while remaining an Android app:

- Landscape layout with a dark desktop-style title strip and left navigation sidebar.
- Sidebar pages: Launch, All Versions, Mods, Servers, News and Settings.
- Large block-built sunset banner with Play action and Preview / Release channel shortcuts.
- Two-column version cards and bottom search / Stable / Preview / Beta / All filter controls.
- Profile and quick-settings surfaces, local version count and app/game status.
- iPhone-inspired touch-down scaling, spring-back release and expanding ripple feedback.
- Responsive horizontal canvas expands on wider landscape phone aspect ratios rather than letterboxing the UI.
- Mods / Add-ons page links to the official Marketplace, a community add-on directory and the Bedrock add-on help guide.
- Servers page links to the official Minecraft server directory and server-joining guide.
- News page links to official Minecraft news and Help Center pages.

The layout is a native OpenGL recreation of the reference's structure and visual style; it is not a pixel-for-pixel copy of the Windows desktop image. Desktop-only title-bar buttons are decorative window chrome, not Android window-management controls.

## Native mobile launcher

- **Application entry point:** Android `NativeActivity` loading `libtsi_bedrock.so`.
- **Rendering:** C++17 and OpenGL ES 2.0, rounded panels and a compact bitmap typeface.
- **Package detection:** JNI calls into Android `PackageManager` to inspect `com.mojang.minecraftpe`.
- **Version information:** installed version name, full 64-bit version code on Android 9+, and last update timestamp.
- **Version library:** locally records the latest 20 installed versions observed by this launcher.
- **Launch:** starts the official installed Minecraft package through Android's package manager.
- **Store and Beta controls:** opens the official Google Play listing so the user can manage Minecraft Beta enrollment.
- **ABIs:** `arm64-v8a` and `armeabi-v7a`.
- **App version:** 0.5.0.

## Android version-management limits

The Versions page is a **local history**, not a download catalogue or installer. It can show only versions this launcher has actually observed after it was installed. It cannot recreate versions that were never recorded.

The standard Android Minecraft app uses the package `com.mojang.minecraftpe`. Google Play manages that installation and updates it in place. This launcher does not claim to install arbitrary old APKs, keep multiple official Bedrock versions side by side, downgrade the app, or bypass Play Store license and package-signature checks.

Minecraft Beta enrollment is managed through Google Play. Joining Beta replaces the normal Minecraft client; it is not a second independently installed Android client. The Android package API does not expose a reliable flag proving whether a Google Play account is enrolled in Beta. TSI therefore opens the official Play listing for channel changes and refreshes the detected version when the app regains focus.

Back up worlds before changing channels because Beta replaces the normal client and test builds can be unstable.

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

Device-level layout, touch-target, game-launch and animation testing still needs to be performed on an Android phone after the CI APK builds.
