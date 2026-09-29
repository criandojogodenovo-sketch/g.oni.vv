#pragma once
// platform/FileApi.h — File API POSIX DIRETA (F5.2), o caminho primário de
// I/O de ficheiros do utilizador quando o All Files Access está concedido.
//
// Diferença do core/FsStorage (projeto): aqui o alvo é o ARMAZENAMENTO
// PÚBLICO (/storage/emulated/0/Download, /Documents) — leitura de candidatos
// a import (obj/gltf/glb/png) e escrita de exports num sítio visível no
// gestor de ficheiros. Toda a operação loga errno em falha (self-check do
// dono: a CAUSA aparece no engine.log, sem PC).
//
// GL-free e sem Android: no device as pastas chegam das constantes abaixo;
// nos testes do CI passam-se caminhos de /tmp — a lógica é a mesma.
#include <string>
#include <vector>
#include "core/Types.h"

namespace vv::fileapi {

// raiz do armazenamento externo no device (o mesmo padrão do fallback do
// elog: caminho fixo conhecido; getExternalStorageDirectory é deprecated)
constexpr const char* kExternalRoot = "/storage/emulated/0";

// pastas varridas para IMPORT (ordem; as duas que o dono usa com o browser)
constexpr const char* kImportDirs[] = {"Download", "Documents"};
constexpr int       kImportDirCount = 2;

// pasta de EXPORT no armazenamento público (relativa a kExternalRoot) —
// o ficheiro fica em Download/GOneVV/export/export_<nome>.obj
constexpr const char* kExportRelDir = "Download/GOneVV/export";

// candidato a import encontrado numa pasta pública
struct Candidate {
    std::string name;   // "casa.obj"
    std::string path;   // caminho absoluto
    char kind;          // 'm' = mesh (obj/gltf/glb), 't' = textura (png)
};

// extensão → tipo ('m'/'t'), 0 = não suportado (case-insensitive)
char kindOfExtension(const std::string& name);

// lista os candidatos de UMA pasta (nomes/paths absolutos, ordenados por
// nome — case-insensitive). false = opendir falhou (sem acesso/pasta
// ausente — errno já logado).
bool listCandidates(const std::string& dir, std::vector<Candidate>& out);

// leitura binária inteira. false = fopen falhou (errno logado).
bool readAll(const std::string& path, std::vector<u8>& out);

// escrita binária inteira (cria as pastas-mãe em falta; substitui se
// existir). false = mkdirs/fopen/write falharam (errno logado).
bool writeAll(const std::string& path, const void* data, size_t n);

// mkdir -p (recursivo). true = existe no fim.
bool makeDirs(const std::string& dir);

// "errno=13 (Permission denied)" — texto ESTÁVEL do último erro (o formato
// é aferido no CI: sempre "errno=<n> (<strerror>)")
std::string errnoText();

// BOOT SELF-CHECK (F5.2, item 5): escreve no engine.log o resultado do
// mapeamento da raiz (null?) + sonda opendir/fopen com errno. root=null →
// linha de erro explícita. Corre no boot ANTES de qualquer I/O pesado.
void logStorageSelfCheck(const char* root, bool isExternal);

} // namespace vv::fileapi
