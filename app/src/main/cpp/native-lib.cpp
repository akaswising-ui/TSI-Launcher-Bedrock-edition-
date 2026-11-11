#include <android/log.h>
#include <android/native_activity.h>
#include <android/input.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <jni.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#define LOG_TAG "TSI-Bedrock"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
constexpr char kBedrockPackage[] = "com.mojang.minecraftpe";
constexpr int kDesignWidth = 1280;
constexpr int kDesignHeight = 720;

struct AppState {
    android_app* app = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    GLuint program = 0;
    GLuint buffer = 0;
    GLint positionLocation = -1;
    GLint colorLocation = -1;
    int width = 0;
    int height = 0;
    bool installed = false;
    std::string version = "UNKNOWN";
    long long versionCode = 0;
    long long lastUpdateTime = 0;
    std::string status = "CHECKING MINECRAFT";
    std::string history = "NO VERSION HISTORY YET";
    std::vector<std::string> versionEntries;
    int selectedVersionIndex = 0;
    int historyScrollOffset = 0;
    int selectedTab = 0;
    float touchStartX = 0.0f;
    float touchStartY = 0.0f;
    bool pointerDown = false;
};

struct Color { float r, g, b; };
constexpr Color kBackground{0.035f, 0.039f, 0.047f};
constexpr Color kPanel{0.12f, 0.125f, 0.13f};
constexpr Color kAccent{0.08f, 0.62f, 0.28f};
constexpr Color kWhite{0.95f, 0.95f, 0.92f};
constexpr Color kMuted{0.68f, 0.69f, 0.67f};
constexpr Color kDark{0.035f, 0.08f, 0.045f};

std::array<unsigned char, 7> glyph(char c) {
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(c)))) {
        case 'A': return {14,17,17,31,17,17,17};
        case 'B': return {30,17,17,30,17,17,30};
        case 'C': return {14,17,16,16,16,17,14};
        case 'D': return {30,17,17,17,17,17,30};
        case 'E': return {31,16,16,30,16,16,31};
        case 'F': return {31,16,16,30,16,16,16};
        case 'G': return {14,17,16,23,17,17,15};
        case 'H': return {17,17,17,31,17,17,17};
        case 'I': return {14,4,4,4,4,4,14};
        case 'J': return {7,2,2,2,18,18,12};
        case 'K': return {17,18,20,24,20,18,17};
        case 'L': return {16,16,16,16,16,16,31};
        case 'M': return {17,27,21,21,17,17,17};
        case 'N': return {17,25,21,19,17,17,17};
        case 'O': return {14,17,17,17,17,17,14};
        case 'P': return {30,17,17,30,16,16,16};
        case 'Q': return {14,17,17,17,21,18,13};
        case 'R': return {30,17,17,30,20,18,17};
        case 'S': return {15,16,16,14,1,1,30};
        case 'T': return {31,4,4,4,4,4,4};
        case 'U': return {17,17,17,17,17,17,14};
        case 'V': return {17,17,17,17,17,10,4};
        case 'W': return {17,17,17,21,21,21,10};
        case 'X': return {17,17,10,4,10,17,17};
        case 'Y': return {17,17,10,4,4,4,4};
        case 'Z': return {31,1,2,4,8,16,31};
        case '0': return {14,17,19,21,25,17,14};
        case '1': return {4,12,4,4,4,4,14};
        case '2': return {14,17,1,2,4,8,31};
        case '3': return {30,1,1,14,1,1,30};
        case '4': return {2,6,10,18,31,2,2};
        case '5': return {31,16,16,30,1,1,30};
        case '6': return {14,16,16,30,17,17,14};
        case '7': return {31,1,2,4,8,8,8};
        case '8': return {14,17,17,14,17,17,14};
        case '9': return {14,17,17,15,1,1,14};
        case '.': return {0,0,0,0,0,12,12};
        case ':': return {0,12,12,0,12,12,0};
        case '-': return {0,0,0,31,0,0,0};
        case '/': return {1,2,2,4,8,8,16};
        case '_': return {0,0,0,0,0,0,31};
        case ' ': return {0,0,0,0,0,0,0};
        default: return {31,17,1,2,4,0,4};
    }
}

void addVertex(std::vector<float>& v, float x, float y, Color c, int width, int height) {
    const float nx = (2.0f * x / static_cast<float>(width)) - 1.0f;
    const float ny = 1.0f - (2.0f * y / static_cast<float>(height));
    v.insert(v.end(), {nx, ny, c.r, c.g, c.b});
}

void addRect(std::vector<float>& v, float x, float y, float w, float h, Color c, int sw, int sh) {
    const float x2 = x + w;
    const float y2 = y + h;
    addVertex(v, x, y, c, sw, sh);
    addVertex(v, x2, y, c, sw, sh);
    addVertex(v, x, y2, c, sw, sh);
    addVertex(v, x, y2, c, sw, sh);
    addVertex(v, x2, y, c, sw, sh);
    addVertex(v, x2, y2, c, sw, sh);
}

void addTriangle(std::vector<float>& v, float x1, float y1, float x2, float y2,
                 float x3, float y3, Color c, int sw, int sh) {
    addVertex(v, x1, y1, c, sw, sh);
    addVertex(v, x2, y2, c, sw, sh);
    addVertex(v, x3, y3, c, sw, sh);
}

void addGrassCube(std::vector<float>& v, float x, float y, float size, int sw, int sh) {
    const float half = size * 0.5f;
    const float top = y;
    const float mid = y + size * 0.28f;
    const float bottom = y + size;
    const Color topGreen{0.34f, 0.62f, 0.25f};
    const Color leftSoil{0.40f, 0.27f, 0.16f};
    const Color rightSoil{0.29f, 0.19f, 0.12f};
    const Color grassSide{0.25f, 0.48f, 0.19f};
    addTriangle(v, x + half, top, x + size, mid, x + half, y + size * 0.56f,
                topGreen, sw, sh);
    addTriangle(v, x + half, top, x + half, y + size * 0.56f, x, mid,
                Color{0.42f, 0.69f, 0.29f}, sw, sh);
    addTriangle(v, x, mid, x + half, y + size * 0.56f, x, y + size * 0.78f,
                grassSide, sw, sh);
    addTriangle(v, x, y + size * 0.78f, x + half, y + size * 0.56f,
                x + half, bottom, leftSoil, sw, sh);
    addTriangle(v, x + half, y + size * 0.56f, x + size, mid,
                x + size, y + size * 0.78f, grassSide, sw, sh);
    addTriangle(v, x + half, y + size * 0.56f, x + size, y + size * 0.78f,
                x + half, bottom, rightSoil, sw, sh);
}

void addText(std::vector<float>& v, const std::string& text, float x, float y,
             float pixel, Color c, int sw, int sh) {
    float cursor = x;
    for (char ch : text) {
        const auto rows = glyph(ch);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((rows[row] & (1u << (4 - col))) != 0) {
                    addRect(v, cursor + col * pixel, y + row * pixel,
                            pixel * 0.88f, pixel * 0.88f, c, sw, sh);
                }
            }
        }
        cursor += pixel * 6.0f;
    }
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint okay = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &okay);
    if (okay != GL_TRUE) {
        char error[1024]{};
        glGetShaderInfoLog(shader, sizeof(error), nullptr, error);
        LOGE("Shader compile failed: %s", error);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool createGraphics(AppState& s) {
    const EGLint attributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8, EGL_NONE
    };
    s.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s.display == EGL_NO_DISPLAY || !eglInitialize(s.display, nullptr, nullptr)) return false;
    EGLConfig config{};
    EGLint count = 0;
    if (!eglChooseConfig(s.display, attributes, &config, 1, &count) || count == 0) return false;
    EGLint format = 0;
    eglGetConfigAttrib(s.display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(s.app->window, 0, 0, format);
    const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    s.context = eglCreateContext(s.display, config, EGL_NO_CONTEXT, contextAttributes);
    s.surface = eglCreateWindowSurface(s.display, config, s.app->window, nullptr);
    if (s.context == EGL_NO_CONTEXT || s.surface == EGL_NO_SURFACE) return false;
    if (!eglMakeCurrent(s.display, s.surface, s.surface, s.context)) return false;

    const char* vertexSource =
        "attribute vec2 aPosition; attribute vec3 aColor; varying vec3 vColor;"
        "void main(){ gl_Position=vec4(aPosition,0.0,1.0); vColor=aColor; }";
    const char* fragmentSource =
        "precision mediump float; varying vec3 vColor;"
        "void main(){ gl_FragColor=vec4(vColor,1.0); }";
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertex || !fragment) return false;
    s.program = glCreateProgram();
    glAttachShader(s.program, vertex);
    glAttachShader(s.program, fragment);
    glBindAttribLocation(s.program, 0, "aPosition");
    glBindAttribLocation(s.program, 1, "aColor");
    glLinkProgram(s.program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(s.program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) return false;
    glGenBuffers(1, &s.buffer);
    s.positionLocation = 0;
    s.colorLocation = 1;
    eglQuerySurface(s.display, s.surface, EGL_WIDTH, &s.width);
    eglQuerySurface(s.display, s.surface, EGL_HEIGHT, &s.height);
    return true;
}

void destroyGraphics(AppState& s) {
    if (s.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(s.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s.buffer) glDeleteBuffers(1, &s.buffer);
        if (s.program) glDeleteProgram(s.program);
        if (s.surface != EGL_NO_SURFACE) eglDestroySurface(s.display, s.surface);
        if (s.context != EGL_NO_CONTEXT) eglDestroyContext(s.display, s.context);
        eglTerminate(s.display);
    }
    s.display = EGL_NO_DISPLAY;
    s.surface = EGL_NO_SURFACE;
    s.context = EGL_NO_CONTEXT;
    s.program = 0;
    s.buffer = 0;
}

JNIEnv* getEnv(JavaVM* vm, bool& attached) {
    attached = false;
    JNIEnv* env = nullptr;
    jint result = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
        attached = true;
    } else if (result != JNI_OK) {
        return nullptr;
    }
    return env;
}

void clearException(JNIEnv* env) {
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

void readVersionHistory(AppState& s) {
    if (!s.app || !s.app->activity || !s.app->activity->internalDataPath) return;

    const std::string path =
        std::string(s.app->activity->internalDataPath) + "/bedrock_versions.txt";
    const std::string temporaryPath = path + ".tmp";
    constexpr size_t kMaximumHistoryEntries = 20;

    std::vector<std::string> entries;
    {
        std::ifstream input(path);
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty()) entries.push_back(line);
        }
    }

    if (s.installed) {
        const std::string current = s.version + "|" +
            std::to_string(s.versionCode) + "|" +
            std::to_string(s.lastUpdateTime);

        if (entries.empty() || entries.back() != current) {
            entries.push_back(current);
        }
    }

    if (entries.size() > kMaximumHistoryEntries) {
        entries.erase(entries.begin(),
            entries.end() - static_cast<std::ptrdiff_t>(kMaximumHistoryEntries));
    }

    s.versionEntries.clear();
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        const auto separator = it->find('|');
        const std::string label = separator == std::string::npos
            ? *it : it->substr(0, separator);
        if (!label.empty() &&
            (s.versionEntries.empty() || s.versionEntries.back() != label)) {
            s.versionEntries.push_back(label);
        }
    }

    {
        std::ofstream output(temporaryPath, std::ios::trunc);
        if (output) {
            for (const auto& entry : entries) output << entry << "\n";
            output.flush();
            if (output.good()) {
                output.close();
                if (std::rename(temporaryPath.c_str(), path.c_str()) != 0) {
                    std::remove(temporaryPath.c_str());
                    LOGE("Could not atomically replace version history file");
                }
            } else {
                LOGE("Could not write version history file");
                output.close();
                std::remove(temporaryPath.c_str());
            }
        } else {
            LOGE("Could not open version history temporary file");
        }
    }

    if (!entries.empty()) {
        const std::string& latest = entries.back();
        const auto first = latest.find('|');
        const auto second = latest.find('|',
            first == std::string::npos ? first : first + 1);
        s.history = first == std::string::npos
            ? latest : latest.substr(0, first);
        if (second != std::string::npos) {
            s.history += " CODE " +
                latest.substr(first + 1, second - first - 1);
        }
    }
}

void detectBedrock(AppState& s) {
    s.installed = false;
    s.version = "NOT INSTALLED";
    s.versionCode = 0;
    s.lastUpdateTime = 0;
    bool attached = false;
    JNIEnv* env = getEnv(s.app->activity->vm, attached);
    if (!env) {
        s.status = "ANDROID PACKAGE API ERROR";
        return;
    }
    jobject activity = s.app->activity->clazz;
    jclass activityClass = env->GetObjectClass(activity);
    jmethodID getPackageManager = env->GetMethodID(
        activityClass, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject manager = getPackageManager ? env->CallObjectMethod(activity, getPackageManager) : nullptr;
    clearException(env);
    jclass managerClass = manager ? env->GetObjectClass(manager) : nullptr;
    jmethodID getPackageInfo = managerClass ? env->GetMethodID(
        managerClass, "getPackageInfo",
        "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;") : nullptr;
    jstring packageName = env->NewStringUTF(kBedrockPackage);
    jobject info = (manager && getPackageInfo)
        ? env->CallObjectMethod(manager, getPackageInfo, packageName, 0)
        : nullptr;
    if (env->ExceptionCheck()) {
        clearException(env);
        s.status = "MINECRAFT BEDROCK NOT FOUND";
    } else if (info) {
        jclass infoClass = env->GetObjectClass(info);
        jfieldID nameField = env->GetFieldID(infoClass, "versionName", "Ljava/lang/String;");
        jfieldID codeField = env->GetFieldID(infoClass, "versionCode", "I");
        jfieldID updateField = env->GetFieldID(infoClass, "lastUpdateTime", "J");
        jstring name = nameField
            ? static_cast<jstring>(env->GetObjectField(info, nameField)) : nullptr;
        if (name) {
            const char* chars = env->GetStringUTFChars(name, nullptr);
            if (chars) {
                s.version = chars;
                env->ReleaseStringUTFChars(name, chars);
            }
        }
        // Android 9+ exposes the full 64-bit version code. Keep the legacy
        // field only for API 23-27 devices.
        jclass buildVersionClass = env->FindClass("android/os/Build$VERSION");
        jfieldID sdkIntField = buildVersionClass
            ? env->GetStaticFieldID(buildVersionClass, "SDK_INT", "I") : nullptr;
        const jint sdkInt = sdkIntField
            ? env->GetStaticIntField(buildVersionClass, sdkIntField) : 0;

        if (sdkInt >= 28) {
            jmethodID getLongVersionCode =
                env->GetMethodID(infoClass, "getLongVersionCode", "()J");
            if (getLongVersionCode) {
                s.versionCode = env->CallLongMethod(info, getLongVersionCode);
            }
            clearException(env);
        } else if (codeField) {
            s.versionCode = env->GetIntField(info, codeField);
        }

        if (updateField) s.lastUpdateTime = env->GetLongField(info, updateField);
        if (buildVersionClass) env->DeleteLocalRef(buildVersionClass);
        s.installed = true;
        s.status = "OFFICIAL APP DETECTED";
        if (name) env->DeleteLocalRef(name);
        env->DeleteLocalRef(infoClass);
        env->DeleteLocalRef(info);
    } else {
        s.status = "MINECRAFT BEDROCK NOT FOUND";
    }
    readVersionHistory(s);
    if (packageName) env->DeleteLocalRef(packageName);
    if (managerClass) env->DeleteLocalRef(managerClass);
    if (manager) env->DeleteLocalRef(manager);
    if (activityClass) env->DeleteLocalRef(activityClass);
    if (attached) s.app->activity->vm->DetachCurrentThread();
}

bool launchIntent(AppState& s, const char* action, const char* uri, const char* package) {
    bool attached = false;
    JNIEnv* env = getEnv(s.app->activity->vm, attached);
    if (!env) return false;

    jobject activity = s.app->activity->clazz;
    jclass activityClass = env->GetObjectClass(activity);
    jobject intent = nullptr;
    if (package) {
        jmethodID getPackageManager = env->GetMethodID(
            activityClass, "getPackageManager", "()Landroid/content/pm/PackageManager;");
        jobject manager = getPackageManager ? env->CallObjectMethod(activity, getPackageManager) : nullptr;
        clearException(env);
        if (manager) {
            jclass managerClass = env->GetObjectClass(manager);
            jmethodID getLaunchIntent = env->GetMethodID(
                managerClass, "getLaunchIntentForPackage",
                "(Ljava/lang/String;)Landroid/content/Intent;");
            jstring packageName = env->NewStringUTF(package);
            intent = getLaunchIntent
                ? env->CallObjectMethod(manager, getLaunchIntent, packageName) : nullptr;
            clearException(env);
            if (packageName) env->DeleteLocalRef(packageName);
            env->DeleteLocalRef(managerClass);
            env->DeleteLocalRef(manager);
        }
    } else {
        jclass uriClass = env->FindClass("android/net/Uri");
        jmethodID parse = uriClass ? env->GetStaticMethodID(
            uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;") : nullptr;
        jstring uriString = env->NewStringUTF(uri);
        jobject parsedUri = parse ? env->CallStaticObjectMethod(uriClass, parse, uriString) : nullptr;
        jclass intentClass = env->FindClass("android/content/Intent");
        jmethodID constructor = intentClass ? env->GetMethodID(
            intentClass, "<init>",
            "(Ljava/lang/String;Landroid/net/Uri;)V") : nullptr;
        jstring actionString = env->NewStringUTF(action);
        intent = constructor
            ? env->NewObject(intentClass, constructor, actionString, parsedUri) : nullptr;
        clearException(env);
        if (actionString) env->DeleteLocalRef(actionString);
        if (intentClass) env->DeleteLocalRef(intentClass);
        if (parsedUri) env->DeleteLocalRef(parsedUri);
        if (uriString) env->DeleteLocalRef(uriString);
        if (uriClass) env->DeleteLocalRef(uriClass);
    }

    bool launched = false;
    if (intent) {
        jmethodID startActivity = env->GetMethodID(
            activityClass, "startActivity", "(Landroid/content/Intent;)V");
        if (startActivity) {
            env->CallVoidMethod(activity, startActivity, intent);
            launched = !env->ExceptionCheck();
            clearException(env);
        }
        env->DeleteLocalRef(intent);
    }
    env->DeleteLocalRef(activityClass);
    if (attached) s.app->activity->vm->DetachCurrentThread();
    return launched;
}

void draw(AppState& s) {
    if (s.display == EGL_NO_DISPLAY || s.surface == EGL_NO_SURFACE ||
        s.width <= 0 || s.height <= 0) return;

    std::vector<float> vertices;
    vertices.reserve(72000);
    const float scale = std::min(
        static_cast<float>(s.width) / kDesignWidth,
        static_cast<float>(s.height) / kDesignHeight);
    const float ox = (s.width - kDesignWidth * scale) * 0.5f;
    const float oy = (s.height - kDesignHeight * scale) * 0.5f;
    auto rect = [&](float x, float y, float w, float h, Color color) {
        addRect(vertices, ox + x * scale, oy + y * scale,
                w * scale, h * scale, color, s.width, s.height);
    };
    auto triangle = [&](float x1, float y1, float x2, float y2,
                        float x3, float y3, Color color) {
        addTriangle(vertices, ox + x1 * scale, oy + y1 * scale,
                    ox + x2 * scale, oy + y2 * scale,
                    ox + x3 * scale, oy + y3 * scale,
                    color, s.width, s.height);
    };
    auto text = [&](const std::string& value, float x, float y,
                    float size, Color color) {
        addText(vertices, value, ox + x * scale, oy + y * scale,
                size * scale, color, s.width, s.height);
    };
    auto cube = [&](float x, float y, float size) {
        addGrassCube(vertices, ox + x * scale, oy + y * scale,
                     size * scale, s.width, s.height);
    };

    const Color frame{0.075f, 0.078f, 0.082f};
    const Color card{0.18f, 0.18f, 0.18f};
    const Color cardLight{0.235f, 0.235f, 0.235f};
    const Color border{0.035f, 0.035f, 0.035f};
    const Color green{0.055f, 0.56f, 0.24f};
    const Color greenLight{0.12f, 0.72f, 0.32f};
    const Color sky{0.35f, 0.66f, 0.91f};
    const Color skyLight{0.62f, 0.79f, 0.96f};
    const Color grass{0.27f, 0.54f, 0.20f};
    const Color grassDark{0.14f, 0.35f, 0.15f};
    const Color soil{0.37f, 0.25f, 0.16f};
    const Color water{0.15f, 0.48f, 0.69f};

    // Outer launcher shell.
    rect(0, 0, 1280, 720, kBackground);
    rect(14, 14, 1252, 692, frame);
    rect(24, 24, 1232, 672, Color{0.105f, 0.105f, 0.105f});

    // Left column: locally observed Bedrock versions.
    rect(34, 48, 400, 624, card);
    rect(34, 48, 400, 62, Color{0.15f, 0.15f, 0.15f});
    text("BEDROCK EDITION", 58, 66, 3.0f, kWhite);
    text("ALL VERSIONS", 58, 91, 1.8f, kMuted);
    text("LOCAL VERSION HISTORY", 56, 128, 1.65f, kMuted);

    std::vector<std::string> rows = s.versionEntries;
    if (rows.empty() && s.installed) rows.push_back(s.version);
    if (rows.empty()) rows.push_back("NO LOCAL HISTORY");
    const int visibleRows = 6;
    const int maxOffset = std::max(0, static_cast<int>(rows.size()) - visibleRows);
    s.historyScrollOffset = std::max(0, std::min(s.historyScrollOffset, maxOffset));

    for (int i = 0; i < visibleRows; ++i) {
        const int index = s.historyScrollOffset + i;
        const float y = 150.0f + static_cast<float>(i) * 77.0f;
        const bool hasEntry = index < static_cast<int>(rows.size());
        const bool selected = hasEntry && index == s.selectedVersionIndex;
        rect(46, y, 376, 68, border);
        rect(50, y + 3, 368, 62, selected ? Color{0.25f, 0.28f, 0.25f} : cardLight);
        if (selected) {
            rect(50, y + 3, 5, 62, greenLight);
        }
        cube(64, y + 15, 38);
        text("MINECRAFT BEDROCK", 116, y + 17, 1.85f, kWhite);
        if (hasEntry) {
            const std::string label = rows[static_cast<size_t>(index)].substr(0, 22);
            text(label, 116, y + 43, 2.0f, selected ? greenLight : kMuted);
        } else {
            text("NO VERSION RECORDED", 116, y + 43, 1.6f, kMuted);
        }
    }

    rect(48, 621, 372, 40, green);
    text("BETAS", 194, 635, 2.2f, kWhite);
    text("HISTORY IS LOCAL ONLY", 58, 674, 1.3f, kMuted);

    // Center column: block-world showcase artwork.
    rect(456, 48, 500, 624, Color{0.09f, 0.09f, 0.09f});
    rect(470, 62, 472, 510, border);
    rect(476, 68, 460, 498, sky);
    rect(476, 68, 460, 76, skyLight);

    // Blocky clouds.
    rect(520, 112, 66, 18, Color{0.88f, 0.92f, 0.96f});
    rect(544, 94, 62, 18, Color{0.94f, 0.96f, 0.98f});
    rect(778, 144, 82, 19, Color{0.89f, 0.94f, 0.98f});
    rect(802, 126, 57, 18, Color{0.95f, 0.97f, 0.99f});
    rect(864, 220, 42, 16, Color{0.87f, 0.93f, 0.98f});

    // Distant cubic mountains and a bright horizon.
    triangle(476, 365, 610, 205, 742, 365, Color{0.39f, 0.57f, 0.43f});
    triangle(590, 365, 735, 190, 895, 365, Color{0.28f, 0.48f, 0.38f});
    triangle(730, 365, 850, 235, 936, 365, Color{0.40f, 0.58f, 0.45f});
    rect(476, 360, 460, 206, grass);
    rect(476, 445, 460, 121, grassDark);

    // Water winding through the world.
    triangle(476, 458, 626, 410, 710, 566, water);
    triangle(626, 410, 770, 456, 710, 566, Color{0.18f, 0.55f, 0.73f});
    rect(476, 520, 260, 46, Color{0.19f, 0.49f, 0.66f});

    // Terrain shelves and exposed dirt blocks.
    rect(476, 392, 170, 28, Color{0.33f, 0.59f, 0.23f});
    rect(476, 420, 170, 42, soil);
    rect(810, 396, 126, 24, Color{0.35f, 0.61f, 0.24f});
    rect(810, 420, 126, 46, soil);
    rect(754, 482, 182, 28, Color{0.39f, 0.62f, 0.26f});

    // Small village house.
    rect(790, 320, 92, 72, Color{0.62f, 0.42f, 0.23f});
    triangle(778, 326, 836, 278, 894, 326, Color{0.37f, 0.19f, 0.13f});
    rect(822, 350, 25, 42, Color{0.25f, 0.15f, 0.10f});
    rect(802, 340, 18, 18, Color{0.54f, 0.77f, 0.88f});
    rect(854, 340, 18, 18, Color{0.54f, 0.77f, 0.88f});

    // Pixel trees and shrubs.
    rect(520, 320, 20, 70, soil);
    rect(492, 282, 76, 62, Color{0.16f, 0.43f, 0.18f});
    rect(506, 262, 50, 36, Color{0.25f, 0.54f, 0.22f});
    rect(895, 330, 18, 68, soil);
    rect(872, 292, 68, 54, Color{0.18f, 0.44f, 0.18f});
    rect(886, 270, 42, 34, Color{0.26f, 0.56f, 0.22f});

    // Block-style player in the foreground.
    rect(668, 300, 54, 54, Color{0.72f, 0.49f, 0.34f});
    rect(668, 286, 54, 22, Color{0.30f, 0.20f, 0.13f});
    rect(680, 321, 8, 8, Color{0.08f, 0.08f, 0.08f});
    rect(704, 321, 8, 8, Color{0.08f, 0.08f, 0.08f});
    rect(686, 343, 19, 5, Color{0.30f, 0.13f, 0.10f});
    rect(660, 354, 70, 92, Color{0.12f, 0.49f, 0.72f});
    rect(640, 360, 20, 76, Color{0.72f, 0.49f, 0.34f});
    rect(730, 360, 20, 76, Color{0.72f, 0.49f, 0.34f});
    rect(665, 440, 26, 62, Color{0.13f, 0.16f, 0.48f});
    rect(700, 440, 26, 62, Color{0.13f, 0.16f, 0.48f});

    // Overlay title and a sharp inner border.
    text("MINECRAFT", 610, 510, 4.0f, Color{0.06f, 0.07f, 0.06f});
    text("BEDROCK", 650, 538, 2.1f, kWhite);
    rect(476, 68, 460, 4, Color{0.035f, 0.035f, 0.035f});
    rect(476, 562, 460, 4, Color{0.035f, 0.035f, 0.035f});

    // Primary action.
    rect(536, 594, 340, 68, border);
    rect(542, 600, 328, 56, green);
    rect(548, 606, 316, 4, greenLight);
    text("PLAY", 657, 621, 3.7f, kWhite);

    // Right navigation and management column.
    rect(976, 48, 270, 624, Color{0.105f, 0.105f, 0.105f});
    cube(1000, 74, 50);
    text("BEDROCK", 1060, 85, 2.7f, kWhite);
    text("EDITION", 1060, 115, 2.5f, kWhite);
    text("ALL VERSIONS", 1060, 145, 1.6f, kMuted);

    const auto tab = [&](int y, const std::string& label, int value) {
        const bool selected = s.selectedTab == value;
        rect(994, static_cast<float>(y), 232, 58,
             selected ? green : Color{0.16f, 0.16f, 0.16f});
        rect(994, static_cast<float>(y), 5, 58,
             selected ? greenLight : Color{0.09f, 0.09f, 0.09f});
        text(label, 1030, static_cast<float>(y) + 20.0f,
             label == "INSTALLATIONS" ? 1.75f : 2.4f,
             selected ? kWhite : kMuted);
    };
    tab(218, "NEWS", 0);
    tab(286, "MODS", 1);
    tab(354, "INSTALLATIONS", 2);

    rect(994, 432, 232, 68, card);
    text("INSTALLED VERSION", 1012, 447, 1.45f, kMuted);
    const std::string installedVersion = s.installed ? s.version : "NOT INSTALLED";
    text(installedVersion.substr(0, 18), 1012, 473, 1.8f,
         s.installed ? greenLight : kWhite);

    rect(994, 514, 232, 48, Color{0.16f, 0.16f, 0.16f});
    text("REFRESH", 1065, 530, 2.0f, kWhite);

    rect(994, 576, 232, 72, border);
    rect(1000, 582, 220, 60, green);
    text("MANAGE", 1066, 604, 2.7f, kWhite);
    text(s.status.substr(0, 31), 990, 660, 1.25f, kMuted);

    glViewport(0, 0, s.width, s.height);
    glClearColor(kBackground.r, kBackground.g, kBackground.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(s.program);
    glBindBuffer(GL_ARRAY_BUFFER, s.buffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                 vertices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 5));
    eglSwapBuffers(s.display, s.surface);
}
void onCommand(android_app* app, int32_t command) {
    auto* s = static_cast<AppState*>(app->userData);
    if (!s) return;
    switch (command) {
        case APP_CMD_INIT_WINDOW:
            if (app->window && createGraphics(*s)) {
                detectBedrock(*s);
                draw(*s);
            } else {
                s->status = "GRAPHICS INITIALIZATION FAILED";
            }
            break;
        case APP_CMD_TERM_WINDOW:
            destroyGraphics(*s);
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONTENT_RECT_CHANGED:
            if (s->display != EGL_NO_DISPLAY && s->surface != EGL_NO_SURFACE) {
                eglQuerySurface(s->display, s->surface, EGL_WIDTH, &s->width);
                eglQuerySurface(s->display, s->surface, EGL_HEIGHT, &s->height);
                draw(*s);
            }
            break;
        case APP_CMD_GAINED_FOCUS:
            detectBedrock(*s);
            draw(*s);
            break;
        default:
            break;
    }
}

bool inside(float x, float y, float left, float top, float right, float bottom) {
    return x >= left && x <= right && y >= top && y <= bottom;
}

int32_t onInput(android_app* app, AInputEvent* event) {
    auto* s = static_cast<AppState*>(app->userData);
    if (!s || AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    const int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    if (action == AMOTION_EVENT_ACTION_DOWN) {
        s->touchStartX = AMotionEvent_getX(event, 0);
        s->touchStartY = AMotionEvent_getY(event, 0);
        s->pointerDown = true;
        return 1;
    }
    if (action == AMOTION_EVENT_ACTION_MOVE) return 1;
    if (action != AMOTION_EVENT_ACTION_UP) return 1;

    const float scale = std::min(
        static_cast<float>(s->width) / kDesignWidth,
        static_cast<float>(s->height) / kDesignHeight);
    if (scale <= 0.0f) {
        s->pointerDown = false;
        return 1;
    }
    const float ox = (s->width - kDesignWidth * scale) * 0.5f;
    const float oy = (s->height - kDesignHeight * scale) * 0.5f;
    const float x = (AMotionEvent_getX(event, 0) - ox) / scale;
    const float y = (AMotionEvent_getY(event, 0) - oy) / scale;
    const float startY = (s->touchStartY - oy) / scale;
    const bool wasPointerDown = s->pointerDown;
    s->pointerDown = false;

    // A vertical swipe inside the version list scrolls local history.
    if (wasPointerDown &&
        inside(x, startY, 46, 150, 422, 612) &&
        std::fabs(y - startY) > 28.0f) {
        const int steps = std::max(1, static_cast<int>(std::fabs(y - startY) / 55.0f));
        const int maxOffset = std::max(0, static_cast<int>(s->versionEntries.size()) - 6);
        if (y < startY) s->historyScrollOffset = std::min(maxOffset, s->historyScrollOffset + steps);
        else s->historyScrollOffset = std::max(0, s->historyScrollOffset - steps);
        draw(*s);
        return 1;
    }

    // Selecting a saved row never downgrades or replaces the installed game.
    if (inside(x, y, 46, 150, 422, 612)) {
        const int row = static_cast<int>((y - 150.0f) / 77.0f);
        const int index = s->historyScrollOffset + row;
        if (row >= 0 && row < 6 &&
            index >= 0 && index < static_cast<int>(s->versionEntries.size())) {
            s->selectedVersionIndex = index;
            s->status = "LOCAL VERSION HISTORY SELECTED";
        }
    } else if (inside(x, y, 48, 621, 420, 661)) {
        const bool openedMarket = launchIntent(
            *s, "android.intent.action.VIEW",
            "market://details?id=com.mojang.minecraftpe", nullptr);
        const bool openedWeb = openedMarket || launchIntent(
            *s, "android.intent.action.VIEW",
            "https://play.google.com/store/apps/details?id=com.mojang.minecraftpe",
            nullptr);
        s->status = openedWeb ? "OPENED OFFICIAL MINECRAFT LISTING"
                              : "COULD NOT OPEN GOOGLE PLAY";
    } else if (inside(x, y, 536, 594, 876, 662)) {
        if (!s->installed) {
            s->status = "INSTALL OFFICIAL MINECRAFT FIRST";
        } else if (launchIntent(*s, nullptr, nullptr, kBedrockPackage)) {
            s->status = "MINECRAFT LAUNCH REQUEST SENT";
        } else {
            s->status = "MINECRAFT LAUNCH FAILED";
        }
    } else if (inside(x, y, 994, 218, 1226, 276)) {
        s->selectedTab = 0;
        const bool opened = launchIntent(
            *s, "android.intent.action.VIEW",
            "https://www.minecraft.net/en-us/articles", nullptr);
        s->status = opened ? "OPENED OFFICIAL MINECRAFT NEWS"
                           : "COULD NOT OPEN NEWS PAGE";
    } else if (inside(x, y, 994, 286, 1226, 344)) {
        s->selectedTab = 1;
        const bool opened = launchIntent(
            *s, "android.intent.action.VIEW",
            "https://www.minecraft.net/en-us/addons", nullptr);
        s->status = opened ? "OPENED OFFICIAL ADDONS PAGE"
                           : "COULD NOT OPEN ADDONS PAGE";
    } else if (inside(x, y, 994, 354, 1226, 412)) {
        s->selectedTab = 2;
        s->status = s->installed ? "OFFICIAL BEDROCK INSTALLATION DETECTED"
                                 : "MINECRAFT BEDROCK NOT INSTALLED";
    } else if (inside(x, y, 994, 432, 1226, 500)) {
        s->selectedTab = 2;
        s->status = s->installed ? "OFFICIAL CLIENT VERSION SHOWN"
                                 : "INSTALL OFFICIAL CLIENT FROM PLAY";
    } else if (inside(x, y, 994, 514, 1226, 562)) {
        detectBedrock(*s);
        s->status = s->installed ? "INSTALLED VERSION REFRESHED"
                                 : "MINECRAFT BEDROCK NOT INSTALLED";
    } else if (inside(x, y, 994, 576, 1226, 648)) {
        const bool openedMarket = launchIntent(
            *s, "android.intent.action.VIEW",
            "market://details?id=com.mojang.minecraftpe", nullptr);
        const bool openedWeb = openedMarket || launchIntent(
            *s, "android.intent.action.VIEW",
            "https://play.google.com/store/apps/details?id=com.mojang.minecraftpe",
            nullptr);
        s->status = openedWeb ? "PLAY LISTING: MANAGE OFFICIAL INSTALL"
                              : "COULD NOT OPEN GOOGLE PLAY";
    }

    draw(*s);
    return 1;
}
} // namespace

void android_main(android_app* app) {
    app_dummy();
    AppState state;
    state.app = app;
    app->userData = &state;
    app->onAppCmd = onCommand;
    app->onInputEvent = onInput;

    while (true) {
        int events = 0;
        android_poll_source* source = nullptr;

        // The launcher screen is static between events. Block instead of
        // redrawing at 60 FPS, which wastes CPU and battery on a phone.
        while (ALooper_pollOnce(-1, nullptr, &events,
                                reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested != 0) {
                destroyGraphics(state);
                return;
            }
        }

        if (app->destroyRequested != 0) {
            destroyGraphics(state);
            return;
        }
    }
}
