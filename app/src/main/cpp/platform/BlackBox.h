#pragma once
// platform/BlackBox.h — F4-Z2: A CAIXA NEGRA (o pedido do dono).
//
// O snapshot forense de UM frame: .txt + PNG no Downloads (device, via
// MediaStore — Download/GOneVV/blackbox/) ou no diretório de logs do
// hospedeiro (testes/harness). O .txt leva O ESTADO COMPLETO do momento:
// GL (renderer/versão/erros), o Nº DE RECRIAÇÕES de contexto, a câmara, o
// rect do viewport 3D, os ponteiros, o gizmo, o WorldEnvironment do TIC
// selecionado, os contadores de recursos GL (nascidos/mortos por kind), a
// linha de estado do frame (fps/verts/dc) e as ÚLTIMAS 200 linhas do
// engine.log. O PNG é o backbuffer inteiro do frame em que a caixa corre.
//
// Os GATILHOS AUTOMÁTICOS: a recuperação de contexto (contextRecover) e a
// câmara inválida corrigida (cameraEnsureSane) ARMAM a caixa — o snapshot
// sai no FIM do frame corrente (o MESMO ponto seguro do thumb/layoutDump:
// frame completo no backbuffer, antes do swap). O botão manual vive em
// Settings › Diagnóstico («Caixa negra (snapshot)»).
//
// A parte GL (glReadPixels) corre na thread principal no ponto seguro; o
// encode PNG + a escrita correm no thread de jobs (o padrão ImportJob — o
// hot path nunca espera).
#include <string>
#include <vector>

// AS FUNÇÕES (arm/buildText/waitIdle/hostDir) vivem em platform/main.cpp
// com LINKAGE INTERNO (static) — main.cpp é incluído como TU único pelos
// harnesses (o padrão 1-TU da casa: tests/raster_z2.cpp, c33_virtual);
// símbolos externos neste ficheiro = multiple definition na ligação.
// O contrato é o comentário acima; as assinaturas:
//   void arm(const char* why);            // arma a captura no fim do frame
//   std::string buildText();              // o conteúdo do .txt (puro)
//   bool waitIdle(int timeoutMs);         // só testes: espera o job
//   const char* hostDir();                // o dir de logs no hospedeiro

