// tests/FakeStorage.h — ProjectStorage em memória, partilhado pelos testes.
// Mesmo contrato de guarda de caminhos das implementações reais: toda
// operação recusa rel-path inválido (validRelPath).
//
// F5.4-hotfix: implementar probe() (tri-estado) e injetores de falha
// (failProbe/failWrites) para afervelar os caminhos honestos — Unknown
// NUNCA vira "não existe" e falha de escrita propaga com a causa.
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>
#include "core/ProjectStorage.h"

struct FakeStorage final : public vv::ProjectStorage {
    std::string rootPath = "/fake";
    std::map<std::string, std::string> files;
    std::set<std::string> dirs;
    // injetores de falha (o comportamento real de um provider em problemas)
    bool failProbe = false;    // probe → Unknown (verificação indecidida)
    bool failWrites = false;   // writeText/writeBytes → false

    std::string root() const override { return rootPath; }
    bool makeDirs(const std::string& relDir) override {
        if (!vv::validRelPath(relDir)) return false;
        dirs.insert(relDir);
        return true;
    }
    bool exists(const std::string& relPath) const override {
        if (!vv::validRelPath(relPath)) return false;
        return files.count(relPath) != 0 || dirs.count(relPath) != 0;
    }
    vv::Presence probe(const std::string& relPath) const override {
        if (failProbe || !vv::validRelPath(relPath)) {
            return vv::Presence::Unknown;   // indecidido — honesto
        }
        if (files.count(relPath) != 0 || dirs.count(relPath) != 0) {
            return vv::Presence::Present;
        }
        // pai confirmado ausente → filho ausente (mesma recursão do real);
        // pai indecidido não acontece aqui (failProbe é global)
        const size_t slash = relPath.rfind('/');
        if (slash != std::string::npos) {
            const std::string parent = relPath.substr(0, slash);
            if (files.count(parent) == 0 && dirs.count(parent) == 0) {
                return vv::Presence::Absent;
            }
        }
        return vv::Presence::Absent;
    }
    bool writeText(const std::string& relPath, const std::string& text) override {
        if (failWrites || !vv::validRelPath(relPath)) return false;
        files[relPath] = text;
        return true;
    }
    bool readText(const std::string& relPath, std::string& out) const override {
        if (!vv::validRelPath(relPath)) return false;
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        out = it->second;
        return true;
    }
    bool writeBytes(const std::string& relPath, const void* data, size_t n) override {
        if (failWrites || !vv::validRelPath(relPath)) return false;
        files[relPath].assign(static_cast<const char*>(data), n);
        return true;
    }
    bool readBytes(const std::string& relPath, std::vector<vv::u8>& out) const override {
        if (!vv::validRelPath(relPath)) return false;
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        out.assign(it->second.begin(), it->second.end());
        return true;
    }
    bool remove(const std::string& relPath) override {
        if (!vv::validRelPath(relPath)) return false;
        return files.erase(relPath) != 0;
    }
    bool listDir(const std::string& relDir,
                 std::vector<std::string>& outFiles) const override {
        if (!vv::validRelPath(relDir) || dirs.count(relDir) == 0) return false;
        outFiles.clear();
        const std::string prefix = relDir + "/";
        for (const auto& kv : files) {
            if (kv.first.compare(0, prefix.size(), prefix) == 0 &&
                kv.first.find('/', prefix.size()) == std::string::npos) {
                outFiles.push_back(kv.first.substr(prefix.size()));
            }
        }
        return true;
    }
};
