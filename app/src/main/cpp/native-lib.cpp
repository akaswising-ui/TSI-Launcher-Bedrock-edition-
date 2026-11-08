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
};

struct Color { float r, g, b; };
constexpr Color kBackground{0.063f, 0.094f, 0.125f};
constexpr Color kPanel{0.102f, 0.157f, 0.204f};
constexpr Color kAccent{0.263f, 0.780f, 0.545f};
constexpr Color kWhite{0.96f, 0.98f, 1.0f};
constexpr Color kMuted{0.65f, 0.72f, 0.77f};
constexpr Color kDark{0.035f, 0.08f, 0.06f};

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
        readVersionHistory(s);
        if (name) env->DeleteLocalRef(name);
        env->DeleteLocalRef(infoClass);
        env->DeleteLocalRef(info);
    } else {
        s.status = "MINECRAFT BEDROCK NOT FOUND";
    }
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
    vertices.reserve(18000);
    const float scale = std::min(
        static_cast<float>(s.width) / kDesignWidth,
        static_cast<float>(s.height) / kDesignHeight);
    const float ox = (s.width - kDesignWidth * scale) * 0.5f;
    const float oy = (s.height - kDesignHeight * scale) * 0.5f;
    auto rect = [&](float x, float y, float w, float h, Color c) {
        addRect(vertices, ox + x * scale, oy + y * scale,
                w * scale, h * scale, c, s.width, s.height);
    };
    auto text = [&](const std::string& value, float x, float y, float size, Color c) {
        addText(vertices, value, ox + x * scale, oy + y * scale,
                size * scale, c, s.width, s.height);
    };

    rect(0, 0, 360, 720, kBackground);
    text("TSI BEDROCK", 24, 28, 3, kAccent);
    text("MINECRAFT BEDROCK", 24, 68, 2, kWhite);

    rect(18, 112, 324, 150, kPanel);
    text("APP STATUS", 34, 132, 2, kMuted);
    text(s.installed ? "BEDROCK FOUND" : "NOT INSTALLED", 34, 162, 2.4f,
         s.installed ? kAccent : kWhite);
    text("VERSION", 34, 200, 1.8f, kMuted);
    text(s.version.substr(0, 27), 34, 224, 1.9f, kWhite);

    rect(20, 294, 320, 58, kAccent);
    text("PLAY MINECRAFT", 94, 316, 2.1f, kDark);
    rect(20, 368, 320, 54, kPanel);
    text("OPEN GOOGLE PLAY", 89, 388, 1.9f, kWhite);
    rect(20, 438, 320, 54, kPanel);
    text("REFRESH VERSION", 95, 458, 1.9f, kWhite);

    rect(18, 516, 324, 126, kPanel);
    text("ANDROID BETA CHANNEL", 32, 530, 1.8f, kAccent);
    text("BETA REPLACES STABLE APP", 32, 552, 1.7f, kWhite);
    text("PREVIEW IS NOT ON ANDROID", 32, 574, 1.55f, kMuted);
    rect(30, 590, 300, 38, kAccent);
    text("MANAGE BETA IN PLAY", 92, 603, 1.65f, kDark);

    text("LAST HISTORY ENTRY", 24, 651, 1.7f, kMuted);
    text(s.history.substr(0, 28), 24, 676, 1.7f, kWhite);
    text(s.status.substr(0, 34), 24, 704, 1.2f, kMuted);

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
    if (AMotionEvent_getAction(event) != AMOTION_EVENT_ACTION_UP) return 1;
    const float scale = std::min(
        static_cast<float>(s->width) / kDesignWidth,
        static_cast<float>(s->height) / kDesignHeight);
    if (scale <= 0) return 1;
    const float ox = (s->width - kDesignWidth * scale) * 0.5f;
    const float oy = (s->height - kDesignHeight * scale) * 0.5f;
    const float x = (AMotionEvent_getX(event, 0) - ox) / scale;
    const float y = (AMotionEvent_getY(event, 0) - oy) / scale;

    if (inside(x, y, 20, 294, 340, 352)) {
        if (!s->installed) {
            s->status = "INSTALL OFFICIAL MINECRAFT FIRST";
        } else if (launchIntent(*s, nullptr, nullptr, kBedrockPackage)) {
            s->status = "MINECRAFT LAUNCH REQUEST SENT";
        } else {
            s->status = "MINECRAFT LAUNCH FAILED";
        }
    } else if (inside(x, y, 20, 368, 340, 422)) {
        if (launchIntent(*s, "android.intent.action.VIEW",
                         "https://play.google.com/store/apps/details?id=com.mojang.minecraftpe",
                         nullptr)) {
            s->status = "OPENED GOOGLE PLAY";
        } else {
            s->status = "COULD NOT OPEN GOOGLE PLAY";
        }
    } else if (inside(x, y, 20, 438, 340, 492)) {
        detectBedrock(*s);
        s->status = s->installed ? "INSTALLED VERSION REFRESHED" : "MINECRAFT NOT INSTALLED";
    } else if (inside(x, y, 30, 590, 330, 628)) {
        const bool openedMarket = launchIntent(
            *s, "android.intent.action.VIEW",
            "market://details?id=com.mojang.minecraftpe", nullptr);
        const bool openedWeb = openedMarket || launchIntent(
            *s, "android.intent.action.VIEW",
            "https://play.google.com/store/apps/details?id=com.mojang.minecraftpe",
            nullptr);
        s->status = openedWeb
            ? "PLAY LISTING: JOIN OR LEAVE BETA"
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
