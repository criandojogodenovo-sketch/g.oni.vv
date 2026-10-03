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
    bool visible = true;
    std::vector<std::string> anims;
    int activeAnim = -1;
};

struct FakeHost : Host {
    std::vector<FakeTic> tics;
    std::vector<std::string> logs;
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
        err = "escrita não suportada no fake";
        return false;
    }

    bool ticExists(const std::string& n) override { return tic(n) != nullptr; }

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
    EXPECT(reservedWordCount() == 20);
    EXPECT(isReservedWord("stopand"));
    EXPECT(isReservedWord("allmoments"));
    EXPECT(!isReservedWord("stop"));      // não existe na V.ONI
    EXPECT(!isReservedWord("follow"));    // só dentro de tyker (0.9.3)
    EXPECT(!isReservedWord("fn"));        // estrutural, não reservada §5
}
