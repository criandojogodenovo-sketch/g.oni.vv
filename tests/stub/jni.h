// tests/stub/jni.h — FAKE JNI CONTROLÁVEL para o hospedeiro (F5.3).
//
// HISTÓRIA: era um stub mínimo de assinaturas para o CHECK DE SINTAXE de
// StorageBridge.cpp no CI (o NDK tem o jni.h real). Com o handshake
// INVERTIDO (F5.3), o StorageBridge.cpp passou a compilar DENTRO da suíte
// de testes (tests/CMakeLists.txt) e o "stub Java regista activity" da
// TAREFA 5 é literal: o teste chama Java_vv_goni_VvActivity_nativeRegisterActivity
// diretamente contra este fake — o mesmo código que corre no device é
// exercido no hospedeiro (registo, attach, fluxo de permissão, mensagens).
//
// O estado fake (g_jni) é uma variável inline C++17 — UMA instância por
// binário, partilhada entre o TU do bridge e o TU de testes. reset() volta
// ao estado inicial no início de cada caso.
//
// A build Android NUNCA vê este ficheiro (include dirs dos testes só).
#pragma once
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

typedef int32_t jint;
typedef int64_t jlong;
typedef uint8_t jboolean;
typedef int8_t jbyte;
typedef uint16_t jchar;
typedef int16_t jshort;
typedef float jfloat;
typedef double jdouble;
typedef jint jsize;

#define JNI_OK 0
#define JNI_ERR (-1)
#define JNI_EDETACHED (-2)
#define JNI_EEXIST (-3)
#define JNI_EINVAL (-4)
#define JNI_VERSION_1_6 0x00010006
#define JNI_TRUE 1
#define JNI_FALSE 0
#define JNIEXPORT
#define JNICALL

struct _jobject;
typedef _jobject* jobject;
typedef _jobject* jclass;
typedef _jobject* jstring;
typedef _jobject* jbyteArray;
typedef _jobject* jobjectArray;
typedef void* jmethodID;

struct JNIEnv;
struct JavaVM;

typedef struct {
    const char* name;
    const char* signature;
    void* fnPtr;
} JNINativeMethod;

// F5.3: argumentos do attach (o nome da thread aparece nos logs do device)
typedef struct JavaVMAttachArgs {
    jint version;
    const char* name;
    jobject group;
} JavaVMAttachArgs;

// helper: ponteiro fake a partir de um valor pequeno (os ponteiros opacos
// do fake nunca são desreferenciados — só comparados/gravados)
template <typename T>
inline T fakePtr(intptr_t v) {
    return reinterpret_cast<T>(static_cast<intptr_t>(v));
}

// ---- estado fake global (inline = 1 instância por binário) ------------------
struct JniFake {
    void reset() { *this = JniFake{}; }

    // env fake entregue a Java/attach (ponteiro opaco — o fake JNIEnv vive
    // por baixo); as chamadas reais passam-no de volta como JNIEnv*
    JNIEnv* env = fakePtr<JNIEnv*>(0x3000);

    // VM: thread atual anexado? (GetEnv: anexado → OK; senão → EDETACHED,
    // como a JNI real). attach_rc/getjavavm_rc simulam falhas com código.
    bool vm_attached = false;
    int  attach_rc = 0;      // rc do AttachCurrentThread (0 = OK)
    int  getjavavm_rc = 0;   // rc do GetJavaVM (0 = OK)
    std::string attached_thread_name;    // nome passado no attach
    int  attach_calls = 0;
    int  detach_calls = 0;

    // FindClass / métodos
    bool environment_class_ok = true;
    bool vvactivity_class_ok = true;
    std::vector<std::string> find_class_calls;
    std::map<std::string, bool> fail_methods;   // nome → devolver nullptr
    std::map<void*, std::string> mid_names;     // methodID → nome
    int  mid_counter = 0;

    // registo de nativos (RegisterNatives) — F5.4: também as ASSINATURAS,
    // para os testes afervelarem a paridade nome+assinatura (a divergência
    // de assinatura entre a Java e a tabela era uma classe de bug possível
    // do UnsatisfiedLinkError)
    int register_natives_calls = 0;
    std::vector<std::string> register_natives_names;
    std::vector<std::string> register_natives_sigs;

    // chamadas gravadas
    std::vector<std::pair<std::string, long>> void_calls;   // (método, arg int)
    std::vector<std::string> static_bool_calls;
    bool manager_result = false;     // valor de isExternalStorageManager()
    int  export_int_result = 3;      // resultado do exportLogsToDownloads
    std::string last_new_string;     // última NewStringUTF

    // strings fabricadas (NewStringUTF → GetStringUTFChars)
    std::map<void*, std::string> strings;
    int refs_new = 0;
    int refs_del = 0;

    // F5.4: arrays fake de strings (conteúdo guardado aqui; os elementos
    // materializam como jstrings no GetObjectArrayElement)
    std::map<void*, std::vector<std::string>> fake_arrays;

    // F5.4 — a "Java fake" (a VvActivity do teste): callbacks por método
    // bridge; não definidos = comportamento de falha honesta (sentinela)
    std::function<std::string(const std::string&)> bridge_root_doc;
    std::function<int(const std::string&, const std::string&)> bridge_open_fd;
    std::function<std::vector<std::string>(const std::string&)> bridge_list;
    std::function<std::string(const std::string&, const std::string&,
                              const std::string&)> bridge_create;
    std::function<bool(const std::string&)> bridge_delete;
    // F5.4-hotfix — bridgeFindFile (tri-estado): devolve URI / "" (ausência
    // confirmada) / null (query falhou) — find_file_null = chamadas restantes
    // a devolver null (indecidido), para simular falha pontual + recuperação
    std::function<std::string(const std::string&, const std::string&)> bridge_find_file;
    int find_file_null = 0;            // nº de chamadas que devolvem null
    int  find_file_calls = 0;          // aferir a repetição da query

    jstring newString(const char* s) {
        void* p = reinterpret_cast<void*>(static_cast<intptr_t>(
            0x50000000L + strings.size()));
        strings[p] = s;
        return static_cast<jstring>(p);
    }

    // nome do methodID ("?" se desconhecido)
    std::string midName(jmethodID m) const {
        auto it = mid_names.find(m);
        return it != mid_names.end() ? it->second : "?";
    }

    // conteúdo de uma jstring fabricada (por valor — o mapa pode crescer)
    std::string strOf(jstring s) const {
        auto it = strings.find(reinterpret_cast<void*>(s));
        return it != strings.end() ? it->second : std::string();
    }
};

inline JniFake g_jni;

// instância única da VM fake (os testes passam fakeVm() ao JNI_OnLoad)
inline JavaVM* fakeVm();

// ---- JavaVM fake ------------------------------------------------------------
struct JavaVM {
    jint GetEnv(void** pe, jint) {
        if (g_jni.vm_attached) {
            *pe = fakePtr<void*>(0x3000);
            return JNI_OK;
        }
        return JNI_EDETACHED;
    }
    jint AttachCurrentThread(JNIEnv** pe, void* thr_args) {
        ++g_jni.attach_calls;
        if (g_jni.attach_rc == 0) {
            g_jni.vm_attached = true;
            if (thr_args) {
                g_jni.attached_thread_name =
                    static_cast<JavaVMAttachArgs*>(thr_args)->name
                        ? static_cast<JavaVMAttachArgs*>(thr_args)->name
                        : "";
            }
            *pe = fakePtr<JNIEnv*>(0x3000);
        }
        return static_cast<jint>(g_jni.attach_rc);
    }
    jint DetachCurrentThread() {
        ++g_jni.detach_calls;
        g_jni.vm_attached = false;
        return JNI_OK;
    }
};

inline JavaVM* fakeVm() {
    static JavaVM vm;   // única por binário (C++11: init thread-safe)
    return &vm;
}

// ---- JNIEnv fake ------------------------------------------------------------
struct JNIEnv {
    jclass GetObjectClass(jobject o) { return o; }

    jclass FindClass(const char* n) {
        g_jni.find_class_calls.push_back(n);
        if (std::strcmp(n, "android/os/Environment") == 0) {
            return g_jni.environment_class_ok ? fakePtr<jclass>(0x1001)
                                              : nullptr;
        }
        if (std::strcmp(n, "vv/goni/VvActivity") == 0) {
            return g_jni.vvactivity_class_ok ? fakePtr<jclass>(0x1002)
                                             : nullptr;
        }
        return nullptr;
    }

    jint RegisterNatives(jclass, const JNINativeMethod* m, jint n) {
        ++g_jni.register_natives_calls;
        for (jint i = 0; i < n; ++i) {
            g_jni.register_natives_names.push_back(m[i].name);
            g_jni.register_natives_sigs.push_back(m[i].signature);
        }
        return JNI_OK;
    }

    jmethodID GetMethodID(jclass, const char* n, const char* sig) {
        if (g_jni.fail_methods[n]) {
            return nullptr;
        }
        void* mid = reinterpret_cast<void*>(
            static_cast<intptr_t>(0x2000 + ++g_jni.mid_counter));
        g_jni.mid_names[mid] = n;
        return static_cast<jmethodID>(mid);
    }

    jmethodID GetStaticMethodID(jclass, const char* n, const char* sig) {
        if (g_jni.fail_methods[n]) {
            return nullptr;
        }
        void* mid = reinterpret_cast<void*>(
            static_cast<intptr_t>(0x2000 + ++g_jni.mid_counter));
        g_jni.mid_names[mid] = n;
        return static_cast<jmethodID>(mid);
    }

    jobject CallObjectMethod(jobject, jmethodID m, ...) {
        const std::string name = g_jni.midName(m);
        if (name == "toString") {
            return nullptr;   // comportamento antigo (uri→string fica "")
        }
        va_list ap;
        va_start(ap, m);
        if (name == "bridgeRootDoc" && g_jni.bridge_root_doc) {
            jstring jt = va_arg(ap, jstring);
            va_end(ap);
            const std::string r = g_jni.bridge_root_doc(g_jni.strOf(jt));
            return r.empty() ? nullptr : g_jni.newString(r.c_str());
        }
        if (name == "bridgeCreate" && g_jni.bridge_create) {
            jstring jp = va_arg(ap, jstring);
            jstring jm = va_arg(ap, jstring);
            jstring jn = va_arg(ap, jstring);
            va_end(ap);
            const std::string r = g_jni.bridge_create(
                g_jni.strOf(jp), g_jni.strOf(jm), g_jni.strOf(jn));
            return r.empty() ? nullptr : g_jni.newString(r.c_str());
        }
        if (name == "bridgeList" && g_jni.bridge_list) {
            jstring jd = va_arg(ap, jstring);
            va_end(ap);
            const std::vector<std::string> flat = g_jni.bridge_list(g_jni.strOf(jd));
            void* p = reinterpret_cast<void*>(static_cast<intptr_t>(
                0x60000000L + g_jni.fake_arrays.size()));
            g_jni.fake_arrays[p] = flat;
            return static_cast<jobject>(p);
        }
        if (name == "bridgeFindFile") {
            jstring jd = va_arg(ap, jstring);
            jstring jn = va_arg(ap, jstring);
            va_end(ap);
            ++g_jni.find_file_calls;
            if (g_jni.find_file_null > 0) {
                --g_jni.find_file_null;
                return nullptr;   // null = query FALHOU (indecidido)
            }
            if (!g_jni.bridge_find_file) {
                return nullptr;   // sem callback = falha honesta
            }
            // "" VÁLIDO aqui (ausência confirmada) — NÃO é null: o contrato
            // tri-estado do bridge distingue as duas respostas
            const std::string r = g_jni.bridge_find_file(g_jni.strOf(jd),
                                                         g_jni.strOf(jn));
            return g_jni.newString(r.c_str());
        }
        va_end(ap);
        return nullptr;
    }

    jint CallIntMethod(jobject, jmethodID m, ...) {
        const std::string name = g_jni.midName(m);
        va_list ap;
        va_start(ap, m);
        if (name == "bridgeOpenFd" && g_jni.bridge_open_fd) {
            jstring ju = va_arg(ap, jstring);
            jstring jm = va_arg(ap, jstring);
            va_end(ap);
            return static_cast<jint>(
                g_jni.bridge_open_fd(g_jni.strOf(ju), g_jni.strOf(jm)));
        }
        va_end(ap);
        return static_cast<jint>(g_jni.export_int_result);
    }

    jboolean CallBooleanMethod(jobject, jmethodID m, ...) {
        const std::string name = g_jni.midName(m);
        va_list ap;
        va_start(ap, m);
        if (name == "bridgeDelete" && g_jni.bridge_delete) {
            jstring jd = va_arg(ap, jstring);
            va_end(ap);
            return g_jni.bridge_delete(g_jni.strOf(jd)) ? JNI_TRUE : JNI_FALSE;
        }
        va_end(ap);
        return JNI_FALSE;
    }

    jobject CallStaticObjectMethod(jclass, jmethodID, ...) { return nullptr; }

    jboolean CallStaticBooleanMethod(jclass, jmethodID, ...) {
        g_jni.static_bool_calls.push_back("isExternalStorageManager");
        return g_jni.manager_result ? JNI_TRUE : JNI_FALSE;
    }

    void CallVoidMethod(jobject, jmethodID m, ...) {
        // gravar o método + o 1º vararg inteiro (request code do
        // openAllFilesSettings(4301) — exatamente o que o device passa)
        va_list ap;
        va_start(ap, m);
        const jint arg = va_arg(ap, jint);
        va_end(ap);
        const std::string& name = g_jni.mid_names.count(reinterpret_cast<void*>(m))
                                      ? g_jni.mid_names[reinterpret_cast<void*>(m)]
                                      : "?";
        g_jni.void_calls.push_back({name, static_cast<long>(arg)});
    }

    jobject NewGlobalRef(jobject o) { ++g_jni.refs_new; return o; }
    void DeleteGlobalRef(jobject) { ++g_jni.refs_del; }
    void DeleteLocalRef(jobject) {}

    jstring NewStringUTF(const char* s) {
        g_jni.last_new_string = s ? s : "";
        return g_jni.newString(s ? s : "");
    }

    const char* GetStringUTFChars(jstring s, jboolean*) {
        auto it = g_jni.strings.find(reinterpret_cast<void*>(s));
        return it != g_jni.strings.end() ? it->second.c_str() : "";
    }
    void ReleaseStringUTFChars(jstring, const char*) {}

    jboolean ExceptionCheck() { return JNI_FALSE; }
    void ExceptionClear() {}
    jsize GetArrayLength(jobjectArray a) {
        auto it = g_jni.fake_arrays.find(reinterpret_cast<void*>(a));
        return it == g_jni.fake_arrays.end()
                   ? 0
                   : static_cast<jsize>(it->second.size());
    }
    jbyteArray NewByteArray(jsize) { return nullptr; }
    void GetByteArrayRegion(jbyteArray, jsize, jsize, jbyte*) {}
    void SetByteArrayRegion(jbyteArray, jsize, jsize, const jbyte*) {}
    jobject GetObjectArrayElement(jobjectArray a, jsize i) {
        auto it = g_jni.fake_arrays.find(reinterpret_cast<void*>(a));
        if (it == g_jni.fake_arrays.end() || i < 0 ||
            i >= static_cast<jsize>(it->second.size())) {
            return nullptr;
        }
        return g_jni.newString(it->second[static_cast<size_t>(i)].c_str());
    }

    // F5.3: registo da activity (handshake invertido)
    jint GetJavaVM(JavaVM** vm) {
        *vm = fakeVm();
        return static_cast<jint>(g_jni.getjavavm_rc);
    }
};
