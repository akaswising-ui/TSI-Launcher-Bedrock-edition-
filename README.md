# TSI Launcher Bedrock Edition

An Android companion launcher for the official Minecraft Bedrock app. The application layer is implemented in **C++17** with Android NDK `NativeActivity`, CMake, EGL and OpenGL ES 2.0. There are no Kotlin or Java application source files.

## Launcher interface (0.7.0)

The interface follows the structure of the supplied wide desktop-launcher reference while remaining an Android app:

- Landscape layout with a dark left navigation sidebar and desktop-style title strip.
- Sidebar pages: Launch, All Versions, Mods, Servers, News and Settings.
- Large block-built sunset banner with Play action and Preview / Release shortcuts.
- Two-column version cards and bottom search / Stable / Preview / Beta / All filter controls.
- Profile and quick-settings surfaces, local version count and game status.
- Responsive horizontal canvas expands on wider landscape phone aspect ratios.
- Mods / Add-ons links to the official Marketplace, community add-on directory and Bedrock add-on help.
- Servers links to the official Minecraft server directory and server-joining guide.
- News links to official Minecraft news and Help Center pages.
- iPhone-inspired touch-down scaling, spring-back release and expanding ripple feedback.

The UI is a native OpenGL recreation of the reference's structure and visual style. It is not a pixel-for-pixel copy of the Windows desktop image. Desktop title-bar controls are visual chrome, not Android window-management controls.

## In-launcher official download buttons

The **All Versions** page offers:
- **Download Stable:** opens the official Minecraft Google Play listing in the Play Store. The user presses Install or Update there; Google Play delivers the official signed Android package.
- **Get Beta:** opens the same official Play Store listing so the user can opt into or leave Minecraft Beta.
- **All Release Notes / Version History:** opens Mojang's official Release Changelogs archive.
- **Recently installed on this device:** shows the latest versions TSI has actually observed on the phone.

This removes the APK file import / Share workflow. TSI no longer declares `REQUEST_INSTALL_PACKAGES`, receives shared APK files, or attempts to install packages itself.

### Historical-version download limitation

Mojang's public Android guidance directs users to update Minecraft through the mobile operating system's app store. Its Android Beta instructions also use Google Play opt-in; Beta replaces the normal Android client. Mojang publishes release notes and version history, but does not provide a public official archive of every historical Android APK binary.

Therefore, the Stable and Beta buttons download through the official Google Play distribution route. The changelog button gives access to historical release notes, **not old APK files**. TSI does not download proprietary historical Minecraft APKs from unofficial mirrors or redistribute game packages. If Mojang provides an official historical Android binary distribution channel in the future, a version-specific direct source can be added.

Official sources:
- Mobile update instructions: https://help.minecraft.net/hc/en-us/articles/4409172223501-Manually-Update-Minecraft-on-Mobile-Devices
- Beta instructions: https://help.minecraft.net/hc/en-us/articles/4423653831821
- Official release changelogs: https://feedback.minecraft.net/hc/en-us/sections/360001186971-Release-Changelogs
- Google Play listing: https://play.google.com/store/apps/details?id=com.mojang.minecraftpe

## Android version-management limits

The standard Android Minecraft app uses package `com.mojang.minecraftpe`. Google Play manages the official installation and updates it in place. Android installs one package at a time; this launcher cannot keep multiple official Bedrock Android versions installed side by side, downgrade the game against Android package-manager rules, bypass licensing, or change package signatures.

Minecraft Beta enrollment is managed through Google Play. Joining Beta replaces the normal Minecraft client; it is not a second independently installed Android client. Back up worlds before changing channels because test builds can be unstable and world data may not be compatible across versions.

## Windows launchers are a different platform

The Windows Bedrock Launcher works with Windows Store package formats and Windows deployment APIs. AppX/MSIX packages, PowerShell and Windows package registration cannot be used to install or launch Windows game packages on Android. A matching Android interface is possible, but the Windows installation backend is not portable to an Android APK.

## Scope and safety

Minecraft Bedrock is proprietary. TSI does not host, patch, unpack, or redistribute Minecraft game packages or data. Android's package manager and Google Play remain responsible for official game installation and updates.

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

Device-level verification of Play Store routing, Beta opt-in, touch-target and game-launch behavior still needs to be performed on an Android phone after the CI APK builds.
