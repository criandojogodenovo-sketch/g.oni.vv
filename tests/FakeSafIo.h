// tests/FakeSafIo.h — árvore SAF in-memory (docs com id fake://doc/N),
// EXTRAÍDA do test_saf_project.cpp (0.8.12) para partilhar com o harness do
// C33 VIRTUAL: o dispositivo virtual exercita o caminho content:// da
// migração (staging no cache dir) com o MESMO modelo de provider SAF que
// a suíte afere desde a F5.4 — sem duplicação.
#pragma once
// memfd_create exige _GNU_SOURCE no glibc (o test_saf_project obtinha-o por
// include de ordem; aqui é EXPLÍCITO para qualquer TU que use o modelo)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <map>
#include <string>
#include <vector>
#include <unistd.h>
// sys/memfd.h não existe em todos os sistemas (a glibc fornece o SÍMBOLO
// desde a 2.27) — protótipo explícito, assinatura estável da glibc (o mesmo
// que o test_saf_project fazia antes da extração)
extern "C" int memfd_create(const char* name, unsigned int flags);
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif
#include "platform/SafIo.h"

// (verbatim do test_saf_project.cpp; namespace global como o FakeStorage.h)
struct Node {
    std::string name;
    std::string mime;
    std::string data;      // só ficheiros
    bool isDir = false;
    std::vector<std::string> kids;   // ids (ordem de criação — NÃO ordenada)
};

struct FakeSafIo final : public vv::storage::SafIo {
    std::map<std::string, Node> docs;
    int nextId = 1;
    std::map<int, std::string> openWrites;   // fd → docUri (mode "w")
    // F5.4-hotfix: injeção de falha — list/resolveChild falham (provider
    // recusou a query). O SafStorage tem de responder Unknown/nunca criar.
    bool failQueries = false;

    FakeSafIo() {
        Node root;
        root.name = "pasta-escolhida";
        root.mime = vv::storage::kSafDirMime;
        root.isDir = true;
        docs["fake://doc/1"] = root;
    }

    std::string newId() { return "fake://doc/" + std::to_string(++nextId); }

    std::string childNamed(const std::string& dirUri, const std::string& name) {
        const auto it = docs.find(dirUri);
        if (it == docs.end()) {
            return "";
        }
        for (const std::string& kid : it->second.kids) {
            const auto k = docs.find(kid);
            if (k != docs.end() && k->second.name == name) {
                return kid;
            }
        }
        return "";
    }

    bool rootDoc(const std::string&, std::string& outDocUri,
                 std::string&) override {
        outDocUri = "fake://doc/1";
        return true;
    }

    bool list(const std::string& dirDocUri, std::vector<vv::storage::SafEntry>& out,
              std::string& err) override {
        const auto it = docs.find(dirDocUri);
        if (failQueries) {
            err = "provider recusou a query (injetado)";
            return false;
        }
        if (it == docs.end() || !it->second.isDir) {
            err = "não é pasta: " + dirDocUri;
            return false;
        }
        out.clear();
        for (const std::string& kid : it->second.kids) {
            const auto k = docs.find(kid);
            if (k != docs.end()) {
                vv::storage::SafEntry e;
                e.uri = kid;
                e.name = k->second.name;
                e.mime = k->second.mime;
                out.push_back(std::move(e));
            }
        }
        return true;
    }

    // F5.4-hotfix — modela o DocumentsContract.createDocument REAL
    // (FileUtils.buildUniqueFile):
    //   • application/octet-stream → nome verbatim (sem extensão nova);
    //   • application/json com extensão desconhecida do mime → o provider
    //     ACRESCENTA ".json" ("main.goni" → "main.goni.json") — é exatamente
    //     o rename que duplicava no device;
    //   • colisão de nome → base + " (n)" + extensão ("main.goni (1).json").
    // A suíte antiga só modelava a colisão — o rename passava invisível.
    bool create(const std::string& parentDocUri, const char* mime,
                const char* displayName, std::string& outDocUri,
                std::string& err) override {
        const auto it = docs.find(parentDocUri);
        if (it == docs.end() || !it->second.isDir) {
            err = "pai não é pasta: " + parentDocUri;
            return false;
        }
        const std::string m = mime ? mime : "";
        std::string base = displayName;
        std::string ext;
        if (m == "application/json") {
            // ext "json" é a canónica do mime → mantém; ext desconhecida
            // ("goni") → o provider trata como parte do base + acrescenta
            const size_t dot = base.rfind('.');
            const std::string leafExt =
                dot == std::string::npos ? "" : base.substr(dot + 1);
            if (leafExt != "json") {
                ext = "json";   // rename do provider (device real)
            }
        }
        std::string finalName = ext.empty() ? base : base + "." + ext;
        for (int n = 1; !childNamed(parentDocUri, finalName).empty(); ++n) {
            finalName = base + " (" + std::to_string(n) + ")" +
                        (ext.empty() ? "" : "." + ext);
        }
        const std::string id = newId();
        Node n;
        n.name = finalName;
        n.mime = m;
        n.isDir = (m == vv::storage::kSafDirMime);
        docs[id] = n;
        it->second.kids.push_back(id);
        outDocUri = id;
        return true;
    }

    // mesmo CONTRATO do bridgeFindFile no device: exato primeiro; compat
    // "name + .json" (cura dos ficheiros renomeados pela 0.6.4); erro →
    // false (nunca "não existe" por falha alheia)
    bool resolveChild(const std::string& dirDocUri, const char* name,
                      bool& found, std::string& outUri,
                      std::string& err) override {
        found = false;
        outUri.clear();
        if (failQueries) {
            err = "provider recusou a query (injetado)";
            return false;
        }
        std::string hit = childNamed(dirDocUri, name);
        if (hit.empty()) {
            hit = childNamed(dirDocUri, std::string(name) + ".json");
        }
        if (hit.empty()) {
            return true;   // ausência CONFIRMADA (found=false, sem erro)
        }
        found = true;
        outUri = hit;
        return true;
    }

    bool remove(const std::string& docUri, std::string& err) override {
        const auto it = docs.find(docUri);
        if (it == docs.end()) {
            err = "doc ausente: " + docUri;
            return false;
        }
        docs.erase(it);
        return true;
    }

    bool openFd(const std::string& docUri, const char* mode, int* outFd,
                std::string& err) override {
        const auto it = docs.find(docUri);
        if (it == docs.end()) {
            err = "doc ausente: " + docUri;
            return false;
        }
        if (it->second.isDir) {
            err = "é pasta: " + docUri;
            return false;
        }
        const int fd = ::memfd_create("vv-fake-saf", 0);
        if (fd < 0) {
            err = "memfd_create falhou";
            return false;
        }
        if (std::string(mode) == "r") {
            if (!it->second.data.empty()) {
                ::write(fd, it->second.data.data(), it->second.data.size());
            }
            ::lseek(fd, 0, SEEK_SET);
            *outFd = fd;
            return true;
        }
        // "w"/"wt" (F5.4-hotfix: o SafStorage abre "wt" — WRITE+TRUNCATE
        // explícito): o modelo guarda um DUP do memfd aberto — o fd devolvido
        // ao SafStorage é fechado por ele (no device é o COMMIT no provider);
        // o original sobrevive para o flushWrites() ler o que foi escrito.
        // Sem o dup, o kernel reutilizava o nº do fd fechado e a 2ª escrita
        // sobrepunha a 1ª no mapa (fd=3 para sempre).
        openWrites[fd] = docUri;
        *outFd = ::dup(fd);
        return true;
    }

    // o passo que no device é invisível (o provider persiste ao fechar o fd)
    size_t flushWrites() {
        size_t n = 0;
        for (const auto& kv : openWrites) {
            auto dit = docs.find(kv.second);
            if (dit == docs.end()) {
                ::close(kv.first);
                continue;
            }
            std::string data;
            char buf[4096];
            ::lseek(kv.first, 0, SEEK_SET);
            ssize_t r;
            while ((r = ::read(kv.first, buf, sizeof(buf))) > 0) {
                data.append(buf, static_cast<size_t>(r));
            }
            dit->second.data = data;
            ::close(kv.first);
            ++n;
        }
        openWrites.clear();
        return n;
    }
};

