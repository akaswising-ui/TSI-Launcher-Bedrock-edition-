cmake_minimum_required(VERSION 3.22.1)
project(tsi_bedrock_launcher LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_ANDROID_NDK)
    message(FATAL_ERROR "Android NDK path was not provided by the Android Gradle Plugin.")
endif()

set(ANDROID_NATIVE_APP_GLUE "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
if(NOT EXISTS "${ANDROID_NATIVE_APP_GLUE}/android_native_app_glue.c")
    message(FATAL_ERROR "android_native_app_glue.c was not found in the selected NDK.")
endif()

add_library(android_native_app_glue STATIC
    "${ANDROID_NATIVE_APP_GLUE}/android_native_app_glue.c"
)
target_include_directories(android_native_app_glue PUBLIC
    "${ANDROID_NATIVE_APP_GLUE}"
)

add_library(tsi_bedrock SHARED native-lib.cpp)
target_include_directories(tsi_bedrock PRIVATE
    "${ANDROID_NATIVE_APP_GLUE}"
)
target_compile_options(tsi_bedrock PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(tsi_bedrock PRIVATE android_native_app_glue android log EGL GLESv2)
