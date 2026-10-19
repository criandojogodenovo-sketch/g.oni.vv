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

### every
- **Sintaxe**: `every(segundos) { }`
- **Descrição**: Corre o bloco a cada N segundos (o resto do período carrega para o ciclo seguinte — sem drift); dispara no máximo 1× por frame.
- **Equivalência**: setInterval(() => { }, ms)
- **Exemplo**:
```
central main {
  allmoments {
    every(0.5) { View P "pulso" }
  }
}
```

### after
- **Sintaxe**: `after(segundos) { }`
- **Descrição**: Corre o bloco UMA vez, passados N segundos desde o 1.º encontro desta linha (depois fica inativo para sempre).
- **Equivalência**: setTimeout(() => { }, ms) (1×)
- **Exemplo**:
```
central main {
  allmoments {
    after(3) { View P "portao" }
  }
}
```

### when
- **Sintaxe**: `when(condição) { }`
- **Descrição**: Corre o bloco UMA vez, no 1.º frame em que a condição está verdadeira (se já nasce verdadeira, é logo no 1.º; depois fica inativo para sempre).
- **Equivalência**: on condition became true (edge)
- **Exemplo**:
```
central main {
  allmoments {
    when(vida == 0) { View P "fim" }
  }
}
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
- **Sintaxe**: `View P "texto" · View P expressão`
- **Descrição**: Escreve no log da engine (prefixo voni:) — texto entre aspas OU qualquer valor (Int Num Txt Bool Vec2 Vec3 TIC); 0.12-U (3.9a).
- **Equivalência**: print()
- **Exemplo**:
```
View P "ola mundo"
View P 42
View P jogador.pos
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

## Funções

### abs
- **Sintaxe**: `abs(x)`
- **Descrição**: Valor absoluto: Int devolve Int, Num devolve Num.
- **Equivalência**: abs() (igual)
- **Exemplo**:
```
v++d=abs(-5)
```

### min
- **Sintaxe**: `min(a, b)`
- **Descrição**: O menor de dois números (Int com Int devolve Int; Num misturado devolve Num).
- **Equivalência**: min(a, b)
- **Exemplo**:
```
v++m=min(vida, 0)
```

### max
- **Sintaxe**: `max(a, b)`
- **Descrição**: O maior de dois números (Int com Int devolve Int; Num misturado devolve Num).
- **Equivalência**: max(a, b)
- **Exemplo**:
```
v++m=max(vida, 0)
```

### clamp
- **Sintaxe**: `clamp(v, lo, hi)`
- **Descrição**: Limita v ao intervalo [lo, hi] (v abaixo de lo devolve lo; acima de hi devolve hi).
- **Equivalência**: max(lo, min(hi, v))
- **Exemplo**:
```
v++v=clamp(vel, 0, 10)
```

### lerp
- **Sintaxe**: `lerp(a, b, t)`
- **Descrição**: Interpolação linear de a até b por t (0 = a, 1 = b, 0.5 = a meio); devolve Num.
- **Equivalência**: a + (b - a) * t
- **Exemplo**:
```
v++x=lerp(0, 10, 0.25)
```

### floor
- **Sintaxe**: `floor(x)`
- **Descrição**: Arredonda PARA BAIXO ao inteiro (2.7 → 2, -2.1 → -3); devolve Int.
- **Equivalência**: math.floor(x) / Math.floor(x)
- **Exemplo**:
```
v++n=floor(2.7)
```

### ceil
- **Sintaxe**: `ceil(x)`
- **Descrição**: Arredonda PARA CIMA ao inteiro (2.1 → 3, -2.7 → -2); devolve Int.
- **Equivalência**: math.ceil(x) / Math.ceil(x)
- **Exemplo**:
```
v++n=ceil(2.1)
```

### round
- **Sintaxe**: `round(x)`
- **Descrição**: Arredonda ao inteiro mais próximo (meio afasta do zero: 2.5 → 3, -2.5 → -3); devolve Int.
- **Equivalência**: round(x) / Math.round(x)
- **Exemplo**:
```
v++n=round(2.5)
```

### sqrt
- **Sintaxe**: `sqrt(x)`
- **Descrição**: Raiz quadrada; x negativo é erro legível (com linha).
- **Equivalência**: math.sqrt(x) / Math.sqrt(x)
- **Exemplo**:
```
v++r=sqrt(9)
```

### sin
- **Sintaxe**: `sin(graus)`
- **Descrição**: Seno de um ângulo em GRAUS (não radianos); devolve Num.
- **Equivalência**: math.sin(radians(x)) / Math.sin(x * PI / 180)
- **Exemplo**:
```
v++s=sin(90)
```

### cos
- **Sintaxe**: `cos(graus)`
- **Descrição**: Cosseno de um ângulo em GRAUS (não radianos); devolve Num.
- **Equivalência**: math.cos(radians(x)) / Math.cos(x * PI / 180)
- **Exemplo**:
```
v++c=cos(0)
```

### random
- **Sintaxe**: `random()`
- **Descrição**: Número aleatório Num em [0, 1) (0 incluso, 1 exclusivo) — novo a cada chamada.
- **Equivalência**: random() / Math.random()
- **Exemplo**:
```
v++d=random()
```

### random_int
- **Sintaxe**: `random_int(min, max)`
- **Descrição**: Inteiro aleatório ENTRE min e max (ambos inclusos); min > max é erro legível.
- **Equivalência**: randint(min, max)
- **Exemplo**:
```
v++dado=random_int(1, 6)
```

### distance
- **Sintaxe**: `distance(a, b)`
- **Descrição**: Distância entre dois pontos: cada argumento é um TIC (usa a pos dele) ou um Vec3 (ex.: jogador.pos); devolve Num.
- **Equivalência**: hipot / dist entre posições
- **Exemplo**:
```
v++d=distance(jogador, alvo)
v++d2=distance(jogador.pos, alvo.pos)
```

### spawn
- **Sintaxe**: `spawn(tipo, x, y, z)`
- **Descrição**: Cria um TIC na cena: o tipo é "esfera" ou "box" (os nomes do .goni); nasce active/visible com nome único; devolve o TIC novo. Tipo desconhecido = erro com linha.
- **Equivalência**: instanciar objeto / add_child
- **Exemplo**:
```
v#novo:TIC=spawn("box", 0, 0, -5)
novo.pos.y=1
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
- **Descrição**: point(x,y,z) fixa um ponto absoluto como alvo do follow/look/orbit/camrig; point() limpa o ponto e volta ao destino vivo.
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

### stop
- **Sintaxe**: `stop()`
- **Descrição**: Para a animação do TIC dono do script (o par do play/Import.Animation): volta ao início e cancela o blend a meio.
- **Equivalência**: parar uma animação
- **Exemplo**:
```
tyker(t){ find(a) stop() }
```

### blend
- **Sintaxe**: `blend(animação, segundos)`
- **Descrição**: Mistura suavemente (crossfade) a animação atual com a animação 'nome' em 'segundos' (0.3 se faltar); ao completar, ela é a ativa.
- **Equivalência**: misturar duas animações
- **Exemplo**:
```
tyker(t){ find(a) blend(correr, 0.4) }
```

### limit
- **Sintaxe**: `limit(min,max)`
- **Descrição**: Limita a distância da origem ao alvo entre min e max (aplica-se ao follow, ao orbit e ao camrig — no camrig clampa a distância final, após a suavização).
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

### camrig
- **Sintaxe**: `camrig(d,h) · camrig(d,h,suav)`
- **Descrição**: Câmara de terceira pessoa: assenta a distância d e a altura h acima do alvo (o pivô roda com o yaw/pitch do camdrag), olha para alvo+h/2; suav (por segundo) suaviza a perseguição; limit(min,max) clampa a distância final. Não se mistura com follow/look.
- **Equivalência**: câmara de terceira pessoa
- **Exemplo**:
```
linker(camera)to(jogador)=RF(cam)
tyker(camera_jogo){ find(cam) camrig(8, 3, 6) camdrag(0.3) limit(3, 12) }
```

### camdrag
- **Sintaxe**: `camdrag(sens)`
- **Descrição**: Roda a câmara com o arrasto do dedo: yaw 360° livre, pitch limitado a -20..70 graus; só lê o que os controlos de toque não reclamaram e só corre em Play (o sens negativo inverte).
- **Equivalência**: olhar com o dedo (touch look)
- **Exemplo**:
```
tyker(camera_jogo){ find(cam) camrig(8, 3, 6) camdrag(0.3) }
```

### shading
- **Sintaxe**: `shading()`
- **Descrição**: Reservado: aceita e não faz nada (o futuro sombreado consumirá os parâmetros do colorpars).
- **Equivalência**: sombreado (reservado)
- **Exemplo**:
```
tyker(s){ find(p) shading() }
```

