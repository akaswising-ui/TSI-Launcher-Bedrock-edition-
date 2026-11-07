# TSI Launcher Bedrock Edition

A separate Android companion launcher for **Minecraft Bedrock Edition**.

## Features

- Detects the official Android Minecraft package: com.mojang.minecraftpe.
- Displays the installed version name and version code.
- Opens the official Minecraft app using Android's package launch API.
- Opens the official Google Play listing for installation and updates.
- Keeps a local history of installed version changes observed by TSI.
- Explains Android Stable/Beta channel behaviour without pretending Preview is a separate Android package.

## Android channel behaviour

Minecraft Bedrock on Android is distributed as the official Google Play app. The Android Beta track updates that same package; it does not create a second independent Bedrock app that can be launched beside Stable. Android Preview is not a separate Android app channel.

The Android package manager does not reliably expose which Google Play testing track a user selected. TSI therefore reports the installed version and directs channel management to Google Play. It does not guess the track or promise to switch it silently.

## Separation from Java Edition

This repository is independent from TSI Launcher Java Edition.

- This app contains no Java runtime, JVM, LWJGL, Amethyst engine, Java version catalogue or Java mod loader.
- The Java launcher contains no Bedrock detection or Bedrock channel UI.
- The package ID is com.tsi.launcher.bedrock, so it can be installed alongside the Java Edition app package.

## Scope

Minecraft Bedrock is proprietary. TSI does not host, patch, downgrade, unpack or redistribute the official game APK or game data. Android's package manager and the signed official package continue to control installation and updates.

Version history is local to this TSI app. It records versions observed after installation/update; it is not an authoritative Mojang release archive and does not recover past versions that were never observed.

## Build

Requirements:
- Android SDK Platform 36
- JDK 17
- Gradle 8.13

Build from the repository root:

    gradle :app:assembleDebug

APK output:

    app/build/outputs/apk/debug/app-debug.apk

GitHub Actions builds the debug APK on pushes to main and manual workflow runs. The artifact is named TSI-Launcher-Bedrock-debug.
