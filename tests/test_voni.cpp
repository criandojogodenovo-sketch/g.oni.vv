// tests/test_voni.cpp — 0.9.2 §13: TODOS os testes da spec da linguagem.
//
// A lista é a da campanha (§13, verbatim) + sandbox (§12) + extras das
// decisões 🔶 (Int/Int=Int, move relativo, transition origem=atual…).
// O Host é um FAKE rico (TICs com pos/visible, anims, transições, log em
// memória) — os testes de MOTOR (VoniSystem+EngineHost sobre Scene real)
// estão no test_wiring092.
#include "TestFramework.h"

#include "voni/Voni.h"
#include "voni/VoniInternal.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace voni;

namespace {

// ---------------------------------------------------------------------------
// FakeHost — TICs com propriedades, anims, transições, log em memória
// ---------------------------------------------------------------------------
struct FakeTic {
    std::string name;
    f32 pos[3] = {0, 0, 0};
    f32 rot[3] = {0, 0, 0};        // GRAUS (convenção do engine) — 0.9.5
    f32 escala[3] = {1, 1, 1};     // 0.9.5: copy(prop)
    f32 cor[3] = {1, 1, 1};        // 0.9.5: colorpars (tint do material)
    bool visible = true;
    std::vector<std::string> anims;
    int activeAnim = -1;
};

struct FakeHost : Host {
    std::vector<FakeTic> tics;
    std::vector<std::string> logs;
    std::string hostTic;           // 0.9.5: o TIC dono do script (play())
    f32 moved[3] = {0, 0, 0};
    int explodeCalls = 0;
    int lastExplodeHide = 0;
    std::string currentScene = "cena1";
    std::vector<std::string> scenes{"cena1", "cena2"};
    std::string transitionedTo;
    std::string importedAnim;
    f64 dt = 1.0 / 60.0;

    FakeTic* tic(const std::string& n) {
        for (auto& t : tics) {
            if (t.name == n) {
                return &t;
            }
        }
        return nullptr;
    }

    void log(const char* line) override { logs.push_back(line); }

    bool getProp(const std::string& n, const std::vector<std::string>& chain,
                 Value& out, std::string& err) override {
        FakeTic* t = tic(n);
        if (!t) {
            err = "TIC '" + n + "' não existe";
            return false;
        }
        if (chain.empty()) {
            out = Value::ofTic(t->name);
            return true;
        }
        if (chain[0] == "pos") {
            out = Value::ofVec3(t->pos[0], t->pos[1], t->pos[2]);
        } else if (chain[0] == "rot") {          // 0.9.5 (graus)
            out = Value::ofVec3(t->rot[0], t->rot[1], t->rot[2]);
        } else if (chain[0] == "escala") {       // 0.9.5: copy
            out = Value::ofVec3(t->escala[0], t->escala[1], t->escala[2]);
        } else if (chain[0] == "cor") {          // 0.9.5: colorpars
            out = Value::ofVec3(t->cor[0], t->cor[1], t->cor[2]);
        } else if (chain[0] == "visible") {
            out = Value::ofBool(t->visible);
        } else if (chain[0] == "name") {
            out = Value::ofTxt(t->name);
        } else {
            err = "propriedade '" + chain[0] + "' não existe";
            return false;
        }
        for (size_t i = 1; i < chain.size(); ++i) {
            u32 idx = chain[i] == "x" ? 0 : chain[i] == "y" ? 1 : 2;
            f32 c = 0;
            if (out.vecComponent(idx, c)) {
                out = Value::ofNum(c);
            } else {
                err = "componente inválida";
                return false;
            }
        }
        return true;
    }

    bool setProp(const std::string& n, const std::vector<std::string>& chain,
                 const Value& v, std::string& err) override {
        FakeTic* t = tic(n);
        if (!t) {
            err = "TIC '" + n + "' não existe";
            return false;
        }
        if (chain.size() == 2 && chain[0] == "pos") {
            u32 idx = chain[1] == "x" ? 0 : chain[1] == "y" ? 1 : 2;
            f32 c = 0;
            if (v.t == Type::Num) {
                c = (f32)v.n;
            } else if (v.t == Type::Int) {
                c = (f32)v.i;
            } else {
                v.vecComponent(idx, c);
            }
            t->pos[idx] = c;
            return true;
        }
        if (chain.size() == 1 && chain[0] == "pos" && v.t == Type::Vec3) {
            t->pos[0] = v.v3[0];
            t->pos[1] = v.v3[1];
            t->pos[2] = v.v3[2];
            return true;
        }
        if (chain.size() == 1 && chain[0] == "visible" && v.t == Type::Bool) {
            t->visible = v.b;
            return true;
        }
        // 0.9.5: rot/escala/cor como Vec3 (o caminho dos tykers)
        if (chain.size() == 1 && v.t == Type::Vec3) {
            if (chain[0] == "rot") {
                t->rot[0] = v.v3[0];
                t->rot[1] = v.v3[1];
                t->rot[2] = v.v3[2];
                return true;
            }
            if (chain[0] == "escala") {
                t->escala[0] = v.v3[0];
                t->escala[1] = v.v3[1];
                t->escala[2] = v.v3[2];
                return true;
            }
            if (chain[0] == "cor") {
                t->cor[0] = v.v3[0];
                t->cor[1] = v.v3[1];
                t->cor[2] = v.v3[2];
                return true;
            }
        }
        err = "escrita não suportada no fake";
        return false;
    }

    bool ticExists(const std::string& n) override { return tic(n) != nullptr; }

    // 0.12-U (3.9b): o spawn do fake — valida o tipo (a lista é DELE) e
    // CRIA o TIC (active/visible/nome único) para o devolvido ser usável
    std::vector<std::string> spawned;
    bool spawnTic(const std::string& tipo, f32 x, f32 y, f32 z,
                  std::string& outName, std::string& err) override {
        if (tipo != "esfera" && tipo != "box") {
            err = "spawn: o tipo '" + tipo +
                  "' não existe (tipos: esfera box)";
            return false;
        }
        std::string nome = tipo;
        int suf = 1;
        char b[48];
        while (tic(nome) != nullptr) {
            std::snprintf(b, sizeof(b), "%s.%03d", tipo.c_str(), suf++);
            nome = b;
        }
        FakeTic t;
        t.name = nome;
        t.pos[0] = x;
        t.pos[1] = y;
        t.pos[2] = z;
        tics.push_back(t);
        spawned.push_back(nome);
        outName = nome;
        return true;
    }

    void moveTic(f32 dx, f32 dy, f32 dz) override {
        moved[0] += dx;
        moved[1] += dy;
        moved[2] += dz;
    }
    void explodeTic(bool hide) override {
        explodeCalls++;
        lastExplodeHide = hide ? 1 : 0;
    }
    bool importAnim(const std::string& name, std::string& err) override {
        importedAnim = name;
        // 0.9.5: o play() do tyker — a animação vive no TIC DONO do script
        FakeTic* t = tic(hostTic);
        if (t) {
            for (size_t i = 0; i < t->anims.size(); ++i) {
                if (t->anims[i] == name) {
                    t->activeAnim = (int)i;
                    return true;
                }
            }
        }
        err = "Import.Animation: animação '" + name + "' não encontrada";
        return false;
    }
    bool transitionTo(const std::string& from, const std::string& to,
                      std::string& err) override {
        if (from != currentScene) {
            err = "'" + from + "' não é a cena atual";
            return false;
        }
        for (const std::string& s : scenes) {
            if (s == to) {
                transitionedTo = to;
                return true;
            }
        }
        err = "cena '" + to + "' não existe";
        return false;
    }
    std::string currentSceneName() override { return currentScene; }
    bool search(const std::string& target,
                const std::vector<std::string>& chain, Value& out,
                std::string& err) override {
        return getProp(target, chain, out, err);
    }
    f64 frameDt() override { return dt; }
};

// helpers ---------------------------------------------------------------
struct RunResult {
    bool ok = false;
    Error err;
    Script script;
    FakeHost host;
};

RunResult runScript(const char* src, int frames = 0, f64 dt = 1.0 / 60.0) {
    RunResult r;
    r.script = Script::compile(src, r.err);
    if (!r.err.ok) {
        return r;
    }
    if (!r.script.runStart(r.host, r.err)) {
        return r;
    }
    for (int i = 0; i < frames; ++i) {
        if (!r.script.runFrame(r.host, dt, r.err)) {
            return r;
        }
    }
    r.ok = true;
    return r;
}

bool logsContain(const FakeHost& h, const std::string& needle) {
    for (const std::string& l : h.logs) {
        if (l.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// §3 entry point — on moment 1× · allmoments/frame · top-level 1×
// ---------------------------------------------------------------------------
TEST(on_moment_corre_exatamente_uma_vez) {
    // (a afervação do "1× após 10 frames" com contador é no teste com @+
    // abaixo — este valida o ciclo sem exports: 10 frames sem erro)
    RunResult r = runScript(R"VONI(
v++vezes=0
central main {
  on moment { vezes+=1 }
  allmoments { }
}
)VONI", 10);
    EXPECT(r.ok);
    EXPECT(r.script.started());
    EXPECT(r.script.running());
    EXPECT(r.script.lastTickInstructions() >= 0);   // allmoments vazio = 0 ok
}

TEST(on_moment_uma_vez_contador_via_export) {
    RunResult r = runScript(R"VONI(
v++@+vezes=0
central main {
  on moment { vezes+=1 }
  allmoments { }
}
)VONI", 10);
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex.size() == 1);
    EXPECT(ex[0].name == "vezes");
    EXPECT(ex[0].value.t == Type::Int);
    EXPECT(ex[0].value.i == 1);   // 1× — NÃO 11
}

TEST(allmoments_corre_a_cada_frame) {
    RunResult r = runScript(R"VONI(
v++@+frames=0
central main {
  on moment { }
  allmoments { frames+=1 }
}
)VONI", 10);
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex.size() == 1);
    EXPECT(ex[0].value.i == 10);   // cada frame +1
}

TEST(top_level_corre_uma_vez) {
    RunResult r = runScript(R"VONI(
v++@+top=1
top+=1
central main { on moment { } allmoments { } }
)VONI", 5);
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex[0].value.i == 2);   // declarado 1, top-level 1×
}

TEST(central_main_mal_formado_erro_com_linha) {
    Error err;
    Script s = Script::compile("central main {\n  on moment\n}", err);
    EXPECT(!err.ok);
    EXPECT(err.line >= 1);
}

TEST(central_main_duplicado_erro) {
    Error err;
    Script s = Script::compile(
        "central main { on moment { } }\ncentral main { allmoments { } }",
        err);
    EXPECT(!err.ok);
    EXPECT(err.line == 2);
    EXPECT(err.message.find("central main") != std::string::npos);
}

TEST(evento_duplicado_erro) {
    Error err;
    Script s = Script::compile(
        "central main { on moment { } on moment { } }", err);
    EXPECT(!err.ok);
    EXPECT(err.message.find("on moment") != std::string::npos);
}

// ---------------------------------------------------------------------------
// §2 comentários e fonte vazio
// ---------------------------------------------------------------------------
TEST(comentarios_linha_e_bloco) {
    RunResult r = runScript(R"VONI(
// comentário de linha
v++@+x=1
/* bloco
   de várias linhas */
v++@+y=2
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported().size() == 2);
}

TEST(fonte_vazio_e_sozinho_whitespace_valido) {
    Error e1;
    Script s1 = Script::compile("", e1);
    EXPECT(e1.ok);
    Error e2;
    Script s2 = Script::compile("  \n// só um comentário\n", e2);
    EXPECT(e2.ok);
}

// ---------------------------------------------------------------------------
// §6 loops
// ---------------------------------------------------------------------------
TEST(repeat_n_vezes) {
    RunResult r = runScript(R"VONI(
v++@+n=0
central main {
  on moment { repeat(3) { n+=1 } }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported()[0].value.i == 3);
}

TEST(last_with_contador_conta_e_condicao_usa_n) {
    RunResult r = runScript(R"VONI(
v++@+n=0
v++@+total=0
central main {
  on moment {
    last(n < 10) with n+=1 { total+=1 }
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex[0].name == "n" && ex[0].value.i == 10);
    EXPECT(ex[1].name == "total" && ex[1].value.i == 10);
}

TEST(continue_salta_resto_da_iteracao) {
    RunResult r = runScript(R"VONI(
v++@+antes=0
v++@+depois=0
central main {
  on moment {
    repeat(4) {
      antes+=1
      continue
      depois+=1
    }
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex[0].value.i == 4);   // antes do continue: sempre
    EXPECT(ex[1].value.i == 0);   // depois do continue: nunca
}

TEST(resume_sai_do_loop) {
    RunResult r = runScript(R"VONI(
v++@+i=0
central main {
  on moment {
    repeat(10) {
      i+=1
      exist(i == 3) { resume }
    }
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported()[0].value.i == 3);
}

TEST(continue_fora_de_loop_erro_com_linha) {
    RunResult r = runScript("central main { on moment { continue } "
                            "allmoments { } }");
    EXPECT(!r.ok);
    EXPECT(!r.err.ok);
    EXPECT(r.err.line >= 1);
    EXPECT(r.err.message.find("fora de loop") != std::string::npos);
}

// ---------------------------------------------------------------------------
// §7 condicionais
// ---------------------------------------------------------------------------
TEST(exist_verdadeiro_salta_casos_e_default) {
    RunResult r = runScript(R"VONI(
v++@+marca=0
central main {
  on moment {
    exist(1 == 1) {
      marca=1
    } notexist {
      marca=2
    } and(
      1 == 2(marca=3) stopand
    )
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported()[0].value.i == 1);   // then; nem default nem caso
}

TEST(and_caso_verdadeiro_corre_e_salta_default) {
    RunResult r = runScript(R"VONI(
v++@+x=2
v++@+marca=0
central main {
  on moment {
    exist(x == 99) {
      marca=1
    } notexist {
      marca=10
    } and(
      x == 1(marca=2) stopand
      x == 2(marca=3) stopand
      x == 3(marca=4) stopand
    )
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported()[1].value.i == 3);   // 2º caso; default saltado
}

TEST(and_nenhum_caso_verdadeiro_corre_default) {
    RunResult r = runScript(R"VONI(
v++@+x=7
v++@+marca=0
central main {
  on moment {
    exist(x == 99) {
      marca=1
    } notexist {
      marca=10
    } and(
      x == 1(marca=2) stopand
      x == 2(marca=3) stopand
    )
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported()[1].value.i == 10);   // default
}

TEST(notexist_sem_and_e_else_simples) {
    RunResult r = runScript(R"VONI(
v++@+marca=0
central main {
  on moment {
    exist(1 == 2) {
      marca=1
    } notexist {
      marca=5
    }
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported()[0].value.i == 5);
}

TEST(and_caso_na_forma_nua_condicao_variavel) {
    // `flag(ação)` — a condição nua é a VARIÁVEL flag (desambiguação 🔶)
    RunResult r = runScript(R"VONI(
v++flag=false
v++@+marca=0
central main {
  on moment {
    exist(1 == 2) {
      marca=1
    } notexist {
      marca=10
    } and(
      flag(marca=2) stopand
      1 == 1(marca=3) stopand
    )
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    // flag=false → salta; 1==1 → marca=3 (o default NÃO corre)
    EXPECT(r.script.exported()[0].value.i == 3);
}

TEST(option_forma_do_autor) {
    RunResult r = runScript(R"VONI(
v++@+nivel=2
v++@+marca=0
central main {
  on moment {
    option(nivel) {
      and 1(marca=10) stopand
      and 2(marca=20) stopand
      notoption{ marca=99 }
    }
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported()[1].value.i == 20);
}

TEST(option_default_notoption) {
    RunResult r = runScript(R"VONI(
v++@+nivel=7
v++@+marca=0
central main {
  on moment {
    option(nivel) {
      and 1(marca=10) stopand
      and 2(marca=20) stopand
      notoption{ marca=99 }
    }
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported()[1].value.i == 99);
}

TEST(option_caso_nu_valor_variavel) {
    RunResult r = runScript(R"VONI(
v++alvo=5
v++@+marca=0
central main {
  on moment {
    option(alvo) {
      and 5(marca=1) stopand
      notoption{ marca=2 }
    }
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported()[0].value.i == 1);
}

// ---------------------------------------------------------------------------
// §8 fn + operadores
// ---------------------------------------------------------------------------
TEST(fn_chama_e_devolve) {
    RunResult r = runScript(R"VONI(
fn dobro(n:Num):Num {
  return n * 2
}
v#@+r:Num=0
central main {
  on moment { r=dobro(21) }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported()[0].value.t == Type::Num);
    EXPECT(std::fabs(r.script.exported()[0].value.n - 42.0) < 1e-9);
}

TEST(fn_dois_args_e_escopo_local) {
    RunResult r = runScript(R"VONI(
fn soma(a:Num, b:Num):Num {
  return a + b
}
v#@+r:Num=0
central main { on moment { r=soma(3, 4) } allmoments { } }
)VONI");
    EXPECT(r.ok);
    EXPECT(std::fabs(r.script.exported()[0].value.n - 7.0) < 1e-9);
}

TEST(fn_arity_errada_erro_com_linha) {
    RunResult r = runScript(R"VONI(
fn soma(a:Num, b:Num):Num { return a + b }
central main { on moment { soma(1) } allmoments { } }
)VONI");
    EXPECT(!r.ok);
    EXPECT(r.err.message.find("argumento") != std::string::npos);
}

TEST(operadores_aritmeticos_e_comparacoes) {
    RunResult r = runScript(R"VONI(
v++@+soma=2+3
v++@+sub=10-4
v++@+mul=3*4
v++@+div=10/4
v++@+divf=10.0/4
v++@+eq=1==1
v++@+ne=1!=2
v++@+lt=1<2
v++@+ge=2>=3
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex[0].value.i == 5);
    EXPECT(ex[1].value.i == 6);
    EXPECT(ex[2].value.i == 12);
    EXPECT(ex[3].value.i == 2);     // Int/Int = Int (🔶 divisão inteira)
    EXPECT(std::fabs(ex[4].value.n - 2.5) < 1e-9);
    EXPECT(ex[5].value.b == true);
    EXPECT(ex[6].value.b == true);
    EXPECT(ex[7].value.b == true);
    EXPECT(ex[8].value.b == false);
}

TEST(operadores_logicos_com_curto_circuito) {
    RunResult r = runScript(R"VONI(
v++@+ok=true and false
v++@+ok2=true or false
v++@+ok3=not false
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex[0].value.b == false);
    EXPECT(ex[1].value.b == true);
    EXPECT(ex[2].value.b == true);
}

TEST(concat_txt) {
    RunResult r = runScript(R"VONI(
v++@+s="ola" + " " + "voni"
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported()[0].value.s == "ola voni");
}

TEST(unario_negativo) {
    RunResult r = runScript(R"VONI(
v++@+g=-9.8
v++@+i=-5
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    EXPECT(std::fabs(r.script.exported()[0].value.n + 9.8) < 1e-9);
    EXPECT(r.script.exported()[1].value.i == -5);
}

// ---------------------------------------------------------------------------
// §5 nomes — reservadas + maiúsculas
// ---------------------------------------------------------------------------
TEST(reservadas_como_nome_erro_claro) {
    const char* words[] = {"exist",   "notexist", "option",  "and",
                           "stopand", "notoption", "repeat", "last",
                           "with",    "continue", "resume",  "move",
                           "linker",  "to",       "tyker",   "central",
                           "main",    "on",       "moment",  "allmoments"};
    for (const char* w : words) {
        std::string src = std::string("v++") + w + "=1\n"
                          "central main { on moment { } allmoments { } }";
        Error err;
        Script s = Script::compile(src.c_str(), err);
        EXPECT(!err.ok);
        EXPECT(err.message.find("reservada") != std::string::npos);
    }
}

TEST(nome_maiuscula_erro_claro) {
    Error err;
    Script s = Script::compile("v++Velocidade=1", err);
    EXPECT(!err.ok);
    EXPECT(err.message.find("maiúscula") != std::string::npos);
}

TEST(fn_e_param_maiusculas_erro) {
    Error e1;
    Script s1 = Script::compile("fn Dois():Num { return 2 }", e1);
    EXPECT(!e1.ok);
    Error e2;
    Script s2 = Script::compile("fn f(A:Num):Num { return A }", e2);
    EXPECT(!e2.ok);
}

TEST(redeclaracao_erro_com_linha) {
    Error err;
    Script s = Script::compile("v++x=1\nv++x=2", err);
    EXPECT(!err.ok);
    EXPECT(err.line == 2);
    EXPECT(err.message.find("já foi declarada") != std::string::npos);
}

TEST(nomes_de_componentes_tyker_livres_fora_do_tyker) {
    // §5: follow/look/orbit/… reservam-se SÓ dentro de bloco tyker (0.9.3)
    RunResult r = runScript(R"VONI(
v++follow=1
v++look=2
v++point=3
v++map=4
v++find=5
v++@+s=0
central main { on moment { s=follow+look+point+map+find } allmoments { } }
)VONI");
    EXPECT(r.ok);
    EXPECT(r.script.exported()[0].value.i == 15);
}

TEST(nome_maiuscula_change_erro) {
    // Change é nome de componente tyker mas COMEÇA POR MAIÚSCULA → regra
    // geral §5 aplica-se: nomes de utilizador começam por minúscula
    Error err;
    Script s = Script::compile("v++Change=1", err);
    EXPECT(!err.ok);
    EXPECT(err.message.find("maiúscula") != std::string::npos);
}

// ---------------------------------------------------------------------------
// §9 comandos
// ---------------------------------------------------------------------------
TEST(view_p_escreve_no_log_com_prefixo_voni) {
    RunResult r = runScript(R"VONI(
central main {
  on moment { View P "ola voni" }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    EXPECT(r.host.logs.size() == 1);
    EXPECT(r.host.logs[0].rfind("voni: ", 0) == 0);
    EXPECT(r.host.logs[0] == "voni: ola voni");
}

TEST(view_p_utf8_acentos) {
    RunResult r = runScript(
        "central main { on moment { View P \"ação é éção\" } "
        "allmoments { } }");
    EXPECT(r.ok);
    EXPECT(r.host.logs[0] == "voni: ação é éção");
}

TEST(view_object_erro_legivel) {
    RunResult r = runScript(
        "central main { on moment { View Object } allmoments { } }");
    EXPECT(!r.ok);
    EXPECT(r.err.message.find("consola ainda não existe") !=
           std::string::npos);
}

TEST(view_p_sem_texto_erro_forma) {
    RunResult r = runScript(
        "central main { on moment { View P } allmoments { } }");
    EXPECT(!r.ok);
    EXPECT(r.err.message.find("View P") != std::string::npos);
}

TEST(move_move_o_tic_relativo) {
    RunResult r = runScript(R"VONI(
central main {
  on moment { move(1, 2, 3) }
  allmoments { move(0, 0, 1) }
}
)VONI", 10);
    EXPECT(r.ok);
    EXPECT(std::fabs(r.host.moved[0] - 1.0f) < 1e-6);
    EXPECT(std::fabs(r.host.moved[1] - 2.0f) < 1e-6);
    EXPECT(std::fabs(r.host.moved[2] - 13.0f) < 1e-6);   // 3 + 10×1
}

TEST(explode_et_er_visible) {
    RunResult r = runScript(R"VONI(
central main {
  on moment { Explode.TIC.et }
  allmoments { Explode.TIC.er }
}
)VONI", 5);
    EXPECT(r.ok);
    EXPECT(r.host.explodeCalls == 6);
    EXPECT(r.host.lastExplodeHide == 0);   // o último frame chamou .er
}

TEST(import_animation_erro_legivel_sem_anim) {
    RunResult r = runScript(R"VONI(
central main {
  on moment { Import.Animation("correr") }
  allmoments { }
}
)VONI");
    EXPECT(!r.ok);
    EXPECT(r.err.message.find("correr") != std::string::npos);
}

TEST(transition_for_valida_origem_e_destino) {
    // origem != cena atual → erro
    RunResult r1 = runScript(
        "central main { on moment { outra.transition.for(\"cena2\") } "
        "allmoments { } }");
    EXPECT(!r1.ok);
    EXPECT(r1.err.message.find("não é a cena atual") != std::string::npos);

    // destino inexistente → erro
    RunResult r2 = runScript(
        "central main { on moment { cena1.transition.for(\"naoexiste\") } "
        "allmoments { } }");
    EXPECT(!r2.ok);
    EXPECT(r2.err.message.find("não existe") != std::string::npos);

    // caminho certo → transiciona
    RunResult r3 = runScript(
        "central main { on moment { cena1.transition.for(\"cena2\") } "
        "allmoments { } }");
    EXPECT(r3.ok);
    EXPECT(r3.host.transitionedTo == "cena2");
}

TEST(deltatime_increment_acumula_valor_vezes_dt) {
    RunResult r = runScript(R"VONI(
v#@+tempo:Num=0
central main {
  on moment { }
  allmoments { Deltatime.Increment(tempo, 1) }
}
)VONI", 60, 1.0 / 60.0);
    EXPECT(r.ok);
    EXPECT(std::fabs(r.script.exported()[0].value.n - 1.0) < 1e-6);
}

TEST(search_alvo_propriedade_le) {
    FakeHost h;
    h.tics.push_back(FakeTic{"jogador"});
    h.tics[0].pos[0] = 5.5f;
    Error err;
    Script s = Script::compile(
        "v#@+px:Num=0\ncentral main { on moment { px=Search.jogador.pos.x } "
        "allmoments { } }",
        err);
    EXPECT(err.ok);
    EXPECT(s.runStart(h, err));
    auto ex = s.exported();
    EXPECT(ex.size() == 1);
    EXPECT(std::fabs(ex[0].value.n - 5.5) < 1e-9);   // leu via RTTI
}

TEST(search_alvo_inexistente_erro) {
    RunResult r = runScript(
        "v++x=Search.fantasma.pos\ncentral main { on moment { } "
        "allmoments { } }");
    // compila (dinâmico); o ERRO aparece no runStart (top-level corre)
    EXPECT(!r.ok);
    EXPECT(r.err.message.find("fantasma") != std::string::npos);
}

TEST(propriedades_tic_ponto_leitura_escrita) {
    FakeHost h;
    h.tics.push_back(FakeTic{"jogador"});
    h.tics.push_back(FakeTic{"alvo"});
    Error err;
    Script s = Script::compile(
        "central main {\n"
        "  on moment {\n"
        "    jogador.pos.x=5\n"
        "    alvo.pos=jogador.pos\n"
        "  }\n"
        "  allmoments { }\n"
        "}",
        err);
    EXPECT(err.ok);
    EXPECT(s.runStart(h, err));
    EXPECT(std::fabs(h.tics[0].pos[0] - 5.0f) < 1e-6);
    EXPECT(std::fabs(h.tics[1].pos[0] - 5.0f) < 1e-6);
}

TEST(comando_desconhecido_erro_claro) {
    RunResult r = runScript(
        "central main { on moment { Saltar(3) } allmoments { } }");
    EXPECT(!r.ok);
    EXPECT(r.err.message.find("Saltar") != std::string::npos);
    EXPECT(r.err.message.find("Docs") != std::string::npos);
}

// ---------------------------------------------------------------------------
// §1 🔶 — mudar um literal na GRAMÁTICA muda o comportamento (sem C++)
// ---------------------------------------------------------------------------
TEST(gramatica_literal_muda_comportamento_sem_tocar_em_cpp) {
    const char* src = "repeat(2) { }\ncentral main { on moment { } "
                      "allmoments { } }";

    // 1) gramática default: parseia
    setGrammar(nullptr);
    Error e1;
    Script s1 = Script::compile(src, e1);
    EXPECT(e1.ok);

    // 2) override: 'repeat' passa a 'repita' (literal MUDADO — zero C++)
    std::string gram = activeGrammar();
    const size_t at = gram.find("'repeat'");
    EXPECT(at != std::string::npos);
    gram.replace(at, 8, "'repita'");
    setGrammar(gram.c_str());

    // a fonte com a palavra VELHA deixa de parsear
    Error e2;
    Script s2 = Script::compile(src, e2);
    EXPECT(!e2.ok);

    // e a fonte com a palavra NOVA passa
    Error e3;
    Script s3 = Script::compile(
        "repita(2) { }\ncentral main { on moment { } allmoments { } }", e3);
    EXPECT(e3.ok);

    // 3) repor o default
    setGrammar(nullptr);
    Error e4;
    Script s4 = Script::compile(src, e4);
    EXPECT(e4.ok);
}

// ---------------------------------------------------------------------------
// §12 sandbox — budgets + profundidade
// ---------------------------------------------------------------------------
TEST(budget_aborta_loop_infinito_com_erro_legivel) {
    RunResult r = runScript(
        "central main { on moment { last(true) { } } allmoments { } }");
    EXPECT(!r.ok);
    EXPECT(r.err.message.find("budget") != std::string::npos);
    EXPECT(r.err.line >= 1);
}

TEST(budget_aborta_repeat_gigante) {
    RunResult r = runScript(
        "v++x=0\ncentral main { on moment { repeat(999999999) { x+=1 } } "
        "allmoments { } }");
    EXPECT(!r.ok);
    EXPECT(r.err.message.find("budget") != std::string::npos);
}

TEST(profundidade_256_recursao_infinita_aborta) {
    RunResult r = runScript(R"VONI(
fn eco():Num {
  return eco()
}
central main { on moment { eco() } allmoments { } }
)VONI");
    EXPECT(!r.ok);
    EXPECT(r.err.message.find("profundidade") != std::string::npos);
}

TEST(divisao_por_zero_erro_legivel) {
    RunResult r = runScript(
        "v++x=1/0\ncentral main { on moment { } allmoments { } }");
    EXPECT(!r.ok);
    EXPECT(r.err.message.find("zero") != std::string::npos);
}

// ---------------------------------------------------------------------------
// tipos (§4) — anotações, inferência, coerção
// ---------------------------------------------------------------------------
TEST(tipo_explitico_e_inferencia) {
    RunResult r = runScript(R"VONI(
v#@+vel:Num=5.5
v++@+vida=100
v#nome:Txt="jogador"
v#ativo:Bool=true
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex.size() == 2);   // só os @+
    EXPECT(ex[0].name == "vel" && ex[0].value.t == Type::Num);
    EXPECT(ex[1].name == "vida" && ex[1].value.t == Type::Int);
}

TEST(tipo_errado_erro_claro) {
    // a verificação de tipos é DINÂMICA (o init pode ser qualquer expressão):
    // o erro aparece no runStart com linha
    RunResult r1 = runScript("v#x:Num=\"texto\"\ncentral main { on moment "
                             "{ } allmoments { } }");
    EXPECT(!r1.ok);
    EXPECT(r1.err.message.find("Num") != std::string::npos);

    RunResult r2 = runScript("v#y:Int=1.5\ncentral main { on moment { } "
                             "allmoments { } }");
    EXPECT(!r2.ok);
    EXPECT(r2.err.message.find("Int") != std::string::npos);
}

TEST(tipo_desconhecido_erro_claro) {
    Error err;
    Script s = Script::compile("v#x:Float=1.0", err);
    EXPECT(!err.ok);
    EXPECT(err.message.find("Float") != std::string::npos);
}

TEST(atribuicao_tipo_respeitado) {
    RunResult r = runScript(
        "v#x:Num=1\nx=\"txt\"\ncentral main { on moment { } "
        "allmoments { } }");
    EXPECT(!r.ok);
    EXPECT(r.err.message.find("Num") != std::string::npos);
}

TEST(valor_tic_e_propriedades_de_valor) {
    FakeHost h;
    h.tics.push_back(FakeTic{"jogador"});
    Error err;
    Script s = Script::compile(
        "v#alvo:TIC=jogador\nv#@+px:Num=0\ncentral main { on moment { "
        "px=alvo.pos.x } allmoments { } }",
        err);
    EXPECT(err.ok);
    EXPECT(s.runStart(h, err));
    auto ex = s.exported();
    EXPECT(ex.size() == 1 && ex[0].value.i == 0);   // pos.x do fake = 0
}

// ---------------------------------------------------------------------------
// ciclo da Script API (§3) — idempotência, stop/restart, exported, setVar
// ---------------------------------------------------------------------------
TEST(runstart_duplo_devolve_false) {
    FakeHost h;
    Error err;
    Script s = Script::compile(
        "central main { on moment { } allmoments { } }", err);
    EXPECT(err.ok);
    EXPECT(s.runStart(h, err));
    Error err2;
    EXPECT(!s.runStart(h, err2));
    EXPECT(!err2.ok);
}

TEST(stop_reinicia_do_zero) {
    FakeHost h;
    Error err;
    Script s = Script::compile(
        "v++@+x=0\ncentral main { on moment { x=5 } allmoments { x+=1 } }",
        err);
    EXPECT(err.ok);
    EXPECT(s.runStart(h, err));
    EXPECT(s.runFrame(h, 1.0 / 60.0, err));
    EXPECT(s.runFrame(h, 1.0 / 60.0, err));
    EXPECT(s.exported()[0].value.i == 7);

    s.stop();
    EXPECT(!s.started());
    Error e2;
    EXPECT(s.runStart(h, e2));   // recomeça: declaração de novo
    EXPECT(s.exported()[0].value.i == 5);
}

TEST(setvar_inspector_aplica_ao_vivo) {
    FakeHost h;
    Error err;
    Script s = Script::compile(
        "v#@+vida:Num=100\ncentral main { on moment { } allmoments { } }",
        err);
    EXPECT(err.ok);
    EXPECT(s.runStart(h, err));
    Error e2;
    EXPECT(s.setVar("vida", Value::ofNum(50.0), e2));
    EXPECT(std::fabs(s.exported()[0].value.n - 50.0) < 1e-9);
    // tipo errado rejeitado
    Error e3;
    EXPECT(!s.setVar("vida", Value::ofTxt("x"), e3));
}

TEST(erro_fatal_para_frames_seguintes) {
    RunResult r = runScript(
        "v++@+i=0\ncentral main { on moment { } allmoments { i+=1 } }", 3);
    EXPECT(r.ok);
    // forçar erro num frame seguinte: variável inexistente em allmoments
    FakeHost h;
    Error err;
    // erro de RUNTIME no allmoments (variável inexistente — compila, falha
    // ao correr; View P 5 seria erro de SINTAXE porque o 5 sobra)
    Script s = Script::compile(
        "central main { on moment { } allmoments { inexistente+=1 } }", err);
    EXPECT(err.ok);
    EXPECT(s.runStart(h, err));
    Error e2;
    EXPECT(!s.runFrame(h, 1.0 / 60.0, e2));   // erro: View P com número
    Error e3;
    EXPECT(!s.runFrame(h, 1.0 / 60.0, e3));   // morto continua a falhar
    EXPECT(!s.running());
}

// ---------------------------------------------------------------------------
// highlighter (§10) — classes apenas
// ---------------------------------------------------------------------------
#include "voni/VoniHighlight.h"

TEST(highlight_classes_da_paleta) {
    using voni::hl::Cls;
    voni::hl::BlockCommentState st;
    auto toks = voni::hl::classifyLine("repeat(3) { View P \"txt\" } // fim",
                                       st);
    // repeat: Reserved · 3: Number · View/Engine: Engine · P: Engine?
    // P é maiúscula → Engine ✓ · "txt": Str · comentário: Comment
    bool sawReserved = false, sawNum = false, sawStr = false, sawComment = false,
         sawEngine = false;
    for (const auto& t : toks) {
        std::string w = std::string("repeat(3) { View P \"txt\" } // fim")
                            .substr(t.begin, t.len);
        if (w == "repeat") {
            sawReserved = sawReserved || t.cls == Cls::Reserved;
        }
        if (w == "3") {
            sawNum = sawNum || t.cls == Cls::Number;
        }
        if (w == "View") {
            sawEngine = sawEngine || t.cls == Cls::Engine;
        }
        if (w.size() > 2 && w[0] == '"') {
            sawStr = sawStr || t.cls == Cls::Str;
        }
        if (w.rfind("//", 0) == 0) {
            sawComment = sawComment || t.cls == Cls::Comment;
        }
    }
    EXPECT(sawReserved);
    EXPECT(sawNum);
    EXPECT(sawEngine);
    EXPECT(sawStr);
    EXPECT(sawComment);
}

TEST(highlight_comentario_bloco_atravessa_linhas) {
    using voni::hl::Cls;
    voni::hl::BlockCommentState st;
    auto t1 = voni::hl::classifyLine("codigo /* inicio", st);
    EXPECT(st.inBlock);
    EXPECT(!t1.empty() && t1.back().cls == Cls::Comment);
    auto t2 = voni::hl::classifyLine("continua dentro", st);
    EXPECT(t2.size() == 1 && t2[0].cls == Cls::Comment);
    auto t3 = voni::hl::classifyLine("fim */ v++x=1", st);
    EXPECT(!st.inBlock);
    EXPECT(t3.front().cls == Cls::Comment);
    // depois do fecho: v++x=1 → v Reserved, x User, 1 Number
    bool sawUser = false;
    for (const auto& t : t3) {
        if (t.begin > 5) {
            std::string w = std::string("fim */ v++x=1").substr(t.begin, t.len);
            if (w == "x") {
                sawUser = sawUser || t.cls == Cls::User;
            }
        }
    }
    EXPECT(sawUser);
}

TEST(highlight_user_minuscula_vs_engine_maiuscula) {
    EXPECT(voni::hl::wordClass("jogador") == voni::hl::Cls::User);
    EXPECT(voni::hl::wordClass("Search") == voni::hl::Cls::Engine);
    EXPECT(voni::hl::wordClass("last") == voni::hl::Cls::Reserved);
    EXPECT(voni::hl::wordClass("v") == voni::hl::Cls::Reserved);
    EXPECT(voni::hl::wordClass("fn") == voni::hl::Cls::Reserved);
    EXPECT(voni::hl::wordClass("return") == voni::hl::Cls::Reserved);
}

// ---------------------------------------------------------------------------
// Docs (§11)
// ---------------------------------------------------------------------------
#include "voni/VoniDocs.h"

TEST(docs_entradas_populadas_e_pesquisam) {
    auto& all = voni::docs::all();
    EXPECT(all.size() >= 25);   // linguagem + comandos
    // cada entrada com nome/desc/sintaxe/exemplo
    for (const auto& e : all) {
        EXPECT(e.name && *e.name);
        EXPECT(e.desc && *e.desc);
        EXPECT(e.syntax && *e.syntax);
        EXPECT(e.example && *e.example);
    }
    // pesquisa apanha por nome e por descrição
    auto r1 = voni::docs::search("view");
    EXPECT(!r1.empty());
    auto r2 = voni::docs::search("loop");
    EXPECT(!r2.empty());   // "loop infinito" na desc do budget? — last/repeat
    auto r3 = voni::docs::search("zzzz");
    EXPECT(r3.empty());
    // categorias: 0.9.2 = Comando+Linguagem; linker/tyker ficam p/ 0.9.3
    bool hasCmd = false, hasLang = false;
    for (const auto& e : all) {
        hasCmd = hasCmd || e.cat == voni::docs::Cat::Comando;
        hasLang = hasLang || e.cat == voni::docs::Cat::Linguagem;
    }
    EXPECT(hasCmd && hasLang);
}

TEST(docs_lista_reservadas_completa) {
    // 0.12-U (3.9c): 20 → 23 — os TIMERS entram na linguagem (every/after/
    // when são reservados: a gramática não dá o nome ao utilizador e o
    // statement não nasce partido; recalibração com nota no relatório 3.9c)
    EXPECT(reservedWordCount() == 23);
    EXPECT(isReservedWord("stopand"));
    EXPECT(isReservedWord("allmoments"));
    EXPECT(isReservedWord("every") && isReservedWord("after") &&
           isReservedWord("when"));
    EXPECT(!isReservedWord("stop"));      // não existe na V.ONI
    EXPECT(!isReservedWord("follow"));    // só dentro de tyker (0.9.3)
    EXPECT(!isReservedWord("fn"));        // estrutural, não reservada §5
}

// ===========================================================================
// 0.9.5 · METADE 1 — LINKERS & TYKERS (spec fechada da entrega)
// ===========================================================================
// O REGISTO central (VoniRegistry) valida nomes/argc no compile, despacha
// os handlers no runtime e alimenta as Docs — o teste parser_independente
// PROVA que adicionar componente novo não muda a gramática.
// ===========================================================================
#include "voni/VoniRegistry.h"
#include "voni/VoniTykers.h"

namespace {

// corre N frames contra o FakeHost (falha ruidosa se algo morrer a meio)
bool runFrames(Script& s, FakeHost& h, int n) {
    Error err;
    for (int i = 0; i < n; ++i) {
        if (!s.runFrame(h, h.dt, err)) {
            return false;
        }
    }
    return true;
}

bool hasLog(const FakeHost& h, const std::string& sub) {
    for (const std::string& l : h.logs) {
        if (l.find(sub) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(linker_declara_e_regista_no_rf) {
    const char* src =
        "linker(seguidor)to(alvo)=RF(principal)\n"
        "linker(cubo)to(esfera)=RF(outro)\n"
        "tyker(t){ find(principal) follow() }\n"
        "central main { on moment { } allmoments { } }\n";
    Error err;
    Script s = Script::compile(src, err);
    EXPECT(err.ok);
    FakeHost h;
    EXPECT(s.runStart(h, err));
    // o RF regista os links (METADE 1: o registo é apropriável nos testes)
    const auto* rf = s.impl().rfReg.findRf("principal");
    EXPECT(rf != nullptr && rf->size() == 1);
    EXPECT((*rf)[0].origem[0] == "seguidor");
    EXPECT((*rf)[0].destino[0] == "alvo");
    const auto* rf2 = s.impl().rfReg.findRf("outro");
    EXPECT(rf2 != nullptr && rf2->size() == 1);
    // RF inexistente no registo → null
    EXPECT(s.impl().rfReg.findRf("naoexiste") == nullptr);
}

TEST(linker_sem_rf_erro_que_ensina) {
    // "RF obrigatória" da spec: sem o =RF(nome) a declaração deixa de ser
    // um linker — vira comando desconhecido e o runtime ENSINA a forma
    Error err;
    Script s = Script::compile("linker(a)to(b)\ncentral main { }", err);
    EXPECT(err.ok);
    FakeHost h;
    EXPECT(!s.runStart(h, err));
    EXPECT(err.message.find("linker") != std::string::npos);
    EXPECT(err.message.find("RF") != std::string::npos);
}

TEST(tyker_find_obrigatorio_de_primeiro) {
    Error err;
    Script a = Script::compile(
        "linker(x)to(y)=RF(p)\n"
        "tyker(t){ follow() }\n"
        "central main { }", err);
    EXPECT(!err.ok);
    EXPECT(err.message.find("find(RF)") != std::string::npos);

    Error err2;
    Script b = Script::compile(
        "linker(x)to(y)=RF(p)\n"
        "tyker(t){ look() find(p) }\n"
        "central main { }", err2);
    EXPECT(!err2.ok);
    EXPECT(err2.message.find("1º") != std::string::npos);
}

TEST(tyker_corpo_so_componentes_ensina) {
    Error err;
    Script s = Script::compile(
        "linker(x)to(y)=RF(p)\n"
        "tyker(t){ find(p) View P \"ola\" }\n"
        "central main { }", err);
    EXPECT(!err.ok);
    EXPECT(err.message.find("só aceita componentes") != std::string::npos);
    EXPECT(err.message.find("View") != std::string::npos);
}

TEST(tyker_componente_desconhecido) {
    Error err;
    Script s = Script::compile(
        "linker(x)to(y)=RF(p)\n"
        "tyker(t){ find(p) frobnicate() }\n"
        "central main { }", err);
    EXPECT(!err.ok);
    EXPECT(err.message.find("não existe") != std::string::npos);
}

TEST(tyker_argc_ensina_com_a_sintaxe_do_registo) {
    Error err;
    Script s = Script::compile(
        "linker(x)to(y)=RF(p)\n"
        "tyker(t){ find(p) follow(1, 2, 3) }\n"
        "central main { }", err);
    EXPECT(!err.ok);
    EXPECT(err.message.find("escreve assim") != std::string::npos);
    EXPECT(err.message.find("follow") != std::string::npos);
}

TEST(rf_em_falta_o_tyker_nao_corre) {
    // R-012: o erro LEGÍVEL da spec; o tyker não corre; o script CONTINUA
    const char* src =
        "linker(a)to(b)=RF(principal)\n"
        "tyker(bom){ find(principal) follow() }\n"
        "tyker(mau){ find(fantasma) follow() }\n"
        "central main { allmoments { View P \"anda\" } }\n";
    Error err;
    Script s = Script::compile(src, err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"a", {10, 0, 0}});
    h.tics.push_back({"b", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    // o erro exato da spec
    EXPECT(hasLog(h, "RF 'fantasma' não encontrada"));
    EXPECT(hasLog(h, "tyker 'mau' não corre"));
    // o OUTRO tyker corre (follow) e o allmoments continua
    EXPECT(runFrames(s, h, 1));
    EXPECT(hasLog(h, "anda"));
    FakeTic* a = h.tic("a");
    EXPECT(a != nullptr);
    EXPECT(a->pos[0] == 0.0f);   // follow() colou no destino
}

TEST(ciclo_direto_rejeitado) {
    // R-011: a→b + b→a no MESMO RF → erro legível, nunca crash
    const char* src =
        "linker(a)to(b)=RF(p)\n"
        "linker(b)to(a)=RF(p)\n"
        "central main { }\n";
    Error err;
    Script s = Script::compile(src, err);
    EXPECT(err.ok);
    FakeHost h;
    EXPECT(!s.runStart(h, err));
    EXPECT(!err.ok);
    EXPECT(err.message.find("ciclo") != std::string::npos);
    EXPECT(err.message.find("'a'") != std::string::npos);
    EXPECT(err.message.find("'b'") != std::string::npos);
}

TEST(ciclo_longo_rejeitado) {
    // a→b→c→a: o DFS apanha também os ciclos compridos
    Error err;
    Script s = Script::compile(
        "linker(a)to(b)=RF(p)\n"
        "linker(b)to(c)=RF(p)\n"
        "linker(c)to(a)=RF(p)\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    EXPECT(!s.runStart(h, err));
    EXPECT(err.message.find("ciclo") != std::string::npos);
}

TEST(profundidade_256_aborta_legivel) {
    // cadeia de 300 linkers no MESMO RF: o DFS tem teto 256 → abort legível
    std::string src;
    for (int i = 0; i < 299; ++i) {
        src += "linker(n" + std::to_string(i) + ")to(n" +
               std::to_string(i + 1) + ")=RF(c)\n";
    }
    src += "central main { }\n";
    Error err;
    Script s = Script::compile(src.c_str(), err);
    EXPECT(err.ok);
    FakeHost h;
    EXPECT(!s.runStart(h, err));
    EXPECT(err.message.find("profundidade") != std::string::npos);
    EXPECT(err.message.find("256") != std::string::npos);
}

TEST(follow_cola_e_guarda_distancia) {
    const char* src =
        "linker(seguidor)to(alvo)=RF(p)\n"
        "tyker(s){ find(p) follow() }\n"
        "central main { }\n";
    Error err;
    Script s = Script::compile(src, err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"seguidor", {10, 0, 0}});
    h.tics.push_back({"alvo", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    FakeTic* seg = h.tic("seguidor");
    EXPECT(seg != nullptr);
    // follow() SEM args: cola no destino
    EXPECT(seg->pos[0] == 0.0f && seg->pos[1] == 0.0f && seg->pos[2] == 0.0f);
}

TEST(follow_distancia) {
    Error err;
    Script s = Script::compile(
        "linker(seguidor)to(alvo)=RF(p)\n"
        "tyker(s){ find(p) follow(2) }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"seguidor", {10, 0, 0}});
    h.tics.push_back({"alvo", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    FakeTic* seg = h.tic("seguidor");
    EXPECT(seg != nullptr);
    EXPECT(test::nearEqF(seg->pos[0], 2.0f));
    EXPECT(test::nearEqF(seg->pos[1], 0.0f));
    EXPECT(test::nearEqF(seg->pos[2], 0.0f));
}

TEST(follow_suaviza_e_converge) {
    Error err;
    Script s = Script::compile(
        "linker(seguidor)to(alvo)=RF(p)\n"
        "tyker(s){ find(p) follow(0, 5) }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"seguidor", {10, 0, 0}});
    h.tics.push_back({"alvo", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    FakeTic* seg = h.tic("seguidor");
    EXPECT(seg != nullptr);
    const f32 after1 = seg->pos[0];
    EXPECT(after1 > 1.0f && after1 < 10.0f);   // mexeu MAS não colou
    EXPECT(runFrames(s, h, 600));
    EXPECT(test::nearEqF(seg->pos[0], 0.0f, 1e-2f));   // converge
}

TEST(look_aponta_ao_destino) {
    Error err;
    Script s = Script::compile(
        "linker(vigia)to(alvo)=RF(p)\n"
        "tyker(s){ find(p) look() }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"vigia", {0, 0, 0}});
    h.tics.push_back({"alvo", {5, 0, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    FakeTic* v = h.tic("vigia");
    EXPECT(v != nullptr);
    EXPECT(test::nearEqF(v->rot[1], 90.0f, 1e-2f));   // yaw para +X
    EXPECT(test::nearEqF(v->rot[0], 0.0f, 1e-2f));    // sem pitch (mesma altura)
}

TEST(orbit_circula_o_destino) {
    Error err;
    Script s = Script::compile(
        "linker(lua)to(planeta)=RF(p)\n"
        "tyker(s){ find(p) orbit(2, 90) }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"lua", {2, 0, 0}});
    h.tics.push_back({"planeta", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 60));   // 1 s a 60 fps = 90°
    FakeTic* lua = h.tic("lua");
    EXPECT(lua != nullptr);
    EXPECT(test::nearEqF(lua->pos[0], 0.0f, 1e-2f));
    EXPECT(test::nearEqF(lua->pos[2], 2.0f, 1e-2f));
    EXPECT(test::nearEqF(lua->pos[1], 0.0f));
}

TEST(copy_copia_propriedade_por_frame) {
    Error err;
    Script s = Script::compile(
        "linker(espelho)to(modelo)=RF(p)\n"
        "tyker(s){ find(p) copy(escala) }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"espelho", {0, 0, 0}});
    FakeTic modelo;
    modelo.name = "modelo";
    modelo.escala[0] = 3;
    modelo.escala[1] = 4;
    modelo.escala[2] = 5;
    h.tics.push_back(modelo);
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    FakeTic* esp = h.tic("espelho");
    EXPECT(esp != nullptr);
    EXPECT(test::nearEqF(esp->escala[0], 3.0f));
    EXPECT(test::nearEqF(esp->escala[1], 4.0f));
    EXPECT(test::nearEqF(esp->escala[2], 5.0f));
}

TEST(map_e_shading_sao_noop_validos) {
    Error err;
    Script s = Script::compile(
        "linker(a)to(b)=RF(p)\n"
        "tyker(s){ find(p) map() shading() }\n"
        "central main { allmoments { View P \"vivo\" } }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"a", {7, 0, 0}});
    h.tics.push_back({"b", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 5));
    // no-op: nada mexeu, o script vive
    FakeTic* a = h.tic("a");
    EXPECT(a != nullptr && a->pos[0] == 7.0f);
    EXPECT(hasLog(h, "vivo"));
}

TEST(change_reescreve_o_rf_partilhado) {
    // Change(destino)to(novo): o follow passa a seguir o NOVO alvo
    Error err;
    Script s = Script::compile(
        "linker(seguidor)to(alvo1)=RF(p)\n"
        "tyker(s){ find(p) Change(destino)to(alvo2) follow() }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"seguidor", {10, 0, 0}});
    h.tics.push_back({"alvo1", {0, 0, 0}});
    h.tics.push_back({"alvo2", {0, 5, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    FakeTic* seg = h.tic("seguidor");
    EXPECT(seg != nullptr);
    // foi atrás do alvo2 (0,5,0) — não do alvo1
    EXPECT(test::nearEqF(seg->pos[0], 0.0f, 1e-3f));
    EXPECT(test::nearEqF(seg->pos[1], 5.0f, 1e-3f));
    // o RF foi REESCRITO (partilhado)
    const auto* rf = s.impl().rfReg.findRf("p");
    EXPECT(rf != nullptr && !rf->empty());
    EXPECT((*rf)[0].destino[0] == "alvo2");
}

TEST(point_fixa_e_limpa_o_alvo) {
    Error err;
    Script s = Script::compile(
        "linker(seguidor)to(alvo)=RF(p)\n"
        "tyker(s){ find(p) point(0, 5, 0) follow() }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"seguidor", {10, 0, 0}});
    h.tics.push_back({"alvo", {3, 3, 3}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    FakeTic* seg = h.tic("seguidor");
    EXPECT(seg != nullptr);
    // seguiu o PONTO (0,5,0), não o destino vivo (3,3,3)
    EXPECT(seg->pos[0] == 0.0f && seg->pos[1] == 5.0f && seg->pos[2] == 0.0f);

    // point() limpa: outro tyker no mesmo script volta ao destino vivo
    Error err2;
    Script s2 = Script::compile(
        "linker(seguidor)to(alvo)=RF(p)\n"
        "tyker(s){ find(p) point() follow() }\n"
        "central main { }\n", err2);
    EXPECT(err2.ok);
    FakeHost h2;
    h2.tics.push_back({"seguidor", {10, 0, 0}});
    h2.tics.push_back({"alvo", {3, 3, 3}});
    EXPECT(s2.runStart(h2, err2));
    EXPECT(runFrames(s2, h2, 1));
    FakeTic* seg2 = h2.tic("seguidor");
    EXPECT(seg2 != nullptr);
    EXPECT(seg2->pos[0] == 3.0f && seg2->pos[1] == 3.0f && seg2->pos[2] == 3.0f);
}

TEST(colorpars_hex_nome_e_outro_parametro) {
    Error err;
    Script s = Script::compile(
        "linker(seguidor)to(alvo)=RF(p)\n"
        "tyker(s){ find(p) colorpars(cor)(#FF0000) }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"seguidor", {0, 0, 0}});
    h.tics.push_back({"alvo", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    FakeTic* seg = h.tic("seguidor");
    EXPECT(seg != nullptr);
    EXPECT(test::nearEqF(seg->cor[0], 1.0f));
    EXPECT(test::nearEqF(seg->cor[1], 0.0f));
    EXPECT(test::nearEqF(seg->cor[2], 0.0f));

    // nome da paleta
    Error err2;
    Script s2 = Script::compile(
        "linker(a)to(b)=RF(p)\n"
        "tyker(s){ find(p) colorpars(cor)(azul) }\n"
        "central main { }\n", err2);
    EXPECT(err2.ok);
    FakeHost h2;
    h2.tics.push_back({"a", {0, 0, 0}});
    h2.tics.push_back({"b", {0, 0, 0}});
    EXPECT(s2.runStart(h2, err2));
    EXPECT(runFrames(s2, h2, 1));
    FakeTic* a2 = h2.tic("a");
    EXPECT(a2 != nullptr);
    EXPECT(test::nearEqF(a2->cor[2], 1.0f));
    EXPECT(test::nearEqF(a2->cor[0], 0.0f));

    // outro parâmetro: guardado, NÃO tinge (o shading() futuro consome)
    Error err3;
    Script s3 = Script::compile(
        "linker(a)to(b)=RF(p)\n"
        "tyker(s){ find(p) colorpars(brilho)(vermelho) }\n"
        "central main { }\n", err3);
    EXPECT(err3.ok);
    FakeHost h3;
    h3.tics.push_back({"a", {0, 0, 0}});
    h3.tics.push_back({"b", {0, 0, 0}});
    EXPECT(s3.runStart(h3, err3));
    EXPECT(runFrames(s3, h3, 1));
    FakeTic* a3 = h3.tic("a");
    EXPECT(a3 != nullptr);
    EXPECT(test::nearEqF(a3->cor[0], 1.0f));   // intocado (branco)
}

TEST(colorpars_cor_desconhecida_erro_legivel) {
    Error err;
    Script s = Script::compile(
        "linker(a)to(b)=RF(p)\n"
        "tyker(s){ find(p) colorpars(cor)(chartreuse) }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"a", {0, 0, 0}});
    h.tics.push_back({"b", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    Error ferr;
    EXPECT(!s.runFrame(h, 1.0 / 60.0, ferr));   // falha legível (fatal)
    EXPECT(ferr.message.find("não existe") != std::string::npos);
}

TEST(play_toca_a_animacao_do_linker) {
    Error err;
    Script s = Script::compile(
        "linker(ator)to(correr)=RF(p)\n"
        "tyker(t){ find(p) play() }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.hostTic = "ator";   // o dono do script
    h.tics.push_back({"ator", {0, 0, 0}});
    h.tics.push_back({"cubo", {0, 0, 0}});
    h.tic("ator")->anims = {"andar", "correr"};
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    EXPECT(h.tic("ator") != nullptr);
    EXPECT(h.tic("ator")->activeAnim == 1);   // "correr" (o lado animação)
}

TEST(play_sem_animacao_erro_legivel) {
    Error err;
    Script s = Script::compile(
        "linker(a)to(b)=RF(p)\n"
        "tyker(t){ find(p) play() }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"a", {0, 0, 0}});
    h.tics.push_back({"b", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    Error ferr;
    EXPECT(!s.runFrame(h, 1.0 / 60.0, ferr));
    EXPECT(ferr.message.find("animação") != std::string::npos);
}

TEST(limit_corta_a_distancia) {
    Error err;
    Script s = Script::compile(
        "linker(seguidor)to(alvo)=RF(p)\n"
        "tyker(s){ find(p) follow() limit(5, 10) }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"seguidor", {100, 0, 0}});
    h.tics.push_back({"alvo", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    FakeTic* seg = h.tic("seguidor");
    EXPECT(seg != nullptr);
    // follow() queria COLAR (dist 0); o limit(5,10) segura a 5 (o mínimo)
    EXPECT(test::nearEqF(seg->pos[0], 5.0f, 1e-3f));
}

TEST(delay_adiia_a_ativacao) {
    Error err;
    Script s = Script::compile(
        "linker(a)to(b)=RF(p)\n"
        "tyker(s){ find(p) delay(0.5) colorpars(cor)(#FF0000) }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"a", {0, 0, 0}});
    h.tics.push_back({"b", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    FakeTic* a = h.tic("a");
    EXPECT(a != nullptr);
    EXPECT(runFrames(s, h, 15));   // 0.25 s: AINDA não ativou
    EXPECT(test::nearEqF(a->cor[0], 1.0f));   // branco intocado
    EXPECT(test::nearEqF(a->cor[1], 1.0f));
    EXPECT(runFrames(s, h, 20));   // +0.33 s: passa 0.5 s → ativa
    EXPECT(test::nearEqF(a->cor[0], 1.0f));
    EXPECT(test::nearEqF(a->cor[1], 0.0f));   // vermelho
}

TEST(args_dos_componentes_podem_ser_variaveis) {
    // decisão 🔶: resolvem 1× na ativação — literais OU variáveis do script
    Error err;
    Script s = Script::compile(
        "v++distancia=2\n"
        "linker(seguidor)to(alvo)=RF(p)\n"
        "tyker(s){ find(p) follow(distancia) }\n"
        "central main { on moment { } allmoments { } }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"seguidor", {10, 0, 0}});
    h.tics.push_back({"alvo", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    FakeTic* seg = h.tic("seguidor");
    EXPECT(seg != nullptr);
    EXPECT(test::nearEqF(seg->pos[0], 2.0f));
}

TEST(varios_tykers_partilham_o_mesmo_rf) {
    Error err;
    Script s = Script::compile(
        "linker(seguidor)to(alvo)=RF(p)\n"
        "tyker(um){ find(p) follow(3) }\n"
        "tyker(dois){ find(p) look() }\n"
        "central main { }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"seguidor", {10, 0, 0}});
    h.tics.push_back({"alvo", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    FakeTic* seg = h.tic("seguidor");
    EXPECT(seg != nullptr);
    EXPECT(test::nearEqF(seg->pos[0], 3.0f));   // follow do tyker "um"
    // look do tyker "dois": DE (3,0,0) para (0,0,0) → yaw -90°
    EXPECT(test::nearEqF(seg->rot[1], -90.0f, 0.5f));
}

TEST(tyker_sem_central_main_comporta_se) {
    // só linkers + tykers: o tick corre na mesma (decisão 🔶 documentada)
    Error err;
    Script s = Script::compile(
        "linker(a)to(b)=RF(p)\n"
        "tyker(s){ find(p) follow() }\n", err);
    EXPECT(err.ok);
    FakeHost h;
    h.tics.push_back({"a", {4, 0, 0}});
    h.tics.push_back({"b", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    FakeTic* a = h.tic("a");
    EXPECT(a != nullptr);
    EXPECT(a->pos[0] == 0.0f);
}

TEST(parser_independente_do_registo) {
    // A PROVA 🔶: componente NOVO instalado no REGISTO (sem tocar na
    // gramática) — o tyker parseia, valida e corre na mesma
    static bool rodou = false;
    rodou = false;
    auto handler = [](tykers::Ctx& c, const tykers::Comp& m,
                      std::string& err) -> bool {
        (void)c;
        (void)err;
        if (m.vals.size() != 1) {
            err = "ecoteste: 1 argumento";
            return false;
        }
        if (m.vals[0].t == Type::Int && m.vals[0].i == 7) {
            rodou = true;
        }
        return true;
    };
    EXPECT(reg::installForTest(
        "ecoteste", handler, "ecoteste(n)", "componente de teste do registo",
        "tyker(t){ find(p) ecoteste(7) }"));
    // a entrada tem Docs (obrigatória) e o find funciona
    const reg::Entry* e = reg::find("ecoteste");
    EXPECT(e != nullptr && e->kind == reg::Kind::Componente);
    EXPECT(e->syntax && e->desc && e->example);
    EXPECT(reg::findComponent("ecoteste") == e);
    EXPECT(reg::prefixMatch("ecot") == e);
    EXPECT(reg::handlerFor("ecoteste") != nullptr);

    Error err;
    Script s = Script::compile(
        "linker(a)to(b)=RF(p)\n"
        "tyker(t){ find(p) ecoteste(7) }\n"
        "central main { }\n", err);
    EXPECT(err.ok);   // validou contra o REGISTO — a gramática nem sabe
    FakeHost h;
    h.tics.push_back({"a", {0, 0, 0}});
    h.tics.push_back({"b", {0, 0, 0}});
    EXPECT(s.runStart(h, err));
    EXPECT(runFrames(s, h, 1));
    EXPECT(rodou);   // o handler correu pelo dispatch do registo

    // argc contra o registo: ecoteste aceita 0..9 → 10 args é demais? 9 é o
    // teto do installForTest — 1 arg válido (testado acima); limpeza:
    reg::resetForTest();
    EXPECT(reg::find("ecoteste") == nullptr);
    EXPECT(reg::handlerFor("ecoteste") == nullptr);
}

TEST(registo_docs_obrigatorias_por_entrada) {
    // "Docs obrigatória por entrada": TODA a entrada do registo tem
    // sintaxe + descrição + exemplo preenchidos
    for (const reg::Entry& e : reg::all()) {
        EXPECT(e.name && *e.name);
        EXPECT(e.syntax && *e.syntax);
        EXPECT(e.desc && *e.desc);
        EXPECT(e.example && *e.example);
    }
    // BIJEÇÃO componente↔handler: toda a entrada Componente tem handler;
    // (o contrário — handler sem entrada — não pode acontecer porque a
    // tabela vive no próprio registo; o findComponent falharia no compile)
    for (const reg::Entry& e : reg::all()) {
        if (e.kind == reg::Kind::Componente) {
            EXPECT(reg::handlerFor(e.name) != nullptr);
        }
    }
    // os 13 componentes da spec fechada, TODOS no registo
    const char* kSpec[] = {"follow", "look", "orbit", "copy", "map",
                           "Change", "point", "colorpars", "play",
                           "limit", "delay", "shading"};
    for (const char* n : kSpec) {
        const reg::Entry* e = reg::findComponent(n);
        EXPECT(e != nullptr);
    }
    EXPECT(reg::findComponent("find") == nullptr);   // find é forma do tyker
}

TEST(docs_incluem_o_registo_linker_tyker_componente) {
    auto& all = voni::docs::all();
    bool hasLinker = false, hasTyker = false, hasComp = false;
    for (const auto& e : all) {
        hasLinker = hasLinker || e.cat == voni::docs::Cat::Linker;
        hasTyker = hasTyker || e.cat == voni::docs::Cat::Tyker;
        hasComp = hasComp || e.cat == voni::docs::Cat::Componente;
    }
    EXPECT(hasLinker && hasTyker && hasComp);
    // a pesquisa apanha os novos (a lupa do editor)
    EXPECT(!voni::docs::search("follow").empty());
    EXPECT(!voni::docs::search("linker").empty());
    EXPECT(!voni::docs::search("tyker").empty());
}

TEST(exemplo_8_5_no_c33) {
    // O TESTE da spec: "follow/Change/point+colorpars/RF em falta/ciclo"
    // (versão scriptada; a humana corre no device — checklist do relatório)

    // (1) follow
    {
        Error err;
        Script s = Script::compile(
            "linker(heroi)to(bau)=RF(p)\n"
            "tyker(seguelo){ find(p) follow(2) }\n"
            "central main { }\n", err);
        EXPECT(err.ok);
        FakeHost h;
        h.tics.push_back({"heroi", {9, 0, 0}});
        h.tics.push_back({"bau", {0, 0, 0}});
        EXPECT(s.runStart(h, err));
        EXPECT(runFrames(s, h, 1));
        FakeTic* heroi = h.tic("heroi");
        EXPECT(heroi != nullptr);
        EXPECT(test::nearEqF(heroi->pos[0], 2.0f));
    }
    // (2) Change + point + colorpars num tyker só
    {
        Error err;
        Script s = Script::compile(
            "linker(heroi)to(bau1)=RF(p)\n"
            "tyker(t){ find(p) Change(destino)to(bau2) point(0, 1, 0) "
            "colorpars(cor)(#00FF00) follow(1) }\n"
            "central main { }\n", err);
        EXPECT(err.ok);
        FakeHost h;
        h.tics.push_back({"heroi", {9, 9, 9}});
        h.tics.push_back({"bau1", {5, 5, 5}});
        h.tics.push_back({"bau2", {7, 7, 7}});
        EXPECT(s.runStart(h, err));
        EXPECT(runFrames(s, h, 1));
        FakeTic* heroi = h.tic("heroi");
        EXPECT(heroi != nullptr);
        // Change→bau2 foi anulado pelo point(0,1,0): segue o PONTO a 1 de
        // distância (o follow mantém a distância na direção de onde vem)
        EXPECT(heroi->pos[0] != 9.0f);   // mexeu
        EXPECT(test::nearEqF(heroi->cor[1], 1.0f));   // verde #00FF00
    }
    // (3) RF em falta
    {
        Error err;
        Script s = Script::compile(
            "linker(a)to(b)=RF(p)\n"
            "tyker(t){ find(naoexiste) follow() }\n"
            "central main { allmoments { View P \"ok\" } }\n", err);
        EXPECT(err.ok);
        FakeHost h;
        h.tics.push_back({"a", {1, 0, 0}});
        h.tics.push_back({"b", {0, 0, 0}});
        EXPECT(s.runStart(h, err));
        EXPECT(hasLog(h, "RF 'naoexiste' não encontrada"));
        EXPECT(runFrames(s, h, 1));
        EXPECT(hasLog(h, "ok"));   // o script segue
        FakeTic* a = h.tic("a");
        EXPECT(a != nullptr && a->pos[0] == 1.0f);   // o tyker NÃO correu
    }
    // (4) ciclo
    {
        Error err;
        Script s = Script::compile(
            "linker(a)to(b)=RF(p)\n"
            "linker(b)to(a)=RF(p)\n"
            "central main { }\n", err);
        EXPECT(err.ok);
        FakeHost h;
        EXPECT(!s.runStart(h, err));
        EXPECT(err.message.find("ciclo") != std::string::npos);
    }
}

// ===========================================================================
// 0.12-U (3.9a) — View P UNIVERSAL + MATEMÁTICA INLINE + DISTANCE
// O registo é a fonte única (as 14 entradas Kind::Funcao); a fn do
// utilizador vence; sin/cos em GRAUS; erros com linha que ensinam.
// ===========================================================================

TEST(view_p_012u_num_bool_txt) {
    RunResult r = runScript(R"VONI(
central main {
  on moment {
    View P 42
    View P true
    View P 2.5
    View P "texto de sempre"
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    if (!r.ok || r.host.logs.size() < 4) return;   // guarda: a mutação falha antes
    EXPECT(r.host.logs.size() == 4);
    EXPECT(r.host.logs[0] == "voni: 42");
    EXPECT(r.host.logs[1] == "voni: true");
    EXPECT(r.host.logs[2] == "voni: 2.5");
    EXPECT(r.host.logs[3] == "voni: texto de sempre");   // compat byte a byte
}

TEST(view_p_012u_vec_tic_e_componentes) {
    FakeHost h;
    h.tics.push_back({"jogador", {1, 2, 3}});
    Error err;
    Script s = Script::compile(
        "central main { on moment { View P jogador View P jogador.pos "
        "View P jogador.pos.x } allmoments { } }", err);
    EXPECT(err.ok);
    const bool st = s.runStart(h, err);
    EXPECT(st);
    if (!st || h.logs.size() < 3) return;   // guarda: a mutação falha antes
    EXPECT(h.logs.size() == 3);
    EXPECT(h.logs[0] == "voni: jogador");          // TIC → o nome
    EXPECT(h.logs[1] == "voni: (1, 2, 3)");        // Vec3 → formato do valor
    EXPECT(h.logs[2] == "voni: 1");                // componente .x → Num
}

TEST(view_p_012u_expressao_e_concat) {
    RunResult r = runScript(R"VONI(
central main {
  on moment {
    View P ("a" + "b")
    View P (2 * 3)
    View P abs(-7)
  }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    if (!r.ok || r.host.logs.size() < 3) return;   // guarda: a mutação falha antes
    EXPECT(r.host.logs.size() == 3);
    EXPECT(r.host.logs[0] == "voni: ab");
    EXPECT(r.host.logs[1] == "voni: 6");
    EXPECT(r.host.logs[2] == "voni: 7");
}

TEST(mat_abs_preserva_o_tipo) {
    RunResult r = runScript(R"VONI(
v#@+i:Int=abs(-5)
v#@+n:Num=abs(-2.5)
v#@+z:Int=abs(3)
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex.size() == 3);
    EXPECT(ex[0].value.t == Type::Int && ex[0].value.i == 5);
    EXPECT(ex[1].value.t == Type::Num);
    EXPECT(std::fabs(ex[1].value.n - 2.5) < 1e-12);
    EXPECT(ex[2].value.t == Type::Int && ex[2].value.i == 3);
}

TEST(mat_min_max_com_promocao) {
    RunResult r = runScript(R"VONI(
v#@+a:Int=min(3, 7)
v#@+b:Num=max(2, 5.5)
v#@+c:Int=min(-1, -7)
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex[0].value.t == Type::Int && ex[0].value.i == 3);
    EXPECT(ex[1].value.t == Type::Num);   // Num misturado → Num
    EXPECT(std::fabs(ex[1].value.n - 5.5) < 1e-12);
    EXPECT(ex[2].value.t == Type::Int && ex[2].value.i == -7);
}

TEST(mat_clamp_limita_e_erro_ordem) {
    RunResult r = runScript(R"VONI(
v#@+a:Int=clamp(15, 0, 10)
v#@+b:Int=clamp(-3, 0, 10)
v#@+c:Int=clamp(5, 0, 10)
v#@+d:Num=clamp(2.5, 0, 10)
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex[0].value.i == 10);   // acima → hi
    EXPECT(ex[1].value.i == 0);    // abaixo → lo
    EXPECT(ex[2].value.i == 5);    // dentro → v
    EXPECT(ex[3].value.t == Type::Num);   // Num misturado → Num
    // o mínimo maior que o máximo é erro legível (com linha)
    RunResult e = runScript(
        "central main { on moment { v++x=clamp(1, 5, 2) } allmoments { } }");
    EXPECT(!e.ok);
    EXPECT(e.err.message.find("mínimo") != std::string::npos);
    EXPECT(e.err.line == 1);
}

TEST(mat_lerp_interpolacao_linear) {
    RunResult r = runScript(R"VONI(
v#@+a:Num=lerp(0, 10, 0.25)
v#@+b:Num=lerp(10, 20, 0)
v#@+c:Num=lerp(10, 20, 1)
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(std::fabs(ex[0].value.n - 2.5) < 1e-12);
    EXPECT(std::fabs(ex[1].value.n - 10.0) < 1e-12);
    EXPECT(std::fabs(ex[2].value.n - 20.0) < 1e-12);
}

TEST(mat_floor_ceil_round_exatos) {
    RunResult r = runScript(R"VONI(
v#@+a:Int=floor(2.7)
v#@+b:Int=floor(-2.1)
v#@+c:Int=ceil(2.1)
v#@+d:Int=ceil(-2.7)
v#@+e:Int=round(2.5)
v#@+f:Int=round(-2.5)
v#@+g:Int=round(2.4)
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex[0].value.i == 2);
    EXPECT(ex[1].value.i == -3);   // floor para baixo
    EXPECT(ex[2].value.i == 3);
    EXPECT(ex[3].value.i == -2);   // ceil para cima
    EXPECT(ex[4].value.i == 3);    // meio afasta do zero
    EXPECT(ex[5].value.i == -3);
    EXPECT(ex[6].value.i == 2);
}

TEST(mat_sqrt_e_negativo_erro_com_linha) {
    RunResult r = runScript(R"VONI(
v#@+a:Num=sqrt(9)
v#@+b:Num=sqrt(2)
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex[0].value.t == Type::Num && std::fabs(ex[0].value.n - 3.0) < 1e-12);
    EXPECT(std::fabs(ex[1].value.n - 1.4142135623730951) < 1e-9);
    RunResult e = runScript(
        "central main { on moment { v++x=sqrt(-1) } allmoments { } }");
    EXPECT(!e.ok);
    EXPECT(e.err.message.find("negativo") != std::string::npos);
    EXPECT(e.err.line == 1);
}

TEST(mat_sin_cos_em_graus) {
    RunResult r = runScript(R"VONI(
v#@+a:Num=sin(0)
v#@+b:Num=sin(90)
v#@+c:Num=cos(0)
v#@+d:Num=cos(180)
v#@+e:Num=sin(30)
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(std::fabs(ex[0].value.n) < 1e-12);
    EXPECT(std::fabs(ex[1].value.n - 1.0) < 1e-9);     // 90 GRAUS = 1
    EXPECT(std::fabs(ex[2].value.n - 1.0) < 1e-12);
    EXPECT(std::fabs(ex[3].value.n + 1.0) < 1e-9);     // 180 GRAUS = -1
    EXPECT(std::fabs(ex[4].value.n - 0.5) < 1e-9);     // 30 GRAUS = 0.5
}

TEST(mat_random_na_faixa_0_1) {
    RunResult r = runScript(R"VONI(
v#@+a:Num=random()
v#@+b:Num=random()
v#@+c:Num=random()
v#@+d:Num=random()
v#@+e:Num=random()
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    int distintos = 0;
    for (size_t i = 0; i < ex.size(); ++i) {
        EXPECT(ex[i].value.t == Type::Num);
        EXPECT(ex[i].value.n >= 0.0 && ex[i].value.n < 1.0);
        if (ex[i].value.n != ex[0].value.n) {
            ++distintos;
        }
    }
    EXPECT(distintos >= 1);   // novo a cada chamada (5 amostras)
}

TEST(mat_random_int_no_intervalo_fechado) {
    RunResult r = runScript(R"VONI(
v#@+a:Int=random_int(1, 6)
v#@+b:Int=random_int(1, 6)
v#@+c:Int=random_int(1, 6)
v#@+d:Int=random_int(-2, 2)
v#@+e:Int=random_int(7, 7)
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    for (int i = 0; i < 3; ++i) {
        EXPECT(ex[(size_t)i].value.t == Type::Int);
        EXPECT(ex[(size_t)i].value.i >= 1 && ex[(size_t)i].value.i <= 6);
    }
    EXPECT(ex[3].value.i >= -2 && ex[3].value.i <= 2);
    EXPECT(ex[4].value.i == 7);   // intervalo unitário
    // min > max = erro legível
    RunResult e = runScript(
        "central main { on moment { v++x=random_int(5, 3) } "
        "allmoments { } }");
    EXPECT(!e.ok);
    EXPECT(e.err.message.find("maior que o máximo") != std::string::npos);
    // limite não inteiro = erro legível
    RunResult e2 = runScript(
        "central main { on moment { v++x=random_int(1.5, 6) } "
        "allmoments { } }");
    EXPECT(!e2.ok);
    EXPECT(e2.err.message.find("inteiros") != std::string::npos);
}

TEST(mat_arity_e_desconhecida_ensinam) {
    // o argc vem da ENTRADA do registo (a fonte única) e o erro ENSINA
    RunResult a = runScript(
        "central main { on moment { v++x=abs() } allmoments { } }");
    EXPECT(!a.ok);
    EXPECT(a.err.message.find("abs(x)") != std::string::npos);
    EXPECT(a.err.line == 1);
    RunResult b = runScript(
        "central main { on moment { v++x=min(1) } allmoments { } }");
    EXPECT(!b.ok);
    EXPECT(b.err.message.find("min(a, b)") != std::string::npos);
    // desconhecida continua com o erro de sempre
    RunResult c = runScript(
        "central main { on moment { v++x=gato(1) } allmoments { } }");
    EXPECT(!c.ok);
    EXPECT(c.err.message.find("'gato' não é uma função") != std::string::npos);
}

TEST(distance_tic_e_vec3) {
    FakeHost h;
    h.tics.push_back({"a", {0, 0, 0}});
    h.tics.push_back({"b", {3, 4, 0}});
    h.tics.push_back({"c", {0, 0, 2}});
    Error err;
    // TIC × TIC, Vec3 × Vec3 (propriedades), TIC × Vec3 (variável) e o
    // EIXO Z (o 3-4-5 tem z=0 — o c do eixo z puro fecha a vaca)
    Script s = Script::compile(R"VONI(
v#p:Vec3=Search.b.pos
v#@+d1:Num=distance(a, b)
v#@+d2:Num=distance(a.pos, b.pos)
v#@+d3:Num=distance(a, p)
v#@+d4:Num=distance(a, c)
central main { on moment { } allmoments { } }
)VONI", err);
    EXPECT(err.ok);
    EXPECT(s.runStart(h, err));
    auto ex = s.exported();
    EXPECT(ex.size() == 4);
    if (ex.size() < 4) return;
    EXPECT(std::fabs(ex[0].value.n - 5.0) < 1e-9);    // o 3-4-5
    EXPECT(std::fabs(ex[1].value.n - 5.0) < 1e-9);
    EXPECT(std::fabs(ex[2].value.n - 5.0) < 1e-9);
    EXPECT(std::fabs(ex[3].value.n - 2.0) < 1e-9);    // o eixo z conta
    // argumento que não é TIC nem Vec3 = erro que ensina
    RunResult e = runScript(
        "central main { on moment { v++x=distance(\"x\", 1) } "
        "allmoments { } }");
    EXPECT(!e.ok);
    EXPECT(e.err.message.find("TIC ou um Vec3") != std::string::npos);
    // TIC inexistente = erro de resolução com linha
    RunResult e2 = runScript(
        "central main { on moment { v++x=distance(fantasma, a) } "
        "allmoments { } }");
    EXPECT(!e2.ok);
    EXPECT(e2.err.message.find("fantasma") != std::string::npos);
    EXPECT(e2.err.line == 1);
}

TEST(mat_fn_do_utilizador_vence_a_funcao_do_registo) {
    // compat: quem já tinha fn min() continua a correr a DELE (o shadow é
    // a regra da casa — params/globais fazem shadow, fns também)
    RunResult r = runScript(R"VONI(
fn min(a:Num, b:Num):Num { return a + b }
v#@+x:Num=min(3, 7)
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex[0].value.t == Type::Num);
    EXPECT(std::fabs(ex[0].value.n - 10.0) < 1e-12);   // a DELE soma
}

// ===========================================================================
// 0.12-U (3.9b) — spawn(tipo, x, y, z) → TIC
// O tipo é da lista do HOST (a engine: esfera/box); o TIC nasce
// active/visible/nome único; o devolvido é USÁVEL (pos, distance).
// ===========================================================================

TEST(spawn_cria_tic_e_devolve) {
    RunResult r = runScript(R"VONI(
v#@+novo:TIC=spawn("esfera", 1, 2, 3)
central main {
  on moment { View P novo View P novo.pos }
  allmoments { }
}
)VONI");
    EXPECT(r.ok);
    if (!r.ok || r.host.logs.size() < 2) return;
    EXPECT(r.host.logs[0] == "voni: esfera");       // o TIC novo pelo NOME
    EXPECT(r.host.logs[1] == "voni: (1, 2, 3)");    // nasceu NA POSIÇÃO
    auto ex = r.script.exported();
    EXPECT(ex.size() == 1 && ex[0].value.t == Type::Tic);
    EXPECT(ex[0].value.tic == "esfera");
}

TEST(spawn_tipos_validos_e_registo) {
    RunResult r = runScript(R"VONI(
v#@+a:TIC=spawn("box", 0, 0, 0)
v#@+b:TIC=spawn(esfera, 1, 1, 1)
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex.size() == 2);
    EXPECT(ex[0].value.tic == "box");
    EXPECT(ex[1].value.tic == "esfera");   // o nome CRU também é tipo
}

TEST(spawn_tipo_desconhecido_erro_com_linha) {
    // o tipo é da lista do HOST — o "cilindro" REMOVIDO (0.8.10) também
    // é erro hoje (o primFromName de load é defesa, não porta)
    RunResult e = runScript(
        "central main { on moment { v#x:TIC=spawn(\"dragon\", 0, 0, 0) } "
        "allmoments { } }");
    EXPECT(!e.ok);
    EXPECT(e.err.message.find("'dragon' não existe") != std::string::npos);
    EXPECT(e.err.message.find("esfera box") != std::string::npos);
    EXPECT(e.err.line == 1);
    RunResult e2 = runScript(
        "central main { on moment { v#x:TIC=spawn(\"cilindro\", 0, 0, 0) } "
        "allmoments { } }");
    EXPECT(!e2.ok);
    EXPECT(e2.err.message.find("cilindro") != std::string::npos);
    EXPECT(e2.err.line == 1);
}

TEST(spawn_forma_errada_ensina_a_sintaxe) {
    RunResult a = runScript(
        "central main { on moment { v#x:TIC=spawn(\"esfera\", 1, 2) } "
        "allmoments { } }");
    EXPECT(!a.ok);
    EXPECT(a.err.message.find("spawn(tipo, x, y, z)") != std::string::npos);
    RunResult b = runScript(
        "central main { on moment { v#x:TIC=spawn(\"esfera\", \"a\", 2, 3) } "
        "allmoments { } }");
    EXPECT(!b.ok);
    EXPECT(b.err.message.find("têm de ser números") != std::string::npos);
    EXPECT(b.err.line == 1);
    RunResult c = runScript(
        "central main { on moment { v#x:TIC=spawn(1, 2, 3, 4) } "
        "allmoments { } }");
    EXPECT(!c.ok);
    EXPECT(c.err.message.find("entre aspas") != std::string::npos);
}

TEST(spawn_nome_unico_na_serie) {
    RunResult r = runScript(R"VONI(
v#@+a:TIC=spawn("esfera", 0, 0, 0)
v#@+b:TIC=spawn("esfera", 1, 0, 0)
v#@+c:TIC=spawn("esfera", 2, 0, 0)
v#@+d:TIC=spawn("box", 0, 0, 0)
central main { on moment { } allmoments { } }
)VONI");
    EXPECT(r.ok);
    auto ex = r.script.exported();
    EXPECT(ex.size() == 4);
    EXPECT(ex[0].value.tic == "esfera");        // a base
    EXPECT(ex[1].value.tic == "esfera.001");    // Godot-style
    EXPECT(ex[2].value.tic == "esfera.002");
    EXPECT(ex[3].value.tic == "box");           // a base do OUTRO tipo
    EXPECT(r.host.spawned.size() == 4);
}

TEST(spawn_tic_usavel_pos_distance) {
    // o TIC devolvido é USÁVEL: escreve pos, lê, e entra na distance
    FakeHost h;
    h.tics.push_back({"jogador", {0, 0, 0}});
    Error err;
    Script s = Script::compile(R"VONI(
v#@+novo:TIC=spawn("box", 0, 5, 0)
central main {
  on moment {
    novo.pos.y=9
    View P novo.pos.y
    View P distance(novo, jogador)
  }
  allmoments { }
}
)VONI", err);
    EXPECT(err.ok);
    const bool st = s.runStart(h, err);
    EXPECT(st);
    if (!st || h.logs.size() < 2) return;
    EXPECT(h.logs[0] == "voni: 9");    // a escrita no TIC novo chegou
    EXPECT(h.logs[1] == "voni: 9");    // distance (0,9,0)→(0,0,0) = 9
    FakeTic* n = h.tic("box");
    EXPECT(n != nullptr && n->pos[1] == 9.0f);
}

TEST(spawn_host_sem_cena_erro_honesto) {
    // o default do Host (sem cena) responde "indisponível" — o script não
    // morre de forma seca; o erro sobe com a linha
    struct HostSemSpawn : Host {
        std::vector<std::string> logs;
        void log(const char* l) override { logs.push_back(l); }
        bool getProp(const std::string&, const std::vector<std::string>&,
                     Value&, std::string&) override { return false; }
        bool setProp(const std::string&, const std::vector<std::string>&,
                     const Value&, std::string&) override { return false; }
        bool ticExists(const std::string&) override { return false; }
        void moveTic(f32, f32, f32) override {}
        void explodeTic(bool) override {}
        bool importAnim(const std::string&, std::string&) override {
            return false;
        }
        bool transitionTo(const std::string&, const std::string&,
                          std::string&) override { return false; }
        std::string currentSceneName() override { return "c"; }
        bool search(const std::string&, const std::vector<std::string>&,
                    Value&, std::string&) override { return false; }
        f64 frameDt() override { return 1.0 / 60.0; }
    } h;
    Error err;
    Script s = Script::compile(
        "central main { on moment { v#x:TIC=spawn(\"esfera\", 0, 0, 0) } "
        "allmoments { } }", err);
    EXPECT(err.ok);
    EXPECT(!s.runStart(h, err));
    EXPECT(err.message.find("indisponível") != std::string::npos);
    EXPECT(err.line == 1);
}

// ===========================================================================
// 0.12-U (3.9c) — every(s) / after(s) / when(cond)
// O ESTADO vive POR NÓ AST (no Impl — morre no stop()); every carrega o
// resto (sem drift, 1× por frame); after/when disparam 1× e morrem.
// ===========================================================================

TEST(every_cada_meio_segundo) {
    Error err;
    Script s = Script::compile(
        "central main { on moment { } allmoments { every(0.5) { View P "
        "\"pulso\" } } }", err);
    EXPECT(err.ok);
    FakeHost h;
    EXPECT(s.runStart(h, err));
    for (int i = 0; i < 10; ++i) {   // 10 × dt 0.1 = 1.0 s
        EXPECT(s.runFrame(h, 0.1, err));
    }
    // acc: .1 .2 .3 .4 [.5 dispara, carrega 0] .1 .2 .3 .4 [.5 dispara]
    EXPECT(h.logs.size() == 2);
    EXPECT(h.logs[0] == "voni: pulso");
    EXPECT(h.logs[1] == "voni: pulso");
}

TEST(every_o_resto_carrega_sem_drift) {
    Error err;
    Script s = Script::compile(
        "central main { on moment { } allmoments { every(0.5) { View P "
        "\"t\" } } }", err);
    EXPECT(err.ok);
    FakeHost h;
    EXPECT(s.runStart(h, err));
    for (int i = 0; i < 10; ++i) {   // 10 × dt 0.2 = 2.0 s
        EXPECT(s.runFrame(h, 0.2, err));
    }
    // com o resto a CARREGAR: .2 .4 [.6 dispara → .1] .3 [.5 dispara → 0]
    // .2 .4 [.6 dispara → .1] .3 [.5 dispara → 0] = 4 disparos
    EXPECT(h.logs.size() == 4);
}

TEST(every_intervalo_invalido_erro_com_linha) {
    RunResult z = runScript(
        "central main { on moment { every(0) { View P \"x\" } } "
        "allmoments { } }");
    EXPECT(!z.ok);
    EXPECT(z.err.message.find("> 0") != std::string::npos);
    EXPECT(z.err.line == 1);
    RunResult t = runScript(
        "central main { on moment { every(\"a\") { } } allmoments { } }");
    EXPECT(!t.ok);
    EXPECT(t.err.message.find("número") != std::string::npos);
}

TEST(after_dispara_uma_vez_e_morre) {
    Error err;
    Script s = Script::compile(
        "central main { on moment { } allmoments { after(1) { View P "
        "\"porta\" } } }", err);
    EXPECT(err.ok);
    FakeHost h;
    EXPECT(s.runStart(h, err));
    for (int i = 0; i < 20; ++i) {   // 2.0 s — o after(1) já passou há muito
        EXPECT(s.runFrame(h, 0.1, err));
    }
    EXPECT(h.logs.size() == 1);      // 1× e morre (não repete)
    EXPECT(h.logs[0] == "voni: porta");
}

TEST(after_dois_nos_independentes) {
    Error err;
    Script s = Script::compile(
        "central main { on moment { } allmoments { after(0.5) { View P "
        "\"a\" } after(1.0) { View P \"b\" } } }", err);
    EXPECT(err.ok);
    FakeHost h;
    EXPECT(s.runStart(h, err));
    for (int i = 0; i < 15; ++i) {
        EXPECT(s.runFrame(h, 0.1, err));
    }
    // o a dispara no frame 5, o b no frame 10 — o estado de cada um é
    // PRÓPRIO (por nó AST, não um relógio global do script)
    EXPECT(h.logs.size() == 2);
    if (h.logs.size() == 2) {
        EXPECT(h.logs[0] == "voni: a");
        EXPECT(h.logs[1] == "voni: b");
    }
}

TEST(after_tempo_invalido_erro_com_linha) {
    RunResult z = runScript(
        "central main { on moment { after(-1) { } } allmoments { } }");
    EXPECT(!z.ok);
    EXPECT(z.err.message.find("> 0") != std::string::npos);
    EXPECT(z.err.line == 1);
}

TEST(when_dispara_na_subida_uma_vez) {
    Error err;
    Script s = Script::compile(
        "v#vida:Int=5\n"
        "central main { on moment { } allmoments { vida=vida-1 "
        "when(vida < 3) { View P \"caiu\" } } }", err);
    EXPECT(err.ok);
    if (!err.ok) return;
    FakeHost h;
    const bool st = s.runStart(h, err);
    EXPECT(st);
    if (!st) return;
    for (int i = 0; i < 10; ++i) {   // vida: 4 3 2 1 0 -1 … (o nível dura 8)
        EXPECT(s.runFrame(h, 0.016, err));
    }
    // a subida acontece UMA vez (o frame em que vida fica < 3 e lá fica)
    if (h.logs.size() < 1) return;   // guarda: o vermelho limpo
    EXPECT(h.logs.size() == 1);
    EXPECT(h.logs[0] == "voni: caiu");
}

TEST(when_nasce_verdadeira_dispara_no_primeiro_frame) {
    Error err;
    Script s = Script::compile(
        "central main { on moment { } allmoments { when(true) { View P "
        "\"logo\" } } }", err);
    EXPECT(err.ok);
    FakeHost h;
    EXPECT(s.runStart(h, err));
    for (int i = 0; i < 5; ++i) {
        EXPECT(s.runFrame(h, 0.016, err));
    }
    EXPECT(h.logs.size() == 1);      // no 1.º frame; nunca mais
}

TEST(when_uma_vez_no_total_mesmo_com_duas_subidas) {
    // o when é 1× TOTAL (o portão), não 1× por subida: a condição sobe
    // DUAS vezes (t 3-4 e t 7-8) e o bloco corre UMA só vez
    Error err;
    Script s = Script::compile(
        "v#t:Int=0\n"
        "central main { on moment { } allmoments { t=t+1 "
        "when((t > 2 and t < 5) or (t > 6 and t < 9)) { View P \"subiu\" } } }",
        err);
    EXPECT(err.ok);
    if (!err.ok) return;
    FakeHost h;
    const bool st = s.runStart(h, err);
    EXPECT(st);
    if (!st) return;
    for (int i = 0; i < 12; ++i) {   // t = 1..12
        EXPECT(s.runFrame(h, 0.016, err));
    }
    if (h.logs.empty()) return;      // guarda: o vermelho limpo
    EXPECT(h.logs.size() == 1);      // a 1.ª subida; a 2.ª (t=7) é morta
    EXPECT(h.logs[0] == "voni: subiu");
}

TEST(when_tipo_errado_erro_com_linha) {
    RunResult r = runScript(
        "central main { on moment { when(5) { } } allmoments { } }");
    EXPECT(!r.ok);
    EXPECT(r.err.message.find("true/false") != std::string::npos);
    EXPECT(r.err.line == 1);
}

TEST(timers_o_estado_morre_no_stop) {
    Error err;
    Script s = Script::compile(
        "central main { on moment { } allmoments { after(1) { View P "
        "\"viva\" } } }", err);
    EXPECT(err.ok);
    FakeHost h;
    // 1.ª vida: dispara 1×
    EXPECT(s.runStart(h, err));
    for (int i = 0; i < 15; ++i) {
        EXPECT(s.runFrame(h, 0.1, err));
    }
    EXPECT(h.logs.size() == 1);
    // stop() limpa o ESTADO POR NÓ AST (o Impl.reset)…
    s.stop();
    // …e a 2.ª vida recomeça do zero: o after volta a disparar
    EXPECT(s.runStart(h, err));
    for (int i = 0; i < 15; ++i) {
        EXPECT(s.runFrame(h, 0.1, err));
    }
    EXPECT(h.logs.size() == 2);      // a 2.ª vida tem o seu próprio 1×
}

TEST(timers_no_corpo_do_tyker_sao_rejeitados_que_ensinam) {
    // o corpo do tyker só aceita COMPONENTES — o every é código de script
    // (a mesma lição do View P/repeat lá dentro; o erro DIZ o que entrou)
    Error err;
    Script s = Script::compile(
        "linker(jogador)to(cubo)=RF(p)\n"
        "tyker(t){ find(p) every(0.5) { } }", err);
    EXPECT(!err.ok);
    EXPECT(err.message.find("só aceita componentes") != std::string::npos);
    EXPECT(err.message.find("every") != std::string::npos);
    EXPECT(err.line >= 2);
}

// ===========================================================================
// 0.12-U · FECHO — O CRITÉRIO DO PLATFORMER (o JUIZ FINAL do Bloco A)
// UM ÚNICO script usa AS SETE FEATURES em cadeia — spawn (a plataforma, a
// moeda, a porta), a matemática (min/clamp/lerp/abs/random_int), a
// distance (a chegada), o when (a queda repõe + a moeda apanhada), o
// after (a porta abre) e o every (a moeda pulsa) — contra o FakeHost com
// dt simulado, frame a frame, do arranque ao objetivo. O Bloco A só
// fecha com ESTE pin verde (o CI é a testemunha).
// ===========================================================================
TEST(platformer_bloco_a_juiz_final) {
    static const char* kFase = R"VONI(
// o platformer do dono — 7 features, zero relógios à mão
v#@+dado:Int=random_int(1, 6)
v#jogador:TIC=spawn("box", 0, 1, 0)
v#plataforma:TIC=spawn("box", 0, 0, -4)
v#moeda:TIC=spawn("esfera", 0, 1, -4)
v#porta:TIC=spawn("box", 0, 0, -8)
v#vy:Num=0
v#vel:Num=0
central main {
  on moment {
    View P "fase a correr"
    View P abs(-42)
  }
  allmoments {
    vel = min(vel + 0.1, 0.5)
    jogador.pos.z = jogador.pos.z - vel
    vy = clamp(vy - 0.5, -2, 0)
    jogador.pos.y = jogador.pos.y + vy
    moeda.pos.y = lerp(moeda.pos.y, 1 + vel, 0.5)
    every(0.5) { View P "moeda pulsa" }
    after(2) { View P "porta aberta" }
    exist(jogador.pos.y < -1) {
      jogador.pos.y = 1
      vy = 0
      View P "reposicao"
    }
    when(distance(jogador, moeda) < 1.5) { View P "moeda apanhada" }
  }
}
)VONI";
    Error err;
    Script s = Script::compile(kFase, err);
    EXPECT(err.ok);
    if (!err.ok) {
        std::printf("  (o juiz nem compilou: linha %u: %s)\n", err.line,
                    err.message.c_str());
        return;
    }
    FakeHost h;
    const bool st = s.runStart(h, err);
    EXPECT(st);
    if (!st) return;

    // 25 frames × dt 0.1 = 2.5 s de jogo
    for (int i = 0; i < 25; ++i) {
        EXPECT(s.runFrame(h, 0.1, err));
    }

    // ---- os contadores do juiz --------------------------------------------
    int repos = 0, pulsos = 0, portas = 0, moedas = 0;
    for (const std::string& l : h.logs) {
        if (l == "voni: reposicao") ++repos;
        if (l == "voni: moeda pulsa") ++pulsos;
        if (l == "voni: porta aberta") ++portas;
        if (l == "voni: moeda apanhada") ++moedas;
    }
    EXPECT(h.logs.size() >= 2);
    if (h.logs.size() >= 2) {
        EXPECT(h.logs[0] == "voni: fase a correr");   // o arranque
        EXPECT(h.logs[1] == "voni: 42");              // o abs no debug
    }
    // a FÍSICA repôs 8 vezes (o ciclo de queda é 3 frames em 25 — a
    // REPOLÍTICA é exist, que corre sempre; o when é EVENTO 1×)
    EXPECT_MSG(repos == 8, "reposicao ×%d (o contrato é 8 — o exist da "
                           "queda deixou de apanhar a subida?)", repos);
    // o EVERY pulsa 5× (0.5s × 5 em 2.5s — o resto carrega)
    EXPECT_MSG(pulsos == 5, "moeda pulsa ×%d (o contrato é 5)", pulsos);
    // o AFTER abriu a porta 1× (aos 2.0s)
    EXPECT_MSG(portas == 1, "porta aberta ×%d (o contrato é 1)", portas);
    // o WHEN da chegada (a distance) apanhou a moeda 1× (o 1×-total)
    EXPECT_MSG(moedas == 1, "moeda apanhada ×%d (o contrato é 1)", moedas);

    // ---- o MUNDO mudou de verdade (o Host tem os estados) ------------------
    // os spawns com nome único: box, box.001, esfera, box.002
    EXPECT(h.tic("box") != nullptr);
    EXPECT(h.tic("box.001") != nullptr);
    EXPECT(h.tic("esfera") != nullptr);
    EXPECT(h.tic("box.002") != nullptr);
    // o jogador andou para a frente (a vel capped pelo min)
    FakeTic* j = h.tic("box");
    EXPECT(j != nullptr && j->pos[2] < -10.0f);
    // a moeda subiu (o lerp a puxar para 1+vel)
    FakeTic* m = h.tic("esfera");
    EXPECT(m != nullptr && m->pos[1] > 1.0f);

    // ---- os exports do Inspector -------------------------------------------
    auto ex = s.exported();
    EXPECT(ex.size() == 1);
    if (ex.size() == 1) {
        EXPECT(ex[0].name == "dado");
        EXPECT(ex[0].value.t == Type::Int);
        EXPECT(ex[0].value.i >= 1 && ex[0].value.i <= 6);   // o dado honesto
    }
}
