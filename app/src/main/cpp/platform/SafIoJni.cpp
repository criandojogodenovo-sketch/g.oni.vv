// platform/SafIoJni.cpp — SafBackend sobre JNI → vv.goni.SafIo (F5.1-C,
// device-only). Traduz as operações do core/SafStorage.h para chamadas
// estáticas Java (DocumentsContract). Falha Java → false/null — nunca
// exceção que atravessa a JNI.
#include "core/SafStorage.h"
#include "platform/Log.h"
#include <jni.h>
#include <string>
#include <vector>

namespace vv {
namespace saf {

namespace {

jstring toJString(JNIEnv* env, const std::string& s) {
    return env->NewStringUTF(s.c_str());
}

class SafJniBackend final : public SafBackend {
public:
    // cache de métodos — chamar 1× no boot (após saf::initJava)
    bool init(JNIEnv* env, jclass ioCls, jobject activity, jstring treeUri) {
        midExists_ = env->GetStaticMethodID(
            ioCls, "ioExists",
            "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Z");
        midMakeDirs_ = env->GetStaticMethodID(
            ioCls, "ioMakeDirs",
            "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Z");
        midList_ = env->GetStaticMethodID(
            ioCls, "ioList",
            "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)"
            "[Ljava/lang/String;");
        midRead_ = env->GetStaticMethodID(
            ioCls, "ioRead",
            "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)[B");
        midWrite_ = env->GetStaticMethodID(
            ioCls, "ioWrite",
            "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;[B)Z");
        ctx_ = env->NewGlobalRef(activity);
        uri_ = static_cast<jstring>(env->NewGlobalRef(treeUri));
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return false;
        }
        return midExists_ && midMakeDirs_ && midList_ && midRead_ && midWrite_;
    }

    bool initSingles(JNIEnv* env) {
        // métodos de documento ÚNICO (import/export — URI de ficheiro, não árvore)
        midDisplayName_ = env->GetStaticMethodID(
            clsRef(), "ioDisplayName",
            "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;");
        midReadSingle_ = env->GetStaticMethodID(
            clsRef(), "ioReadSingle",
            "(Landroid/content/Context;Ljava/lang/String;)[B");
        midWriteSingle_ = env->GetStaticMethodID(
            clsRef(), "ioWriteSingle",
            "(Landroid/content/Context;Ljava/lang/String;[B)Z");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return false;
        }
        return midDisplayName_ && midReadSingle_ && midWriteSingle_;
    }

    jmethodID midDisplayName() const { return midDisplayName_; }
    jmethodID midReadSingle() const { return midReadSingle_; }
    jmethodID midWriteSingle() const { return midWriteSingle_; }
    jobject context() const { return ctx_; }
    jclass cls() const { return clsRef(); }

    bool listFiles(const std::string& treeUri, const std::string& relDir,
                   std::vector<std::string>& out) const override {
        JNIEnv* env = envOf();
        if (!env) {
            return false;
        }
        auto arr = static_cast<jobjectArray>(env->CallStaticObjectMethod(
            cls(), midList_, ctx_, toJString(env, treeUri),
            toJString(env, relDir)));
        if (env->ExceptionCheck() || !arr) {
            env->ExceptionClear();
            return false;
        }
        const jsize n = env->GetArrayLength(arr);
        out.reserve(static_cast<size_t>(n));
        for (jsize i = 0; i < n; ++i) {
            jstring s = static_cast<jstring>(env->GetObjectArrayElement(arr, i));
            const char* c = env->GetStringUTFChars(s, nullptr);
            if (c) {
                out.emplace_back(c);
                env->ReleaseStringUTFChars(s, c);
            }
            env->DeleteLocalRef(s);
        }
        env->DeleteLocalRef(arr);
        return true;
    }

    bool makeDirs(const std::string& treeUri, const std::string& relDir) override {
        JNIEnv* env = envOf();
        if (!env) {
            return false;
        }
        const jboolean r = env->CallStaticBooleanMethod(
            cls(), midMakeDirs_, ctx_, toJString(env, treeUri),
            toJString(env, relDir));
        env->ExceptionClear();
        return r == JNI_TRUE;
    }

    bool exists(const std::string& treeUri, const std::string& rel) const override {
        JNIEnv* env = envOf();
        if (!env) {
            return false;
        }
        const jboolean r = env->CallStaticBooleanMethod(
            cls(), midExists_, ctx_, toJString(env, treeUri),
            toJString(env, rel));
        env->ExceptionClear();
        return r == JNI_TRUE;
    }

    bool writeBytes(const std::string& treeUri, const std::string& rel,
                    const void* data, size_t n) override {
        JNIEnv* env = envOf();
        if (!env) {
            return false;
        }
        jbyteArray arr = env->NewByteArray(static_cast<jsize>(n));
        if (!arr) {
            return false;
        }
        env->SetByteArrayRegion(arr, 0, static_cast<jsize>(n),
                                static_cast<const jbyte*>(data));
        const jboolean r = env->CallStaticBooleanMethod(
            cls(), midWrite_, ctx_, toJString(env, treeUri),
            toJString(env, rel), arr);
        env->DeleteLocalRef(arr);
        env->ExceptionClear();
        return r == JNI_TRUE;
    }

    bool readBytes(const std::string& treeUri, const std::string& rel,
                   std::vector<u8>& out) const override {
        JNIEnv* env = envOf();
        if (!env) {
            return false;
        }
        auto arr = static_cast<jbyteArray>(env->CallStaticObjectMethod(
            cls(), midRead_, ctx_, toJString(env, treeUri),
            toJString(env, rel)));
        if (env->ExceptionCheck() || !arr) {
            env->ExceptionClear();
            return false;
        }
        const jsize n = env->GetArrayLength(arr);
        out.resize(static_cast<size_t>(n));
        if (n > 0) {
            env->GetByteArrayRegion(arr, 0, n, reinterpret_cast<jbyte*>(out.data()));
        }
        env->DeleteLocalRef(arr);
        return true;
    }

private:
    static JNIEnv* envOf() {
        // o loop do engine corre no thread do NativeActivity (anexado);
        // android_main guarda o env no init — reutiliza via JavaVM.
        if (!g_vm) {
            return nullptr;
        }
        JNIEnv* env = nullptr;
        return g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK
                   ? env : nullptr;
    }

    static jclass& clsRef() {
        static jclass cls = nullptr;
        return cls;
    }
    static JavaVM* g_vm;

public:
    // acesso p/ os helpers de documento único (fora do anon namespace)
    static JNIEnv* envOfPublic() { return envOf(); }


public:
    static bool initClass(JavaVM* vm, JNIEnv* env, jclass ioCls) {
        g_vm = vm;
        clsRef() = static_cast<jclass>(env->NewGlobalRef(ioCls));
        return true;
    }

    jmethodID midExists_ = nullptr;
    jmethodID midMakeDirs_ = nullptr;
    jmethodID midList_ = nullptr;
    jmethodID midRead_ = nullptr;
    jmethodID midWrite_ = nullptr;
    jmethodID midDisplayName_ = nullptr;
    jmethodID midReadSingle_ = nullptr;
    jmethodID midWriteSingle_ = nullptr;
    jobject ctx_ = nullptr;
    jstring uri_ = nullptr;
};

JavaVM* SafJniBackend::g_vm = nullptr;

SafJniBackend g_safBackend;
bool g_safBackendReady = false;

} // namespace

// exposto para o main.cpp: prepara o backend JNI (device)
bool safInitJniBackend(void* vm, void* envPtr, void* activityObject) {
    JavaVM* vmP = static_cast<JavaVM*>(vm);
    JNIEnv* env = static_cast<JNIEnv*>(envPtr);
    jobject activity = static_cast<jobject>(activityObject);
    if (!vmP || !env || !activity) {
        return false;
    }
    jclass actCls = env->GetObjectClass(activity);
    // SafIo via classloader da activity (FindClass nativo não vê classes da app)
    jmethodID midLoader = env->GetMethodID(actCls, "getClassLoader",
                                           "()Ljava/lang/ClassLoader;");
    jobject loader = env->CallObjectMethod(activity, midLoader);
    jclass loaderCls = env->GetObjectClass(loader);
    jmethodID midLoad = env->GetMethodID(
        loaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring jname = env->NewStringUTF("vv.goni.SafIo");
    jclass ioCls = static_cast<jclass>(
        env->CallObjectMethod(loader, midLoad, jname));
    env->DeleteLocalRef(jname);
    if (!ioCls || env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    if (!SafJniBackend::initClass(vmP, env, ioCls)) {
        return false;
    }
    // URI inicial vazia — a verdadeira chega quando o utilizador escolhe
    jstring empty = env->NewStringUTF("");
    g_safBackendReady = g_safBackend.init(env, ioCls, activity, empty);
    env->DeleteLocalRef(empty);
    if (!g_safBackendReady) {
        return false;
    }
    return g_safBackend.initSingles(env);
}

SafJniBackend* safJniBackend() {
    return g_safBackendReady ? &g_safBackend : nullptr;
}

// ---- documento único: import/export de ficheiros ----------------------------

std::string safJniDisplayName(const std::string& docUri) {
    JNIEnv* env = SafJniBackend::envOfPublic();
    if (!g_safBackendReady || !env) {
        return "";
    }
    jstring s = static_cast<jstring>(env->CallStaticObjectMethod(
        g_safBackend.cls(), g_safBackend.midDisplayName(), g_safBackend.context(),
        env->NewStringUTF(docUri.c_str())));
    if (env->ExceptionCheck() || !s) {
        env->ExceptionClear();
        return "";
    }
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string out = c ? c : "";
    if (c) {
        env->ReleaseStringUTFChars(s, c);
    }
    env->DeleteLocalRef(s);
    return out;
}

std::vector<u8> safJniReadSingle(const std::string& docUri) {
    std::vector<u8> out;
    JNIEnv* env = SafJniBackend::envOfPublic();
    if (!g_safBackendReady || !env) {
        return out;
    }
    auto arr = static_cast<jbyteArray>(env->CallStaticObjectMethod(
        g_safBackend.cls(), g_safBackend.midReadSingle(),
        g_safBackend.context(), env->NewStringUTF(docUri.c_str())));
    if (env->ExceptionCheck() || !arr) {
        env->ExceptionClear();
        return out;
    }
    const jsize n = env->GetArrayLength(arr);
    out.resize(static_cast<size_t>(n));
    if (n > 0) {
        env->GetByteArrayRegion(arr, 0, n, reinterpret_cast<jbyte*>(out.data()));
    }
    env->DeleteLocalRef(arr);
    return out;
}

bool safJniWriteSingle(const std::string& docUri, const void* data, size_t n) {
    JNIEnv* env = SafJniBackend::envOfPublic();
    if (!g_safBackendReady || !env) {
        return false;
    }
    jbyteArray arr = env->NewByteArray(static_cast<jsize>(n));
    if (!arr) {
        return false;
    }
    env->SetByteArrayRegion(arr, 0, static_cast<jsize>(n),
                            static_cast<const jbyte*>(data));
    const jboolean r = env->CallStaticBooleanMethod(
        g_safBackend.cls(), g_safBackend.midWriteSingle(), g_safBackend.context(),
        env->NewStringUTF(docUri.c_str()), arr);
    env->DeleteLocalRef(arr);
    env->ExceptionClear();
    return r == JNI_TRUE;
}

} // namespace saf
} // namespace vv
