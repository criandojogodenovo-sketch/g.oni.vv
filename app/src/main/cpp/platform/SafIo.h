#pragma once
// platform/SafIo.h — interface PURA de I/O SAF (F5.4, Gestor de Projetos).
//
// O SAF entrou de volta pela porta certa: APENAS para escolher a pasta de
// CADA projeto no ecrã inicial (Gestor de Projetos, estilo Godot) —
// ACTION_OPEN_DOCUMENT_TREE + takePersistableUriPermission, pastas
// diferentes por projeto, SEM depender de All Files Access. O All Files
// Access mantém-se para import/export de assets DENTRO de um projeto
// aberto — as duas coisas COEXISTEM (não se substituem).
//
// Contrato: o native fala em URIs de DOCUMENTO (strings) que RECEBE da
// Java (rootDoc/resolveChild/create devolvem; openFd/list/remove
// consomem) — o native NUNCA constrói URIs (DocumentsContract é Java).
// Toda falha devolve false + err com a causa REAL (as mensagens honestas
// do contrato — "ponte Java indisponível (handshake)" quando a ponte está
// em baixo, nunca um "sistema sem suporte" que mente).
//
// GL-free / Android-free: a suíte do CI implementa esta interface com um
// modelo em memória (test_saf_project) e o core/SafStorage é testado
// contra ela — a ponte JNI real (JniSafIo, device-only) só traduz para
// os métodos bridge da VvActivity.
#include <string>
#include <vector>

namespace vv::storage {

// mime dos DIRETÓRIOS no DocumentsContract (constante do framework)
inline const char* kSafDirMime = "vnd.android.document/directory";

// entrada de um filho de diretório (list)
struct SafEntry {
    std::string uri;    // URI de documento do filho (string)
    std::string name;   // display name
    std::string mime;   // mime type (kSafDirMime nos diretórios)
};

class SafIo {
public:
    virtual ~SafIo() = default;

    // URI de DOCUMENTO da raiz de um treeUri (buildDocumentUriUsingTree —
    // o único salto tree→documento, feito do lado Java)
    virtual bool rootDoc(const std::string& treeUri, std::string& outDocUri,
                         std::string& err) = 0;

    // abre um documento como fd POSIX ("r" leitura, "w" escrita truncante;
    // o fd é PROPRIEDADE do chamador — fechar grava no provider)
    virtual bool openFd(const std::string& docUri, const char* mode,
                        int* outFd, std::string& err) = 0;

    // filhos de um diretório (flat: nome+mime+uri; a ordem é a do provider
    // — SafStorage ordena os FICHEIROS que entrega à UI)
    virtual bool list(const std::string& dirDocUri,
                      std::vector<SafEntry>& out, std::string& err) = 0;

    // cria documento (ficheiro ou diretório) num pai; devolve o URI criado
    // ATENÇÃO (semântica dos providers): nome duplicado NÃO falha — cria
    // "nome (1)". SafStorage SEMPRE resolve antes de criar (nunca duplica).
    virtual bool create(const std::string& parentDocUri, const char* mime,
                        const char* displayName, std::string& outDocUri,
                        std::string& err) = 0;

    // apaga documento (true = apagado; false = err com a causa)
    virtual bool remove(const std::string& docUri, std::string& err) = 0;

    // resolve um FILHO por nome — found=true + outUri se existe.
    // CONTRATO TRI-ESTADO (F5.4-hotfix — a semântica que mata a duplicação):
    //   true  + found=true  → existe (reabrir outUri; escrever "wt")
    //   true  + found=false → ausência CONFIRMADA (só aqui se chama create)
    //   false              → a VERIFICAÇÃO falhou (err com a causa) — o
    //                        chamador NUNCA decide criação por "não sei"
    // Implementação DEFAULT sobre list() (o modelo de testes; o JniSafIo no
    // device usa a query DEDICADA bridgeFindFile — displayName exato, não
    // confia no list da primeira tentativa).
    virtual bool resolveChild(const std::string& dirDocUri, const char* name,
                              bool& found, std::string& outUri,
                              std::string& err) {
        found = false;
        outUri.clear();
        std::vector<SafEntry> kids;
        if (!list(dirDocUri, kids, err)) {
            return false;
        }
        for (const SafEntry& k : kids) {
            if (k.name == name) {
                found = true;
                outUri = k.uri;
                break;
            }
        }
        return true;
    }
};

} // namespace vv::storage
