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
        u32 lodDrawn = 0;         // 0.11-T (3.5a): desenhados como LOD
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
    // 0.11-T (FASE 3.5a): os blocos LOD da cauda GLOD (1:1 POR ORDEM com a
    // tabela — a entrada k é o LOD do bloco k; indexCount==0 = sem LOD).
    // Vazio em ficheiros sem cauda (pré-3.5a) — o draw usa o normal.
    const std::vector<GMeshV3Block>& lodBlocks() const { return lodBlocks_; }
    bool hasLod() const { return !lodBlocks_.empty(); }
    const std::vector<std::string>& materials() const { return materials_; }
    // 0.10-M HOTFIX TEXTURAS-VINCULA: as vinculações do bloco GTB1 (o
    // import escreve material→slot→.gtext; o GpuAssets resolve a baseColor
    // de cada material para GL e o draw usa-a POR BLOCO — nunca um
    // material global a substituir todos)
    const std::vector<GMeshMatBind>& binds() const { return binds_; }
    // 0.11-M (FASE 2): as PROPRIEDADES do bloco GTB2 (alphaMode/factors/
    // twoSided/transform UV/samplers por material — o draw aplica POR
    // MATERIAL; vazio = ficheiro pré-FASE 2 = defaults de sempre)
    const std::vector<GMeshMatProps>& matProps() const { return matProps_; }
    // as props de UM material (nullptr = defaults — sem entrada no GTB2)
    const GMeshMatProps* propsOf(u32 materialIndex) const {
        for (const GMeshMatProps& p : matProps_) {
            if (p.materialIndex == materialIndex) {
                return &p;
            }
        }
        return nullptr;
    }
    // a textura RESOLVIDA de um material (o GpuAssets injeta; nullptr =
    // o material fica sem — o draw cai no tex do chamador)
    void setMaterialTexture(u32 materialIndex, const Texture* tex) {
        if (materialIndex < matTex_.size()) {
            matTex_[static_cast<size_t>(materialIndex)] = tex;
        }
    }
    const Texture* materialTexture(u32 materialIndex) const {
        return materialIndex < matTex_.size()
                   ? matTex_[static_cast<size_t>(materialIndex)]
                   : nullptr;
    }
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
    // 0.11-T (FASE 3.5a · PURO): o MESMO teste para um AABB QUALQUER (o
    // skinned cull do AnimationSystem usa-o com os bounds do TIC; o
    // blockVisible de cima é o MESMO cálculo sobre o AABB do bloco).
    // Conservativo igual: false só quando TODOS os 8 cantos estão fora
    // do MESMO plano.
    static bool worldAabbVisible(const Mat4& model, const Vec3& mn,
                                 const Vec3& mx, const f32 planes[6][4]);
    // o INCHAMENTO do AABB do skinned cull (a pose animada ultrapassa o
    // AABB de bind — 25% da extensão em cada lado cobre o swing típico;
    // conservativo = nunca culle um personagem visível; o pin C33 valida
    // nos rigs reais do dono — se saltar, é UMA constante a subir).
    static constexpr f32 kSkinCullAabbSwell = 0.25f;
    // 0.11-T (FASE 3.5a · PURO): o TIC está além da distância do LOD? O
    // centro do AABB (transformado pelo model) contra a posição da
    // câmara. dist > lodDistance ativa (no próprio limiar ainda é o
    // normal — a fronteira é exclusiva no lado longe).
    static bool lodActive(const Mat4& model, const Vec3& mn, const Vec3& mx,
                          const Vec3& camPos, f32 lodDistance);
    // 0.11-M (FASE 2 · PURO): a ORDEM do passe BLEND — longe→perto. `zNdc`
    // = o z de NDC do centro de cada bloco (o -1 é perto, +1 longe; z fora
    // de [-1,1] = atrás/diante do clip → desenha PRIMEIRO, o «mais longe»).
    // out recebe os índices 0..count-1 ordenados por z DESCENDENTE (estável).
    static void blendDrawOrder(const f32* zNdc, u32 count, u32* outOrder);
    // o AUDIT: os índices dos blocos visíveis para (model, vp) — a prova
    // do pin do culling (o teste puro chama ISTO, o draw chama o teste)
    u32 visibleBlocks(const Mat4& model, const Mat4& vp,
                      std::vector<u32>& out) const;

    // ---- 0.11-W (3.5b) · GPU INSTANCING -----------------------------------
    // Um pick = UM bloco visível de UM TIC (a colheita não desenha nem
    // carrega — o agrupamento corre DEPOIS do culling e ANTES do upload,
    // §3.5b.2 do prompt). `blend` = o bloco é do passe BLEND (alphaMode 2 —
    // os blends NÃO se fundem: a ordem longe→perto é por fragmento e um
    // draw por bloco é o que a ordena; viajam com o z NDC de sempre).
    struct InstPick {
        u32 block = 0;    // o índice NORMAL na tabela (o material é dele)
        u32 entry = 0;    // o índice a desenhar (o LOD quando o TIC está
                          // além da distância — o MESMO critério do draw)
        bool blend = false;
        f32 zNdc = 0.0f;  // só os blends (a ordem do passe)
    };
    // o MESMO cull/LOD do draw de sempre (a MESMA função classify) sobre os
    // blocos deste TIC — devolve os picks VISÍVEIS (opacos primeiro, blends
    // depois; a ordem da tabela dentro de cada grupo). NADA é desenhado
    // nem carregado aqui (o lazy corre na submissão do grupo).
    u32 collectVisible(const Mat4& model, const Mat4& vp, const Vec3* camPos,
                       f32 lodDistance, std::vector<InstPick>& out) const;
    // submete UM grupo instanciado: `count` TICs a desenhar a MESMA entrada
    // `entryIdx` (o mesmo mesh GPU!) — 1+ draw(s) instanciados (o teto de 64
    // matrizes por draw fatia AQUI, §3.5b.3; nunca falha). O lazy/LRU correm
    // como no draw de sempre; `st` soma drawn/loaded/lodDrawn do grupo.
    DrawStats drawInstancedGroup(Renderer& r, u32 entryIdx, const Mat4* mats,
                                 u32 count, const Mat4& vp, const Texture* tex,
                                 const f32* tint, Stats& st);
    // os blends de UM TIC instanciado (não se fundem): a ordem longe→perto
    // de sempre (blendDrawOrder) sobre os picks colhidos; 1 draw por bloco.
    DrawStats drawBlendPicks(Renderer& r, const u32* entries, const Mat4* model,
                             const f32* zNdc, u32 count, const Mat4& vp,
                             const Texture* tex, const f32* tint, Stats& st);
    // a contabilidade VIVA (resident/ram/vram/evicted) SEM desenhar — o HUD
    // soma UMA vez por mesh tocado no frame (os bytes são do mesh, não do
    // TIC — somar por TIC duplicava na cena dos 6 clones)
    void storageStats(Stats& st) const;

    // ---- 0.11-W (3.75a) · STREAMING PROGRESSIVO ----------------------------
    // O freeze do import não é a abertura (a TABELA — o PASSO 4) nem o
    // conversor (a thread do import — o PASSO 3): é o PRIMEIRO frame em
    // que a cena fica visível — o lazy de sempre carrega TODOS os blocos
    // visíveis não residentes NO MESMO frame (863 leituras+decodes+uploads
    // na cena do dono = segundos de freeze). A cura: o carregamento ganha
    // ORÇAMENTO POR FRAME e prioridade POR DISTÂNCIA — as CAMADAS: perto
    // primeiro, o resto chega nas vagas dos frames seguintes (o HUD diz
    // «a transmitir=N» até a vaga secar). ZERO mudança ao formato .gmesh —
    // as camadas calculam-se da TABELA (os AABB) em runtime; o ficheiro de
    // 203 MB do dono NÃO precisa de reimport. ZERO mudança aos caminhos de
    // draw (o lazy inline de sempre é o caminho dos chamadores antigos —
    // byte a byte); a política vive no ORQUESTRADOR (o drawTics), que
    // colhe os pedidos, manda o streamIn e SÓ submete o residente.
    //
    // O ORÇAMENTO por frame (o anti-freeze declarado): 8 blocos/frame no
    // CAMINHO INTEIRO (a soma de todos os meshes do frame) — ~1-3 ms por
    // bloco (leitura+decode+upload) = um frame de trabalho honesto a
    // 30-60 FPS; o fill de 863 blocos espalha-se por ~108 frames (~2-4 s)
    // em vez de UM frame de segundos.
    static constexpr u32 kBlockStreamBudgetPerFrame = 8;

    // UM pedido de carregamento (o orquestrador colhe; o streamIn ordena e
    // carrega). `dist` = a distância do camEye ao CENTRO do AABB do bloco
    // NORMAL transformado pelo model do TIC (o MESMO centro que o classify
    // usa para o z dos blends; o LOD partilha o AABB do normal) — a
    // distância MÍNIMA dos clones quando o mesmo entry serve N TICs.
    struct StreamReq {
        BlockMesh* bm = nullptr;   // o mesh dono do entry (não-dono)
        u32 entry = 0;             // o índice na entries_ (normal ou LOD)
        f32 dist = 0.0f;           // em unidades de mundo (perto primeiro)
    };

    // O STREAM IN (o coração do 3.75a · PURO exceto o load): ordena os
    // pedidos por distância ASCENDENTE (estável — empates ficam na ordem
    // dada: a ordem da tabela sobrevive) e carrega até `budget` blocos,
    // PERTO PRIMEIRO (as camadas). Os já residentes NÃO consomem a vaga
    // (o dedup é o próprio resident()); os que FALHAM (corrupção/faixa)
    // TAMBÉM não (um bloco partido não esfomeia os vizinhos — o retry por
    // frame é o de sempre do lazy). Devolve os carregados com sucesso
    // (o orquestrador soma no BlockFrameStats.loaded). NÃO desenha NADA —
    // a submissão é dos caminhos de sempre, agora com a PORTA (resident).
    static u32 streamIn(StreamReq* reqs, u32 count, u32 budget);

    // A PORTA da submissão com o stream ativo: o entry tem GPU vivo AGORA?
    // O orquestrador SÓ submete o residente — o não-residente NÃO desenha
    // neste frame (não carrega inline: a vaga é do streamIn) e conta como
    // «a transmitir». Fora do stream (os chamadores antigos) ninguém
    // pergunta — o lazy inline de sempre, byte a byte.
    bool resident(u32 entry) const;

    // O load ÚNICO do stream (o mesmo ensureBlock do lazy de sempre — a
    // mesma faixa, o mesmo decode, o mesmo upload, o mesmo anti
    // retry-storm). Público para o streamIn estático e para os pins;
    // false = falhou (o err diz a causa; NÃO consome a vaga).
    bool streamLoad(u32 entry, std::string& err);

    // ---- o FRAME -----------------------------------------------------------
    // cull → lazy load → draw → evict. Os DrawStats somam-se ao do frame
    // (dc/verts coerentes com a status line); `bones/boneCount` seguem o
    // MESMO contrato do drawMesh de sempre (skin por TIC).
    // 0.11-T (FASE 3.5a): `camPos` + `lodDistance` (OPCIONAIS) ligam o
    // LOD por distância — com o TIC além da distância, cada bloco visível
    // desenha a ENTRADA LOD (a cauda GLOD) em vez do normal; sem LOD no
    // ficheiro, sem camPos, ou lodDistance ≤ 0 o comportamento é byte a
    // byte o de sempre (os chamadores antigos intocados).
    DrawStats draw(Renderer& r, const Mat4& model, const Mat4& vp,
                   const Texture* tex, const f32* tint,
                   const Mat4* bones, u32 boneCount, Stats& st,
                   const Vec3* camPos = nullptr, f32 lodDistance = 0.0f);

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

    // 0.11-W (3.5b): a classificação do frame EXTRAÍDA do draw (a MESMA
    // lógica de cull/LOD/partição, uma ÚNICA fonte para o draw de sempre e
    // para a colheita do instancing — zero drift). Conta st.visible; os
    // ensureBlock/drawn ficam nos chamadores (o draw nos passes, a
    // submissão instanciada no drawInstancedGroup/drawBlendPicks).
    void classifyFrame(const Mat4& model, const Mat4& vp,
                       const f32 planes[6][4], bool lodOn,
                       const Vec3* camPos, f32 lodDistance,
                       std::vector<u32>& opaqueIdx, std::vector<u32>& opaqueEntry,
                       std::vector<u32>& blendIdx, std::vector<u32>& blendEntry,
                       std::vector<f32>& blendZ, Stats& st) const;

    bool ensureBlock(size_t index, std::string& err);   // lazy: faixa→decode→upload
    void evictOverBudget();                             // LRU estrito por pool
    void destroyGpu();                                  // destrutor/close

    ProjectStorage* storage_ = nullptr;   // não-dono
    std::string path_;
    GMeshV3Meta meta_{};
    std::vector<GMeshV3Block> blocks_;
    std::vector<GMeshV3Block> lodBlocks_;         // 0.11-T (3.5a): a cauda
                                                  // GLOD (1:1 com blocks_)
    u32 lodBase_ = 0;                             // entries_[lodBase_+k] é o
                                                  // LOD do bloco k
    std::vector<std::string> materials_;
    std::vector<GMeshMatBind> binds_;             // o GTB1 (o auto do import)
    std::vector<GMeshMatProps> matProps_;         // 0.11-M: o GTB2 (as props)
    std::vector<const Texture*> matTex_;          // baseColor por material
                                                  // (não-dono: o cache do
                                                  // GpuAssets é o dono)
    std::vector<Entry> entries_;

    // o registo da tabela de UM índice de entrada (normal ou LOD)
    const GMeshV3Block& blockAt(size_t index) const {
        return index < blocks_.size()
                   ? blocks_[index]
                   : lodBlocks_[static_cast<size_t>(index) - blocks_.size()];
    }
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
