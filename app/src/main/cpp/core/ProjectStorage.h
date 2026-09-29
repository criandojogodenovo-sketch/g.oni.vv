#pragma once
// core/ProjectStorage.h — interface de storage do projeto (F5-A).
//
// Abstrai ONDE o projeto vive: F5 usa FsStorage (raiz = getExternalFilesDir
// no device, /tmp nos testes); F5.2 (SAF) entra como OUTRA implementação
// desta mesma interface — nada acima dela muda.
//
// CAMINHOS: sempre RELATIVOS à raiz do projeto, separador '/', sem '..' e
// nunca absolutos (validRelPath). Refs entre ficheiros do projeto (manifesto
// → cena, MeshRenderer → mesh/textura) guardam exatamente estes caminhos
// relativos: reabrir o projeto noutro device mantém os refs intactos.
//
// GL-free / Android-free: compila e roda na suíte do CI Linux.
#include <cstddef>
#include <string>
#include <vector>
#include "core/Types.h"

namespace vv {

class ProjectStorage {
public:
    virtual ~ProjectStorage() = default;

    // Raiz real (diagnóstico/telemetria — o engine não depende do valor).
    virtual std::string root() const = 0;

    // mkdir -p do diretório relativo (cria a raiz se preciso).
    virtual bool makeDirs(const std::string& relDir) = 0;

    virtual bool exists(const std::string& relPath) const = 0;

    virtual bool writeText(const std::string& relPath, const std::string& text) = 0;
    virtual bool readText(const std::string& relPath, std::string& out) const = 0;

    virtual bool writeBytes(const std::string& relPath, const void* data, size_t n) = 0;
    virtual bool readBytes(const std::string& relPath, std::vector<u8>& out) const = 0;

    // Nomes de FICHEIROS (não diretórios) dentro de relDir, ordenados.
    // false se relDir não existe; lista vazia = diretório sem ficheiros.
    virtual bool listDir(const std::string& relDir,
                         std::vector<std::string>& outFiles) const = 0;
};

// Guarda de caminho: true se é um rel-path seguro ("scenes/main.goni").
// Recusa: vazio, absoluto ('/'), "..", segmento vazio ("a//b") e '\\'.
bool validRelPath(const std::string& rel);

// Junta raiz + rel (para implementações com FS real); rel TEM de passar em
// validRelPath — devolve "" caso contrário.
std::string joinRelPath(const std::string& root, const std::string& rel);

} // namespace vv
