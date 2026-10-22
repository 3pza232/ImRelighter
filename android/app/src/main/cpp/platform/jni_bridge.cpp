#include <android/asset_manager_jni.h>
#include <android/bitmap.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/android_app.h"

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

#define LOG_TAG "ImRelighterAndroid"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

JavaVM* g_vm = nullptr;
jobject g_activity = nullptr;
jmethodID g_request_open_method = nullptr;
jmethodID g_request_model_method = nullptr;
jmethodID g_request_model_folder_method = nullptr;
jmethodID g_export_method = nullptr;
AAssetManager* g_assets = nullptr;
std::string g_files_dir;
std::string g_model_storage_label;
std::vector<std::string> g_pending_model_files;
std::atomic<int> g_system_inset_top{0};
std::atomic<int> g_system_inset_bottom{0};
float g_density = 1.0f;

std::mutex g_app_mutex;
std::unique_ptr<imrelighter::AndroidApp> g_app;

// The document picker pauses the activity and may destroy the SurfaceView.
// Keep the last decoded image so the first frame after an activity rebuild can
// still display it. This process-global buffer is cleared only on process exit.
std::mutex g_image_mutex;
std::vector<uint8_t> g_last_rgba;
int g_last_width = 0;
int g_last_height = 0;
bool g_pending_image = false;

// The render thread survives surface changes. It owns the EGL context and the
// single AndroidApp instance; the UI must not be torn down just because the
// window surface went away (Home, picker, notification shade, etc.).
std::mutex g_surface_mutex;
ANativeWindow* g_pending_window = nullptr;
bool g_detach_requested = false;
std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};

std::mutex g_render_mutex;
std::thread g_render_thread;
std::atomic<bool> g_running{false};

void notify_open_image() {
    if (g_vm == nullptr || g_activity == nullptr || g_request_open_method == nullptr) {
        return;
    }
    JNIEnv* env = nullptr;
    bool attached = false;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            LOGE("failed to attach render thread for picker callback");
            return;
        }
        attached = true;
    }
    env->CallVoidMethod(g_activity, g_request_open_method);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    if (attached) {
        g_vm->DetachCurrentThread();
    }
}

void notify_model_file() {
    if (g_vm == nullptr || g_activity == nullptr || g_request_model_method == nullptr) {
        return;
    }
    JNIEnv* env = nullptr;
    bool attached = false;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            LOGE("failed to attach render thread for model picker callback");
            return;
        }
        attached = true;
    }
    env->CallVoidMethod(g_activity, g_request_model_method);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    if (attached) {
        g_vm->DetachCurrentThread();
    }
}

void notify_model_folder() {
    if (g_vm == nullptr || g_activity == nullptr ||
        g_request_model_folder_method == nullptr) {
        return;
    }
    JNIEnv* env = nullptr;
    bool attached = false;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            LOGE("failed to attach render thread for model folder callback");
            return;
        }
        attached = true;
    }
    env->CallVoidMethod(g_activity, g_request_model_folder_method);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    if (attached) {
        g_vm->DetachCurrentThread();
    }
}

void notify_export(const std::vector<uint8_t>& rgba, int width, int height,
                   const std::string& kind) {
    if (g_vm == nullptr || g_activity == nullptr || g_export_method == nullptr) {
        return;
    }
    JNIEnv* env = nullptr;
    bool attached = false;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            LOGE("failed to attach render thread for export callback");
            return;
        }
        attached = true;
    }
    jbyteArray array = env->NewByteArray(static_cast<jsize>(rgba.size()));
    if (array != nullptr) {
        env->SetByteArrayRegion(array, 0, static_cast<jsize>(rgba.size()),
                                reinterpret_cast<const jbyte*>(rgba.data()));
        jstring kind_string = env->NewStringUTF(kind.c_str());
        env->CallVoidMethod(g_activity, g_export_method, array, width, height, kind_string);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        if (kind_string != nullptr) {
            env->DeleteLocalRef(kind_string);
        }
        env->DeleteLocalRef(array);
    }
    if (attached) {
        g_vm->DetachCurrentThread();
    }
}

std::string jstring_to_string(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return {};
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) {
        return {};
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

bool create_egl_context(EGLDisplay& display, EGLConfig& config, EGLContext& context,
                        EGLSurface& pbuffer) {
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || eglInitialize(display, nullptr, nullptr) != EGL_TRUE) {
        LOGE("eglInitialize failed");
        return false;
    }
    const EGLint config_attributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE,
    };
    EGLint config_count = 0;
    if (eglChooseConfig(display, config_attributes, &config, 1, &config_count) != EGL_TRUE ||
        config_count < 1) {
        LOGE("eglChooseConfig failed");
        return false;
    }
    const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE,
    };
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
    if (context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed: 0x%x", eglGetError());
        return false;
    }
    const EGLint pbuffer_attributes[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE,
    };
    pbuffer = eglCreatePbufferSurface(display, config, pbuffer_attributes);
    if (pbuffer == EGL_NO_SURFACE) {
        LOGE("eglCreatePbufferSurface failed: 0x%x (continuing without pbuffer)",
             eglGetError());
    }
    return true;
}

bool attach_surface(EGLDisplay display, EGLConfig config, EGLContext context,
                    ANativeWindow* window, EGLSurface& surface, int& width,
                    int& height) {
    if (window == nullptr) {
        return false;
    }
    surface = eglCreateWindowSurface(display, config, window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed: 0x%x", eglGetError());
        return false;
    }
    if (eglMakeCurrent(display, surface, surface, context) != EGL_TRUE) {
        LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
        eglDestroySurface(display, surface);
        surface = EGL_NO_SURFACE;
        return false;
    }
    eglSwapInterval(display, 1);
    eglQuerySurface(display, surface, EGL_WIDTH, &width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &height);
    g_surface_width.store(width);
    g_surface_height.store(height);
    LOGI("EGL surface attached: %dx%d", width, height);
    return true;
}

void detach_surface(EGLDisplay display, EGLSurface& surface, ANativeWindow*& window,
                    EGLSurface pbuffer, EGLContext context) {
    if (display != EGL_NO_DISPLAY) {
        // Keep the EGLContext current through a tiny pbuffer. Textures,
        // shaders, the ImGui backend and all CPU state survive Home/resume,
        // and GL deletion during shutdown remains valid even without a window.
        if (pbuffer != EGL_NO_SURFACE) {
            eglMakeCurrent(display, pbuffer, pbuffer, context);
        } else {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
        }
    }
    if (surface != EGL_NO_SURFACE && display != EGL_NO_DISPLAY) {
        eglDestroySurface(display, surface);
        surface = EGL_NO_SURFACE;
    }
    if (window != nullptr) {
        ANativeWindow_release(window);
        window = nullptr;
    }
}

void destroy_egl_context(EGLDisplay& display, EGLContext& context,
                         EGLSurface& pbuffer) {
    if (display == EGL_NO_DISPLAY) {
        return;
    }
    if (pbuffer != EGL_NO_SURFACE) {
        eglDestroySurface(display, pbuffer);
        pbuffer = EGL_NO_SURFACE;
    }
    if (context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
    }
    eglTerminate(display);
    display = EGL_NO_DISPLAY;
}

void queue_rgba_image(std::vector<uint8_t> rgba, int width, int height) {
    {
        std::lock_guard<std::mutex> lock(g_app_mutex);
        if (g_app) {
            {
                std::lock_guard<std::mutex> image_lock(g_image_mutex);
                g_pending_image = false;
            }
            g_app->queue_image_rgba(std::move(rgba), width, height);
            return;
        }
    }
    std::lock_guard<std::mutex> image_lock(g_image_mutex);
    g_last_rgba = std::move(rgba);
    g_last_width = width;
    g_last_height = height;
    g_pending_image = true;
    LOGI("saved %dx%d image for renderer startup", width, height);
}

void render_thread_main() {
    JNIEnv* env = nullptr;
    bool attached = false;
    if (g_vm != nullptr &&
        g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            attached = true;
        }
    }

    EGLDisplay display = EGL_NO_DISPLAY;
    EGLConfig config = nullptr;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLSurface pbuffer = EGL_NO_SURFACE;
    ANativeWindow* window = nullptr;
    int width = 0;
    int height = 0;

    if (!create_egl_context(display, config, context, pbuffer)) {
        destroy_egl_context(display, context, pbuffer);
        if (attached) {
            g_vm->DetachCurrentThread();
        }
        return;
    }

    while (g_running.load()) {
        ANativeWindow* new_window = nullptr;
        bool detach = false;
        {
            std::lock_guard<std::mutex> lock(g_surface_mutex);
            new_window = g_pending_window;
            g_pending_window = nullptr;
            detach = g_detach_requested;
            g_detach_requested = false;
        }

        if (detach || new_window != nullptr) {
            detach_surface(display, surface, window, pbuffer, context);
            if (new_window != nullptr) {
                if (!attach_surface(display, config, context, new_window, surface, width, height)) {
                    ANativeWindow_release(new_window);
                    new_window = nullptr;
                } else {
                    window = new_window;
                }
            }
        }

        if (surface != EGL_NO_SURFACE) {
            {
                std::lock_guard<std::mutex> lock(g_app_mutex);
                if (!g_app) {
                    g_app = std::make_unique<imrelighter::AndroidApp>(
                        g_assets, g_files_dir, g_density);
                    g_app->set_open_image_callback([]() { notify_open_image(); });
                    g_app->set_model_file_callback([]() { notify_model_file(); });
                    g_app->set_model_folder_callback([]() { notify_model_folder(); });
                    g_app->set_export_callback(
                        [](const std::vector<uint8_t>& rgba, int w, int h,
                           const std::string& kind) { notify_export(rgba, w, h, kind); });
                    std::string error;
                    if (!g_app->initialize(error)) {
                        LOGE("AndroidApp initialize failed: %s", error.c_str());
                        g_app.reset();
                    } else {
                        if (!g_model_storage_label.empty()) {
                            g_app->set_model_storage_label(g_model_storage_label);
                        }
                        g_app->set_system_insets(g_system_inset_top.load(),
                                                 g_system_inset_bottom.load());
                        if (!g_pending_model_files.empty()) {
                            g_app->queue_model_files(
                                std::move(g_pending_model_files));
                            g_pending_model_files.clear();
                        }
                        std::vector<uint8_t> pending;
                        int pending_width = 0;
                        int pending_height = 0;
                        bool has_pending = false;
                        {
                            std::lock_guard<std::mutex> image_lock(g_image_mutex);
                            if (g_pending_image) {
                                pending = std::move(g_last_rgba);
                                pending_width = g_last_width;
                                pending_height = g_last_height;
                                g_last_rgba.clear();
                                g_pending_image = false;
                                has_pending = true;
                            }
                        }
                        if (has_pending) {
                            LOGI("restoring pending image at startup: %dx%d",
                                 pending_width, pending_height);
                            g_app->queue_image_rgba(std::move(pending), pending_width,
                                                    pending_height);
                        }
                    }
                }
                if (g_app) {
                    const int requested_width = g_surface_width.load();
                    const int requested_height = g_surface_height.load();
                    if (requested_width > 0 && requested_height > 0) {
                        g_app->resize(requested_width, requested_height);
                    }
                    g_app->render();
                }
            }

            if (eglSwapBuffers(display, surface) != EGL_TRUE) {
                LOGE("eglSwapBuffers failed: 0x%x", eglGetError());
                detach_surface(display, surface, window, pbuffer, context);
            }
        } else {
            usleep(8000);
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_app_mutex);
        if (g_app) {
            g_app->shutdown();
            g_app.reset();
        }
    }
    detach_surface(display, surface, window, pbuffer, context);
    destroy_egl_context(display, context, pbuffer);
    if (attached) {
        g_vm->DetachCurrentThread();
    }
}

void stop_renderer() {
    g_running.store(false);
    std::lock_guard<std::mutex> lock(g_render_mutex);
    if (g_render_thread.joinable()) {
        g_render_thread.join();
    }
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeInit(
    JNIEnv* env, jobject, jobject activity, jobject asset_manager, jstring files_dir,
    jfloat density) {
    g_assets = AAssetManager_fromJava(env, asset_manager);
    g_files_dir = jstring_to_string(env, files_dir);
    g_density = density;
    if (g_activity != nullptr) {
        env->DeleteGlobalRef(g_activity);
    }
    g_activity = env->NewGlobalRef(activity);
    jclass activity_class = env->GetObjectClass(activity);
    g_request_open_method =
        env->GetMethodID(activity_class, "requestOpenImageFromNative", "()V");
    g_request_model_method =
        env->GetMethodID(activity_class, "requestModelFileFromNative", "()V");
    g_request_model_folder_method =
        env->GetMethodID(activity_class, "requestModelFolderFromNative", "()V");
    g_export_method = env->GetMethodID(activity_class, "onExportReady",
                                       "([BIILjava/lang/String;)V");
    env->DeleteLocalRef(activity_class);
    if (g_request_open_method == nullptr) {
        LOGE("requestOpenImageFromNative() not found");
    }
    if (g_request_model_method == nullptr) {
        LOGE("requestModelFileFromNative() not found");
    }
    if (g_request_model_folder_method == nullptr) {
        LOGE("requestModelFolderFromNative() not found");
    }
    if (g_export_method == nullptr) {
        LOGE("onExportReady() not found");
    }
    LOGI("nativeInit: files=%s density=%.2f", g_files_dir.c_str(), g_density);
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeSurfaceCreated(
    JNIEnv* env, jobject, jobject surface, jint width, jint height) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        LOGE("ANativeWindow_fromSurface failed");
        return;
    }
    g_surface_width.store(width);
    g_surface_height.store(height);
    {
        std::lock_guard<std::mutex> lock(g_surface_mutex);
        if (g_pending_window != nullptr) {
            ANativeWindow_release(g_pending_window);
        }
        g_pending_window = window;
        g_detach_requested = false;
    }
    if (!g_running.load()) {
        std::lock_guard<std::mutex> lock(g_render_mutex);
        if (!g_render_thread.joinable()) {
            g_running.store(true);
            g_render_thread = std::thread([]() { render_thread_main(); });
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeSurfaceChanged(
    JNIEnv*, jobject, jint width, jint height) {
    g_surface_width.store(width);
    g_surface_height.store(height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeSurfaceDestroyed(JNIEnv*, jobject) {
    // Do not join the render thread here; the main thread must remain responsive.
    // The render thread detaches only the EGL surface and keeps the context/app.
    std::lock_guard<std::mutex> lock(g_surface_mutex);
    g_detach_requested = true;
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeTouch(
    JNIEnv*, jobject, jint action, jint pointer_count, jfloat x0, jfloat y0,
    jfloat x1, jfloat y1) {
    std::lock_guard<std::mutex> lock(g_app_mutex);
    if (g_app) {
        g_app->queue_touch(action, pointer_count, x0, y0, x1, y1);
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_imrelighter_android_MainActivity_nativeBack(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_app_mutex);
    return g_app && g_app->handle_back() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeOpenImage(
    JNIEnv* env, jobject, jbyteArray data) {
    if (data == nullptr) {
        return;
    }
    const jsize size = env->GetArrayLength(data);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    env->GetByteArrayRegion(data, 0, size, reinterpret_cast<jbyte*>(bytes.data()));
    std::lock_guard<std::mutex> lock(g_app_mutex);
    if (g_app) {
        g_app->queue_image(std::move(bytes));
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeOpenImageRgba(
    JNIEnv* env, jobject, jbyteArray data, jint width, jint height) {
    if (data == nullptr || width <= 0 || height <= 0) {
        return;
    }
    const jsize size = env->GetArrayLength(data);
    if (size != width * height * 4) {
        LOGE("nativeOpenImageRgba: invalid RGBA size %d (expected %d)", size,
             width * height * 4);
        return;
    }
    std::vector<uint8_t> rgba(static_cast<size_t>(size));
    env->GetByteArrayRegion(data, 0, size, reinterpret_cast<jbyte*>(rgba.data()));
    queue_rgba_image(std::move(rgba), width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeOpenBitmap(
    JNIEnv* env, jobject, jobject bitmap) {
    if (bitmap == nullptr) {
        return;
    }
    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS) {
        LOGE("nativeOpenBitmap: AndroidBitmap_getInfo failed");
        return;
    }
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 ||
        info.width == 0 || info.height == 0) {
        LOGE("nativeOpenBitmap: unsupported bitmap format %d", info.format);
        return;
    }
    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS ||
        pixels == nullptr) {
        LOGE("nativeOpenBitmap: AndroidBitmap_lockPixels failed");
        return;
    }
    const size_t bytes = static_cast<size_t>(info.width) * info.height * 4;
    std::vector<uint8_t> rgba(bytes);
    std::memcpy(rgba.data(), pixels, bytes);
    AndroidBitmap_unlockPixels(env, bitmap);
    queue_rgba_image(std::move(rgba), static_cast<int>(info.width),
                     static_cast<int>(info.height));
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeSelectModel(
    JNIEnv* env, jobject, jstring model_id) {
    std::string id = jstring_to_string(env, model_id);
    std::lock_guard<std::mutex> lock(g_app_mutex);
    if (g_app) {
        g_app->queue_model(id);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeSelectModelFile(
    JNIEnv* env, jobject, jstring path) {
    const std::string model_path = jstring_to_string(env, path);
    std::lock_guard<std::mutex> lock(g_app_mutex);
    if (g_app) {
        g_app->queue_model_file(model_path);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeRegisterModelFiles(
    JNIEnv* env, jobject, jobjectArray paths) {
    std::vector<std::string> model_paths;
    if (paths != nullptr) {
        const jsize count = env->GetArrayLength(paths);
        for (jsize i = 0; i < count; ++i) {
            auto value = static_cast<jstring>(env->GetObjectArrayElement(paths, i));
            model_paths.push_back(jstring_to_string(env, value));
            if (value != nullptr) {
                env->DeleteLocalRef(value);
            }
        }
    }
    std::lock_guard<std::mutex> lock(g_app_mutex);
    if (g_app) {
        g_app->queue_model_files(std::move(model_paths));
    } else {
        for (const std::string& path : model_paths) {
            g_pending_model_files.push_back(path);
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeSetModelStorage(
    JNIEnv* env, jobject, jstring label) {
    const std::string value = jstring_to_string(env, label);
    std::lock_guard<std::mutex> lock(g_app_mutex);
    g_model_storage_label = value;
    if (g_app) {
        g_app->set_model_storage_label(value);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeSetSystemInsets(
    JNIEnv*, jobject, jint top, jint bottom) {
    g_system_inset_top.store(std::max(0, static_cast<int>(top)));
    g_system_inset_bottom.store(std::max(0, static_cast<int>(bottom)));
    std::lock_guard<std::mutex> lock(g_app_mutex);
    if (g_app) {
        g_app->set_system_insets(top, bottom);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeResetModel(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_app_mutex);
    if (g_app) {
        g_app->queue_reset_model();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeClearSession(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> image_lock(g_image_mutex);
    g_last_rgba.clear();
    g_last_width = 0;
    g_last_height = 0;
    g_pending_image = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeShutdown(JNIEnv* env, jobject) {
    stop_renderer();
    std::lock_guard<std::mutex> lock(g_app_mutex);
    g_app.reset();
    if (g_activity != nullptr) {
        env->DeleteGlobalRef(g_activity);
        g_activity = nullptr;
    }
    g_request_open_method = nullptr;
    g_request_model_method = nullptr;
    g_request_model_folder_method = nullptr;
    g_export_method = nullptr;
    g_assets = nullptr;
    g_files_dir.clear();
    g_model_storage_label.clear();
    g_pending_model_files.clear();
}

extern "C" JNIEXPORT void JNICALL
Java_com_imrelighter_android_MainActivity_nativeDestroy(JNIEnv* env, jobject) {
    Java_com_imrelighter_android_MainActivity_nativeShutdown(env, nullptr);
}
