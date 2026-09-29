// platform/SafIoJni.cpp — SafBackend sobre JNI → vv.goni.SafIo (F5.1-C,
// device-only). Traduz as operações do core/SafStorage.h para chamadas
// estáticas Java (DocumentsContract). Falha Java → false/null — nunca
// exceção que atravessa a JNI.
#include "core/SafStorage.h"
#include "platform/EngineLog.h"
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
    // cache de métodos — chamar 1× no boot (após saf::initJava).
    // AUDITORIA F5.1-hotfix: cada lookup loga OK/FAIL (no engine.log — se
    // o boot morrer aqui, o ficheiro diz QUAL método/assinatura falhou).
    bool init(JNIEnv* env, jclass ioCls, jobject activity, jstring treeUri) {
        struct Lookup { jmethodID* mid; const char* name; const char* sig; };
        const Lookup lookups[] = {
            { &midExists_,  "ioExists",
              "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Z" },
            { &midMakeDirs_, "ioMakeDirs",
              "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Z" },
            { &midList_,    "ioList",
              "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)"
              "[Ljava/lang/String;" },
            { &midRead_,    "ioRead",
              "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)[B" },
            { &midWrite_,   "ioWrite",
              "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;[B)Z" },
        };
        bool allOk = true;
        for (const Lookup& lk : lookups) {
            *lk.mid = env->GetStaticMethodID(ioCls, lk.name, lk.sig);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
            if (!*lk.mid) {
                elog::error("jni/saf: SafIo.%s NÃO encontrada (%s)",
                            lk.name, lk.sig);
                allOk = false;
            } else {
                elog::info("jni/saf: SafIo.%s OK", lk.name);
            }
        }
        ctx_ = env->NewGlobalRef(activity);
        uri_ = static_cast<jstring>(env->NewGlobalRef(treeUri));
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            elog::error("jni/saf: NewGlobalRef(ctx/uri) falhou");
            return false;
        }
        return allOk;
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
        bool allOk = true;
        if (!midDisplayName_) {
            elog::error("jni/saf: SafIo.ioDisplayName NÃO encontrada");
            allOk = false;
        }
        if (!midReadSingle_) {
            elog::error("jni/saf: SafIo.ioReadSingle NÃO encontrada");
            allOk = false;
        }
        if (!midWriteSingle_) {
            elog::error("jni/saf: SafIo.ioWriteSingle NÃO encontrada");
            allOk = false;
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return allOk;
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
// AUDITORIA F5.1-hotfix: cada passo verificado (null check + exceção) e
// logado — o boot mostra exatamente onde a ponte SAF falhou.
bool safInitJniBackend(void* vm, void* envPtr, void* activityObject) {
    JavaVM* vmP = static_cast<JavaVM*>(vm);
    JNIEnv* env = static_cast<JNIEnv*>(envPtr);
    jobject activity = static_cast<jobject>(activityObject);
    if (!vmP || !env || !activity) {
        elog::error("jni/saf: init backend — argumentos nulos (vm/env/activity)");
        return false;
    }
    elog::info("jni/saf: init backend JNI");
    jclass actCls = env->GetObjectClass(activity);
    // SafIo via classloader da activity (FindClass nativo não vê classes da app)
    jmethodID midLoader = env->GetMethodID(actCls, "getClassLoader",
                                           "()Ljava/lang/ClassLoader;");
    if (!midLoader || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        elog::error("jni/saf: getClassLoader indisponível na activity");
        return false;
    }
    jobject loader = env->CallObjectMethod(activity, midLoader);
    if (!loader || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        elog::error("jni/saf: classloader da activity é null");
        return false;
    }
    jclass loaderCls = env->GetObjectClass(loader);
    jmethodID midLoad = env->GetMethodID(
        loaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (!midLoad || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        elog::error("jni/saf: loadClass indisponível no classloader");
        return false;
    }
    jstring jname = env->NewStringUTF("vv.goni.SafIo");
    jclass ioCls = static_cast<jclass>(
        env->CallObjectMethod(loader, midLoad, jname));
    env->DeleteLocalRef(jname);
    if (!ioCls || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        elog::error("jni/saf: loadClass(vv.goni.SafIo) FALHOU — classe ausente do dex?");
        return false;
    }
    elog::info("jni/saf: vv.goni.SafIo carregada");
    if (!SafJniBackend::initClass(vmP, env, ioCls)) {
        elog::error("jni/saf: initClass falhou (global ref?)");
        return false;
    }
    // URI inicial vazia — a verdadeira chega quando o utilizador escolhe
    jstring empty = env->NewStringUTF("");
    g_safBackendReady = g_safBackend.init(env, ioCls, activity, empty);
    env->DeleteLocalRef(empty);
    if (!g_safBackendReady) {
        return false;
    }
    const bool singles = g_safBackend.initSingles(env);
    elog::info("jni/saf: backend pronto (io=%s singles=%s)",
               "ok", singles ? "ok" : "FALHA");
    return singles;
}

SafBackend* safJniBackend() {
    return g_safBackendReady
               ? static_cast<SafBackend*>(&g_safBackend)
               : nullptr;
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
