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

// F5.4-hotfix — sonda TRI-ESTADO de existência. O bug "main.goni (1).json /
// project.goni (2) em TODO boot" existiu porque um bool confundia duas
// respostas diferentes: "não existe" (CONFIRMADO por verificação ok) e
// "não sei" (a verificação é que falhou — provider recusou/excepção).
// Criar por cima de um "não sei" é o que gera duplicados: o SAF nunca
// sobrescreve por nome — createDocument em cima de nome existente cria
// "nome (1)". Regra do projeto: só se cria com Absent CONFIRMADO; com
// Unknown NUNCA se cria nem se sobrescreve (falha honesta).
enum class Presence : u8 {
    Unknown,   // a verificação falhou — existência INDECIDIDA
    Absent,    // verificação correu e o documento NÃO está lá
    Present,   // verificação correu e o documento ESTÁ lá
};

class ProjectStorage {
public:
    virtual ~ProjectStorage() = default;

    // Raiz real (diagnóstico/telemetria — o engine não depende do valor).
    virtual std::string root() const = 0;

    // mkdir -p do diretório relativo (cria a raiz se preciso).
    virtual bool makeDirs(const std::string& relDir) = 0;

    virtual bool exists(const std::string& relPath) const = 0;

    // Sonda tri-estado (ver ProjectStorage::Presence). Implementações:
    //  - FsStorage: stat (ENOENT → Absent; outro errno → Unknown);
    //  - SafStorage: resolveChild sobre o pai (exceção/recusa → Unknown).
    virtual Presence probe(const std::string& relPath) const = 0;

    virtual bool writeText(const std::string& relPath, const std::string& text) = 0;
    virtual bool readText(const std::string& relPath, std::string& out) const = 0;

    virtual bool writeBytes(const std::string& relPath, const void* data, size_t n) = 0;
    virtual bool readBytes(const std::string& relPath, std::vector<u8>& out) const = 0;

    // 0.8.10 — ESCRITA STREAMING (import de ficheiros grandes: a fonte entra
    // por chunks — NUNCA o ficheiro inteiro em RAM). Contrato:
    //   openWriteStream(rel): abre/cria/trunca → handle > 0 (ou -1 = falha);
    //   writeStreamChunk(h, data, n): acrescenta (false = falha/cheio demais);
    //   closeWriteStream(h): fecha e persiste (idempotente).
    // Implementações REAIS (FsStorage FILE*, SafStorage fd SAF) escrevem em
    // streaming de verdade; o DEFAULT acumula com teto (kStreamAccumMax) e
    // faz writeBytes no close — FakeStorage e testes herdam isso.
    virtual int  openWriteStream(const std::string& relPath);
    virtual bool writeStreamChunk(int handle, const void* data, size_t n);
    virtual void closeWriteStream(int handle);

    // 0.8.10 — remove um ficheiro do projeto (setting "largar a fonte":
    // source/<nome> depois de convertido com sucesso). Default: false
    // ("não suportado"); Fs/Saf implementam de verdade.
    virtual bool remove(const std::string& relPath);

    // Nomes de FICHEIROS (não diretórios) dentro de relDir, ordenados.
    // false se relDir não existe; lista vazia = diretório sem ficheiros.
    virtual bool listDir(const std::string& relDir,
                         std::vector<std::string>& outFiles) const = 0;

    // 0.9.6 (G6 · R-017) — tamanho de UM ficheiro do projeto (o bench do
    // relatório precisa do TOTAL em bytes sem LER os ficheiros).
    // default: false (não suportado — o SAF não tem stat barato; o bench
    // reporta "não medido" — nunca inventa). FsStorage = stat real;
    // FakeStorage = bytes em memória (o harness MEDE o projeto dele).
    virtual bool statBytes(const std::string& relPath, u64& outBytes) const {
        (void)relPath; (void)outBytes;
        return false;
    }
};

// teto do DEFAULT acumulador de escrita streaming (implementações reais
// não passam por aqui — Fs/Saf escrevem direto ao fd)
constexpr size_t kStreamAccumMax = 256ull * 1024 * 1024;

// Guarda de caminho: true se é um rel-path seguro ("scenes/main.goni").
// Recusa: vazio, absoluto ('/'), "..", segmento vazio ("a//b") e '\\'.
bool validRelPath(const std::string& rel);

// Junta raiz + rel (para implementações com FS real); rel TEM de passar em
// validRelPath — devolve "" caso contrário.
std::string joinRelPath(const std::string& root, const std::string& rel);

} // namespace vv
