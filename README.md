# TSI Launcher Bedrock Edition

A C++17 Android companion launcher for the official Minecraft Bedrock app. The app uses Android NativeActivity, the Android NDK, CMake, EGL and OpenGL ES 2.0. It contains no Java or Kotlin application source files and uses Groovy Gradle build scripts rather than Kotlin DSL.

## Reference-style launcher UI

The main screen is a landscape, three-column layout based on the supplied mobile reference:

- **Left:** Bedrock version history list. It shows versions this TSI installation actually observed and saved locally. Swipe vertically to browse recorded entries.
- **Center:** block-world showcase artwork drawn natively with C++/OpenGL ES, plus the large green Play action.
- **Right:** News, Mods and Installations navigation, installed-version status, Refresh and Manage actions.

The UI is designed for a phone in landscape orientation and scales to the available display. The decorative phone bezel in the supplied reference is not part of the Android app screen.

## Real launcher behavior

- **Play:** launches the official `com.mojang.minecraftpe` app through Android PackageManager.
- **Manage / Betas:** opens the official Google Play listing. The user joins or leaves the Android Beta through Google Play.
- **News / Mods:** opens official Minecraft articles and add-ons pages in the browser.
- **Installations:** reports the official client currently detected on the device.
- **Refresh:** re-reads the installed package version and updates local history.
- **Version history:** stores only versions observed by this launcher, capped at the latest 20 entries. Selecting a row does not claim to install or downgrade that version.

## Stable and Beta on Android

Mojang's Android test channel is **Minecraft Beta**, enrolled through the official Google Play listing. Joining Beta replaces the normal Minecraft client; Stable and Beta are not two independently installed Android copies. The installed Android package remains `com.mojang.minecraftpe`.

Minecraft Preview is not the Android distribution channel. Android PackageManager exposes the installed package version, but it does not provide a reliable flag proving whether the user's Google Play account is enrolled in Beta.

TSI's **Manage** and **Betas** actions hand off to the official Google Play listing. The user completes Join Beta or Leave Beta there. TSI does not silently change the user's Play testing enrollment. Back up worlds before changing channels because Beta replaces the normal client and test builds can be unstable.

Official guidance:
- https://help.minecraft.net/hc/en-us/articles/4423653831821
- https://help.minecraft.net/hc/en-us/articles/360040841471-How-Do-I-Join-and-Leave-the-Minecraft-Beta

## Platform and licensing limits

Minecraft Bedrock is proprietary. TSI does not host, patch, downgrade, unpack or redistribute Minecraft game packages or data. Android's package manager and the official signed application remain responsible for installation and updates.

The Android launcher cannot install Windows AppX/MSIX packages. Those are Windows deployment formats and require a separate Windows implementation.

The News and Mods navigation opens official web pages. It does not download third-party add-ons or modify game data.

## C++-only application source

- Android entry point: `android.app.NativeActivity` loading `libtsi_bedrock.so`.
- App implementation: C++17 in `app/src/main/cpp/native-lib.cpp`.
- Rendering: EGL + OpenGL ES 2.0.
- Android package inspection and launch: JNI calls from C++ into Android framework APIs.
- Build configuration: Groovy Gradle scripts (`.gradle`), not Kotlin DSL.
- ABIs: `arm64-v8a` and `armeabi-v7a`.

The JDK is required by the Android Gradle build tools; it is not used for application logic.

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

GitHub Actions builds and verifies the native library for both configured ABIs on pushes and pull requests to `main`, plus manual workflow runs. The uploaded artifact is named `TSI-Launcher-Bedrock-native-debug`.

## Device testing still required

CI verifies compilation, APK packaging and both native ABI libraries. Actual Minecraft detection, Play hand-off, touch behavior, display scaling and game launch must still be checked on an Android device. A successful CI build is not proof that the launcher has been tested on every phone.
