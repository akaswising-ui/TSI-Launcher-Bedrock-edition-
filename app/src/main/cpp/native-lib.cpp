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
#include <chrono>
#include <cctype>
#include <cstddef>
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
constexpr int kDesignWidth = 360;
constexpr int kDesignHeight = 720;
constexpr int kAnimationMilliseconds = 380;
using Clock = std::chrono::steady_clock;

enum class Page { Home = 0, Versions = 1, Settings = 2 };

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
    std::vector<std::string> versionHistory;
    Page page = Page::Home;
    Page previousPage = Page::Home;
    bool animationActive = false;
    Clock::time_point animationStart = Clock::now();
};

struct Color { float r, g, b; };
constexpr Color kBackground{0.035f, 0.063f, 0.090f};
constexpr Color kPanel{0.075f, 0.118f, 0.157f};
constexpr Color kPanelBright{0.100f, 0.157f, 0.204f};
constexpr Color kAccent{0.275f, 0.835f, 0.580f};
constexpr Color kAccentDark{0.035f, 0.105f, 0.075f};
constexpr Color kWhite{0.96f, 0.98f, 1.0f};
constexpr Color kMuted{0.61f, 0.70f, 0.77f};
constexpr Color kLine{0.145f, 0.220f, 0.270f};
constexpr Color kWarning{0.96f, 0.69f, 0.34f};

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
        case '+': return {0,4,4,31,4,4,0};
        case ' ': return {0,0,0,0,0,0,0};
        default: return {31,17,1,2,4,0,4};
    }
}

void addVertex(std::vector<float>& v, float x, float y, Color c, int width, int height) {
    const float nx = (2.0f * x / static_cast<float>(width)) - 1.0f;
    const float ny = 1.0f - (2.0f * y / static_cast<float>(height));
    v.insert(v.end(), {nx, ny, c.r, c.g, c.b});
}

void addRect(std::vector<float>& v, float x, float y, float w, float h,
             Color c, int sw, int sh) {
    const float x2 = x + w;
    const float y2 = y + h;
    addVertex(v, x, y, c, sw, sh);
    addVertex(v, x2, y, c, sw, sh);
    addVertex(v, x, y2, c, sw, sh);
    addVertex(v, x, y2, c, sw, sh);
    addVertex(v, x2, y, c, sw, sh);
    addVertex(v, x2, y2, c, sw, sh);
}

void addQuarterCircle(std::vector<float>& v, float cx, float cy, float radius,
                      float startAngle, Color color, int sw, int sh) {
    constexpr int kSegments = 6;
    const float pi = std::acos(-1.0f);
    for (int i = 0; i < kSegments; ++i) {
        const float a1 = startAngle + (pi * 0.5f) * i / kSegments;
        const float a2 = startAngle + (pi * 0.5f) * (i + 1) / kSegments;
        addVertex(v, cx, cy, color, sw, sh);
        addVertex(v, cx + std::cos(a1) * radius,
                  cy + std::sin(a1) * radius, color, sw, sh);
        addVertex(v, cx + std::cos(a2) * radius,
                  cy + std::sin(a2) * radius, color, sw, sh);
    }
}

void addRoundedRect(std::vector<float>& v, float x, float y, float w, float h,
                    float radius, Color c, int sw, int sh) {
    const float r = std::max(0.0f, std::min(radius, std::min(w, h) * 0.5f));
    if (r < 1.0f) {
        addRect(v, x, y, w, h, c, sw, sh);
        return;
    }
    addRect(v, x + r, y, w - 2.0f * r, h, c, sw, sh);
    addRect(v, x, y + r, r, h - 2.0f * r, c, sw, sh);
    addRect(v, x + w - r, y + r, r, h - 2.0f * r, c, sw, sh);
    const float pi = std::acos(-1.0f);
    addQuarterCircle(v, x + r, y + r, r, pi, c, sw, sh);
    addQuarterCircle(v, x + w - r, y + r, r, -pi * 0.5f, c, sw, sh);
    addQuarterCircle(v, x + w - r, y + h - r, r, 0.0f, c, sw, sh);
    addQuarterCircle(v, x + r, y + h - r, r, pi * 0.5f, c, sw, sh);
}

void addText(std::vector<float>& v, const std::string& value, float x, float y,
             float pixel, Color c, int sw, int sh) {
    float cursor = x;
    for (char ch : value) {
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

void startAnimation(AppState& s) {
    s.animationStart = Clock::now();
    s.animationActive = true;
}

void setPage(AppState& s, Page page) {
    if (s.page == page) return;
    s.previousPage = s.page;
    s.page = page;
    startAnimation(s);
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
    if (env->ExceptionCheck()) env->ExceptionClear();
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
        if (entries.empty() || entries.back() != current) entries.push_back(current);
    }

    if (entries.size() > kMaximumHistoryEntries) {
        entries.erase(entries.begin(),
            entries.end() - static_cast<std::ptrdiff_t>(kMaximumHistoryEntries));
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

    s.versionHistory.clear();
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        const size_t separator = it->find('|');
        const size_t second = separator == std::string::npos
            ? std::string::npos : it->find('|', separator + 1);
        std::string display = separator == std::string::npos
            ? *it : it->substr(0, separator);
        if (second != std::string::npos) {
            display += "  CODE " + it->substr(separator + 1, second - separator - 1);
        }
        s.versionHistory.push_back(display);
    }
}

void detectBedrock(AppState& s) {
    s.installed = false;
    s.version = "NOT INSTALLED";
    s.versionCode = 0;
    s.lastUpdateTime = 0;

    if (!s.app || !s.app->activity) {
        s.status = "ANDROID ACTIVITY NOT READY";
        return;
    }

    bool attached = false;
    JNIEnv* env = getEnv(s.app->activity->vm, attached);
    if (!env) {
        s.status = "ANDROID PACKAGE API ERROR";
        return;
    }

    jobject activity = s.app->activity->clazz;
    jclass activityClass = env->GetObjectClass(activity);
    jmethodID getPackageManager = activityClass ? env->GetMethodID(
        activityClass, "getPackageManager",
        "()Landroid/content/pm/PackageManager;") : nullptr;
    jobject manager = getPackageManager
        ? env->CallObjectMethod(activity, getPackageManager) : nullptr;
    clearException(env);

    jclass managerClass = manager ? env->GetObjectClass(manager) : nullptr;
    jmethodID getPackageInfo = managerClass ? env->GetMethodID(
        managerClass, "getPackageInfo",
        "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;") : nullptr;
    jstring packageName = env->NewStringUTF(kBedrockPackage);
    jobject info = (manager && getPackageInfo)
        ? env->CallObjectMethod(manager, getPackageInfo, packageName, 0) : nullptr;

    if (env->ExceptionCheck()) {
        clearException(env);
        s.status = "MINECRAFT BEDROCK NOT FOUND";
    } else if (info) {
        jclass infoClass = env->GetObjectClass(info);
        jfieldID nameField = infoClass
            ? env->GetFieldID(infoClass, "versionName", "Ljava/lang/String;") : nullptr;
        jfieldID codeField = infoClass
            ? env->GetFieldID(infoClass, "versionCode", "I") : nullptr;
        jfieldID updateField = infoClass
            ? env->GetFieldID(infoClass, "lastUpdateTime", "J") : nullptr;
        jstring name = nameField
            ? static_cast<jstring>(env->GetObjectField(info, nameField)) : nullptr;

        if (name) {
            const char* chars = env->GetStringUTFChars(name, nullptr);
            if (chars) {
                s.version = chars;
                env->ReleaseStringUTFChars(name, chars);
            }
        }

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
        if (infoClass) env->DeleteLocalRef(infoClass);
        env->DeleteLocalRef(info);
    } else {
        s.status = "MINECRAFT BEDROCK NOT FOUND";
    }

    if (packageName) env->DeleteLocalRef(packageName);
    if (managerClass) env->DeleteLocalRef(managerClass);
    if (manager) env->DeleteLocalRef(manager);
    if (activityClass) env->DeleteLocalRef(activityClass);
    readVersionHistory(s);
    if (attached) s.app->activity->vm->DetachCurrentThread();
}

bool launchIntent(AppState& s, const char* action, const char* uri, const char* package) {
    if (!s.app || !s.app->activity) return false;
    bool attached = false;
    JNIEnv* env = getEnv(s.app->activity->vm, attached);
    if (!env) return false;

    jobject activity = s.app->activity->clazz;
    jclass activityClass = env->GetObjectClass(activity);
    jobject intent = nullptr;

    if (package) {
        jmethodID getPackageManager = activityClass ? env->GetMethodID(
            activityClass, "getPackageManager",
            "()Landroid/content/pm/PackageManager;") : nullptr;
        jobject manager = getPackageManager
            ? env->CallObjectMethod(activity, getPackageManager) : nullptr;
        clearException(env);

        if (manager) {
            jclass managerClass = env->GetObjectClass(manager);
            jmethodID getLaunchIntent = managerClass ? env->GetMethodID(
                managerClass, "getLaunchIntentForPackage",
                "(Ljava/lang/String;)Landroid/content/Intent;") : nullptr;
            jstring packageName = env->NewStringUTF(package);
            intent = getLaunchIntent
                ? env->CallObjectMethod(manager, getLaunchIntent, packageName) : nullptr;
            clearException(env);
            if (packageName) env->DeleteLocalRef(packageName);
            if (managerClass) env->DeleteLocalRef(managerClass);
            env->DeleteLocalRef(manager);
        }
    } else if (action && uri) {
        jclass uriClass = env->FindClass("android/net/Uri");
        jmethodID parse = uriClass ? env->GetStaticMethodID(
            uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;") : nullptr;
        jstring uriString = env->NewStringUTF(uri);
        jobject parsedUri = parse
            ? env->CallStaticObjectMethod(uriClass, parse, uriString) : nullptr;
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
    if (intent && activityClass) {
        jmethodID startActivity = env->GetMethodID(
            activityClass, "startActivity", "(Landroid/content/Intent;)V");
        if (startActivity) {
            env->CallVoidMethod(activity, startActivity, intent);
            launched = !env->ExceptionCheck();
            clearException(env);
        }
        env->DeleteLocalRef(intent);
    }
    if (activityClass) env->DeleteLocalRef(activityClass);
    if (attached) s.app->activity->vm->DetachCurrentThread();
    return launched;
}

void openPlayListing(AppState& s, bool betaManagement) {
    const char* marketUri =
        "market://details?id=com.mojang.minecraftpe";
    const char* webUri =
        "https://play.google.com/store/apps/details?id=com.mojang.minecraftpe";
    const char* action = "android.intent.action.VIEW";
    bool opened = false;
    if (betaManagement) {
        opened = launchIntent(s, action, marketUri, nullptr);
    } else {
        opened = launchIntent(s, action, webUri, nullptr);
    }
    if (!opened) opened = launchIntent(s, action, webUri, nullptr);
    s.status = opened
        ? (betaManagement ? "MANAGE BETA IN GOOGLE PLAY" : "OPENED GOOGLE PLAY")
        : "COULD NOT OPEN GOOGLE PLAY";
    startAnimation(s);
}

float animationProgress(const AppState& s) {
    if (!s.animationActive) return 1.0f;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - s.animationStart).count();
    const float linear = std::clamp(
        static_cast<float>(elapsed) / kAnimationMilliseconds, 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - linear, 3.0f);
}

float pageIndex(Page page) {
    return static_cast<float>(static_cast<int>(page));
}

void drawNavigation(std::vector<float>& vertices, const AppState& s,
                    float scale, float ox, float oy, int sw, int sh, float progress) {
    auto rect = [&](float x, float y, float w, float h, Color color, float radius) {
        addRoundedRect(vertices, ox + x * scale, oy + y * scale, w * scale, h * scale, 
                       radius * scale, color, sw, sh);
    };
    auto text = [&](const std::string& value, float x, float y, float pixel, Color color) {
        addText(vertices, value, ox + x * scale, oy + y * scale, 
                pixel * scale, color, sw, sh);
    };

    rect(10, 663, 340, 48, kPanel, 15);
    const float oldX = 18.0f + pageIndex(s.previousPage) * 112.0f;
    const float newX = 18.0f + pageIndex(s.page) * 112.0f;
    const float indicatorX = oldX + (newX - oldX) * progress;
    rect(indicatorX, 658, 100, 3, kAccent, 2);

    text("HOME", 43, 681, 1.45f, s.page == Page::Home ? kAccent : kMuted);
    text("VERSIONS", 139, 681, 1.35f, s.page == Page::Versions ? kAccent : kMuted);
    text("SETTINGS", 255, 681, 1.35f, s.page == Page::Settings ? kAccent : kMuted);
}

void draw(AppState& s) {
    if (s.display == EGL_NO_DISPLAY || s.surface == EGL_NO_SURFACE ||
        s.width <= 0 || s.height <= 0) return;

    std::vector<float> vertices;
    vertices.reserve(36000);
    const float scale = std::min(
        static_cast<float>(s.width) / kDesignWidth,
        static_cast<float>(s.height) / kDesignHeight);
    const float ox = (s.width - kDesignWidth * scale) * 0.5f;
    const float oy = (s.height - kDesignHeight * scale) * 0.5f;
    const float progress = animationProgress(s);
    const float slide = 14.0f * (1.0f - progress);

    auto rect = [&](float x, float y, float w, float h, Color color, float radius = 0.0f) {
        addRoundedRect(vertices, ox + x * scale, oy + y * scale,
                       w * scale, h * scale, radius * scale,
                       color, s.width, s.height);
    };
    auto text = [&](const std::string& value, float x, float y,
                    float pixel, Color color, bool moving = true) {
        const float offset = moving ? slide : 0.0f;
        addText(vertices, value, ox + x * scale, oy + (y + offset) * scale,
                pixel * scale, color, s.width, s.height);
    };

    rect(0, 0, 360, 720, kBackground);
    // Fine accent strips make the screen feel less like a debug utility.
    rect(18, 0, 72, 3, kAccent, 1);
    text("TSI BEDROCK", 20, 22, 2.8f, kAccent, false);
    text("MOBILE GAME HUB", 20, 48, 1.35f, kMuted, false);
    rect(270, 22, 72, 25, kPanelBright, 8);
    text("ANDROID", 280, 31, 1.15f, kWhite, false);

    if (s.page == Page::Home) {
        rect(18, 82, 324, 151, kPanel, 16);
        rect(18, 82, 5, 151, kAccent, 2);
        text("MINECRAFT BEDROCK", 34, 100, 1.55f, kMuted);
        text(s.installed ? "READY TO PLAY" : "GAME NOT FOUND",
             34, 128, 2.35f, s.installed ? kAccent : kWarning);
        text(("VERSION " + s.version).substr(0, 31), 34, 169, 1.55f, kWhite);
        text(("BUILD CODE " + std::to_string(s.versionCode)).substr(0, 31),
             34, 197, 1.25f, kMuted);

        rect(18, 250, 324, 56, kAccent, 14);
        text("PLAY MINECRAFT", 103, 270, 1.9f, kAccentDark);
        rect(18, 321, 156, 49, kPanelBright, 12);
        text("VERSION LIBRARY", 32, 340, 1.25f, kWhite);
        rect(186, 321, 156, 49, kPanelBright, 12);
        text("REFRESH APP", 211, 340, 1.25f, kWhite);
        rect(18, 385, 324, 48, kPanelBright, 12);
        text("OPEN GOOGLE PLAY", 105, 403, 1.4f, kWhite);

        rect(18, 448, 324, 164, kPanel, 16);
        text("ANDROID BETA CHANNEL", 32, 467, 1.5f, kAccent);
        text("BETA REPLACES THE STABLE APP", 32, 495, 1.3f, kWhite);
        text("GOOGLE PLAY CONTROLS ENROLMENT", 32, 518, 1.15f, kMuted);
        rect(30, 548, 300, 43, kAccent, 11);
        text("MANAGE BETA IN PLAY", 98, 562, 1.45f, kAccentDark);
        text(s.status.substr(0, 39), 20, 634, 1.15f, kMuted);
    } else if (s.page == Page::Versions) {
        text("VERSION LIBRARY", 20, 91, 2.1f, kWhite);
        text("LOCAL INSTALLATION TIMELINE", 20, 119, 1.2f, kMuted);

        rect(18, 143, 324, 75, kPanel, 14);
        text("CURRENTLY INSTALLED", 31, 157, 1.25f, kMuted);
        text(s.installed ? s.version.substr(0, 28) : "NOT INSTALLED",
             31, 181, 1.9f, s.installed ? kAccent : kWarning);
        text("RECENTLY OBSERVED", 20, 237, 1.35f, kAccent);

        const float rowY[] = {261, 326, 391, 456};
        for (size_t i = 0; i < 4; ++i) {
            rect(18, rowY[i], 324, 55, kPanel, 10);
            if (i < s.versionHistory.size()) {
                text(("0" + std::to_string(i + 1)).substr(0, 2),
                     31, rowY[i] + 18, 1.4f, kAccent);
                text(s.versionHistory[i].substr(0, 31),
                     61, rowY[i] + 20, 1.25f, kWhite);
            } else {
                text("--", 31, rowY[i] + 20, 1.3f, kMuted);
                text("NO SAVED VERSION", 61, rowY[i] + 20, 1.2f, kMuted);
            }
        }

        rect(18, 530, 324, 45, kPanelBright, 10);
        text("HISTORY ONLY - NOT AN INSTALLER", 30, 547, 1.15f, kWarning);
        rect(18, 587, 324, 43, kAccent, 11);
        text("REFRESH INSTALLED VERSION", 65, 601, 1.25f, kAccentDark);
        text(s.status.substr(0, 39), 20, 641, 1.05f, kMuted);
    } else {
        text("SETTINGS", 20, 91, 2.1f, kWhite);
        text("PLATFORM AND CHANNEL CONTROLS", 20, 119, 1.2f, kMuted);

        rect(18, 143, 324, 92, kPanel, 14);
        text("ANDROID PACKAGE MODEL", 31, 158, 1.3f, kAccent);
        text("ONE OFFICIAL GAME INSTALL", 31, 185, 1.35f, kWhite);
        text("STORE MANAGES GAME UPDATES", 31, 208, 1.2f, kMuted);

        rect(18, 251, 324, 48, kPanelBright, 12);
        text("OPEN GOOGLE PLAY", 105, 269, 1.4f, kWhite);
        rect(18, 313, 324, 48, kPanelBright, 12);
        text("MANAGE BETA IN PLAY", 95, 331, 1.4f, kWhite);

        rect(18, 382, 324, 201, kPanel, 14);
        text("COMPATIBILITY NOTES", 31, 400, 1.4f, kAccent);
        text("ANDROID DOES NOT RUN WINDOWS APPX", 31, 430, 1.05f, kWhite);
        text("OR MSIX GAME PACKAGES.", 31, 450, 1.05f, kWhite);
        text("THIS APP TRACKS VERSIONS IT HAS", 31, 484, 1.05f, kMuted);
        text("ACTUALLY SEEN. IT CANNOT RECREATE", 31, 504, 1.05f, kMuted);
        text("OLD BUILDS OR BYPASS PLAY LICENSING.", 31, 524, 1.05f, kMuted);
        text("BACK UP WORLDS BEFORE BETA CHANGES.", 31, 554, 1.0f, kWarning);
        text(s.status.substr(0, 39), 20, 641, 1.05f, kMuted);
    }

    drawNavigation(vertices, s, scale, ox, oy, s.width, s.height, progress);

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
                startAnimation(*s);
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
            startAnimation(*s);
            draw(*s);
            break;
        default:
            break;
    }
}

bool inside(float x, float y, float left, float top, float right, float bottom) {
    return x >= left && x <= right && y >= top && y <= bottom;
}

void handlePlay(AppState& s) {
    if (!s.installed) {
        s.status = "INSTALL OFFICIAL MINECRAFT FIRST";
    } else if (launchIntent(s, nullptr, nullptr, kBedrockPackage)) {
        s.status = "MINECRAFT LAUNCH REQUEST SENT";
    } else {
        s.status = "MINECRAFT LAUNCH FAILED";
    }
    startAnimation(s);
}

int32_t onInput(android_app* app, AInputEvent* event) {
    auto* s = static_cast<AppState*>(app->userData);
    if (!s || AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
    if (AMotionEvent_getAction(event) != AMOTION_EVENT_ACTION_UP) return 1;

    const float scale = std::min(
        static_cast<float>(s->width) / kDesignWidth,
        static_cast<float>(s->height) / kDesignHeight);
    if (scale <= 0) return 1;
    const float ox = (s->width - kDesignWidth * scale) * 0.5f;
    const float oy = (s->height - kDesignHeight * scale) * 0.5f;
    const float x = (AMotionEvent_getX(event, 0) - ox) / scale;
    const float y = (AMotionEvent_getY(event, 0) - oy) / scale;

    if (inside(x, y, 0, 663, 120, 720)) {
        setPage(*s, Page::Home);
    } else if (inside(x, y, 120, 663, 240, 720)) {
        setPage(*s, Page::Versions);
    } else if (inside(x, y, 240, 663, 360, 720)) {
        setPage(*s, Page::Settings);
    } else if (s->page == Page::Home) {
        if (inside(x, y, 18, 250, 342, 306)) {
            handlePlay(*s);
        } else if (inside(x, y, 18, 321, 174, 370)) {
            setPage(*s, Page::Versions);
        } else if (inside(x, y, 186, 321, 342, 370)) {
            detectBedrock(*s);
            s->status = s->installed
                ? "INSTALLED VERSION REFRESHED" : "MINECRAFT NOT INSTALLED";
            startAnimation(*s);
        } else if (inside(x, y, 18, 385, 342, 433)) {
            openPlayListing(*s, false);
        } else if (inside(x, y, 30, 548, 330, 591)) {
            openPlayListing(*s, true);
        }
    } else if (s->page == Page::Versions) {
        if (inside(x, y, 18, 261, 342, 511)) {
            s->status = "SAVED HISTORY IS READ ONLY";
            startAnimation(*s);
        } else if (inside(x, y, 18, 587, 342, 630)) {
            detectBedrock(*s);
            s->status = s->installed
                ? "INSTALLED VERSION REFRESHED" : "MINECRAFT NOT INSTALLED";
            startAnimation(*s);
        }
    } else if (s->page == Page::Settings) {
        if (inside(x, y, 18, 251, 342, 299)) {
            openPlayListing(*s, false);
        } else if (inside(x, y, 18, 313, 342, 361)) {
            openPlayListing(*s, true);
        }
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

    while (app->destroyRequested == 0) {
        int events = 0;
        android_poll_source* source = nullptr;
        const int timeout = state.animationActive ? 16 : -1;
        const int result = ALooper_pollOnce(
            timeout, nullptr, &events, reinterpret_cast<void**>(&source));
        if (result >= 0 && source) source->process(app, source);

        if (state.animationActive) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now() - state.animationStart).count();
            if (elapsed >= kAnimationMilliseconds) {
                state.animationActive = false;
                draw(state);
            } else {
                draw(state);
            }
        }
    }

    destroyGraphics(state);
}
