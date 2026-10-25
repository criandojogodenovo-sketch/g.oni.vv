// tests/test_010m_medicoes.cpp — 0.10-M (PASSO 1c) — MEDIÇÕES REAIS do
// import dos 3 modelos do dono (dragão, Happy Buddha, scene) POR FASE
// (cópia, parse JSON, fusão+ranges, escrita) + RAM de pico, no CAMINHO
// DE PRODUÇÃO (convert::importFile + FsStorage real). Medido, não
// estimado — o relatório 0.10-M PASSO 1 cita estas linhas.
//
// OS PERFIS, o GERADOR GLB e os MEDIDORES vivem no toolkit partilhado
// tests/Medicoes010m.h desde o PASSO 6 (a validação final usa o MESMO
// byte do perfil — uma fonte de verdade; o comentário completo do
// gerador e das cópias de perfil vive LÁ).
//
// Cada medição corre num PROCESSO FILHO (fork) com o pico de RSS
// REINICIADO (/proc/self/clear_refs "5" — o kernel zera o VmHWM): a RAM
// medida é a do import, sem o ruído do processo-pai. Os temp dirs são
// SEMPRE removidos (a lição do wiring010: /tmp nunca fica sujo).
#include "TestFramework.h"

#include "Medicoes010m.h"   // PASSO 6: os perfis/gerador/medidores partilhados

#include "assets/AssetConverter.h"
#include "assets/ResourceManager.h"
#include "core/Bench.h"
#include "core/FsStorage.h"
#include "platform/EngineLog.h"

#include <chrono>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>

using namespace medicoes;   // o toolkit partilhado (PASSO 6)


// ---- O TESTE: um filho por perfil; a linha impressa é a prova ----------

TEST(medicoes_010m_perfis_do_dono_por_fase) {
    for (const Profile& p : kProfiles) {
        const pid_t pid = ::fork();
        ASSERT(pid >= 0);
        if (pid == 0) {
            // ---- FILHO: gerar → reset do pico → import → medir ----------
            const std::string dir =
                std::string("/tmp/goni_010m_") + p.name + "_" +
                std::to_string(int(::getpid()));
            const std::string src = dir + "/fonte.glb";
            u64 glbBytes = 0;
            rmRf(dir);   // resto de corrida anterior — nunca (lição w010)
            if (!mkDir(dir)) {
                std::printf("  [010m] %s: FALHA a criar %s\n", p.name,
                            dir.c_str());
                std::fflush(stdout);
                ::_exit(21);
            }
            const auto tGen = std::chrono::steady_clock::now();
            if (!generateGlb(p, src, glbBytes)) {
                std::printf("  [010m] %s: FALHA a gerar o perfil\n",
                            p.name);
                std::fflush(stdout);
                childCleanup(dir);
                ::_exit(20);
            }
            const double genMs =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - tGen)
                    .count();

            // o pico de RAM mede SÓ o import (o gerador já passou)
            const bool peakReset = resetPeakRss();
            const u64 rssBefore = currentRssKb();

            const std::string logs = dir + "/logs";
            elog::init(logs.c_str());
            FsStorage st((dir + "/proj").c_str());
            convert::Output out;
            convert::Stats stats;
            std::string err;
            const auto t0 = std::chrono::steady_clock::now();
            const bool ok = convert::importFile(src, "fonte.glb", st,
                                                nullptr, out, stats, err,
                                                nullptr, nullptr);
            const double totalMs =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0)
                    .count();
            const u64 peakKb = bench::readPeakRssKb();

            // ---- as fases, pelos MARCADORES de log com timestamps ------
            const std::vector<LogMark> marks =
                readMarks(logs + "/engine.log");
            const double copyMs =
                spanBetween(marks, "I/GONI", "import: fonte copiada");
            const double parseMs = spanBetween(marks, "import: fonte copiada",
                                               "parse ok");
            // no caminho streaming: corte = parse ok → limites finais;
            // assembly = limites finais → registado na lista
            const double mergeMs =
                spanBetween(marks, "parse ok", "limites finais");
            const double writeMs =
                spanBetween(marks, "limites finais",
                            "registado na lista como");

            std::printf(
                "  [010m] %s: fonte=%llu B (gen %.0f ms%s) | "
                "cópia=%.0f parse=%.0f corte=%.0f assembly=%.0f | "
                "total=%.0f ms | RAM pico=%llu MB (base %llu, delta %lld)%s"
                " | verts=%u idx=%u meshes=%u | ok=%d verificado=%d "
                "err=%.120s\n",
                p.name,
                static_cast<unsigned long long>(glbBytes), genMs,
                peakReset ? "" : " (pico SEM reset)",
                copyMs, parseMs, mergeMs, writeMs, totalMs,
                static_cast<unsigned long long>(peakKb / 1024),
                static_cast<unsigned long long>(rssBefore / 1024),
                static_cast<long long>(peakKb / 1024) -
                    static_cast<long long>(rssBefore / 1024),
                "", stats.verts, stats.indices, stats.meshes, ok ? 1 : 0,
                fileHasLine(logs + "/engine.log", "verificado=1") ? 1 : 0,
                err.c_str());
            std::fflush(stdout);   // o _exit NÃO flusheia

            // ---- o VEREDITO (o PASSO 3: a parede morreu — TODOS os
            // perfis completam com a verificação bit a bit no log) ----
            // (a linha verificado=1 lê-se ANTES da limpeza do filho)
            const bool verOk =
                fileHasLine(logs + "/engine.log", "verificado=1");
            elog::shutdown();
            childCleanup(dir);
            int verdict;
            if (p.expectOk) {
                verdict = ok && out.meshes.size() == 1 && verOk ? 0 : 10;
            } else {
                // o diagnóstico do PASSO 1: morre EXATAMENTE no teto
                verdict = !ok && err.find("65535") != std::string::npos
                              ? 0 : 11;
            }
            ::_exit(verdict);
        }
        int status = 0;
        ::waitpid(pid, &status, 0);
        EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
}

// ---- R-039 · A SENTINELA DO PICO DE RAM (a promessa do dono) -----------
// Um modelo de 50M vértices (~1.8 GB — a classe «> 1 GB» da spec) tem de
// converter com RAM pico ≤ 512 MB: o modelo NUNCA está inteiro em RAM
// (o pico é o remap da primitiva em voo + um bloco por worker + as
// páginas do mmap ainda residentes — a higiene dropRange larga-as).
// SE alguém reintroduzir um readAll no caminho streaming (a mutação M1
// do relatório), o pico salta para o tamanho do FICHEIRO e esta sentinela
// fica VERMELHA — é exatamente o que ela vigia.
TEST(sentinela_r039_50m_verts_ram_pico_512mb) {
    static const Profile kSentinel = {"r039-50M", 40, 1, 50000000,
                                      20000000, true, true};
    const pid_t pid = ::fork();
    ASSERT(pid >= 0);
    if (pid == 0) {
        // ---- FILHO: gerar (streaming) → reset do pico → import → medir
        const std::string dir =
            std::string("/tmp/goni_r039_") + std::to_string(int(::getpid()));
        const medicoes::TmpDirRaii tmpGuard{dir};   // F4-Z2: RAII em TODOS
        medicoes::cleanStaleHeavyTmp();             // os transitórios de
        const std::string src = dir + "/fonte.glb";
        u64 glbBytes = 0;
        rmRf(dir);   // resto de corrida anterior — nunca (lição w010)
        if (!mkDir(dir)) {
            std::printf("  [r039] FALHA a criar %s\n", dir.c_str());
            std::fflush(stdout);
            ::_exit(21);
        }
        const auto tGen = std::chrono::steady_clock::now();
        if (!generateGlb(kSentinel, src, glbBytes)) {
            std::printf("  [r039] FALHA a gerar a sentinela\n");
            std::fflush(stdout);
            childCleanup(dir);
            ::_exit(20);
        }
        const double genMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - tGen)
                .count();

        // o pico mede SÓ o import (o gerador já passou — clear_refs 5)
        const bool peakReset = resetPeakRss();
        const u64 rssBefore = currentRssKb();

        const std::string logs = dir + "/logs";
        elog::init(logs.c_str());
        FsStorage st((dir + "/proj").c_str());
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const auto t0 = std::chrono::steady_clock::now();
        const bool ok = convert::importFile(src, "fonte.glb", st, nullptr,
                                            out, stats, err, nullptr,
                                            nullptr);
        const double totalMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - t0)
                .count();
        const u64 peakKb = bench::readPeakRssKb();
        const bool verOk =
            fileHasLine(logs + "/engine.log", "verificado=1");

        std::printf(
            "  [r039] fonte=%llu MB (gen %.0f ms) | total=%.0f ms | "
            "RAM pico=%llu MB (base %llu) | blocos=%llu | verts=%u "
            "idx=%u | ok=%d verificado=%d%s err=%.120s\n",
            static_cast<unsigned long long>(glbBytes / (1024 * 1024)),
            genMs, totalMs,
            static_cast<unsigned long long>(peakKb / 1024),
            static_cast<unsigned long long>(rssBefore / 1024),
            static_cast<unsigned long long>(
                blocosFromLog(logs + "/engine.log")),
            stats.verts, stats.indices, ok ? 1 : 0, verOk ? 1 : 0,
            peakReset ? "" : " (pico SEM reset)", err.c_str());
        std::fflush(stdout);   // o _exit NÃO flusheia

        elog::shutdown();
        childCleanup(dir);   // ~5 GB de /tmp NUNCA ficam (lição w010)
        // O VEREDITO: completo + verificado bit a bit + pico ≤ 512 MB
        const bool peakOk = peakReset && (peakKb / 1024) <= 512;
        ::_exit(ok && out.meshes.size() == 1 && verOk && peakOk ? 0 : 10);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// ---- 0.10-M (PASSO 3B) · A MEDIÇÃO DO LOAD PÓS-CONVERSÃO -------------------
// A CONFIRMAÇÃO MEDIDA da causa do «fail de 203 MB»: o import STREAMING
// completa com pico baixo (a sentinela R-039 acima); o LOAD de hoje
// RECUSA o mesh único (>65 535 verts) com a mensagem «memória insuficiente
// ao carregar mesh (cura no PASSO 4: render por blocos)». Números:
//   · fase=load ms= e o pico de RAM DO LOAD — com o GUARDO CEDO (o espião
//     de 192 B) a recusa custa a BASE do processo, não o ficheiro: a 1ª
//     medição deste teste (sem o guard) queimava 2007 MB para ler um
//     .gmesh de 972 MB e recusar — no C33 a app morria ANTES da mensagem;
//   · a RECUSA com a mensagem do dono (a causa REGISTADA, não contornada).
// A conversão NÃO é medida de novo — o perfil scene-213MB já a provou
// acima (55 MB de pico); aqui mede-se o LOAD, com o pico REINICIADO
// DEPOIS do import (a RAM medida é SÓ a do load).
TEST(medicao_010m_load_pos_conversao_a_parede_do_mesh_unico) {
    static const Profile kProf = {"scene-213MB", 80, 1, 5242880, 10000000,
                                  false, true};
    const pid_t pid = ::fork();
    ASSERT(pid >= 0);
    if (pid == 0) {
        // ---- FILHO: import (o caminho de produção) → reset do pico → LOAD
        const std::string dir =
            std::string("/tmp/goni_010m_load_") +
            std::to_string(int(::getpid()));
        const std::string src = dir + "/fonte.glb";
        u64 glbBytes = 0;
        rmRf(dir);
        if (!mkDir(dir)) {
            std::printf("  [010m-load] FALHA a criar %s\n", dir.c_str());
            std::fflush(stdout);
            ::_exit(21);
        }
        if (!generateGlb(kProf, src, glbBytes)) {
            std::printf("  [010m-load] FALHA a gerar o perfil\n");
            std::fflush(stdout);
            childCleanup(dir);
            ::_exit(20);
        }
        const std::string logs = dir + "/logs";
        elog::init(logs.c_str());
        FsStorage st((dir + "/proj").c_str());
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool impOk = convert::importFile(src, "fonte.glb", st, nullptr,
                                                out, stats, err, nullptr,
                                                nullptr);
        const bool verOk =
            impOk && fileHasLine(logs + "/engine.log", "verificado=1");
        if (!impOk || out.meshes.empty()) {
            std::printf("  [010m-load] o import FALHOU: %.120s\n",
                        err.c_str());
            std::fflush(stdout);
            elog::shutdown();
            childCleanup(dir);
            ::_exit(22);
        }

        // o pico mede SÓ o LOAD (o import + gerador já passaram — clear_refs)
        const bool peakReset = resetPeakRss();
        const u64 rssBefore = currentRssKb();
        ResourceManager rm;
        rm.setStorage(&st);
        std::string lerr;
        const auto t0 = std::chrono::steady_clock::now();
        const MeshData* m = rm.mesh(out.meshes[0], lerr);
        const double loadMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - t0)
                .count();
        const u64 peakKb = bench::readPeakRssKb();
        u64 gmeshBytes = 0;
        {
            std::vector<u8> gm;
            if (st.readBytes(out.meshes[0], gm)) {
                gmeshBytes = gm.size();
            }
        }
        const bool loadMsOk =
            fileHasLine(logs + "/engine.log", "gmesh: fase=load ms=");
        const bool causaOk =
            lerr.find("memória insuficiente ao carregar mesh") !=
                std::string::npos &&
            lerr.find("cura no PASSO 4: render por blocos") !=
                std::string::npos;

        std::printf(
            "  [010m-load] fonte=%llu MB → .gmesh=%llu MB | import "
            "verificado=%d | LOAD: ms=%.1f | RAM pico do load=%llu MB "
            "(base %llu)%s | mesh=%s | causa=%.160s\n",
            static_cast<unsigned long long>(glbBytes / (1024 * 1024)),
            static_cast<unsigned long long>(gmeshBytes / (1024 * 1024)),
            verOk ? 1 : 0, loadMs,
            static_cast<unsigned long long>(peakKb / 1024),
            static_cast<unsigned long long>(rssBefore / 1024),
            peakReset ? "" : " (pico SEM reset)",
            m ? "CARREGOU (mesh único)" : "RECUSADO",
            lerr.c_str());
        std::fflush(stdout);   // o _exit NÃO flusheia

        elog::shutdown();
        childCleanup(dir);
        // O VEREDITO (a causa confirmada, registada, NÃO contornada — e a
        // MENSAGEM entregue barata):
        //  · o import completou e verificou (a conversão está CORRETA);
        //  · o load RECUSOU com a mensagem do dono (mesh único > 65 535);
        //  · a linha de tempo fase=load existe (os minutos têm dono);
        //  · o pico do load é a BASE + ε (o espião de 192 B): a recusa não
        //    incendeia a RAM — no C33 a mensagem CHEGA ao dono (a 1ª versão
        //    sem o guard media 2007 MB de pico: a app morria calada).
        //    A cura de VERDADE continua a ser o PASSO 4 (render por blocos).
        const bool peakBarato = peakReset &&
                                (peakKb / 1024) <= (rssBefore / 1024) + 32;
        ::_exit(verOk && !m && causaOk && loadMsOk && peakBarato ? 0 : 10);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
