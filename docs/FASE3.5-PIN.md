# FASE 3.5 — PIN DE FECHO (GPU Efficiency)

## Alvos (C33 / RMX3624 / PowerVR GE8320)

| Métrica | Alvo | Antes (0.11-S) | Depois |
|---|---|---|---|
| Draw calls (city 72 blocos + 10 pers + 500 cubos) | ≤ 80 | (vazio) | (vazio) |
| Vértices/frame | ≤ 500.000 | (vazio) | (vazio) |
| Blocos visíveis/total (vista lateral, city) | ≤ 40% | (vazio) | (vazio) |
| Skinned fora do frustum a animar | 0 | (vazio) | (vazio) |
| Draw calls 500 cubos idênticos | 1 | (vazio) | (vazio) |
| CPU/frame (bench R-017) | ≤ 8 ms | 0.211 ms (anim) | (vazio) |
| GPU/frame (estimado pelo frame total − CPU) | ≤ 8 ms | (vazio) | (vazio) |
| Crash dumps novos | 0 | (vazio) | (vazio) |

## Cena de teste

City expandida (72 TICs do PASSO 5A) + 10 personagens do FASE3-PIN
a andar + 500 cubos idênticos + vista lateral fixa. 60 segundos.

## Contexto do CPU medido

O bench da 0.11-S mostra 0.211 ms/frame de CPU para a animação. Isto
é 1,3% de 60 fps. Se o frame total exceder 16,6 ms no C33, o gargalo
é GPU (draws, verts, state changes) — é isso que a FASE 3.5 ataca.

## Critério

Todos os alvos verdes = FECHADO. A FASE 5 (Luzes PBR) só arranca
com este pin fechado.

---

## Nota de abertura (preenchimento)

A coluna «Depois» é preenchida NA SESSÃO C33 do dono (o HUD e o
engine.log dizem o MESMO que o frame fez — fonte única DrawStats/
BlockFrameStats). A coluna «Antes» fica vazia onde a 0.11-S não mediu
no device (o sign-off físico é do dono — o padrão de sempre). A
numeração das sentinelas desta campanha é R-045..R-048 (a R-044 já
estava ocupada pelas thumbs do PASSO 5C — ver §0.1 da ABERTURA).
