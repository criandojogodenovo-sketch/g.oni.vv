#pragma once
// render/BlockMesh.h — 0.10-M (PASSO 4): O RENDER POR BLOCOS.
//
// O mesh ÚNICO morria na parede dos 65 535 verts / 256 MB (a recusa do
// PASSO 3B: «memória insuficiente ao carregar mesh — cura no PASSO 4:
// render por blocos»). ESTE é a cura: o .gmesh v3 ABRE pela TABELA
// (192 B de header+meta + a faixa da tabela + a faixa dos materiais —
// ZERO bytes dos dados dos blocos) e o render anda BLOCO A BLOCO:
//
//   • FRUSTUM por bloco: o AABB de cada entrada da tabela (transformado
//     pelo model do TIC dono) é testado contra os 6 planos do VP — só o
//     que INTERSETA o frustum desenha (8 cantos × 6 planos, conservativo:
//     nunca exclui geometria visível; um bloco rodado pode ficar dentro
//     por excesso — honesto e barato, documentado);
//   • LAZY: um bloco só é LIDO do disco (readBytesAt da faixa EXATA +
//     gmeshV3MaterializeBlock) e subido ao GPU na PRIMEIRA frame em que
//     fica visível — importar NÃO carrega nada (a abertura é a tabela);
//   • LRU com ORÇAMENTOS DECLARADOS: a cache de blocos residentes evicta
//     pelo menos-recentemente-visto quando passa o orçamento — RAM (os
//     MeshData retidos, kBlockCacheRamBudgetBytes) e VRAM (os uploads
//     estimados, kBlockCacheVramBudgetBytes). Os 64+192 MB são O MESMO
//     256 MB da casa (kMeshLoadBudgetBytes) — agora LIMITADO POR
//     CONSTRUÇÃO, seja o modelo de 38 MB ou de 972 MB;
//   • MÉTRICAS REAIS por frame (Stats): blocos totais/visíveis/desenhados,
//     dc/verts somados ao DrawStats do frame (a status line e o log dizem
//     o MESMO que o frame fez);
//   • HULL de bounds: o AABB global do meta como mesh de 8 cantos — o
//     "Mesh*" do contrato do picker/serializer (bounds/fit/cena SEM
//     carregar dados; o RENDER é daqui — drawTics consulta mr->blocks
//     PRIMEIRO, o hull nunca desenha).
//
// GL só nos .cpp (Mesh/Renderer) — o culling e a abertura por faixas são
// PUROS e correm no CI; o stub GLES3 do hospedeiro afere os uploads.
#include <memory>
#include <string>
#include <vector>
#include "assets/Assets.h"
#include "assets/GOwnFormats.h"
#include "core/Types.h"
#include "math/Math.h"
#include "render/DrawStats.h"

namespace vv {

class Mesh;
class ProjectStorage;
class Renderer;
class Texture;

// os ORÇAMENTOS DECLARADOS da cache de blocos (o MESMO total de 256 MB
// da casa — agora por construção, independentemente do tamanho do modelo)
constexpr u64 kBlockCacheRamBudgetBytes  = 64ull * 1024 * 1024;
constexpr u64 kBlockCacheVramBudgetBytes = 192ull * 1024 * 1024;

// estimativa de bytes GPU de um bloco subido (o contrato AFERÍVEL: os
// uploads são sizeof(Vertex)×verts + 2×índices (+ pele 20 B/vert) — o
// driver pode gastar mais, o ORÇAMENTO declara a estimativa da casa)
u64 blockMeshVramBytes(const MeshData& block);

class BlockMesh {
public:
    // métricas por frame (o HUD e o log mostram ISTO — nada inventado)
    struct Stats {
        u32 totalBlocks = 0;      // da TABELA (a verdade do ficheiro)
        u32 visible = 0;          // passaram no frustum neste frame
        u32 drawn = 0;            // desenhados DE FACTO neste frame
        u32 loadedThisFrame = 0;  // lazy: 1ª visibilidade = leitura+upload
        u32 resident = 0;         // blocos com GPU vivo agora
        u32 evictedTotal = 0;     // evictions LRU desde o open
        u64 ramBytes = 0;         // MeshData retidos (cache CPU)
        u64 vramBytes = 0;        // uploads estimados (cache GPU)
    };

    BlockMesh() = default;
    ~BlockMesh();
    BlockMesh(const BlockMesh&) = delete;
    BlockMesh& operator=(const BlockMesh&) = delete;

    // ABRE pela tabela (faixas: peek 192 B + tabela + materiais — ZERO
    // dados). 0.10.6 (SAF-SEAM): a FONTE da abertura é a MESMA cascata do
    // conversor — `st.openReadFd` (o fd do bridge sob content:// — NUNCA
    // um caminho POSIX) → `mapFd64` (o mapa serve as 3 leituras) → pread
    // das faixas NO MESMO fd (o provider recusou o mapa) → os RANGES do
    // readBytesAt (storages sem fds / o pipe do provider). O fd/mapping
    // vivem SÓ durante a abertura — o LAZY de sempre é o readBytesAt
    // (1 leitura por bloco, o contrato do PASSO 4). Em falha o `err` diz
    // QUAL das 3 leituras, o errno e o tamanho do ficheiro VISTO (a linha
    // de sempre truncava a causa). Uma TABELA/materiais que não valida
    // (com o peek v3 verde) põe o asset em QUARENTENA (rename .corrupt)
    // e o err diz «asset corrompido, reimporta» — nunca o swap silencioso.
    // `st` não-dono: as leituras por faixa voltam a ele em CADA
    // lazy load (o storage tem de viver enquanto o BlockMesh viver).
    bool open(ProjectStorage& st, const std::string& relPath,
              std::string& err);
    bool ok() const { return opened_; }
    void close();

    const GMeshV3Meta& meta() const { return meta_; }
    const std::vector<GMeshV3Block>& table() const { return blocks_; }
    const std::vector<std::string>& materials() const { return materials_; }
    const std::string& path() const { return path_; }
    bool skinned() const { return (meta_.flags & 1u) != 0; }

    // o AABB GLOBAL do meta (a tabela SEM os dados — colisão/cena/fit)
    const Vec3& boundsMin() const { return meta_.aabbMin; }
    const Vec3& boundsMax() const { return meta_.aabbMax; }
    Vec3 boundsExtent() const {
        return Vec3{meta_.aabbMax.x - meta_.aabbMin.x,
                    meta_.aabbMax.y - meta_.aabbMin.y,
                    meta_.aabbMax.z - meta_.aabbMin.z};
    }
    f32 boundsMaxExtent() const {
        const Vec3 e = boundsExtent();
        f32 m = e.x;
        if (e.y > m) m = e.y;
        if (e.z > m) m = e.z;
        return m;
    }

    // o HULL de bounds: um Mesh de 8 cantos (12 triângulos) com o AABB
    // do meta — o proxy do picker/serializer/cena. NUNCA desenhado pelo
    // render (drawTics consulta mr->blocks primeiro); se algo o desenhar,
    // mostra a CAIXA do modelo (diagnóstico visível, nunca lixo).
    bool createHull(Mesh& out) const;

    // ---- FRUSTUM (PURO — o audit do culling) -----------------------------
    // extrai os 6 planos do VP (Godbout/Gribb-Hart, colunas do clip):
    // plano[i] = (a,b,c,d) com a·x+b·y+c·z+d < 0 = FORA do frustum.
    static void frustumPlanes(const Mat4& vp, f32 out[6][4]);
    // o AABB do bloco (8 cantos transformados pelo model) interpeta o
    // frustum? Conservativo: false só quando TODOS os cantos estão fora
    // do MESMO plano (nunca exclui o que se vê).
    static bool blockVisible(const Mat4& model, const GMeshV3Block& blk,
                             const f32 planes[6][4]);
    // o AUDIT: os índices dos blocos visíveis para (model, vp) — a prova
    // do pin do culling (o teste puro chama ISTO, o draw chama o teste)
    u32 visibleBlocks(const Mat4& model, const Mat4& vp,
                      std::vector<u32>& out) const;

    // ---- o FRAME -----------------------------------------------------------
    // cull → lazy load → draw → evict. Os DrawStats somam-se ao do frame
    // (dc/verts coerentes com a status line); `bones/boneCount` seguem o
    // MESMO contrato do drawMesh de sempre (skin por TIC).
    DrawStats draw(Renderer& r, const Mat4& model, const Mat4& vp,
                   const Texture* tex, const f32* tint,
                   const Mat4* bones, u32 boneCount, Stats& st);

    // orçamentos (defaults = as constantes da casa; os pins apertam-nos)
    void setBudgets(u64 ramBudgetBytes, u64 vramBudgetBytes);
    u64 ramBudget() const { return ramBudget_; }
    u64 vramBudget() const { return vramBudget_; }

    // contabilidade viva (pins/audits — o que o log/HUD espelham)
    const Stats& lastStats() const { return last_; }
    u64 frameCounter() const { return frame_; }

    // materializa UM bloco pela faixa (leitura + decode SEM cache) — o
    // caminho do export OBJ por blocos e dos pinos de RAM (pico = 1 bloco)
    bool materializeBlock(size_t index, MeshData& out, std::string& err) const;

private:
    struct Entry {
        u64 lastSeen = 0;                  // carimbo LRU (frameCounter_)
        std::unique_ptr<Mesh> gpu;         // VRAM (upload estimado)
        std::shared_ptr<MeshData> cpu;     // RAM retida (cache de re-upload)
        u64 vramBytes = 0;
        u64 ramBytes = 0;
    };

    bool ensureBlock(size_t index, std::string& err);   // lazy: faixa→decode→upload
    void evictOverBudget();                             // LRU estrito por pool
    void destroyGpu();                                  // destrutor/close

    ProjectStorage* storage_ = nullptr;   // não-dono
    std::string path_;
    GMeshV3Meta meta_{};
    std::vector<GMeshV3Block> blocks_;
    std::vector<std::string> materials_;
    std::vector<Entry> entries_;
    bool opened_ = false;
    u64 frame_ = 0;
    u64 evictedTotal_ = 0;      // acumulado de evictions LRU (o Stats lê)
    u32 uploadFails_ = 0;       // anti retry-storm de upload GL
    bool uploadDead_ = false;   // 3 falhas sem TERM → para até renascer
    u64 ramBudget_ = kBlockCacheRamBudgetBytes;
    u64 vramBudget_ = kBlockCacheVramBudgetBytes;
    Stats last_{};
    bool churnWarned_ = false;   // 1× por open: working set > orçamento
};

} // namespace vv
