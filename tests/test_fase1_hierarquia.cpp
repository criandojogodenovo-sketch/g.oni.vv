// tests/test_fase1_hierarquia.cpp — 0.11-H (FASE 1 — HIERARQUIA)
//
// O que a fase pede (a spec do dono) e o que ESTE ficheiro aferva:
//   1. drag-reparent na hierarquia (arrastar TIC para outro TIC muda de
//      pai; filhos acompanham; recusas honestas; undo restaura o pai)
//   2. apagar com filhos (sub-árvore INTEIRA — a corrupção latente do
//      delete simples morre; contagem para o diálogo)
//   3. duplicar com filhos (deep clone com novos handles/nomes/estrutura)
//   4. expandir/colapsar persistente (toggle por nome + round-trip do
//      layout.json — o MESMO formato da casa, retrocompatível)
//   5. ícone por tipo (Script/AnimationPlayer saem do cubo genérico)
//   6. a UI REAL: o drag na zona do ícone com o claim novo, o chevron no
//      tap e o tap da multi-seleção intacto (gestos injetados, o padrão
//      do test_uieditor).
#include "TestFramework.h"

#include "components/AnimationPlayer.h"
#include "components/CameraComp.h"
#include "components/MeshRenderer.h"
#include "components/ScriptComp.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "core/UndoStack.h"
#include "platform/InputState.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/BottomPanel.h"   // 0.11-H: o round-trip do layout.json
#include "ui/Theme.h"
#include "ui/UiContext.h"
#include "ui/UiEditor.h"

#include <string>
#include <vector>

using namespace vv;
using namespace vv::editor;

namespace {

// árvore de teste de 3 NÍVEIS (a aceitação da FASE 1), construída DENTRO da
// cena que o teste passa (a Scene NÃO é copiável/movível — o back-pointer
// do ComponentStore aponta para o dono):
//   Raiz1 → Filho → Neto → Bisneto   (quatro gerações no ramo longo)
//   Raiz2 (a outra raiz — o alvo do drag do neto)
struct Tree {
    Handle raiz1{}, filho{}, neto{}, bisneto{}, raiz2{};

    // ctor explícito sobre a cena do dono (nunca move/copia Scene)
    explicit Tree(Scene& s) {
        raiz1 = s.create("Raiz1");
        filho = s.create("Filho", static_cast<i32>(raiz1.index));
        neto = s.create("Neto", static_cast<i32>(filho.index));
        bisneto = s.create("Bisneto", static_cast<i32>(neto.index));
        raiz2 = s.create("Raiz2");
    }
};

i32 parentSlotOf(Scene& s, Handle h) {
    Tic* t = s.get(h);
    return t ? t->parent : -2;
}

} // namespace

// ---- 1. reparent puro: mudança de pai + recusas + filhos acompanham --------

TEST(fase1_reparent_puro_recusas_e_filhos_acompanham) {
    Scene s;
    Tree t(s);

    // o contrato do descendente (o guard de ciclo)
    EXPECT(isDescendant(s, t.raiz1, t.neto));
    EXPECT(isDescendant(s, t.filho, t.bisneto));
    EXPECT(!isDescendant(s, t.neto, t.raiz1));
    EXPECT(!isDescendant(s, t.raiz1, t.raiz2));

    // recusas: para si próprio; para o próprio descendente (ciclo); morto
    EXPECT(!canReparentTo(s, t.neto, t.neto));
    EXPECT(!canReparentTo(s, t.raiz1, t.neto));   // ciclo
    EXPECT(!canReparentTo(s, t.raiz1, t.bisneto));
    EXPECT(!canReparentTo(s, Handle{999, 9}, t.raiz2));
    EXPECT(!canReparentTo(s, t.neto, Handle{999, 9}));
    // legais: para a outra raiz; para a RAIZ (invalid); raiz→filho de outro
    EXPECT(canReparentTo(s, t.neto, t.raiz2));
    EXPECT(canReparentTo(s, t.neto, Handle::invalid()));
    EXPECT(canReparentTo(s, t.raiz2, t.filho));

    // AS RECUSAS com a árvore INTACTA (antes de qualquer mutação): para si
    // próprio; para o próprio descendente (o guard de ciclo é o mesmo do
    // consumo no main)
    EXPECT(!canReparentTo(s, t.neto, t.neto));
    EXPECT(!canReparentTo(s, t.raiz1, t.neto));   // ciclo
    EXPECT(!canReparentTo(s, t.raiz1, t.bisneto));
    EXPECT(!reparentTic(s, t.raiz1, t.neto));
    EXPECT(!reparentTic(s, t.raiz1, t.bisneto));
    EXPECT(parentSlotOf(s, t.raiz1) == -1);       // a recusa não mexeu
    EXPECT(parentSlotOf(s, t.neto) ==
           static_cast<i32>(t.filho.index));

    // A ACEITAÇÃO: drag-reparent do NETO para a OUTRA RAIZ → os filhos
    // ACOMPANHAM (o bisneto continua pendurado no neto)
    EXPECT(reparentTic(s, t.neto, t.raiz2));
    EXPECT(parentSlotOf(s, t.neto) ==
           static_cast<i32>(t.raiz2.index));
    EXPECT(parentSlotOf(s, t.bisneto) ==
           static_cast<i32>(t.neto.index));   // a sub-árvore veio junto
    // e a árvore antiga ficou coerente (o filho de Raiz1 segue Raiz1)
    EXPECT(parentSlotOf(s, t.filho) ==
           static_cast<i32>(t.raiz1.index));

    // reparent para a RAIZ (drop no vazio)
    EXPECT(reparentTic(s, t.neto, Handle::invalid()));
    EXPECT(parentSlotOf(s, t.neto) == -1);
}

TEST(fase1_reparent_undo_restaura_o_pai) {
    Scene s;
    Tree t(s);
    const editor::TicSnap before =
        editor::snapTic(s, t.neto);   // o pai de origem vai no snap
    EXPECT(reparentTic(s, t.neto, t.raiz2));
    EXPECT(parentSlotOf(s, t.neto) ==
           static_cast<i32>(t.raiz2.index));

    // o MESMO caminho do botão Desfazer do main (UndoStack::undo → restore)
    editor::UndoStack undo;
    editor::TicSnap empty;   // o push do main: before ≠ after
    EXPECT(undo.push(before, editor::snapTic(s, t.neto), t.neto));
    const Handle uh = undo.undo(s);
    EXPECT(uh == t.neto);
    EXPECT(parentSlotOf(s, t.neto) ==
           static_cast<i32>(t.filho.index));   // o pai de origem voltou
    EXPECT(undo.redo(s) == t.neto);
    EXPECT(parentSlotOf(s, t.neto) ==
           static_cast<i32>(t.raiz2.index));
}

// ---- 2. apagar com filhos ----------------------------------------------------

TEST(fase1_apagar_com_filhos_e_a_contagem_do_dialogo) {
    Scene s;
    Tree t(s);
    // a contagem do diálogo (o root incluído)
    EXPECT(countSubtree(s, t.bisneto) == 1u);
    EXPECT(countSubtree(s, t.neto) == 2u);
    EXPECT(countSubtree(s, t.raiz1) == 4u);
    EXPECT(countSubtree(s, t.raiz2) == 1u);
    EXPECT(countSubtree(s, Handle{999, 9}) == 0u);

    // a ordem do collect: o root PRIMEIRO, pais antes de filhos
    const std::vector<Handle> sub = collectSubtree(s, t.raiz1);
    EXPECT(sub.size() == 4u);
    EXPECT(sub[0] == t.raiz1);
    EXPECT(sub[1] == t.filho);

    // APAGAR COM FILHOS: a sub-árvore inteira morre — NUNCA fica um TIC
    // com parent para slot reutilizável (a corrupção latente do delete
    // simples do 0.10.7)
    const u32 antes = s.count();
    const u32 n = destroyWithChildren(s, t.raiz1);
    EXPECT(n == 4u);
    EXPECT(s.count() == antes - 4u);
    EXPECT(s.get(t.raiz1) == nullptr);
    EXPECT(s.get(t.filho) == nullptr);
    EXPECT(s.get(t.neto) == nullptr);
    EXPECT(s.get(t.bisneto) == nullptr);
    EXPECT(s.get(t.raiz2) != nullptr);   // a outra raiz fica

    // o SLOT reutilizado não herda fantasmas: criar sobre os slots livres
    // produz TICs limpos (parent -1)
    const Handle novo = s.create("Novo");
    EXPECT(parentSlotOf(s, novo) == -1);

    // apagar uma FOLHA: só ela
    EXPECT(destroyWithChildren(s, t.raiz2) == 1u);
    EXPECT(s.get(t.raiz2) == nullptr);
    EXPECT(s.get(novo) != nullptr);
}

// ---- 3. duplicar com filhos (deep clone) --------------------------------------

TEST(fase1_duplicar_com_filhos_deep_clone) {
    Scene s;
    Tree t(s);
    // componentes no ramo (o clone copia por VALOR — a política do house)
    if (Tic* r = s.get(t.raiz1)) {
        if (Transform3D* tr = r->addComponent<Transform3D>()) {
            tr->pos = Vec3{1.0f, 2.0f, 3.0f};
        }
    }
    if (Tic* f = s.get(t.filho)) {
        if (MeshRenderer* mr = f->addComponent<MeshRenderer>()) {
            mr->meshPath = "meshes/peca.gmesh";
            mr->texPath = "textures/peca.gtext";
        }
    }
    const u32 antes = s.count();

    const Handle clone = duplicateWithChildren(s, t.raiz1);
    EXPECT(clone.valid());
    EXPECT(clone != t.raiz1);                       // IDs NOVOS
    EXPECT(s.count() == antes + 4u);                // a sub-árvore INTEIRA

    Tic* c = s.get(clone);
    EXPECT(c != nullptr);
    EXPECT(c->name != "Raiz1");                     // nome único Godot-style
    EXPECT(parentSlotOf(s, clone) == -1);           // o clone da raiz é raiz

    // a estrutura ESPELHADA: os filhos do clone penduram-se nos CLONES
    const std::vector<Handle> sub = collectSubtree(s, clone);
    EXPECT(sub.size() == 4u);
    const Handle cFilho = sub[1];
    const Handle cNeto = sub[2];
    const Handle cBisneto = sub[3];
    EXPECT(parentSlotOf(s, cFilho) ==
           static_cast<i32>(clone.index));
    EXPECT(parentSlotOf(s, cNeto) ==
           static_cast<i32>(cFilho.index));
    EXPECT(parentSlotOf(s, cBisneto) ==
           static_cast<i32>(cNeto.index));

    // componentes por valor: as refs iguais, os transforms iguais
    Tic* cf = s.get(cFilho);
    Tic* origF = s.get(t.filho);
    EXPECT(cf != nullptr && origF != nullptr);
    if (cf && origF) {
        const MeshRenderer* a = cf->getComponent<MeshRenderer>();
        const MeshRenderer* b = origF->getComponent<MeshRenderer>();
        EXPECT(a != nullptr && b != nullptr);
        if (a && b) {
            EXPECT(a->meshPath == b->meshPath);
            EXPECT(a->texPath == b->texPath);
        }
    }
    Tic* cr = s.get(clone);
    Tic* orir = s.get(t.raiz1);
    EXPECT(cr != nullptr && orir != nullptr);
    if (cr && orir) {
        const Transform3D* a = cr->getComponent<Transform3D>();
        const Transform3D* b = orir->getComponent<Transform3D>();
        EXPECT(a != nullptr && b != nullptr);
        if (a && b) {
            EXPECT(a->pos.x == b->pos.x && a->pos.y == b->pos.y &&
                   a->pos.z == b->pos.z);
        }
    }

    // o ORIGINAL ficou intacto (4 TICs vivos no ramo de origem)
    EXPECT(countSubtree(s, t.raiz1) == 4u);

    // nomes todos distintos (o find() da cena continua determinístico)
    for (u32 i = 0; i < sub.size(); ++i) {
        for (u32 j = i + 1; j < sub.size(); ++j) {
            EXPECT(s.get(sub[i])->name != s.get(sub[j])->name);
        }
    }

    // duplicar FOLHA = o duplicar de sempre (1 TIC)
    const u32 antesFolha = s.count();
    const Handle dupFolha = duplicateWithChildren(s, t.bisneto);
    EXPECT(dupFolha.valid());
    EXPECT(s.count() == antesFolha + 1u);

    // alvo morto → invalid
    EXPECT(!duplicateWithChildren(s, Handle{999, 9}).valid());
}

// ---- 4. expandir/colapsar persistente (layout.json) ---------------------------

TEST(fase1_colapso_toggle_e_roundtrip_do_layout) {
    std::vector<std::string> cols;
    EXPECT(!hierCollapsedHas(cols, "Raiz1"));
    hierCollapsedToggle(cols, "Raiz1");
    hierCollapsedToggle(cols, "GrupoA");
    EXPECT(hierCollapsedHas(cols, "Raiz1"));
    EXPECT(hierCollapsedHas(cols, "GrupoA"));
    // toggle de novo = expandir (sai da lista)
    hierCollapsedToggle(cols, "Raiz1");
    EXPECT(!hierCollapsedHas(cols, "Raiz1"));
    EXPECT(cols.size() == 1u);

    hierCollapsedToggle(cols, "Raiz1");   // recolhida de novo p/ o round-trip

    // o round-trip do layout.json (o MESMO formato key=value da casa)
    editor::bottom::BottomState bs;
    bs.bottomTab = 1;
    bs.drawerH = 240.0f;
    const std::string data = editor::bottom::serializeLayout(
        bs, true, 0u, -1.0f, -1.0f, false, cols);
    EXPECT(data.find("hierColN=2") != std::string::npos);
    EXPECT(data.find("hierCol=Raiz1") != std::string::npos);

    editor::bottom::BottomState bs2;
    bool insp = false;
    u32 collapsed = 0u;
    std::vector<std::string> lidos{"sujeira"};
    EXPECT(editor::bottom::parseLayout(data, bs2, insp, collapsed, nullptr,
                                       nullptr, nullptr, &lidos));
    EXPECT(lidos.size() == 2u);   // REPOSTO (nunca acumula com o anterior)
    EXPECT(hierCollapsedHas(lidos, "Raiz1"));
    EXPECT(hierCollapsedHas(lidos, "GrupoA"));

    // o ficheiro ANTIGO (sem as linhas novas) lê vazio — retrocompatível
    const std::string antigo =
        "bottomTab=0\ndrawerH=240\ninspector=1\ninspCollapsed=0\n";
    std::vector<std::string> velho{"fantasma"};
    EXPECT(editor::bottom::parseLayout(antigo, bs2, insp, collapsed, nullptr,
                                       nullptr, nullptr, &velho));
    EXPECT(velho.empty());

    // o chamador antigo (sem o out-param) compila e corre igual
    EXPECT(editor::bottom::parseLayout(data, bs2, insp, collapsed));
}

// ---- 5. ícone por tipo (o cubo genérico morre no script/animation) ------------

TEST(fase1_icones_por_tipo_script_e_animacao) {
    Scene s;
    const Handle mesh = s.create("M");
    s.get(mesh)->addComponent<MeshRenderer>();
    const Handle prim = s.create("P");
    if (MeshRenderer* mr = s.get(prim)->addComponent<MeshRenderer>()) {
        mr->primOn = true;
    }
    const Handle cam = s.create("C");
    s.get(cam)->addComponent<CameraComp>();
    const Handle scr = s.create("S");
    s.get(scr)->addComponent<ScriptComp>();
    const Handle anim = s.create("A");
    s.get(anim)->addComponent<AnimationPlayer>();

    EXPECT(hierIconFor(*s.get(mesh)) == icons::Icon::Box);
    EXPECT(hierIconFor(*s.get(prim)) == icons::Icon::Cube);
    EXPECT(hierIconFor(*s.get(cam)) == icons::Icon::Camera);
    EXPECT(hierIconFor(*s.get(scr)) == icons::Icon::Terminal);
    EXPECT(hierIconFor(*s.get(anim)) == icons::Icon::Clapper);
    // a LUZ é da FASE 5 (0.11-L): não há componente de luz na engine hoje
    // — a limitação está DECLARADA no hierIconFor e no relatório
}

// ---- 6. a UI REAL: drag-reparent com o gesto injetado --------------------------

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

// a geometria da lista (a MESMA matemática do draw — densidade 1 nos testes,
// onde os globals kHeaderH/kSearchRowH/kRowH do EditorUi.cpp estão nos
// defaults 28/32/36 e o applyDensity nunca corre):
// listTop = chrome 36 + cabeçalho 28 + pesquisa 32 = 96; linha 36
constexpr f32 kListTop = 36.0f + 28.0f + 32.0f;
constexpr f32 kTestRowH = 36.0f;   // (kRowH é extern de vv::editor — renome
                                   //  para não colidir com o using)

struct HierEnv {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    Scene      scene;
    EditorState st;
    bool       ok = false;
    u32        appliedReparents = 0;
    u32        refusedReparents = 0;

    HierEnv() {
        const char* fontPath = FONT_FIXTURE;
        ok = font.loadFromPaths(&fontPath, 1, 28.0f);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
    }

    void frame() {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        drawHierarchy(ui, scene, st);
        // o MESMO consumo do main (0.11-H): revalida + aplica
        if (st.hierReparentPending) {
            st.hierReparentPending = false;
            const Handle src = st.hierReparentSrc;
            const Handle dstP = st.hierReparentDst;
            if (scene.get(src) && canReparentTo(scene, src, dstP)) {
                if (reparentTic(scene, src, dstP)) {
                    ++appliedReparents;
                }
            } else {
                ++refusedReparents;
            }
        }
        if (st.hierCollapsedDirty) {
            st.hierCollapsedDirty = false;   // o main agenda o save
        }
        ui.statusLine("s");
        ui.endFrame();
        input.clearEdges();
    }

    // centro da zona do ÍCONE DE TIPO da linha `row` (profundidade `depth`):
    // chevX = 16 + 24*depth, zona do ícone = [chevX+24, chevX+64)
    static f32 iconCX(u32 row, u32 depth) {
        return 16.0f + 24.0f * depth + 24.0f + 20.0f;
    }
    static f32 rowCY(u32 row) { return kListTop + row * kTestRowH + 18.0f; }

    void drag(f32 x0, f32 y0, f32 x1, f32 y1) {
        input.injectDown(0, x0, y0);
        frame();                          // press → arm (claim)
        input.injectMove(0, x1, y1);
        frame();                          // move → drag (alvo computado)
        input.injectUp(0);
        frame();                          // release → pending + consumo
    }
};

} // namespace

TEST(fase1_drag_reparent_ui_real_do_neto_para_a_outra_raiz) {
    HierEnv e;
    EXPECT(e.ok);
    Tree t(e.scene);   // Raiz1 → Filho → Neto → Bisneto; Raiz2
    e.st.selected = Handle::invalid();

    // linhas: 0 Raiz1 (d0) · 1 Filho (d1) · 2 Neto (d2) · 3 Bisneto (d3) ·
    // 4 Raiz2 (d0) — o drag do NETO (linha 2) para a RAIZ2 (linha 4)
    e.drag(HierEnv::iconCX(2, 2), HierEnv::rowCY(2),
           HierEnv::iconCX(4, 0) - 10.0f, HierEnv::rowCY(4));
    EXPECT(e.st.hierReparentPending == false);   // consumido no frame
    EXPECT(e.appliedReparents == 1u);
    EXPECT(parentSlotOf(e.scene, t.neto) ==
           static_cast<i32>(t.raiz2.index));
    // os FILHOS ACOMPANHAM (a aceitação da spec)
    EXPECT(parentSlotOf(e.scene, t.bisneto) ==
           static_cast<i32>(t.neto.index));
    // o gesto terminou limpo (o próximo gesto nasce do zero)
    EXPECT(!e.st.hierDrag && !e.st.hierDragArm);
}

TEST(fase1_drag_para_o_vazio_da_lista_vai_para_a_raiz) {
    HierEnv e;
    EXPECT(e.ok);
    Tree t(e.scene);
    e.st.selected = Handle::invalid();

    // o FILHO (linha 1) solto no vazio ABAIXO das linhas (dentro da lista)
    // → RAIZ (o contrato do drop no vazio)
    e.drag(HierEnv::iconCX(1, 1), HierEnv::rowCY(1),
           150.0f, kListTop + 7.0f * kTestRowH + 18.0f);
    EXPECT(e.appliedReparents == 1u);
    EXPECT(parentSlotOf(e.scene, t.filho) == -1);
    // o neto veio junto (o pai dele mudou, ele continua pendurado)
    EXPECT(parentSlotOf(e.scene, t.neto) ==
           static_cast<i32>(t.filho.index));
}

TEST(fase1_drag_ciclo_cancela_sem_aplicar) {
    HierEnv e;
    EXPECT(e.ok);
    Tree t(e.scene);
    e.st.selected = Handle::invalid();

    // arrastar a RAIZ1 (linha 0) para o PRÓPRIO FILHO (linha 1) = ciclo:
    // o alvo ILEGAL não acende e o release CANCELA (nada é aplicado —
    // a recusa é honesta, não um crash nem um estado inválido)
    e.drag(HierEnv::iconCX(0, 0), HierEnv::rowCY(0),
           HierEnv::iconCX(1, 1), HierEnv::rowCY(1));
    EXPECT(e.appliedReparents == 0u);
    EXPECT(parentSlotOf(e.scene, t.raiz1) == -1);
    EXPECT(parentSlotOf(e.scene, t.filho) ==
           static_cast<i32>(t.raiz1.index));   // nada mudou
}

TEST(fase1_tap_na_zona_do_icone_mantem_a_multi_selecao) {
    HierEnv e;
    EXPECT(e.ok);
    Tree t(e.scene);
    e.st.selected = Handle::invalid();

    // o tap SEM movimento na zona do ícone continua a alternar no conjunto
    // (o contrato antigo sobrevive ao claim novo)
    e.input.injectDown(0, HierEnv::iconCX(0, 0), HierEnv::rowCY(0));
    e.frame();
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.st.multiSelectCount == 1u);
    EXPECT(e.st.multiSelect[0] == t.raiz1);
    e.input.injectDown(0, HierEnv::iconCX(0, 0), HierEnv::rowCY(0));
    e.frame();
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.st.multiSelectCount == 0u);
}

// ---- 7. a UI REAL: o chevron expande/colapsa ------------------------------------

TEST(fase1_chevron_colapsa_e_esconde_a_sub_arvore) {
    HierEnv e;
    EXPECT(e.ok);
    Tree t(e.scene);
    e.st.selected = Handle::invalid();

    // linhas: 0 Raiz1 · 1 Filho · 2 Neto · 3 Bisneto · 4 Raiz2
    // o tap no CHEVRON da Raiz1 (zona 24dp antes do ícone) recolhe
    const f32 chevCX0 = 16.0f + 12.0f;
    e.input.injectDown(0, chevCX0, HierEnv::rowCY(0));
    e.frame();
    e.input.injectUp(0);
    e.frame();
    EXPECT(hierCollapsedHas(e.st.hierCollapsed, "Raiz1"));
    EXPECT(e.st.hierCollapsedDirty == false);   // consumido no frame

    // a sub-árvore SUMIU: onde era a linha do Filho (linha 1) agora está a
    // Raiz2 — o tap no corpo dessa linha seleciona a Raiz2, NUNCA o Filho
    e.input.injectDown(0, 150.0f, HierEnv::rowCY(1));
    e.frame();
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.st.selected.valid());
    EXPECT(e.st.selected == t.raiz2);

    // o tap de novo no chevron EXPANDE (toggle) e o Filho volta à linha 1
    e.input.injectDown(0, chevCX0, HierEnv::rowCY(0));
    e.frame();
    e.input.injectUp(0);
    e.frame();
    EXPECT(!hierCollapsedHas(e.st.hierCollapsed, "Raiz1"));
    e.input.injectDown(0, 150.0f, HierEnv::rowCY(1));
    e.frame();
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.st.selected == t.filho);

    // o tap no chevron de uma FOLHA (Bisneto não tem filhos) NÃO recolhe
    // nada (o chevron nem desenha; o tap cai no corpo/folha — sem estado)
    EXPECT(!hierCollapsedHas(e.st.hierCollapsed, "Bisneto"));
}

TEST(fase1_colapso_persiste_no_layout_json) {
    HierEnv e;
    EXPECT(e.ok);
    Tree t(e.scene);

    // o toggle pela UI enche o estado; o serializeLayout leva a MESMA lista
    // (o caminho real do save no main — saveLayoutNow passa st.hierCollapsed)
    const f32 chevCX0 = 16.0f + 12.0f;
    e.input.injectDown(0, chevCX0, HierEnv::rowCY(0));
    e.frame();
    e.input.injectUp(0);
    e.frame();
    EXPECT(hierCollapsedHas(e.st.hierCollapsed, "Raiz1"));

    editor::bottom::BottomState bs;
    const std::string data = editor::bottom::serializeLayout(
        bs, true, 0u, -1.0f, -1.0f, false, e.st.hierCollapsed);
    std::vector<std::string> lidos;
    bool insp = false;
    u32 collapsed = 0u;
    EXPECT(editor::bottom::parseLayout(data, bs, insp, collapsed, nullptr,
                                       nullptr, nullptr, &lidos));
    EXPECT(lidos.size() == 1u);
    EXPECT(lidos[0] == "Raiz1");   // a sessão seguinte reabre recolhida
}
