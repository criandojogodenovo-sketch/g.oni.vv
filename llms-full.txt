# V.ONI — Referência da linguagem

A linguagem de scripting FECHADA da G.One VV. Esta referência é GERADA do registo central do motor (uma só fonte alimenta a Docs, o editor que ensina e os erros legíveis).

Regras de ouro: NÃO EXISTE if/else/switch/break (exist/notexist/option/resume) · nomes de utilizador em MINÚSCULAS (maiúsculas são da engine) · booleanos true/false · sandbox sem ficheiros/rede · erros SEMPRE com linha, nunca crash.

## Linguagem

### central main
- **Sintaxe**: `central main { on moment { } allmoments { } }`
- **Descrição**: Bloco de arranque do TIC: contém on moment e allmoments.
- **Equivalência**: o arranque do script (setup + update)
- **Exemplo**:
```
central main {
  on moment { View P "comecou" }
  allmoments { }
}
```

### on moment
- **Sintaxe**: `on moment { }`
- **Descrição**: Corre 1× no arranque do TIC (depois do top-level).
- **Equivalência**: setup() / start()
- **Exemplo**:
```
central main { on moment { View P "uma vez" } }
```

### allmoments
- **Sintaxe**: `allmoments { }`
- **Descrição**: Corre a cada frame enquanto o TIC está ativo.
- **Equivalência**: update() por frame
- **Exemplo**:
```
central main { allmoments { move(0, 0, 0.1) } }
```

### v++ (declarar)
- **Sintaxe**: `v++nome=valor`
- **Descrição**: Declara variável com o tipo inferido do valor.
- **Equivalência**: x = valor (tipo inferido)
- **Exemplo**:
```
v++vida=100
```

### v# (declarar com tipo)
- **Sintaxe**: `v#nome:Tipo=valor`
- **Descrição**: Declara variável com tipo explícito.
- **Equivalência**: x: tipo = valor
- **Exemplo**:
```
v#velocidade:Num=5.5
```

### @+ (exportar)
- **Sintaxe**: `v#@+nome:Tipo=valor · v++@+nome=valor`
- **Descrição**: Logo depois do declarador: a variável aparece no Inspector.
- **Equivalência**: pública / editável no editor
- **Exemplo**:
```
v#@+velocidade:Num=5.5
```

### repeat
- **Sintaxe**: `repeat(n) { }`
- **Descrição**: Repete o bloco n vezes.
- **Equivalência**: for _ in range(n):
- **Exemplo**:
```
repeat(3) { View P "tres vezes" }
```

### last
- **Sintaxe**: `last(condição) { }`
- **Descrição**: Repete enquanto a condição for verdadeira.
- **Equivalência**: while condição:
- **Exemplo**:
```
last(vida > 0) { vida-=1 }
```

### with n+=1
- **Sintaxe**: `last(condição) with n+=1 { }`
- **Descrição**: Contador de iterações do last; n usável na condição e no corpo.
- **Equivalência**: enumerate / contador
- **Exemplo**:
```
last(n < 10) with n+=1 { }
```

### continue
- **Sintaxe**: `continue`
- **Descrição**: Salta o resto da iteração atual.
- **Equivalência**: continue (igual)
- **Exemplo**:
```
last(true) { continue }
```

### resume
- **Sintaxe**: `resume`
- **Descrição**: Sai do loop (o 'break' da V.ONI).
- **Equivalência**: break
- **Exemplo**:
```
last(true) { resume }
```

### exist
- **Sintaxe**: `exist(condição) { } notexist{ }`
- **Descrição**: Corre o bloco se a condição for verdadeira (o 'if' da V.ONI).
- **Equivalência**: if:
- **Exemplo**:
```
exist(vida == 0) { View P "fim" }
```

### notexist
- **Sintaxe**: `notexist{ } · notexist{ } and( cond(ação) stopand )`
- **Descrição**: O 'senão' do exist; com and(…) colado faz a cadeia de casos.
- **Equivalência**: else:
- **Exemplo**:
```
exist(a) { } notexist{ View P "default" }
```

### and(…) stopand
- **Sintaxe**: `and( cond(ação) stopand cond(ação) stopand )`
- **Descrição**: Casos do notexist: 1ª condição verdadeira corre a ação e para.
- **Equivalência**: elif: (cadeia de casos)
- **Exemplo**:
```
exist(x) { }
notexist{ View P "nada" } and(
  x2(View P "dois") stopand
}
```

### option
- **Sintaxe**: `option(valor) { and valor(ação) stopand notoption{ } }`
- **Descrição**: Escolhe caso por valor; notoption é o default.
- **Equivalência**: switch/case
- **Exemplo**:
```
option(nivel) {
  and 1(View P "facil") stopand
  notoption{ View P "outro" }
}
```

### fn
- **Sintaxe**: `fn nome(a:Num):Num { return a }`
- **Descrição**: Função do utilizador (nome em minúsculas).
- **Equivalência**: def (Python) / function (JS)
- **Exemplo**:
```
fn dobro(n:Num):Num { return n*2 }
```

### return
- **Sintaxe**: `return valor`
- **Descrição**: Devolve o valor da fn.
- **Equivalência**: return (igual)
- **Exemplo**:
```
fn um():Int { return 1 }
```

### operadores
- **Sintaxe**: `a+b · a == b · a and b · not a`
- **Descrição**: Aritmética + - * /, comparação == != < > <= >=, lógicos and or not.
- **Equivalência**: iguais (and/or/not em vez de &&/||/!)
- **Exemplo**:
```
exist(vida > 0 and not fim) { }
```

### tipos
- **Sintaxe**: `v#nome:Tipo=valor`
- **Descrição**: Int Num Txt Bool Vec2 Vec3 TIC (booleanos: true/false).
- **Equivalência**: int/float/str/bool
- **Exemplo**:
```
v#vida:Int=100 · v#@+velocidade:Num=5.5
```

## Comandos

### View P
- **Sintaxe**: `View P "texto"`
- **Descrição**: Escreve texto no log da engine (prefixo voni:).
- **Equivalência**: print()
- **Exemplo**:
```
View P "ola mundo"
```

### View Object
- **Sintaxe**: `View Object`
- **Descrição**: Ainda não existe: a consola é que o usará (erro legível por agora).
- **Equivalência**: console de objetos (futuro)
- **Exemplo**:
```
View Object  // 'consola ainda não existe'
```

### move
- **Sintaxe**: `move(x, y, z)`
- **Descrição**: Move o TIC dono do script (sem animação).
- **Equivalência**: transform.position += (x,y,z)
- **Exemplo**:
```
move(0, 0, 0.5)
```

### Import.Animation
- **Sintaxe**: `Import.Animation("nome da animação")`
- **Descrição**: Importa a animação pelo nome para o TIC dono.
- **Equivalência**: carregar/tocar um clip
- **Exemplo**:
```
Import.Animation("correr")
```

### transition.for
- **Sintaxe**: `nomedacena.transition.for("cena de destino")`
- **Descrição**: Transição de cena (NÃO se chama importar cenas).
- **Equivalência**: mudar de cena/nível
- **Exemplo**:
```
cena1.transition.for("cena2")
```

### Deltatime.Increment
- **Sintaxe**: `Deltatime.Increment(variável, valor_por_segundo)`
- **Descrição**: Acrescenta valor×dt à variável a cada execução (frame).
- **Equivalência**: x += v * dt
- **Exemplo**:
```
allmoments { Deltatime.Increment(tempo, 1) }
```

### Explode.TIC.et
- **Sintaxe**: `Explode.TIC.et`
- **Descrição**: Faz o TIC dono desaparecer.
- **Equivalência**: visible = False
- **Exemplo**:
```
Explode.TIC.et
```

### Explode.TIC.er
- **Sintaxe**: `Explode.TIC.er`
- **Descrição**: Faz o TIC dono aparecer.
- **Equivalência**: visible = True
- **Exemplo**:
```
Explode.TIC.er
```

### Search
- **Sintaxe**: `Search.alvo.propriedade`
- **Descrição**: Pesquisa dentro da cena via RTTI (leitura de propriedades).
- **Equivalência**: find()/get_node + atributo
- **Exemplo**:
```
v++px=Search.jogador.pos.x
```

### propriedades de TIC
- **Sintaxe**: `tic.propriedade · tic.pos.x=5`
- **Descrição**: Leitura/escrita por pontos: pos, rot, escala, cor (Vec3 com .x/.y/.z), name, visible, active.
- **Equivalência**: atributos do objeto
- **Exemplo**:
```
jogador.pos.x=5
```

## Linkers

### linker
- **Sintaxe**: `linker(A)to(B)=RF(nome)`
- **Descrição**: Liga A a B e regista o link no RF(nome); A e B podem ser objeto, TIC, propriedade ou animação.
- **Equivalência**: uma referência entre duas coisas
- **Exemplo**:
```
linker(jogador)to(cubo)=RF(principal)
```

## Tykers

### tyker
- **Sintaxe**: `tyker(nome){ find(RF) componentes… }`
- **Descrição**: Bloco de comportamento sobre os links de um RF; corre a cada frame enquanto o script está ativo.
- **Equivalência**: um comportamento que corre por frame
- **Exemplo**:
```
tyker(seguelo){ find(principal) follow(2) }
```

### find
- **Sintaxe**: `find(nome-do-RF)`
- **Descrição**: Liga o tyker ao RF declarado nos linkers; é SEMPRE o 1º componente e é obrigatório.
- **Equivalência**: o tyker liga-se ao grupo de links
- **Exemplo**:
```
tyker(seguelo){ find(principal) look() }
```

## Componentes

### follow
- **Sintaxe**: `follow() · follow(d) · follow(d,suav)`
- **Descrição**: A origem segue o destino: follow() cola, follow(d) guarda a distância d, suav suaviza a perseguição por segundo.
- **Equivalência**: perseguir (lerp contínuo)
- **Exemplo**:
```
linker(jogador)to(cubo)=RF(p)
tyker(s){ find(p) follow(2) }
```

### look
- **Sintaxe**: `look()`
- **Descrição**: A origem roda para ficar virada para o destino (rotação em graus).
- **Equivalência**: look_at()
- **Exemplo**:
```
tyker(s){ find(p) look() }
```

### orbit
- **Sintaxe**: `orbit(d,vel)`
- **Descrição**: A origem orbita o destino à distância d, avançando vel graus por segundo (círculo no plano XZ).
- **Equivalência**: girar em volta
- **Exemplo**:
```
tyker(s){ find(p) orbit(3, 90) }
```

### copy
- **Sintaxe**: `copy(propriedade)`
- **Descrição**: Copia a propriedade do destino para a origem a cada frame (ex.: copy(pos), copy(cor)).
- **Equivalência**: espelhar um atributo
- **Exemplo**:
```
tyker(s){ find(p) copy(escala) }
```

### map
- **Sintaxe**: `map()`
- **Descrição**: Reservado: mapeia valores entre as pontas; sem mapa definido é no-op (não faz nada, não dá erro).
- **Equivalência**: remapear valores (reservado)
- **Exemplo**:
```
tyker(s){ find(p) map() }
```

### Change
- **Sintaxe**: `Change(origem)to(alvo) · Change(destino)to(alvo)`
- **Descrição**: Muda um lado dos links do RF para o novo alvo (o RF é partilhado: todos os tykers desse RF passam a ver a mudança).
- **Equivalência**: reescrever o alvo da ligação
- **Exemplo**:
```
tyker(s){ find(p) Change(destino)to(cubo2) }
```

### point
- **Sintaxe**: `point() · point(x,y,z)`
- **Descrição**: point(x,y,z) fixa um ponto absoluto como alvo do follow/look/orbit; point() limpa o ponto e volta ao destino vivo.
- **Equivalência**: alvo fixo (coordenadas)
- **Exemplo**:
```
tyker(s){ find(p) point(0, 1, 0) follow(2) }
```

### colorpars
- **Sintaxe**: `colorpars(cor)(nome|#RRGGBB)`
- **Descrição**: Define um parâmetro de cor: o parâmetro 'cor' tinge o material do TIC de origem (nome da paleta ou hex); outros nomes ficam guardados para o shading() futuro.
- **Equivalência**: cor do material
- **Exemplo**:
```
tyker(s){ find(p) colorpars(cor)(#FF0000) }
```

### play
- **Sintaxe**: `play()`
- **Descrição**: Toca a animação do linker (a ponta que é nome de animação, não de TIC) no TIC dono do script.
- **Equivalência**: tocar uma animação
- **Exemplo**:
```
linker(ator)to(correr)=RF(a)
tyker(t){ find(a) play() }
```

### limit
- **Sintaxe**: `limit(min,max)`
- **Descrição**: Limita a distância da origem ao alvo entre min e max (aplica-se ao follow e ao orbit).
- **Equivalência**: clamp da distância
- **Exemplo**:
```
tyker(s){ find(p) follow(2) limit(1, 10) }
```

### delay
- **Sintaxe**: `delay(segundos)`
- **Descrição**: Adia a ativação do tyker: os componentes pontuais só disparam — e os contínuos só começam — passados os segundos.
- **Equivalência**: esperar N segundos
- **Exemplo**:
```
tyker(s){ find(p) delay(2) follow(1) }
```

### shading
- **Sintaxe**: `shading()`
- **Descrição**: Reservado: aceita e não faz nada (o futuro sombreado consumirá os parâmetros do colorpars).
- **Equivalência**: sombreado (reservado)
- **Exemplo**:
```
tyker(s){ find(p) shading() }
```

