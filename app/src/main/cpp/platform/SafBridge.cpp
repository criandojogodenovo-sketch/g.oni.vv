// platform/SafBridge.cpp — JNI do SAF (F5.1-C, device-only).
//
// Java → nativo: VvActivity.onActivityResult chama o método estático nativo
// nativoOnActivityResult — implementado aqui como JNIEXPORT (resolvido por
// nome no libgoni_vv.so, carregado pelo framework).
//
// nativo → Java: openTreePicker/openImportPicker/openExportPicker chamam
// métodos da VvActivity; o I/O (SafIo) é resolvido pelo CLASSLOADER da
// activity (FindClass num thread nativo usa o classloader de sistema, que
// não vê classes da app — workaround padrão loadClass).
#include "platform/SafBridge.h"
#include <jni.h>
#include <cstring>

namespace vv::saf {

namespace {

JavaVM* g_vm = nullptr;
jobject g_activity = nullptr;
jclass g_activityCls = nullptr;
jclass g_safIoCls = nullptr;

jmethodID g_midOpenTree = nullptr;
jmethodID g_midOpenImport = nullptr;
jmethodID g_midOpenExport = nullptr;

ResultHandler g_handler = nullptr;
void* g_handlerUser = nullptr;

JNIEnv* envOrNull() {
    if (!g_vm) {
        return nullptr;
    }
    JNIEnv* env = nullptr;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return nullptr;
    }
    return env;
}

jclass loadAppClass(JNIEnv* env, const char* name) {
    // FindClass no thread nativo = classloader de sistema → não vê classes
    // da app. Usa o classloader DA ACTIVITY (padrão para NativeActivity).
    jclass clsObj = env->GetObjectClass(g_activity);
    jmethodID midLoader = env->GetMethodID(
        clsObj, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject loader = env->CallObjectMethod(g_activity, midLoader);
    if (!loader) {
        return nullptr;
    }
    jclass clsLoader = env->GetObjectClass(loader);
    jmethodID midLoad = env->GetMethodID(
        clsLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring jname = env->NewStringUTF(name);
    jclass result =
        static_cast<jclass>(env->CallObjectMethod(loader, midLoad, jname));
    env->DeleteLocalRef(jname);
    return result;
}

} // namespace

void setHandler(ResultHandler fn, void* user) {
    g_handler = fn;
    g_handlerUser = user;
}

void initJava(void* vm, void* activityObject) {
    g_vm = static_cast<JavaVM*>(vm);
    JNIEnv* env = envOrNull();
    if (!env || !activityObject) {
        return;
    }
    g_activity = env->NewGlobalRef(static_cast<jobject>(activityObject));
    g_activityCls = static_cast<jclass>(
        env->NewGlobalRef(env->GetObjectClass(g_activity)));

    g_midOpenTree = env->GetMethodID(g_activityCls, "openTreePicker", "(I)V");
    g_midOpenImport = env->GetMethodID(g_activityCls, "openImportPicker", "(I)V");
    g_midOpenExport = env->GetMethodID(
        g_activityCls, "openExportPicker", "(ILjava/lang/String;)V");

    jclass io = loadAppClass(env, "vv.goni.SafIo");
    if (io) {
        g_safIoCls = static_cast<jclass>(env->NewGlobalRef(io));
    }
}

bool openTreePicker(void* activityObject, i32 request) {
    JNIEnv* env = envOrNull();
    if (!env || !g_activity || !g_midOpenTree) {
        return false;
    }
    env->CallVoidMethod(g_activity, g_midOpenTree, static_cast<jint>(request));
    return !env->ExceptionCheck();
}

bool openImportPicker(void* activityObject, i32 request) {
    JNIEnv* env = envOrNull();
    if (!env || !g_activity || !g_midOpenImport) {
        return false;
    }
    env->CallVoidMethod(g_activity, g_midOpenImport, static_cast<jint>(request));
    return !env->ExceptionCheck();
}

bool openExportPicker(void* activityObject, i32 request,
                      const char* suggestedName) {
    JNIEnv* env = envOrNull();
    if (!env || !g_activity || !g_midOpenExport) {
        return false;
    }
    jstring jname = suggestedName ? env->NewStringUTF(suggestedName) : nullptr;
    env->CallVoidMethod(g_activity, g_midOpenExport,
                        static_cast<jint>(request), jname);
    if (jname) {
        env->DeleteLocalRef(jname);
    }
    return !env->ExceptionCheck();
}

} // namespace vv::saf

// ---- callback estático chamado pela VvActivity ------------------------------
extern "C" JNIEXPORT void JNICALL
Java_vv_goni_VvActivity_nativeOnActivityResult(JNIEnv* env, jclass,
                                               jint request, jint result,
                                               jobject uri, jint flags) {
    vv::saf::SafResult r;
    r.request = request;
    r.ok = result == 0 /* Activity.RESULT_OK */ && uri != nullptr;
    if (uri) {
        jclass uriCls = env->GetObjectClass(uri);
        jmethodID midToString =
            env->GetMethodID(uriCls, "toString", "()Ljava/lang/String;");
        jstring jstr = static_cast<jstring>(
            env->CallObjectMethod(uri, midToString));
        if (jstr) {
            const char* c = env->GetStringUTFChars(jstr, nullptr);
            if (c) {
                r.uri = c;
                env->ReleaseStringUTFChars(jstr, c);
            }
            env->DeleteLocalRef(jstr);
        }
    }
    r.flags = flags;
    if (vv::saf::g_handler) {
        vv::saf::g_handler(vv::saf::g_handlerUser, r);
    }
}
