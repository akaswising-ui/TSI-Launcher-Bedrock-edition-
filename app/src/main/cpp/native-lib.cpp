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
constexpr int kDesignWidth = 960;
constexpr int kDesignHeight = 540;
constexpr int kAnimationMilliseconds = 380;
using Clock = std::chrono::steady_clock;

enum class Page { Home = 0, Versions = 1, Mods = 2, Servers = 3, News = 4, Settings = 5 };
enum class TouchTarget {
    None,
    NavHome, NavVersions, NavMods, NavServers, NavNews, NavSettings,
    HomePlay, HeaderPreview, HeaderRelease, SearchVersions,
    FilterStable, FilterPreview, FilterBeta, FilterAll,
    Profile, QuickSettings,
    HomeCard0, HomeCard1, HomeCard2, HomeCard3,
    HomeCard4, HomeCard5, HomeCard6, HomeCard7,
    VersionRow,
    ModsMarketplace, ModsCommunity, ModsGuide,
    ServersOfficial, ServersGuide,
    NewsOfficial, NewsHelp,
    SettingsStore, SettingsBeta
};

struct TouchBounds {
    float x;
    float y;
    float width;
    float height;
    float radius;
};

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
    int activeFilter = 3; // 0 stable, 1 preview, 2 beta, 3 all
    Page page = Page::Home;
    Page previousPage = Page::Home;
    bool animationActive = false;
    Clock::time_point animationStart = Clock::now();

    // Direct-manipulation feedback for every interactive surface.
    bool touchDown = false;
    TouchTarget downTarget = TouchTarget::None;
    TouchTarget pressedTarget = TouchTarget::None;
    float touchX = 0.0f;
    float touchY = 0.0f;
    Clock::time_point pressStart = Clock::now();
    bool rippleActive = false;
    TouchTarget rippleTarget = TouchTarget::None;
    bool releaseScaleActive = false;
    TouchTarget releaseTarget = TouchTarget::None;
    Clock::time_point releaseScaleStart = Clock::now();
    float rippleX = 0.0f;
    float rippleY = 0.0f;
    Clock::time_point rippleStart = Clock::now();
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


void openWebsite(AppState& s, const char* url, const char* successMessage) {
    const bool opened = launchIntent(
        s, "android.intent.action.VIEW", url, nullptr);
    s.status = opened ? successMessage : "COULD NOT OPEN WEBSITE";
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
                    float scale, float ox, float oy, int sw, int sh,
                    float progress, float canvasW) {
    auto rect = [&](float x, float y, float w, float h, Color color, float radius = 0.0f) {
        addRoundedRect(vertices, ox + x * scale, oy + y * scale,
                       w * scale, h * scale, radius * scale, color, sw, sh);
    };
    auto text = [&](const std::string& value, float x, float y, float pixel, Color color) {
        addText(vertices, value, ox + x * scale, oy + y * scale,
                pixel * scale, color, sw, sh);
    };

    const Color sidebar{0.055f, 0.067f, 0.082f};
    const Color navIdle{0.13f, 0.15f, 0.18f};
    rect(0, 0, 148, 540, sidebar);
    rect(0, 0, 148, 3, kAccent);
    rect(18, 22, 28, 28, Color{0.22f, 0.47f, 0.30f}, 4);
    rect(22, 18, 20, 12, Color{0.34f, 0.62f, 0.38f}, 2);
    rect(22, 34, 20, 12, Color{0.31f, 0.25f, 0.18f}, 2);
    text("MINECRAFT", 54, 23, 1.55f, kWhite);
    text("BEDROCK EDITION", 54, 40, 0.9f, kMuted);

    const float navY[] = {104, 145, 186, 227, 268, 309};
    const std::string labels[] = {
        "LAUNCH", "ALL VERSIONS", "MODS", "SERVERS", "NEWS", "SETTINGS"
    };
    const Page pages[] = {
        Page::Home, Page::Versions, Page::Mods,
        Page::Servers, Page::News, Page::Settings
    };
    const float oldY = navY[static_cast<int>(s.previousPage)];
    const float newY = navY[static_cast<int>(s.page)];
    const float indicatorY = oldY + (newY - oldY) * progress;
    rect(5, indicatorY - 4, 3, 34, kAccent, 1);
    for (int i = 0; i < 6; ++i) {
        const bool selected = s.page == pages[i];
        if (selected) rect(12, navY[i] - 7, 126, 32, navIdle, 6);
        text(labels[i], 25, navY[i] + 3, 1.45f,
             selected ? kAccent : kMuted);
    }

    rect(10, 440, 128, 1, kLine);
    rect(12, 460, 124, 58, navIdle, 7);
    rect(20, 471, 26, 26, Color{0.26f, 0.34f, 0.43f}, 4);
    text("P", 29, 480, 1.8f, kWhite);
    text("PROFILE", 55, 473, 1.05f, kMuted);
    text("PLAYER", 55, 489, 1.2f, kWhite);
    text("V0.5.0", 55, 504, 0.95f, kAccent);

    // Desktop-style window chrome and content divider.
    rect(148, 0, 2, 540, kLine);
    rect(150, 0, canvasW - 150, 31, Color{0.065f, 0.075f, 0.09f});
    text("TSI BEDROCK LAUNCHER", 166, 12, 1.05f, kMuted);
    const float controlsX = canvasW - 91;
    rect(controlsX, 0, 1, 31, kLine);
    text("_", controlsX + 12, 9, 1.2f, kMuted);
    rect(controlsX + 31, 8, 9, 9, kLine);
    rect(controlsX + 57, 8, 9, 9, kLine);
    text("X", controlsX + 78, 8, 1.2f, kWhite);
}
Color mixColor(Color a, Color b, float amount) {
    const float t = std::clamp(amount, 0.0f, 1.0f);
    return {
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t
    };
}

void addRing(std::vector<float>& vertices, float cx, float cy,
             float outerRadius, float thickness, Color color, int sw, int sh) {
    constexpr int kSegments = 40;
    const float pi = std::acos(-1.0f);
    const float innerRadius = std::max(0.5f, outerRadius - thickness);
    for (int i = 0; i < kSegments; ++i) {
        const float a1 = (2.0f * pi * i) / kSegments;
        const float a2 = (2.0f * pi * (i + 1)) / kSegments;
        const float co1 = std::cos(a1);
        const float si1 = std::sin(a1);
        const float co2 = std::cos(a2);
        const float si2 = std::sin(a2);
        const float xOuter1 = cx + co1 * outerRadius;
        const float yOuter1 = cy + si1 * outerRadius;
        const float xOuter2 = cx + co2 * outerRadius;
        const float yOuter2 = cy + si2 * outerRadius;
        const float xInner1 = cx + co1 * innerRadius;
        const float yInner1 = cy + si1 * innerRadius;
        const float xInner2 = cx + co2 * innerRadius;
        const float yInner2 = cy + si2 * innerRadius;
        addVertex(vertices, xOuter1, yOuter1, color, sw, sh);
        addVertex(vertices, xOuter2, yOuter2, color, sw, sh);
        addVertex(vertices, xInner1, yInner1, color, sw, sh);
        addVertex(vertices, xInner1, yInner1, color, sw, sh);
        addVertex(vertices, xOuter2, yOuter2, color, sw, sh);
        addVertex(vertices, xInner2, yInner2, color, sw, sh);
    }
}

void addRoundedOutline(std::vector<float>& vertices, float x, float y,
                       float width, float height, float radius,
                       float thickness, Color color, int sw, int sh) {
    const float t = std::max(1.0f, thickness);
    const float r = std::max(2.0f, radius);
    addRoundedRect(vertices, x + r, y, width - 2.0f * r, t, t * 0.5f,
                   color, sw, sh);
    addRoundedRect(vertices, x + r, y + height - t, width - 2.0f * r, t,
                   t * 0.5f, color, sw, sh);
    addRect(vertices, x, y + r, t, std::max(0.0f, height - 2.0f * r),
            color, sw, sh);
    addRect(vertices, x + width - t, y + r, t,
            std::max(0.0f, height - 2.0f * r), color, sw, sh);
}

TouchBounds boundsForTarget(const AppState& s, TouchTarget target) {
    const float scale = s.height > 0
        ? static_cast<float>(s.height) / kDesignHeight : 1.0f;
    const float canvasW = std::max(
        static_cast<float>(kDesignWidth),
        s.height > 0 ? static_cast<float>(s.width) / scale
                     : static_cast<float>(kDesignWidth));
    const float contentW = canvasW - 176.0f;
    const float gap = 10.0f;
    const float colW = (contentW - gap) * 0.5f;
    switch (target) {
        case TouchTarget::NavHome: return {5, 96, 138, 38, 5};
        case TouchTarget::NavVersions: return {5, 137, 138, 38, 5};
        case TouchTarget::NavMods: return {5, 178, 138, 38, 5};
        case TouchTarget::NavServers: return {5, 219, 138, 38, 5};
        case TouchTarget::NavNews: return {5, 260, 138, 38, 5};
        case TouchTarget::NavSettings: return {5, 301, 138, 38, 5};
        case TouchTarget::HomePlay: return {canvasW * 0.5f - 78, 103, 156, 46, 8};
        case TouchTarget::HeaderPreview: return {contentW * 0.32f + 160, 185, 180, 34, 5};
        case TouchTarget::HeaderRelease: return {contentW * 0.55f + 160, 185, 180, 34, 5};
        case TouchTarget::SearchVersions: return {160, 472, 184, 35, 6};
        case TouchTarget::FilterStable: return {350, 472, 63, 35, 6};
        case TouchTarget::FilterPreview: return {418, 472, 76, 35, 6};
        case TouchTarget::FilterBeta: return {499, 472, 60, 35, 6};
        case TouchTarget::FilterAll: return {564, 472, 44, 35, 6};
        case TouchTarget::Profile: return {canvasW - 203, 472, 86, 35, 6};
        case TouchTarget::QuickSettings: return {canvasW - 110, 472, 35, 35, 6};
        case TouchTarget::HomeCard0: case TouchTarget::HomeCard1:
        case TouchTarget::HomeCard2: case TouchTarget::HomeCard3:
        case TouchTarget::HomeCard4: case TouchTarget::HomeCard5:
        case TouchTarget::HomeCard6: case TouchTarget::HomeCard7: {
            const int i = static_cast<int>(target) - static_cast<int>(TouchTarget::HomeCard0);
            const int col = i / 4;
            const int row = i % 4;
            return {160.0f + col * (colW + gap), 250.0f + row * 51.0f,
                    colW, 45.0f, 7.0f};
        }
        case TouchTarget::VersionRow: return {160, 246, contentW, 240, 7};
        case TouchTarget::ModsMarketplace: return {170, 138, 370, 106, 8};
        case TouchTarget::ModsCommunity: return {550, 138, contentW - 390, 106, 8};
        case TouchTarget::ModsGuide: return {170, 260, contentW - 20, 106, 8};
        case TouchTarget::ServersOfficial: return {170, 138, 370, 106, 8};
        case TouchTarget::ServersGuide: return {550, 138, contentW - 390, 106, 8};
        case TouchTarget::NewsOfficial: return {170, 138, 370, 106, 8};
        case TouchTarget::NewsHelp: return {550, 138, contentW - 390, 106, 8};
        case TouchTarget::SettingsStore: return {170, 226, 300, 50, 8};
        case TouchTarget::SettingsBeta: return {485, 226, 300, 50, 8};
        case TouchTarget::None: break;
    }
    return {0, 0, 0, 0, 0};
}

bool getTouchScale(const AppState& s, TouchBounds& bounds, float& factor) {
    if (s.touchDown && s.pressedTarget != TouchTarget::None) {
        bounds = boundsForTarget(s, s.pressedTarget);
        const float elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - s.pressStart).count();
        const float t = std::clamp(elapsed / 90.0f, 0.0f, 1.0f);
        const float eased = 1.0f - std::pow(1.0f - t, 2.0f);
        factor = 1.0f - 0.045f * eased;
        return true;
    }
    if (s.releaseScaleActive && s.releaseTarget != TouchTarget::None) {
        bounds = boundsForTarget(s, s.releaseTarget);
        const float elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - s.releaseScaleStart).count();
        const float t = std::clamp(elapsed / 190.0f, 0.0f, 1.0f);
        constexpr float c1 = 1.70158f;
        constexpr float c3 = c1 + 1.0f;
        const float spring = 1.0f + c3 * std::pow(t - 1.0f, 3.0f) +
                             c1 * std::pow(t - 1.0f, 2.0f);
        factor = 0.955f + 0.045f * spring;
        return true;
    }
    return false;
}

void drawTouchFeedback(std::vector<float>& vertices, const AppState& s,
                       float scale, float ox, float oy, int sw, int sh) {
    if (s.touchDown && s.pressedTarget != TouchTarget::None) {
        const TouchBounds b = boundsForTarget(s, s.pressedTarget);
        const float elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - s.pressStart).count();
        const float press = std::clamp(elapsed / 95.0f, 0.0f, 1.0f);
        const float eased = 1.0f - std::pow(1.0f - press, 2.0f);
        const float inset = 1.5f + 1.2f * eased;
        const Color pressColor = mixColor(kAccent, kWhite, 0.16f * eased);
        addRoundedOutline(vertices,
            ox + (b.x + inset) * scale, oy + (b.y + inset) * scale,
            (b.width - 2.0f * inset) * scale,
            (b.height - 2.0f * inset) * scale,
            std::max(3.0f, b.radius - inset) * scale,
            (1.2f + eased * 0.8f) * scale, pressColor, sw, sh);
        addRing(vertices, ox + s.touchX * scale, oy + s.touchY * scale,
                (4.0f + 8.0f * eased) * scale, 1.6f * scale,
                mixColor(kAccent, kPanelBright, 0.22f), sw, sh);
    }

    if (s.rippleActive && s.rippleTarget != TouchTarget::None) {
        const float elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - s.rippleStart).count();
        const float linear = std::clamp(elapsed / 330.0f, 0.0f, 1.0f);
        const float eased = 1.0f - std::pow(1.0f - linear, 3.0f);
        const float radius = (3.0f + 34.0f * eased) * scale;
        const float thickness = (2.4f - 1.1f * linear) * scale;
        const Color rippleColor = mixColor(kAccent, kPanelBright, linear * 0.88f);
        addRing(vertices, ox + s.rippleX * scale, oy + s.rippleY * scale,
                radius, thickness, rippleColor, sw, sh);
    }
}

void draw(AppState& s) {
    if (s.display == EGL_NO_DISPLAY || s.surface == EGL_NO_SURFACE ||
        s.width <= 0 || s.height <= 0) return;

    std::vector<float> vertices;
    vertices.reserve(90000);
    const float scale = static_cast<float>(s.height) / kDesignHeight;
    if (scale <= 0.0f) return;
    const float canvasW = std::max(
        static_cast<float>(kDesignWidth), static_cast<float>(s.width) / scale);
    const float ox = 0.0f;
    const float oy = 0.0f;
    const float progress = animationProgress(s);
    const float slide = 9.0f * (1.0f - progress);

    const Color contentBg{0.105f, 0.115f, 0.13f};
    const Color cardBg{0.145f, 0.155f, 0.17f};
    const Color cardEdge{0.205f, 0.22f, 0.235f};
    const Color green{0.06f, 0.62f, 0.25f};
    const Color greenBright{0.10f, 0.78f, 0.34f};
    const Color blue{0.12f, 0.42f, 0.74f};

    auto rect = [&](float x, float y, float w, float h, Color color, float radius = 0.0f) {
        float drawX = x;
        float drawY = y + slide;
        float drawW = w;
        float drawH = h;
        TouchBounds touchBounds{};
        float touchScale = 1.0f;
        if (getTouchScale(s, touchBounds, touchScale) &&
            x >= touchBounds.x - 0.5f && y >= touchBounds.y - 0.5f &&
            x + w <= touchBounds.x + touchBounds.width + 0.5f &&
            y + h <= touchBounds.y + touchBounds.height + 0.5f) {
            const float cx = touchBounds.x + touchBounds.width * 0.5f;
            const float cy = touchBounds.y + touchBounds.height * 0.5f;
            drawX = cx + (x - cx) * touchScale;
            drawY = cy + (y - cy) * touchScale + slide;
            drawW = w * touchScale;
            drawH = h * touchScale;
        }
        addRoundedRect(vertices, ox + drawX * scale, oy + drawY * scale,
                       drawW * scale, drawH * scale, radius * scale,
                       color, s.width, s.height);
    };
    auto text = [&](const std::string& value, float x, float y,
                    float pixel, Color color, bool moving = true) {
        const float offset = moving ? slide : 0.0f;
        float drawX = x;
        float drawY = y + offset;
        float drawPixel = pixel;
        TouchBounds touchBounds{};
        float touchScale = 1.0f;
        if (getTouchScale(s, touchBounds, touchScale) &&
            x >= touchBounds.x - 2.0f && x <= touchBounds.x + touchBounds.width &&
            y >= touchBounds.y - 2.0f && y <= touchBounds.y + touchBounds.height) {
            const float cx = touchBounds.x + touchBounds.width * 0.5f;
            const float cy = touchBounds.y + touchBounds.height * 0.5f;
            drawX = cx + (x - cx) * touchScale;
            drawY = cy + (y - cy) * touchScale + offset;
            drawPixel = pixel * touchScale;
        }
        addText(vertices, value, ox + drawX * scale, oy + drawY * scale,
                drawPixel * scale, color, s.width, s.height);
    };

    rect(0, 0, canvasW, 540, contentBg);
    rect(150, 31, canvasW - 150, 509, contentBg);
    rect(150, 31, canvasW - 150, 2, kAccent);
    text("BEDROCK EDITION", 166, 49, 1.0f, kMuted);

    if (s.page == Page::Home) {
        const float hx = 160.0f;
        const float hy = 38.0f;
        const float hw = canvasW - 176.0f;
        const float hh = 145.0f;

        // Original block-built sunset landscape, used instead of a flat placeholder.
        rect(hx, hy, hw, hh, Color{0.28f, 0.34f, 0.47f}, 7);
        rect(hx, hy + 2, hw, 37, Color{0.47f, 0.39f, 0.48f});
        rect(hx, hy + 39, hw, 27, Color{0.67f, 0.39f, 0.34f});
        rect(hx, hy + 66, hw, 25, Color{0.29f, 0.37f, 0.39f});
        rect(hx + hw * 0.72f, hy + 13, 38, 28, Color{0.94f, 0.60f, 0.31f}, 15);
        // Distant block hills.
        rect(hx + 8, hy + 70, hw * 0.21f, 36, Color{0.10f, 0.29f, 0.20f});
        rect(hx + 31, hy + 55, hw * 0.17f, 50, Color{0.13f, 0.34f, 0.22f});
        rect(hx + 72, hy + 44, hw * 0.13f, 61, Color{0.16f, 0.39f, 0.24f});
        rect(hx + hw * 0.48f, hy + 62, hw * 0.18f, 43, Color{0.12f, 0.31f, 0.22f});
        rect(hx + hw * 0.66f, hy + 50, hw * 0.18f, 55, Color{0.11f, 0.30f, 0.22f});
        rect(hx + hw * 0.82f, hy + 69, hw * 0.17f, 36, Color{0.15f, 0.36f, 0.24f});
        // Grass ledges, water and a block waterfall.
        rect(hx, hy + 103, hw, 12, Color{0.14f, 0.34f, 0.19f});
        rect(hx, hy + 115, hw, 30, Color{0.08f, 0.23f, 0.31f});
        rect(hx + hw * 0.12f, hy + 112, hw * 0.16f, 8, Color{0.20f, 0.48f, 0.28f});
        rect(hx + hw * 0.34f, hy + 107, hw * 0.12f, 8, Color{0.18f, 0.43f, 0.26f});
        rect(hx + hw * 0.78f, hy + 78, 18, 67, Color{0.14f, 0.40f, 0.53f});
        rect(hx + hw * 0.78f - 8, hy + 73, 34, 8, Color{0.20f, 0.48f, 0.59f});
        // Block trees.
        for (int i = 0; i < 5; ++i) {
            const float tx = hx + 58 + i * 22.0f;
            rect(tx + 8, hy + 80, 7, 25, Color{0.28f, 0.18f, 0.12f});
            rect(tx, hy + 63, 24, 18, Color{0.12f, 0.31f, 0.18f});
            rect(tx + 4, hy + 53, 16, 13, Color{0.16f, 0.39f, 0.22f});
        }
        for (int i = 0; i < 5; ++i) {
            const float tx = hx + hw - 150 + i * 20.0f;
            rect(tx + 7, hy + 82, 7, 23, Color{0.27f, 0.17f, 0.11f});
            rect(tx, hy + 65, 22, 18, Color{0.13f, 0.33f, 0.19f});
            rect(tx + 4, hy + 54, 14, 14, Color{0.17f, 0.40f, 0.23f});
        }
        // Darkened text plate improves contrast over the landscape.
        rect(hx + hw * 0.31f, hy + 4, hw * 0.40f, 65,
             Color{0.055f, 0.065f, 0.075f}, 5);
        text("MINECRAFT", hx + hw * 0.385f, hy + 12, 3.2f, kWhite);
        text("BEDROCK EDITION", hx + hw * 0.43f, hy + 42, 1.45f, kWhite);
        rect(hx + hw * 0.39f, hy + 83, 145, 44, green, 5);
        rect(hx + hw * 0.39f, hy + 83, 145, 4, greenBright, 2);
        text("PLAY", hx + hw * 0.445f, hy + 94, 2.7f, kWhite);
        text(s.installed ? s.version.substr(0, 18) : "NOT INSTALLED",
             hx + hw * 0.418f, hy + 116, 0.9f, kWhite);
        rect(hx + hw * 0.39f + 148, hy + 83, 40, 44, blue, 5);
        rect(hx + hw * 0.39f + 159, hy + 92, 18, 18,
             Color{0.70f, 0.59f, 0.30f}, 2);
        rect(hx + hw * 0.39f + 162, hy + 89, 12, 6,
             Color{0.22f, 0.48f, 0.28f}, 1);

        // Preview / release tabs under the feature banner.
        rect(hx, 187, hw, 34, Color{0.085f, 0.095f, 0.11f});
        rect(hx + hw * 0.29f, 188, hw * 0.22f, 32, cardBg, 3);
        rect(hx + hw * 0.54f, 188, hw * 0.22f, 32, cardBg, 3);
        text("LATEST PREVIEW", hx + hw * 0.32f, 194, 1.15f, kWhite);
        text("LATEST RELEASE", hx + hw * 0.57f, 194, 1.15f, kWhite);
        text("OPEN BETA CHANNEL", hx + hw * 0.33f, 208, 0.78f, kMuted);
        text("OPEN GOOGLE PLAY", hx + hw * 0.59f, 208, 0.78f, kMuted);

        text("ALL AVAILABLE VERSIONS", hx, 229, 1.7f, kWhite);
        text("LOCAL INSTALL HISTORY + OFFICIAL CHANNELS",
             hx + 260, 231, 0.92f, kMuted);

        const float contentW = canvasW - 176.0f;
        const float gap = 10.0f;
        const float colW = (contentW - gap) * 0.5f;
        std::vector<std::string> history;
        for (const auto& item : s.versionHistory) {
            std::string lower = item;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            const bool preview = lower.find("preview") != std::string::npos;
            const bool beta = lower.find("beta") != std::string::npos;
            if (s.activeFilter == 0 && (preview || beta)) continue;
            if (s.activeFilter == 1 && !preview) continue;
            if (s.activeFilter == 2 && !beta) continue;
            history.push_back(item);
            if (history.size() >= 4) break;
        }

        const std::string labels[8] = {
            history.size() > 0 ? history[0] : "NO SAVED VERSION HISTORY",
            history.size() > 1 ? history[1] : "LATEST STABLE CHANNEL",
            history.size() > 2 ? history[2] : "PREVIEW CHANNEL",
            history.size() > 3 ? history[3] : "ANDROID VERSION LIMITS",
            "LATEST RELEASE - GOOGLE PLAY",
            "PREVIEW PROGRAMME",
            "OFFICIAL BEDROCK SERVERS",
            "MINECRAFT NEWS"
        };
        const std::string subtitles[8] = {
            "MOST RECENT VERSION OBSERVED ON THIS DEVICE",
            "LOCAL HISTORY ONLY - NOT AN INSTALLER",
            "BETA ENROLMENT IS MANAGED BY GOOGLE PLAY",
            "OLDER WINDOWS APPX FILES CANNOT RUN ON ANDROID",
            "OFFICIAL GAME INSTALL AND UPDATE PAGE",
            "JOIN OR MANAGE THE OFFICIAL PREVIEW CHANNEL",
            "COMMUNITY SERVERS AND HOW TO JOIN",
            "OFFICIAL GAME UPDATES AND ANNOUNCEMENTS"
        };
        const char* buttons[8] = {"VIEW", "VIEW", "VIEW", "VIEW", "STORE", "BETA", "OPEN", "OPEN"};
        for (int i = 0; i < 8; ++i) {
            const int col = i / 4;
            const int row = i % 4;
            const float x = hx + col * (colW + gap);
            const float y = 250.0f + row * 51.0f;
            rect(x, y, colW, 44, cardBg, 6);
            rect(x, y, 3, 44, i < 4 ? kAccent : cardEdge, 1);
            rect(x + 10, y + 8, 28, 28,
                 i % 2 == 0 ? Color{0.26f, 0.48f, 0.28f}
                            : Color{0.25f, 0.36f, 0.48f}, 3);
            rect(x + 10, y + 8, 28, 8, Color{0.38f, 0.60f, 0.32f}, 2);
            rect(x + 10, y + 16, 28, 20, Color{0.36f, 0.25f, 0.16f}, 2);
            text(labels[i].substr(0, 28), x + 48, y + 10, 1.08f, kWhite);
            text(subtitles[i].substr(0, 38), x + 48, y + 27, 0.72f, kMuted);
            const float bw = 66.0f;
            rect(x + colW - bw - 7, y + 9, bw, 26,
                 i == 4 || i == 5 ? green : cardEdge, 4);
            text(buttons[i], x + colW - bw + 9, y + 17, 0.95f, kWhite);
        }

        // Search, channel filters, account, quick settings and app state.
        rect(160, 472, 184, 34, Color{0.055f, 0.065f, 0.078f}, 5);
        text("SEARCH VERSIONS", 176, 484, 1.0f, kMuted);
        const char* filters[] = {"STABLE", "PREVIEW", "BETA", "ALL"};
        const float fx[] = {350, 418, 499, 564};
        const float fw[] = {63, 76, 60, 44};
        for (int i = 0; i < 4; ++i) {
            rect(fx[i], 472, fw[i], 34,
                 s.activeFilter == i ? green : cardBg, 5);
            text(filters[i], fx[i] + (i == 1 ? 8 : 7), 484, 0.85f,
                 s.activeFilter == i ? kWhite : kMuted);
        }
        rect(618, 472, 87, 34, cardBg, 5);
        text("LOCAL", 628, 482, 0.8f, kMuted);
        text(std::to_string(s.versionHistory.size()) + "/20",
             628, 495, 0.9f, kWhite);
        rect(canvasW - 203, 472, 86, 34, cardBg, 5);
        text("PROFILE", canvasW - 188, 484, 0.95f, kWhite);
        rect(canvasW - 110, 472, 35, 34, cardBg, 5);
        text("*", canvasW - 98, 483, 1.5f, kWhite);
        text("V0.5.0", canvasW - 68, 482, 0.88f, kMuted);
        text(s.installed ? "GAME READY" : "GAME NOT FOUND",
             canvasW - 68, 495, 0.72f, s.installed ? kAccent : kWarning);
    } else if (s.page == Page::Versions) {
        text("ALL VERSIONS", 170, 77, 2.1f, kWhite);
        text("ONLY VERSIONS ACTUALLY OBSERVED ON THIS DEVICE ARE LISTED",
             170, 101, 1.0f, kMuted);
        rect(170, 116, canvasW - 192, 52, cardBg, 7);
        text("CURRENTLY INSTALLED", 184, 129, 1.0f, kMuted);
        text(s.installed ? s.version.substr(0, 32) : "MINECRAFT NOT INSTALLED",
             184, 147, 1.4f, s.installed ? kAccent : kWarning);
        text("LOCAL INSTALLATION HISTORY", 170, 189, 1.2f, kAccent);
        for (int i = 0; i < 8; ++i) {
            const int col = i / 4;
            const int row = i % 4;
            const float contentW = canvasW - 192.0f;
            const float gap = 10.0f;
            const float colW = (contentW - gap) * 0.5f;
            const float x = 170 + col * (colW + gap);
            const float y = 207 + row * 57;
            rect(x, y, colW, 49, cardBg, 6);
            if (static_cast<size_t>(i) < s.versionHistory.size()) {
                text(("0" + std::to_string(i + 1)).substr(0, 2),
                     x + 12, y + 17, 1.2f, kAccent);
                text(s.versionHistory[i].substr(0, 43),
                     x + 42, y + 18, 1.0f, kWhite);
            } else {
                text("--", x + 12, y + 17, 1.2f, kMuted);
                text("NO SAVED VERSION", x + 42, y + 18, 1.0f, kMuted);
            }
        }
        text("HISTORY RECORDS DO NOT CONTAIN INSTALLABLE APK FILES",
             170, 452, 0.95f, kWarning);
        text(s.status.substr(0, 80), 170, 478, 0.9f, kMuted);
    } else if (s.page == Page::Mods) {
        text("MODS AND ADD-ONS", 170, 77, 2.1f, kWhite);
        text("BEDROCK CONTENT USES ADD-ONS, RESOURCE PACKS AND MARKETPLACE CONTENT",
             170, 101, 0.95f, kMuted);
        rect(170, 138, 370, 106, cardBg, 8);
        rect(550, 138, canvasW - 720, 106, cardBg, 8);
        rect(170, 260, canvasW - 192, 106, cardBg, 8);
        text("MINECRAFT MARKETPLACE", 190, 164, 1.45f, kWhite);
        text("OFFICIAL BEDROCK ADD-ONS AND CONTENT", 190, 190, 0.9f, kMuted);
        rect(190, 207, 116, 25, green, 4);
        text("OPEN STORE", 205, 215, 0.9f, kWhite);
        text("COMMUNITY ADD-ONS", 570, 164, 1.45f, kWhite);
        text("THIRD-PARTY COMMUNITY RESOURCE", 570, 190, 0.9f, kMuted);
        rect(570, 207, 116, 25, blue, 4);
        text("OPEN MCPEDL", 582, 215, 0.9f, kWhite);
        text("BEDROCK ADD-ON GUIDE", 190, 286, 1.45f, kWhite);
        text("RESOURCE PACKS AND BEHAVIOUR PACKS", 190, 312, 0.95f, kMuted);
        rect(190, 329, 150, 25, cardEdge, 4);
        text("READ OFFICIAL HELP", 199, 337, 0.8f, kWhite);
        text("NOTE: JAVA MODS ARE NOT BEDROCK ADD-ONS",
             170, 402, 1.0f, kWarning);
    } else if (s.page == Page::Servers) {
        text("BEDROCK SERVERS", 170, 77, 2.1f, kWhite);
        text("BROWSE THE OFFICIAL SERVER LIST OR READ THE JOINING GUIDE",
             170, 101, 1.0f, kMuted);
        rect(170, 138, 370, 106, cardBg, 8);
        rect(550, 138, canvasW - 720, 106, cardBg, 8);
        text("OFFICIAL SERVER LIST", 190, 164, 1.5f, kWhite);
        text("DISCOVER PARTNERED COMMUNITY SERVERS", 190, 190, 0.9f, kMuted);
        rect(190, 207, 120, 25, green, 4);
        text("BROWSE SERVERS", 199, 215, 0.85f, kWhite);
        text("HOW TO JOIN", 570, 164, 1.5f, kWhite);
        text("ADD A SERVER ADDRESS AND PORT IN GAME", 570, 190, 0.9f, kMuted);
        rect(570, 207, 120, 25, blue, 4);
        text("JOINING GUIDE", 581, 215, 0.85f, kWhite);
        rect(170, 260, canvasW - 192, 92, cardBg, 8);
        text("SERVER CONNECTION", 190, 285, 1.35f, kAccent);
        text("THE LAUNCHER DOES NOT MODIFY OR INJECT SERVER ADDRESSES",
             190, 313, 0.95f, kMuted);
    } else if (s.page == Page::News) {
        text("MINECRAFT NEWS", 170, 77, 2.1f, kWhite);
        text("OFFICIAL UPDATES, RELEASE NOTES AND HELP ARTICLES",
             170, 101, 1.0f, kMuted);
        rect(170, 138, 370, 106, cardBg, 8);
        rect(550, 138, canvasW - 720, 106, cardBg, 8);
        text("LATEST MINECRAFT NEWS", 190, 164, 1.45f, kWhite);
        text("OFFICIAL ANNOUNCEMENTS AND GAME UPDATES", 190, 190, 0.88f, kMuted);
        rect(190, 207, 112, 25, green, 4);
        text("READ NEWS", 205, 215, 0.9f, kWhite);
        text("HELP AND SUPPORT", 570, 164, 1.45f, kWhite);
        text("OFFICIAL TROUBLESHOOTING AND GUIDES", 570, 190, 0.9f, kMuted);
        rect(570, 207, 112, 25, blue, 4);
        text("OPEN HELP", 588, 215, 0.9f, kWhite);
    } else {
        text("SETTINGS", 170, 77, 2.1f, kWhite);
        text("GAME PACKAGE, STORE AND BETA CONTROLS",
             170, 101, 1.0f, kMuted);
        rect(170, 126, canvasW - 192, 82, cardBg, 8);
        text("ANDROID GAME PACKAGE", 190, 148, 1.2f, kAccent);
        text(s.installed ? s.version.substr(0, 36) : "MINECRAFT BEDROCK NOT INSTALLED",
             190, 176, 1.4f, s.installed ? kWhite : kWarning);
        rect(170, 226, 300, 50, green, 6);
        rect(485, 226, 300, 50, blue, 6);
        text("OPEN GOOGLE PLAY", 225, 245, 1.2f, kWhite);
        text("MANAGE BETA", 570, 245, 1.2f, kWhite);
        rect(170, 300, canvasW - 192, 116, cardBg, 8);
        text("PLATFORM LIMITS", 190, 322, 1.2f, kAccent);
        text("ANDROID CANNOT INSTALL WINDOWS APPX OR MSIX GAME PACKAGES.",
             190, 349, 0.92f, kWhite);
        text("THIS APP RECORDS LOCAL VERSIONS; IT DOES NOT STORE OLD GAME APKs.",
             190, 375, 0.88f, kMuted);
        text(s.status.substr(0, 85), 170, 452, 0.9f, kMuted);
    }

    drawNavigation(vertices, s, scale, ox, oy, s.width, s.height, progress, canvasW);
    drawTouchFeedback(vertices, s, scale, ox, oy, s.width, s.height);

    glViewport(0, 0, s.width, s.height);
    glClearColor(contentBg.r, contentBg.g, contentBg.b, 1.0f);
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

TouchTarget targetAt(const AppState& s, float x, float y) {
    if (inside(x, y, 5, 96, 143, 134)) return TouchTarget::NavHome;
    if (inside(x, y, 5, 137, 143, 175)) return TouchTarget::NavVersions;
    if (inside(x, y, 5, 178, 143, 216)) return TouchTarget::NavMods;
    if (inside(x, y, 5, 219, 143, 257)) return TouchTarget::NavServers;
    if (inside(x, y, 5, 260, 143, 298)) return TouchTarget::NavNews;
    if (inside(x, y, 5, 301, 143, 339)) return TouchTarget::NavSettings;

    const float scale = s.height > 0
        ? static_cast<float>(s.height) / kDesignHeight : 1.0f;
    const float canvasW = std::max(
        static_cast<float>(kDesignWidth),
        s.height > 0 ? static_cast<float>(s.width) / scale
                     : static_cast<float>(kDesignWidth));
    const float contentW = canvasW - 176.0f;

    if (s.page == Page::Home) {
        if (inside(x, y, canvasW * 0.5f - 78, 141,
                   canvasW * 0.5f + 110, 165)) return TouchTarget::HomePlay;
        if (inside(x, y, 160 + contentW * 0.29f, 187,
                   160 + contentW * 0.51f, 221)) return TouchTarget::HeaderPreview;
        if (inside(x, y, 160 + contentW * 0.54f, 187,
                   160 + contentW * 0.77f, 221)) return TouchTarget::HeaderRelease;

        const float colW = (contentW - 10.0f) * 0.5f;
        const TouchTarget cards[] = {
            TouchTarget::HomeCard0, TouchTarget::HomeCard1,
            TouchTarget::HomeCard2, TouchTarget::HomeCard3,
            TouchTarget::HomeCard4, TouchTarget::HomeCard5,
            TouchTarget::HomeCard6, TouchTarget::HomeCard7
        };
        for (int i = 0; i < 8; ++i) {
            const int col = i / 4;
            const int row = i % 4;
            const float left = 160.0f + col * (colW + 10.0f);
            const float top = 250.0f + row * 51.0f;
            if (inside(x, y, left, top, left + colW, top + 44.0f)) return cards[i];
        }
        if (inside(x, y, 160, 472, 344, 506)) return TouchTarget::SearchVersions;
        if (inside(x, y, 350, 472, 413, 506)) return TouchTarget::FilterStable;
        if (inside(x, y, 418, 472, 494, 506)) return TouchTarget::FilterPreview;
        if (inside(x, y, 499, 472, 559, 506)) return TouchTarget::FilterBeta;
        if (inside(x, y, 564, 472, 608, 506)) return TouchTarget::FilterAll;
        if (inside(x, y, canvasW - 203, 472, canvasW - 117, 506)) return TouchTarget::Profile;
        if (inside(x, y, canvasW - 110, 472, canvasW - 75, 506)) return TouchTarget::QuickSettings;
    } else if (s.page == Page::Versions) {
        if (inside(x, y, 160, 205, canvasW - 16, 447)) return TouchTarget::VersionRow;
    } else if (s.page == Page::Mods) {
        if (inside(x, y, 170, 138, 540, 244)) return TouchTarget::ModsMarketplace;
        if (inside(x, y, 550, 138, canvasW - 170, 244)) return TouchTarget::ModsCommunity;
        if (inside(x, y, 170, 260, canvasW - 170, 366)) return TouchTarget::ModsGuide;
    } else if (s.page == Page::Servers) {
        if (inside(x, y, 170, 138, 540, 244)) return TouchTarget::ServersOfficial;
        if (inside(x, y, 550, 138, canvasW - 170, 244)) return TouchTarget::ServersGuide;
    } else if (s.page == Page::News) {
        if (inside(x, y, 170, 138, 540, 244)) return TouchTarget::NewsOfficial;
        if (inside(x, y, 550, 138, canvasW - 170, 244)) return TouchTarget::NewsHelp;
    } else if (s.page == Page::Settings) {
        if (inside(x, y, 170, 226, 470, 276)) return TouchTarget::SettingsStore;
        if (inside(x, y, 485, 226, 785, 276)) return TouchTarget::SettingsBeta;
    }
    if (inside(x, y, 12, 460, 138, 518)) return TouchTarget::Profile;
    return TouchTarget::None;
}

void activateTarget(AppState& s, TouchTarget target) {
    switch (target) {
        case TouchTarget::NavHome: setPage(s, Page::Home); break;
        case TouchTarget::NavVersions: setPage(s, Page::Versions); break;
        case TouchTarget::NavMods: setPage(s, Page::Mods); break;
        case TouchTarget::NavServers: setPage(s, Page::Servers); break;
        case TouchTarget::NavNews: setPage(s, Page::News); break;
        case TouchTarget::NavSettings:
        case TouchTarget::QuickSettings: setPage(s, Page::Settings); break;
        case TouchTarget::HomePlay:
            handlePlay(s);
            break;
        case TouchTarget::HeaderPreview:
        case TouchTarget::SettingsBeta:
        case TouchTarget::HomeCard5:
            openPlayListing(s, true);
            break;
        case TouchTarget::HeaderRelease:
        case TouchTarget::SettingsStore:
        case TouchTarget::HomeCard4:
            openPlayListing(s, false);
            break;
        case TouchTarget::SearchVersions:
            openWebsite(s,
                "https://www.google.com/search?q=Minecraft+Bedrock+version+history",
                "OPENED VERSION SEARCH");
            break;
        case TouchTarget::FilterStable:
            s.activeFilter = 0; s.status = "SHOWING STABLE LOCAL RECORDS"; startAnimation(s); break;
        case TouchTarget::FilterPreview:
            s.activeFilter = 1; s.status = "SHOWING PREVIEW LOCAL RECORDS"; startAnimation(s); break;
        case TouchTarget::FilterBeta:
            s.activeFilter = 2; s.status = "SHOWING BETA LOCAL RECORDS"; startAnimation(s); break;
        case TouchTarget::FilterAll:
            s.activeFilter = 3; s.status = "SHOWING ALL LOCAL RECORDS"; startAnimation(s); break;
        case TouchTarget::Profile:
            s.status = "PLAYER PROFILE IS MANAGED INSIDE MINECRAFT"; startAnimation(s); break;
        case TouchTarget::HomeCard0:
            if (s.installed) handlePlay(s);
            else openPlayListing(s, false);
            break;
        case TouchTarget::HomeCard1:
        case TouchTarget::HomeCard2:
        case TouchTarget::HomeCard3:
        case TouchTarget::VersionRow:
            s.status = "LOCAL HISTORY IS READ ONLY - NO APK STORED";
            startAnimation(s);
            break;
        case TouchTarget::HomeCard6:
        case TouchTarget::ServersOfficial:
            openWebsite(s, "https://www.minecraft.net/en-us/servers.jspr",
                        "OPENED OFFICIAL SERVER LIST");
            break;
        case TouchTarget::HomeCard7:
        case TouchTarget::NewsOfficial:
            openWebsite(s, "https://www.minecraft.net/en-us/articles",
                        "OPENED MINECRAFT NEWS");
            break;
        case TouchTarget::ModsMarketplace:
            openWebsite(s, "https://www.minecraft.net/en-us/marketplace",
                        "OPENED MINECRAFT MARKETPLACE");
            break;
        case TouchTarget::ModsCommunity:
            openWebsite(s, "https://mcpedl.com/",
                        "OPENED COMMUNITY ADD-ONS");
            break;
        case TouchTarget::ModsGuide:
            openWebsite(s, "https://help.minecraft.net/hc/en-us/articles/4409140076813",
                        "OPENED BEDROCK ADD-ON GUIDE");
            break;
        case TouchTarget::ServersGuide:
            openWebsite(s, "https://help.minecraft.net/hc/en-us/articles/4410316619533",
                        "OPENED SERVER JOINING GUIDE");
            break;
        case TouchTarget::NewsHelp:
            openWebsite(s, "https://help.minecraft.net/hc/en-us/",
                        "OPENED MINECRAFT HELP");
            break;
        case TouchTarget::None:
            break;
    }
}

int32_t onInput(android_app* app, AInputEvent* event) {
    auto* s = static_cast<AppState*>(app->userData);
    if (!s || AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    const float scale = static_cast<float>(s->height) / kDesignHeight;
    if (scale <= 0) return 0;

    const int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    const float x = AMotionEvent_getX(event, 0) / scale;
    const float y = AMotionEvent_getY(event, 0) / scale;

    if (action == AMOTION_EVENT_ACTION_DOWN) {
        s->downTarget = targetAt(*s, x, y);
        if (s->downTarget == TouchTarget::None) return 0;
        s->touchDown = true;
        s->pressedTarget = s->downTarget;
        s->touchX = x;
        s->touchY = y;
        s->pressStart = Clock::now();
        s->rippleActive = false;
        draw(*s);
        return 1;
    }

    if (!s->touchDown) return 0;

    if (action == AMOTION_EVENT_ACTION_MOVE) {
        s->touchX = x;
        s->touchY = y;
        s->pressedTarget = targetAt(*s, x, y) == s->downTarget
            ? s->downTarget : TouchTarget::None;
        draw(*s);
        return 1;
    }

    if (action == AMOTION_EVENT_ACTION_CANCEL) {
        s->touchDown = false;
        s->pressedTarget = TouchTarget::None;
        s->downTarget = TouchTarget::None;
        draw(*s);
        return 1;
    }

    if (action == AMOTION_EVENT_ACTION_UP) {
        s->touchX = x;
        s->touchY = y;
        const TouchTarget releasedTarget = targetAt(*s, x, y);
        const bool shouldActivate =
            s->pressedTarget != TouchTarget::None &&
            releasedTarget == s->downTarget;

        s->touchDown = false;
        s->pressedTarget = TouchTarget::None;
        s->downTarget = TouchTarget::None;

        if (shouldActivate) {
            s->rippleTarget = releasedTarget;
            s->rippleX = x;
            s->rippleY = y;
            s->rippleStart = Clock::now();
            s->rippleActive = true;
            s->releaseTarget = releasedTarget;
            s->releaseScaleStart = Clock::now();
            s->releaseScaleActive = true;
            activateTarget(*s, releasedTarget);
        } else {
            s->rippleActive = false;
            s->releaseScaleActive = false;
            s->releaseTarget = TouchTarget::None;
        }

        draw(*s);
        return 1;
    }

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
        const bool touchAnimating = state.touchDown || state.rippleActive;
        const int timeout = (state.animationActive || touchAnimating) ? 16 : -1;
        const int result = ALooper_pollOnce(
            timeout, nullptr, &events, reinterpret_cast<void**>(&source));
        if (result >= 0 && source) source->process(app, source);

        bool needsFrame = state.touchDown || state.rippleActive ||
                          state.releaseScaleActive;
        if (state.animationActive) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now() - state.animationStart).count();
            if (elapsed >= kAnimationMilliseconds) {
                state.animationActive = false;
            } else {
                needsFrame = true;
            }
        }
        if (state.rippleActive) {
            const auto rippleElapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    Clock::now() - state.rippleStart).count();
            if (rippleElapsed >= 330) {
                state.rippleActive = false;
            }
        }
        if (state.releaseScaleActive) {
            const auto releaseElapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    Clock::now() - state.releaseScaleStart).count();
            if (releaseElapsed >= 190) {
                state.releaseScaleActive = false;
                state.releaseTarget = TouchTarget::None;
            }
        }
        if (needsFrame) draw(state);
    }

    destroyGraphics(state);
}
