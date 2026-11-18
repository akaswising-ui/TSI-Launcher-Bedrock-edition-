# TSI Launcher Bedrock Edition

An Android companion launcher for the official Minecraft Bedrock app. The application layer is implemented in **C++17** with Android NDK `NativeActivity`, CMake, EGL and OpenGL ES 2.0. There are no Kotlin or Java application source files.

## Launcher interface (0.6.0)

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

## Version switching with user-owned APKs

The All Versions page records the latest 20 Minecraft versions that this launcher has actually observed. It now supports handing a user-selected APK to Android's system installer:

1. Keep your own legally obtained, original Minecraft Bedrock APK files in the phone's Files / Downloads folder.
2. In the file manager, select one APK and choose **Share**.
3. Choose **TSI Launcher Bedrock Edition** from the share sheet.
4. TSI forwards that file URI to Android's package installer. Review the package details and confirm installation in Android.
5. Reopen TSI after the install; the newly detected installed version is recorded in local history.

Only a single Minecraft Android package is installed for the device at a time. This is version replacement, not side-by-side multi-version execution. Keep each original APK file yourself; TSI does not download or redistribute Minecraft game packages.

Important limits:

- Use an original, unmodified APK for the package `com.mojang.minecraftpe`. Android verifies package signatures; an APK signed with a different key cannot update the installed official app.
- Android may reject installing an older version over a newer one. The launcher does not bypass Android's package manager or force a downgrade. Where Android refuses the downgrade, the existing Minecraft app may need to be uninstalled before the older APK can be installed, and this can erase its private game data.
- **Export or back up every world from inside Minecraft before uninstalling or changing versions.** TSI cannot reliably copy Minecraft's private app data or restore every world's data layout.
- Keep the APK archive outside Minecraft's app storage. TSI records version names it sees; it does not make an APK archive from that history.
- This share flow accepts one standalone `.apk` file at a time. It does not combine split `.apks` / `.xapk` bundles or Windows `.appx` / `.msix` packages.
- The launcher cannot promise every historical Bedrock build will run on every Android release or device. Compatibility depends on the game's minimum Android/API requirements, device ABI, graphics driver, and the selected build.

## Native mobile launcher

- **Application entry point:** Android `NativeActivity` loading `libtsi_bedrock.so`.
- **Rendering:** C++17 and OpenGL ES 2.0, rounded panels and a compact bitmap typeface.
- **Package detection:** JNI calls into Android `PackageManager` to inspect `com.mojang.minecraftpe`.
- **Version information:** installed version name, full 64-bit version code on Android 9+, and last update timestamp.
- **Version history:** locally records the latest 20 installed versions observed by this launcher.
- **Launch:** starts the official installed Minecraft package through Android's package manager.
- **Store and Beta controls:** opens the official Google Play listing so the user can manage Minecraft Beta enrollment.
- **APK handoff:** receives a shared standalone APK URI and opens Android's system package installer with read permission.
- **ABIs:** `arm64-v8a` and `armeabi-v7a`.
- **App version:** 0.6.0.

## Android version-management limits

The standard Android Minecraft app uses package `com.mojang.minecraftpe`. Google Play manages the official installation and updates it in place. The launcher does not keep multiple official Bedrock versions installed side by side, create a historical release archive, bypass Play Store licensing, or change Android's package signature / downgrade rules.

Minecraft Beta enrollment is managed through Google Play. Joining Beta replaces the normal Minecraft client; it is not a second independently installed Android client. The Android package API does not expose a reliable flag proving whether a Google Play account is enrolled in Beta. TSI therefore opens the official Play listing for channel changes and refreshes the detected version when the app regains focus.

Back up worlds before changing channels or installing another game version because the selected version may have a different world format or app-data behavior.

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

Device-level APK share, Android installer handoff, touch-target and game-launch testing still needs to be performed on an Android phone after the CI APK builds.
