// tests/test_010m_prova_final.cpp — 0.10-M (PASSO 6) — A PROVA FINAL DA
// CAMPANHA: o caminho INTEIRO numa sequência única com A TABELA DE
// MEDIÇÕES.
//
// O que o dono pediu (a spec do PASSO 6): a validação final COM a tabela
// de medições — o que fecha a campanha 0.10-M (.gmesh v3). A prova corre
// O CAMINHO DE PRODUÇÃO inteiro, na ordem da campanha, num processo
// filho (fork) com o pico de RSS REINICIADO a cada fase (clear_refs — a
// RAM medida é da fase, sem o ruído das anteriores):
//
//   [T1] IMPORT      — convert::importFile (dragão-38MB, o perfil que
//                      MORRIA na parede do 65 535 antes da campanha):
//                      as fases pelas LINHAS CONTRATO do engine.log
//                      («import: copia ms=», «gmesh: fase=parse|cut|
//                      assembly|verify ms=») — o MESMO número que o dono
//                      lê no device — + o TOTAL do relógio + o pico de
//                      RAM (a classe da promessa R-039: nunca o modelo
//                      inteiro em RAM);
//   [T2] A ABERTURA  — BlockMesh::open: 192 B + tabela + materiais,
//                      ZERO bytes dos dados (o PASSO 4): ms + Δ RAM;
//   [T3] O CULLING   — o audit puro (visibleBlocks): a câmara centrada
//                      vê TODOS, a lateral NENHUM (o exclusivo funciona);
//   [T4] O BLOCO     — materializeBlock(0): o pico é o SPAN da faixa +
//                      o MeshData (~2× o bloco), NUNCA o modelo;
//   [T5] A THUMB     — ensureThumb(force): o re-render sob demanda do
//                      5C com o piso do dono (<500 ms).
//
// A TABELA impressa é o artefacto do PASSO 6 — o relatório
// (docs/RELATORIO-0.10-M-PASSO6-VALIDACAO-FINAL.md) copia-a e o dono
// reproduz os MESMOS números no C33 pelas MESMAS linhas de log (a parte
// do device é o checklist §4 do relatório).
//
// OS LIMITES DECLARADOS (o porquê de cada um vive no §2 do relatório —
// nenhum número é arbitrário):
//   · import ≤ 512 MB de pico — a classe da R-039 (o gigante de 50M
//     verts/1754 MB mediu 110 MB; o dragão é 46× menor);
//   · abertura ≤ 2 MB de Δ — a abertura é a TABELA (192 B + 80 B/bloco
//     + materiais: bytes; o slack cobre a granularidade de página);
//   · bloco ≤ 16 MB — o SPAN da faixa lida + o MeshData decodificado
//     (a estimativa GPU da casa) — o pico é ~2× o bloco de 65 535 verts,
//     uma FRAÇÃO do modelo (neste perfil, 5 MB vs 230 MB de .gmesh —
//     a duplicação de pools do corte INCHA o ficheiro, o pico NÃO);
//   · thumb < 500 ms — o piso do dono do PASSO 5C.
//
// GL-free: nenhum caminho desta prova toca o GL (o open/culling/
// materialize/thumb são puros; o render GL tem os SEUS pins no
// test_blockmesh e no c33_virtual). O filho limpa o /tmp (a lição do
// wiring010) e o veredicto sai pelo código de exit.
#include "TestFramework.h"

#include "Medicoes010m.h"   // os perfis/gerador/medidores partilhados

#include "assets/AssetConverter.h"
#include "assets/Assets.h"
#include "assets/PngLoader.h"
#include "assets/ThumbIndex.h"
#include "core/Bench.h"
#include "core/FsStorage.h"
#include "math/Math.h"
#include "platform/EngineLog.h"
#include "render/BlockMesh.h"
#include "render/Camera.h"
#include "render/Mesh.h"   // o tipo COMPLETO (o BlockMesh guarda unique_ptr<Mesh>)

#include <chrono>
#include <cstdio>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace vv;
using namespace medicoes;

namespace {

constexpr u64 kImportPeakBudgetMb = 512;    // a promessa da R-039 (a classe)
constexpr u64 kOpenPeakBudgetKb = 2048;     // a abertura é a TABELA (KB)
constexpr u64 kBlockPeakBudgetKb = 16384;   // ~2× o bloco, NUNCA o modelo
constexpr double kThumbFloorMs = 500.0;     // o piso do dono (PASSO 5C)

u64 deltaKb(u64 peakKb, u64 rssBeforeKb) {
    return peakKb > rssBeforeKb ? peakKb - rssBeforeKb : 0;
}

} // namespace

// ---- O TESTE: um filho, cinco fases, UMA TABELA -------------------------

TEST(prova_final_010m_dragao_o_caminho_inteiro_com_a_tabela) {
    const pid_t pid = ::fork();
    ASSERT(pid >= 0);
    if (pid == 0) {
        // ---- o PALCO: o dragão-38MB (o perfil do dono que MORRIA na
        // parede do 65 535 — o diagnóstico que ABRIU a campanha) ------
        const std::string dir =
            std::string("/tmp/goni_p6_") + std::to_string(int(::getpid()));
        const std::string src = dir + "/fonte.glb";
        u64 glbBytes = 0;
        rmRf(dir);   // resto de corrida anterior (a lição w010)
        if (!mkDir(dir)) {
            std::printf("  [prova-final] FALHA a criar %s\n", dir.c_str());
            std::fflush(stdout);
            ::_exit(21);
        }
        const auto tGen = std::chrono::steady_clock::now();
        if (!generateGlb(kProfiles[0], src, glbBytes)) {
            std::printf("  [prova-final] FALHA a gerar o dragão-38MB\n");
            std::fflush(stdout);
            childCleanup(dir);
            ::_exit(20);
        }
        const double genMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - tGen)
                .count();

        const std::string logs = dir + "/logs";
        elog::init(logs.c_str());
        FsStorage st((dir + "/proj").c_str());
        convert::Output out;
        convert::Stats stats;
        std::string err;

        // ---- [T1] O IMPORT (caminho de produção, FUNDIDO — o default) --
        const bool impPeakReset = resetPeakRss();
        const u64 impRssBefore = currentRssKb();
        const auto t0 = std::chrono::steady_clock::now();
        const bool impOk = convert::importFile(src, "fonte.glb", st, nullptr,
                                                out, stats, err, nullptr,
                                                nullptr);
        const double importTotalMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - t0)
                .count();
        const u64 impPeakKb = bench::readPeakRssKb();

        // as FASES pelas linhas contrato (o MESMO número do device)
        const std::string log = logs + "/engine.log";
        const bool verOk = fileHasLine(log, "verificado=1");
        const u64 blocos = blocosFromLog(log);
        const double copiaMs = msOfLine(log, "import: copia ms=");
        const double parseMs = msOfLine(log, "gmesh: fase=parse ms=");
        const double cutMs = msOfLine(log, "gmesh: fase=cut ms=");
        const double assemblyMs = msOfLine(log, "gmesh: fase=assembly ms=");
        const double verifyMs = msOfLine(log, "gmesh: fase=verify ms=");
        u64 gmeshBytes = 0;
        if (impOk && !out.meshes.empty()) {
            st.statBytes(out.meshes[0], gmeshBytes);
        }

        // ---- [T2] A ABERTURA PELA TABELA (PASSO 4 — zero dados) --------
        BlockMesh bm;
        bool openOk = false;
        double openMs = -1.0;
        u64 openDelta = 0;
        u64 tableBlocks = 0;
        if (impOk && !out.meshes.empty()) {
            const bool openReset = resetPeakRss();
            const u64 openRssBefore = currentRssKb();
            const auto tO = std::chrono::steady_clock::now();
            openOk = bm.open(st, out.meshes[0], err);
            openMs =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - tO)
                    .count();
            const u64 openPeakKb = bench::readPeakRssKb();
            openDelta = openReset ? deltaKb(openPeakKb, openRssBefore) : 0;
            tableBlocks = bm.table().size();
        }

        // ---- [T3] O CULLING (o audit puro do frustum por bloco) -------
        u32 nCenter = 0, nLateral = 0;
        if (openOk) {
            // a centrada: orbit default (target 0, dist 6, fov 60°) — o
            // modelo ±1 cabe INTEIRO no frustum
            Camera camC;
            camC.target = Vec3{0.0f, 0.0f, 0.0f};
            camC.yaw = 0.6f;
            camC.pitch = 0.5f;
            camC.dist = 6.0f;
            camC.fovY = 1.0472f;
            const Mat4 vpC = Mat4::mul(camC.proj(1.0f), camC.view());
            std::vector<u32> visC;
            nCenter = bm.visibleBlocks(Mat4::identity(), vpC, visC);
            // a lateral: o alvo 200 unidades à direita — o modelo cai
            // fora dos planos laterais (NENHUM visível — o exclusivo
            // funciona; a prova geométrica do meio-termo vive no
            // test_blockmesh_frustum_por_bloco_audit)
            Camera camL;
            camL.target = Vec3{200.0f, 0.0f, 0.0f};
            camL.yaw = 0.0f;
            camL.pitch = 0.0f;
            camL.dist = 8.0f;
            camL.fovY = 0.5f;
            const Mat4 vpL = Mat4::mul(camL.proj(1.0f), camL.view());
            std::vector<u32> visL;
            nLateral = bm.visibleBlocks(Mat4::identity(), vpL, visL);
        }

        // ---- [T4] O BLOCO (materialize: o pico é 1 bloco) --------------
        bool matOk = false;
        double matMs = -1.0;
        u64 matDelta = 0;
        size_t blkVerts = 0, blkIdx = 0;
        u64 blk0Bytes = 0;
        u64 blk0MeshBytes = 0;
        if (openOk) {
            blk0Bytes = bm.table().empty() ? 0 : bm.table()[0].dataSize;
            const bool matReset = resetPeakRss();
            const u64 matRssBefore = currentRssKb();
            MeshData md;
            const auto tM = std::chrono::steady_clock::now();
            matOk = bm.materializeBlock(0, md, err);
            matMs =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - tM)
                    .count();
            const u64 matPeakKb = bench::readPeakRssKb();
            matDelta = matReset ? deltaKb(matPeakKb, matRssBefore) : 0;
            blkVerts = md.vertices.size();
            blkIdx = md.indices.size();
            // o custo DECLARADO do bloco: a faixa lida + o MeshData
            // (a estimativa GPU da casa — sizeof(Vertex)·verts + 2·idx)
            blk0MeshBytes = matOk ? blockMeshVramBytes(md) : 0;
        }

        // ---- [T5] A THUMB (force — o re-render do 5C) ------------------
        bool thumbOk = false;
        double thumbMs = -1.0;
        u32 thumbW = 0, thumbH = 0;
        if (impOk && !out.meshes.empty()) {
            std::string terr;
            const auto tT = std::chrono::steady_clock::now();
            thumbOk = ThumbIndex::ensureThumb(st, out.meshes[0], true, terr);
            thumbMs =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - tT)
                    .count();
            if (thumbOk) {
                std::vector<u8> png;
                if (st.readBytes(ThumbIndex::relFor(out.meshes[0]), png)) {
                    RawImage img;
                    std::string perr;
                    if (loadPng(png.data(), png.size(), img, perr)) {
                        thumbW = img.width;
                        thumbH = img.height;
                    }
                }
            }
        }

        // ---- A TABELA DE MEDIÇÕES (o artefacto do PASSO 6) -------------
        std::printf(
            "  [prova-final] 0.10-M PASSO 6 · A TABELA DE MEDIÇÕES "
            "(dragão-38MB, caminho FUNDIDO, hospedeiro GL-free)\n");
        std::printf(
            "  [prova-final] fonte glb               | %llu B (%.1f MB; "
            "gerada em %.0f ms)\n",
            static_cast<unsigned long long>(glbBytes),
            double(glbBytes) / (1024.0 * 1024.0), genMs);
        std::printf("  [prova-final] import: copia         | %.0f ms\n",
                    copiaMs);
        std::printf("  [prova-final] import: parse         | %.0f ms\n",
                    parseMs);
        std::printf("  [prova-final] import: corte         | %.0f ms\n",
                    cutMs);
        std::printf("  [prova-final] import: assembly      | %.0f ms\n",
                    assemblyMs);
        std::printf("  [prova-final] import: verificação   | %.0f ms\n",
                    verifyMs);
        std::printf("  [prova-final] import: TOTAL         | %.0f ms\n",
                    importTotalMs);
        std::printf(
            "  [prova-final] import: RAM pico       | %llu MB (base %llu, "
            "delta %lld MB) [limite %llu MB]%s\n",
            static_cast<unsigned long long>(impPeakKb / 1024),
            static_cast<unsigned long long>(impRssBefore / 1024),
            static_cast<long long>(impPeakKb / 1024) -
                static_cast<long long>(impRssBefore / 1024),
            static_cast<unsigned long long>(kImportPeakBudgetMb),
            impPeakReset ? "" : " (pico SEM reset)");
        std::printf(
            "  [prova-final] .gmesh v3              | %.1f MB · %llu bloco(s)"
            " · pools=%u (a duplicação do corte — a fonte tem 300 001) ·"
            " índices=%u (tris=%u) · verificado=%d\n",
            double(gmeshBytes) / (1024.0 * 1024.0),
            static_cast<unsigned long long>(blocos), stats.verts,
            stats.indices, stats.indices / 3u, verOk ? 1 : 0);
        std::printf(
            "  [prova-final] abrir pela tabela      | %.1f ms · %llu bloco(s)"
            " na tabela · RAM delta %llu KB [limite %llu KB]\n",
            openMs, static_cast<unsigned long long>(tableBlocks),
            static_cast<unsigned long long>(openDelta),
            static_cast<unsigned long long>(kOpenPeakBudgetKb));
        std::printf(
            "  [prova-final] culling (centrada)     | %u/%u bloco(s) "
            "visíveis\n",
            nCenter, tableBlocks);
        std::printf(
            "  [prova-final] culling (lateral)      | %u/%u (o exclusivo "
            "funciona)\n",
            nLateral, tableBlocks);
        std::printf(
            "  [prova-final] materializar bloco 0   | %.1f ms · faixa %llu B + "
            "MeshData %llu B (%zu verts / %zu idx) · RAM delta %llu KB "
            "[bloco ≤ %llu KB — NUNCA o modelo de %.1f MB]\n",
            matMs, static_cast<unsigned long long>(blk0Bytes),
            static_cast<unsigned long long>(blk0MeshBytes), blkVerts, blkIdx,
            static_cast<unsigned long long>(matDelta),
            static_cast<unsigned long long>(kBlockPeakBudgetKb),
            double(gmeshBytes) / (1024.0 * 1024.0));
        std::printf(
            "  [prova-final] thumb 256×256 (force) | %.1f ms · PNG %u×%u "
            "[limite %.0f ms]\n",
            thumbMs, thumbW, thumbH, kThumbFloorMs);
        std::printf(
            "  [prova-final] err                    | %.160s\n", err.c_str());
        std::fflush(stdout);   // o _exit NÃO flusheia

        // ---- O VEREDICTO (os pins da prova — um a um) ------------------
        // 1. o import completou, verificou bit a bit e registou 1 mesh
        const bool pinImport =
            impOk && verOk && out.meshes.size() == 1 && gmeshBytes > 0;
        // 2. a parede MORREU: 300 001 verts ⇒ ≥2 blocos (o teto por bloco)
        //    e a tabela do open CONCORDA com a linha contrato do log
        const bool pinBlocos = blocos >= 2 && tableBlocks == blocos;
        // 3. o pico do import dentro da classe R-039
        const bool pinRamImport =
            impPeakReset && (impPeakKb / 1024) <= kImportPeakBudgetMb;
        // 4. a abertura é a TABELA (delta de RAM de KB, não de MB de dados)
        const bool pinOpen = openOk && openDelta <= kOpenPeakBudgetKb;
        // 5. o audit do culling: centrada vê TUDO, lateral NADA
        const bool pinCull =
            nCenter == tableBlocks && tableBlocks > 0 && nLateral == 0;
        // 6. o pico do bloco é o SPAN+MeshData DECLARADOS (a faixa lida +
        //    a estimativa GPU), nunca o modelo — e o Δ de RSS CONFIRMA
        //    (0 KB = as páginas já pagas pelo import foram REUTILIZADAS)
        const bool pinBloco =
            matOk && matDelta <= kBlockPeakBudgetKb &&
            (blk0Bytes + blk0MeshBytes) <= kBlockPeakBudgetKb * 1024ull &&
            blkVerts > 0 && blkIdx > 0;
        // 7. a thumb no piso do dono, no tamanho do contrato
        const bool pinThumb =
            thumbOk && thumbW == 256u && thumbH == 256u && thumbMs >= 0.0 &&
            thumbMs < kThumbFloorMs;
        // 8. as linhas contrato EXISTEM (os minutos têm dono — o PASSO 3B)
        const bool pinLinhas = copiaMs >= 0.0 && parseMs >= 0.0 &&
                               cutMs >= 0.0 && assemblyMs >= 0.0 &&
                               verifyMs >= 0.0;

        elog::shutdown();
        childCleanup(dir);
        const bool all = pinImport && pinBlocos && pinRamImport && pinOpen &&
                         pinCull && pinBloco && pinThumb && pinLinhas;
        ::_exit(all ? 0 : 10);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
