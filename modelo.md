

## 1. Descrição do Problema

O problema de escalonamento de horários em escolas é um problema conhecido com várias variações e soluções como: [ascTimeTables](https://www.asctimetables.com/timetables_pt.html), [PowerCubus](https://powercubus.com.br/features), [CronosTimeTable](https://www.cronostimetable.com/) e outros. Apesar de um ser um problema muito comum em instituições de ensino, pode ter muitas características diferentes entre diferentes instituições. O **Problema de Programação de Horários Universitário e Escolar** (*High School and University Timetabling Problem*), classificado na literatura de complexidade como **NP-difícil** (*NP-hard*) [[Even, Itai & Shamir, 1976](#16-referências-bibliográficas); [Schaerf, 1999](#16-referências-bibliográficas)].

Visando criar uma solução para instituição que trabalho, listei a baixo os requisitos que o sistema deve levar em consideração para escalonar as aulas:
+ Aulas podem ter tempo de 45 minutos ou 50 minutos.
+ Professores tem disponibilidade de acordo com turno
+ Professores podem expressar preferências de dias e horários.
+ Turmas podem ter aulas pela manhã e tarde ou tarde e noite, podendo variar de acordo com o dia da semana.
+ Salas pode ser cadastradas e classificadas como: laboratórios (mecânica, elétrica, informática, química e física), sala teórica e ambiente especial (ginásio, sala de música e etc), com número de alunos suportados.
+ Disciplinas podem ser semestral ou anual, podem usar a sala ou grupo de salas em especifico, quantidade de aulas por dia e se devem ser ministradas em bloco ou não.



# 2. Definição do Modelo

## 2.1. Representação do Tempo

As aulas podem ser de 45 ou 50 minutos. Devido a isso o dia será dividido em blocos (*slots*) de $\Delta = 5$ minutos, numerados a partir da meia-noite (`00:00` = slot 0):
- Uma aula de **45 minutos** ocupa $L_a = 9$ slots consecutivos.
- Uma aula de **50 minutos** ocupa $L_a = 10$ slots consecutivos.
- Uma aula que começa no slot $t$ e dura $L_a$ slots ocupa o intervalo semiaberto $[t, t + L_a)$, ou seja, os slots $t, t+1, \ldots, t+L_a-1$. Assim, uma aula que termina às `08:45` (slot 105) não conflita com outra que começa exatamente às `08:45` (slot 105).

Com essa representação podemos representar qualquer tempo de aula que seja múltiplo de 5 minutos, o problema dela é a grande quantidade *slots* gerado, e um dia temos 288 para gerenciar. Essa abordagem segue a **Formulação Indexada no Tempo** (*Time-Indexed Formulation*) clássica de escalonamento [[Sousa & Wolsey, 1992](#16-referências-bibliográficas); [Daskalaki et al., 2004](#16-referências-bibliográficas)] e está implementada em [`TimeInterval.h`](include/model/TimeInterval.h#L16-L35) (`SLOT_MINUTES = 5`, `SLOTS_PER_DAY = 288`).

## 2.2. Disciplinas Anuais e Semestrais

É considerado que todas as disciplinas são divididas em semestres, mas algumas ocupam dois semestres, as anuais, ($\mathcal{S} = \{1, 2\}$ e outras podem ocorrer em um único semestre ($\mathcal{S} = \{1\}$ ou $\mathcal{S} = \{2\}$) , sendo assim:
- **Disciplinas anuais:** Ocorrem nos dois semestres no mesmo dia, horário e sala. Por isso, usam uma única variável de decisão compartilhada entre o 1º e o 2º semestre.
- **Disciplinas semestrais:** Ocupam professor, turma e sala apenas no semestre em que estão ativas. Isso permite que uma aula do 1º semestre e outra do 2º semestre usem o mesmo horário e a mesma sala sem conflito.

## 2.3. Conjuntos e Índices dos "Recursos"

Na tabela a baixo esta sintetizado os conjuntos e índices dos "recursos" a serem alocados:

| Símbolo | Significado|
|---|---|
| $\mathcal{P},\ p$ | Conjunto de professores e índice de professor | 
| $\mathcal{G},\ g$ | Conjunto de turmas e índice de turma |
| $\mathcal{R},\ r$ | Conjunto de salas/laboratórios e índice de sala | 
| $\mathcal{Q},\ q$ | Conjunto de disciplinas e índice de disciplina | 
| $\mathcal{A},\ a$ | Ocorrências semanais de aulas a serem alocadas | 
| $\mathcal{A}_q \subseteq \mathcal{A}$ | Ocorrências de aula pertencentes à disciplina $q$ |
| $\mathcal{D},\ d$ | Dias úteis da semana (Segunda a Sexta) | 
| $\mathcal{S},\ s$ | Semestres ativos na otimização ($\{1\}$, $\{2\}$ ou $\{1,2\}$) |
| $\mathcal{K}_d,\ k$ | Slots de 5 minutos do dia $d$ ($0 \le k < 288$) |
| $\mathcal{H},\ h$ | Turnos de trabalho (Manhã, Tarde, Noite) | 
| $\mathcal{K}_{d,h}$ | Slots do dia $d$ que pertencem ao turno $h$ | 
| $\mathcal{P}_a,\ \mathcal{G}_a$ | Professores e turmas que participam da aula $a$ | 
| $\mathcal{D}_g \subseteq \mathcal{D}$ | Dias da semana em que a turma $g$ tem aula |
| $\mathcal{S}_a \subseteq \mathcal{S}$ | Semestres em que a aula $a$ está ativa | 
| $\mathcal{R}_g^{teo} \subseteq \mathcal{R}$ | Salas permitidas para serem a sala teórica fixa da turma $g$ |
| $\mathcal{B}_a$ | Combinações de salas válidas para a aula $a$ | 
| $\mathcal{C}_a$ | Conjunto de candidatos viáveis gerados para a aula $a$ | 
  

**Ocorrência de aula ($a \in \mathcal{A}$)?**

Se a disciplina de Matemática tem 4 aulas por semana, o sistema gera 4 ocorrências independentes ($a_1, a_2, a_3, a_4 \in \mathcal{A}_q$). Cada ocorrência precisa receber um dia, um horário inicial e uma sala. Se as aulas ocorrerem sempre de duas em duas, a disciplina pode ser cadastrada com aulas de 90 minutos, que irá gerar metade das ocorrências.

**O que é um candidato ($c \in \mathcal{C}_a$)?**

Cada candidato $c \in \mathcal{C}_a$ representa uma escolha de $(d_c, t_c, b_c)$ para a aula $a$:
- $d_c \in \mathcal{D}$: o dia da semana;
- $t_c \in \mathcal{K}_d$: o slot de início da aula;
- $b_c \subseteq \mathcal{R}$: o conjunto de salas usadas simultaneamente pela aula.


## 3. Parâmetros de Entrada


| Símbolo | Unidade / Domínio | Significado |
|---|---|---|
| $L_a$ | Slots ($\mathbb{Z}_{>0}$) | Duração da aula $a$ em slots de 5 minutos  |
| $\ell_a = 5 L_a$ | Minutos | Duração da aula $a$ em minutos  |
| $T^{ini}_{g,d},\ T^{fim}_{g,d}$ | Slots | Início e fim da jornada diária da turma $g$ no dia $d$ |
| $[b^1_{g,d}, e^1_{g,d}),\ [b^2_{g,d}, e^2_{g,d})$ | Slots | Intervalos de recreio/almoço obrigatórios da turma $g$ no dia $d$ |
| $\mathcal{F}_{g,d} \subseteq \mathcal{K}_d$ | Conjunto de slots | Slots letivos úteis da turma $g$ no dia $d$ (jornada menos intervalos) |
| $\mathcal{V}_{p,d} \subseteq \mathcal{K}_d$ | Conjunto de slots | Slots em que o professor $p$ está disponível para dar aula no dia $d$ |
| $M_{p,d}$ | Minutos | Carga máxima de aula do professor $p$ no dia $d$  |
| $\omega_{p,d}$ | $\{0, \ldots, 10\}$ | Nota de preferência do professor $p$ pelo dia $d$ (10 = prefere muito) |
| $\alpha_p$ | $\{0, \ldots, 10\}$ | Sensibilidade do professor $p$ a janelas entre aulas |
| $\pi_{p,d,h}$ | $\{0, 1\}$ | Vale $1$ se o professor $p$ pretende trabalhar no turno $h$ do dia $d$ |
| $U_{q,g}$ | $\mathbb{Z}_{>0}$ | Limite máximo de aulas da disciplina $q$ por dia para a turma $g$ |
| $c^{hora}_{a,c}$ | $\mathbb{R}_{\ge 0}$ | Penalidade de horário do candidato $c$  |
| $c^{sala}_{a,c}$ | $\mathbb{R}_{\ge 0}$ | Penalidade opcional pelo uso das salas do candidato $c$ |
| $\lambda_J, \lambda_D, \lambda_P, \lambda_W, \lambda_T, \lambda_G$ | $\mathbb{R}_{\ge 0}$ | Pesos configuráveis dos 6 critérios principais da função objetivo |
| $\lambda_E, \lambda_R$ | $\mathbb{R}_{\ge 0}$ | Pesos opcionais de estabilidade da grade e preferência de salas |
| $\rho_s$ | $\mathbb{R}_{>0}$ | Peso do semestre $s$ na função objetivo |


> **Importante:** Preferência baixa ($\omega_{p,d} = 0$ ou $\pi_{p,d,h} = 0$) gera apenas **penalidade** na função objetivo, mas não proíbe o horário. Horários proibidos para o professor ficam fora do conjunto de disponibilidade $\mathcal{V}_{p,d}$.

  

## 5. Gerando Horários Candidatos para as Aulas ($\mathcal{C}_a$)

A jornada da turma $g$ no dia $d$ deve obedecer à ordem cronológica. Começa as aulas $T^{ini}_{g,d}$ e finaliza em $T^{fim}_{g,d}$. Nesse intervalo de tempo pode ter até 3 intervalos começando em $b^1_{g,d}$ e terminado em  $e^1_{g,d}$, sendo 1 a referência de qual dos 3 intervalos  esta sendo referenciado. Retirando os dois intervalos, a jornada se divide em até três **blocos letivos contínuos**, cuja união forma os slots letivos $\mathcal{F}_{g,d}$:
  
$$

\mathcal{F}_{g,d} = \{T^{ini}_{g,d}, \ldots, T^{fim}_{g,d}-1\} \setminus \left([b^1_{g,d}, e^1_{g,d}) \cup [b^2_{g,d}, e^2_{g,d})\right).

$$

Para uma aula $a$, um candidato $c = (d_c, t_c, b_c)$ ocupando os slots $\mathcal{I}_{a,c} = \{t_c, \ldots, t_c + L_a - 1\}$ só é criado se satisfizer **todas** as regras abaixo (pré-filtragem de domínio segundo [[Schaerf, 1999](#16-referências-bibliográficas)], implementada em [`Preprocessor::gerarCandidatosParaAula`](src/solver/Preprocessor.cpp#L249-L481)):

1. **Cabe na jornada e não corta intervalos:** Todas as turmas $g \in \mathcal{G}_a$ têm aula no dia $d_c$ ($d_c \in \mathcal{D}_g$) e o intervalo $\mathcal{I}_{a,c}$ cabe inteiramente dentro de um único bloco letivo contínuo de $\mathcal{F}_{g,d_c}$.
   - *Código:* [`Preprocessor::turmaAcomodaAula`](src/solver/Preprocessor.cpp#L199-L214) e [`Preprocessor.cpp` (L361–L396)](src/solver/Preprocessor.cpp#L361-L396).

2. **Disponibilidade docente:** Todos os professores $p \in \mathcal{P}_a$ estão disponíveis durante todo o intervalo da aula ($\mathcal{I}_{a,c} \subseteq \mathcal{V}_{p,d_c}$).
   - *Referência:* `PreferTimesConstraint` (Hard) no padrão XHSTT [[Post et al., 2012](#16-referências-bibliográficas)].
   - *Código:* [`Preprocessor.cpp` (L410–L419)](src/solver/Preprocessor.cpp#L410-L419) (`Professor::isDisponivelNoIntervalo`).

3. **Turno permitido:** O intervalo $\mathcal{I}_{a,c}$ cabe em pelo menos um dos turnos autorizados para a disciplina (ou a lista de turnos permitidos está vazia).
   - *Código:* [`Preprocessor::shiftAcomodaAula`](src/solver/Preprocessor.cpp#L223-L240) e [`Preprocessor.cpp` (L397–L408)](src/solver/Preprocessor.cpp#L397-L408).

4. **Salas compatíveis:** A combinação de salas $b_c \in \mathcal{B}_a$ é válida para o tipo da aula (se for aula teórica, $b_c = \{r\}$ deve pertencer às salas teóricas elegíveis da turma).
   - *Referência:* Filtragem de subconjuntos de salas por tipo de aula [[Daskalaki et al., 2004](#16-referências-bibliográficas)].
   - *Código:* [`Preprocessor.cpp` (L302–L336)](src/solver/Preprocessor.cpp#L302-L336) e [`Preprocessor.cpp` (L431–L442)](src/solver/Preprocessor.cpp#L431-L442).
  

## 6. Variáveis de Decisão

Agora serão descritas as variáveis de decisão do nosso problema.
  

| Variável | Domínio | O que representa |
|---|---|---|
| $x_{a,c}$ | $\{0,1\}$ | Vale $1$ se o candidato $c \in \mathcal{C}_a$ for o escolhido para a aula $a$ |
| $z_{g,r}$ | $\{0,1\}$ | Vale $1$ se a sala $r \in \mathcal{R}_g^{teo}$ for escolhida como sala teórica fixa da turma $g$ |
| $y_{p,d,s}$ | $\{0,1\}$ | Vale $1$ se o professor $p$ der pelo menos uma aula no dia $d$ (no semestre $s$) |
| $v_{p,d,h,s}$ | $\{0,1\}$ | Vale $1$ se o professor $p$ trabalhar no turno $h$ do dia $d$ (no semestre $s$) |
| $o^P_{p,d,k,s}$ | $[0,1]$ | Indica se o professor $p$ está em aula no slot $k$ do dia $d$ e semestre $s$ |
| $o^G_{g,d,k,s}$ | $[0,1]$ | Indica se a turma $g$ está em aula no slot $k$ do dia $d$ e semestre $s$ |
| $m_{p,d,s}$ | $\mathbb{R}_{\ge 0}$ | Total de minutos de aula do professor $p$ no dia $d$ e semestre $s$ |
| $u^-_{i,k},\ u^+_{i,k}$ | $\{0,1\}$ | Indicam se existe aula **antes** ($u^-$) e **depois** ($u^+$) do slot $k$ na sequência $i$ |
| $j_{i,k}$ | $[0,1]$ | Vale $1$ se o slot $k$ for uma **janela** (tempo vago entre duas aulas) na sequência $i$ |
| $w_{q,d}$ | $\{0,1\}$ | Vale $1$ se a disciplina $q$ tiver um bloco de aulas consecutivas alocado no dia $d$ |
| $\beta_{q,f}$ | $\{0,1\}$ | Seleciona a configuração de aulas adjacentes $f$ para um bloco da disciplina $q$ |
| $e_a$ | $[0,1]$ | Vale $1$ se a aula $a$ mudou de lugar em relação à grade anterior (estabilidade) |

  

---

  

## 7. Expressões Auxiliares de Ocupação

  

Para saber se um candidato $c \in \mathcal{C}_a$ ocupa o slot $k$ do dia $d$, definimos a constante $H_{a,c,d,k}$:

  

$$

H_{a,c,d,k} = \begin{cases}

1, & \text{se } d_c = d \text{ e } t_c \le k < t_c + L_a, \\

0, & \text{caso contrário}.

\end{cases}

$$

  

Com ela, a ocupação do professor $p$ e da turma $g$ no dia $d$, slot $k$ e semestre $s$ são somas lineares diretas:

  

$$

o^P_{p,d,k,s} = \sum_{a \in \mathcal{A}:\, p \in \mathcal{P}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,k}\, x_{a,c},

$$

  

$$

o^G_{g,d,k,s} = \sum_{a \in \mathcal{A}:\, g \in \mathcal{G}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,k}\, x_{a,c}.

$$

  

Também definimos o conjunto de pares $(a,c)$ que envolvem o professor $p$ no dia $d$ e semestre $s$:

  

$$

\mathcal{X}_{p,d,s} = \{(a,c) : a \in \mathcal{A},\ p \in \mathcal{P}_a,\ s \in \mathcal{S}_a,\ c \in \mathcal{C}_a,\ d_c = d\},

$$

  

e o subconjunto $\mathcal{X}_{p,d,h,s} \subseteq \mathcal{X}_{p,d,s}$ contendo os candidatos associados ao turno $h$.
  

## 8. Restrições

  

### R1. Toda aula deve ser alocada exatamente uma vez

Cada ocorrência de aula $a \in \mathcal{A}$ deve receber um único candidato:

  

$$

\sum_{c \in \mathcal{C}_a} x_{a,c} = 1 \qquad \forall a \in \mathcal{A}.

$$

- **Referência na literatura:** `AssignTimeConstraint` e `AssignResourceConstraint` no padrão XHSTT [[Post et al., 2012](#16-referências-bibliográficas); [Schaerf, 1999](#16-referências-bibliográficas)].
- **Implementação no código:**
  - CP-SAT: [`ScheduleSolver.cpp` (L282–L290)](src/solver/ScheduleSolver.cpp#L282-L290)
  - MILP (SCIP / HiGHS / cuOpt): [`ScheduleSolver.cpp` (L1736–L1745)](src/solver/ScheduleSolver.cpp#L1736-L1745)

  

### R2. Sem choque de horário para professores

Um professor não pode estar em duas aulas ao mesmo tempo no mesmo semestre:

  

$$

o^P_{p,d,k,s} \le 1 \qquad \forall p, d, k, s.

$$

- **Referência na literatura:** `AvoidClashesConstraint` (Resource: Teacher) no XHSTT [[Post et al., 2012](#16-referências-bibliográficas)]; inequações de clique indexadas no tempo [[Sousa & Wolsey, 1992](#16-referências-bibliográficas)] e restrições `NoOverlap` sobre variáveis de intervalo opcionais [[Laborie & Rogerie, 2008](#16-referências-bibliográficas); [Perron & Furnon, OR-Tools](#16-referências-bibliográficas)].
- **Implementação no código:**
  - CP-SAT (`NewOptionalIntervalVar` + `AddNoOverlap`): [`ScheduleSolver.cpp` (L292–L321)](src/solver/ScheduleSolver.cpp#L292-L321)
  - MILP (cliques por slot `add_no_overlap_cliques`): [`ScheduleSolver.cpp` (L1747–L1791)](src/solver/ScheduleSolver.cpp#L1747-L1791)
  - Heurística Gulosa: [`ScheduleSolver.cpp` (L1094–L1100)](src/solver/ScheduleSolver.cpp#L1094-L1100)

  

### R3. Sem choque de horário para turmas

Uma turma não pode ter duas aulas simultâneas no mesmo semestre:

  

$$

o^G_{g,d,k,s} \le 1 \qquad \forall g, d, k, s.

$$

- **Referência na literatura:** `AvoidClashesConstraint` (Resource: Class/Student Group) no XHSTT [[Post et al., 2012](#16-referências-bibliográficas); [Santos et al., 2012](#16-referências-bibliográficas)].
- **Implementação no código:**
  - CP-SAT: [`ScheduleSolver.cpp` (L352–L379)](src/solver/ScheduleSolver.cpp#L352-L379)
  - MILP: [`ScheduleSolver.cpp` (L1747–L1791)](src/solver/ScheduleSolver.cpp#L1747-L1791)
  - Heurística Gulosa: [`ScheduleSolver.cpp` (L1102–L1106)](src/solver/ScheduleSolver.cpp#L1102-L1106)

  

### R4. Sem choque de horário para salas

Nenhuma sala física $r$ pode ser usada por mais de uma aula no mesmo horário e semestre:

  

$$

\sum_{a \in \mathcal{A}:\, s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a:\, r \in b_c} H_{a,c,d,k}\, x_{a,c} \le 1 \qquad \forall r, d, k, s.

$$

- **Referência na literatura:** `AvoidClashesConstraint` (Resource: Room) no XHSTT [[Post et al., 2012](#16-referências-bibliográficas); [Daskalaki et al., 2004](#16-referências-bibliográficas)].
- **Implementação no código:**
  - CP-SAT: [`ScheduleSolver.cpp` (L323–L350)](src/solver/ScheduleSolver.cpp#L323-L350)
  - MILP: [`ScheduleSolver.cpp` (L1747–L1791)](src/solver/ScheduleSolver.cpp#L1747-L1791)
  - Heurística Gulosa: [`ScheduleSolver.cpp` (L1153–L1159)](src/solver/ScheduleSolver.cpp#L1153-L1159)
  

### R5. Sala teórica fixa por turma

Cada turma $g$ deve ter uma única sala fixa durante toda a semana para suas aulas teóricas:

  

$$

\sum_{r \in \mathcal{R}_g^{teo}} z_{g,r} = 1 \qquad \forall g \in \mathcal{G}.

$$

  

Uma aula teórica da turma $g$ só pode usar um candidato na sala $r$ se essa sala for a sala fixa escolhida para a turma:

  

$$

x_{a,c} \le z_{g,r} \qquad \forall a \text{ teórica},\ \forall c \in \mathcal{C}_a \text{ com } b_c = \{r\},\ \forall g \in \mathcal{G}_a.

$$

Se a turma já tiver uma sala fixa predefinida $\bar{r}_g$ no cadastro, fixa-se $z_{g,\bar{r}_g} = 1$. Aulas práticas ou de laboratório não usam essa regra.

- **Referência na literatura:** Restrições de estabilidade de sala (*Room Stability* / *Home Room Assignment*) e separação entre salas teóricas e laboratórios [[Daskalaki et al., 2004](#16-referências-bibliográficas); [Post et al., 2012](#16-referências-bibliográficas)].
- **Implementação no código:**
  - Pré-processamento: [`Preprocessor.cpp` (L304–L315)](src/solver/Preprocessor.cpp#L304-L315)
  - CP-SAT: [`ScheduleSolver.cpp` (L245–L278)](src/solver/ScheduleSolver.cpp#L245-L278)
  - MILP (com agregação fortalecida $\sum_{c:\,b_c=\{r\}} x_{a,c} \le z_{g,r}$): [`ScheduleSolver.cpp` (L1696–L1734)](src/solver/ScheduleSolver.cpp#L1696-L1734)


### R6. Limite diário de minutos por professor

Para todo professor $p$ que possui carga máxima diária $M_{p,d} > 0$ cadastrada:

  

$$

m_{p,d,s} = \sum_{(a,c) \in \mathcal{X}_{p,d,s}} \ell_a\, x_{a,c} \le M_{p,d} \qquad \forall p, d, s.

$$

- **Referência na literatura:** `LimitBusyTimesConstraint` / `LimitWorkloadConstraint` no padrão XHSTT [[Post et al., 2012](#16-referências-bibliográficas); [Santos et al., 2012](#16-referências-bibliográficas)].
- **Implementação no código:**
  - CP-SAT: [`ScheduleSolver.cpp` (L420–L441)](src/solver/ScheduleSolver.cpp#L420-L441)
  - MILP: [`ScheduleSolver.cpp` (L1836–L1860)](src/solver/ScheduleSolver.cpp#L1836-L1860)

  

### R7. Aulas fixadas manualmente pelo usuário (Pinned)

Quando o usuário trava uma aula $a$ em um candidato específico $\bar{c}_a$ (ou num subconjunto $\mathcal{C}_a^{fix} \subseteq \mathcal{C}_a$ com dia e horário fixos):

  

$$

\sum_{c \in \mathcal{C}_a^{fix}} x_{a,c} = 1.

$$

  

*(No código, quando a aula está fixada, $\mathcal{C}_a$ já é gerado contendo apenas o candidato fixado, e R1 garante $x_{a,\bar{c}_a} = 1$).*

- **Referência na literatura:** *Preassigned Events* / `PreferTimesConstraint` (Hard) [[Schaerf, 1999](#16-referências-bibliográficas); [Post et al., 2012](#16-referências-bibliográficas)].
- **Implementação no código:**
  - Pré-processamento (`aula.isPinned()`): [`Preprocessor.cpp` (L338–L351)](src/solver/Preprocessor.cpp#L338-L351)
  - Priorização na Heurística Gulosa: [`ScheduleSolver.cpp` (L1031)](src/solver/ScheduleSolver.cpp#L1031)

  

### R8. Sincronização de disciplinas anuais entre semestres

Para qualquer aula anual $a$, $\mathcal{S}_a = \{1, 2\}$ e a **mesma variável** $x_{a,c}$ entra nas restrições do 1º e do 2º semestre. Isso garante que a aula anual ocorra exatamente no mesmo dia, horário e sala durante o ano inteiro.

- **Referência na literatura:** Acoplamento multi-período / multi-semestre por compartilhamento de variáveis de decisão [[Daskalaki et al., 2004](#16-referências-bibliográficas)].
- **Implementação no código:**
  - CP-SAT (`in_s1` e `in_s2` compartilhando a mesma `BoolVar`): [`ScheduleSolver.cpp` (L180–L243)](src/solver/ScheduleSolver.cpp#L180-L243)
  - MILP (`in_s1` e `in_s2` compartilhando a mesma coluna `col`): [`ScheduleSolver.cpp` (L1616–L1694)](src/solver/ScheduleSolver.cpp#L1616-L1694)

  

### R9. Primeira aula do dia deve começar no início da jornada

Se uma turma tiver aulas em um dia letivo $d$, a primeira aula do dia deve obrigatoriamente começar no horário de abertura da jornada ($T^{ini}_{g,d}$). Seja $B_{g,d,s}$ a soma dos candidatos que começam exatamente em $T^{ini}_{g,d}$:

  

$$

B_{g,d,s} = \sum_{a \in \mathcal{A}:\, g \in \mathcal{G}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a:\, d_c = d,\ t_c = T^{ini}_{g,d}} x_{a,c}.

$$

  

Qualquer candidato que comece mais tarde ($t_c > T^{ini}_{g,d}$) só pode ser escolhido se $B_{g,d,s} = 1$:

  

$$

x_{a,c} \le B_{g,d,s} \qquad \forall a \in \mathcal{A} \text{ com } g \in \mathcal{G}_a,\ s \in \mathcal{S}_a,\ \forall c \in \mathcal{C}_a \text{ com } d_c = d \text{ e } t_c > T^{ini}_{g,d}.

$$

- **Referência na literatura:** Restrições de eliminação de aulas vagas no início do turno e quebra de simetria temporal em horários escolares [[Santos et al., 2012](#16-referências-bibliográficas); [Dorneles et al., 2014](#16-referências-bibliográficas)].
- **Implementação no código:**
  - CP-SAT (`add_turma_inicio_obrigatorio`): [`ScheduleSolver.cpp` (L381–L418)](src/solver/ScheduleSolver.cpp#L381-L418)
  - MILP (`add_turma_inicio_obrigatorio`): [`ScheduleSolver.cpp` (L1793–L1834)](src/solver/ScheduleSolver.cpp#L1793-L1834)
  - Heurística Gulosa: [`ScheduleSolver.cpp` (L1129–L1151)](src/solver/ScheduleSolver.cpp#L1129-L1151)

### R10. Preenchimento integral da jornada 

Quando ativada, exige que todos os slots letivos $\mathcal{F}_{g,d}$ da turma estejam 100% preenchidos com aulas (sem horários vagos na jornada):

  

$$

o^G_{g,d,k,s} = 1 \qquad \forall g \in \mathcal{G},\ \forall d \in \mathcal{D}_g,\ \forall k \in \mathcal{F}_{g,d},\ \forall s \in \mathcal{S}.

$$

- **Referência na literatura:** *Compact Class Schedules* / *Complete Class Timetables* em escolas de ensino básico e médio [[Schaerf, 1999](#16-referências-bibliográficas); [Santos et al., 2012](#16-referências-bibliográficas)].
- **Implementação no código:**
  - Validação prévia de minutos letivos: [`Preprocessor.cpp` (L155–L184)](src/solver/Preprocessor.cpp#L155-L184)
  - CP-SAT (`add_preenchimento_integral`): [`ScheduleSolver.cpp` (L443–L474)](src/solver/ScheduleSolver.cpp#L443-L474)
  - MILP (`add_preenchimento_integral`): [`ScheduleSolver.cpp` (L1862–L1893)](src/solver/ScheduleSolver.cpp#L1862-L1893)

### R11. Limite máximo de aulas da mesma disciplina por dia 

Para evitar excesso da mesma matéria num único dia, limita-se a quantidade de ocorrências da disciplina $q$ na turma $g$ e dia $d$ ao teto $U_{q,g}$:

  

$$

\sum_{a \in \mathcal{A}_q:\, g \in \mathcal{G}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a:\, d_c = d} x_{a,c} \le U_{q,g} \qquad \forall q, g, d, s.

$$

- **Referência na literatura:** `DistributeSplitEventsConstraint` / `SpreadEventsConstraint` no padrão XHSTT [[Post et al., 2012](#16-referências-bibliográficas)] e controle de dispersão diária [[Dorneles et al., 2014](#16-referências-bibliográficas)].
- **Implementação no código:**
  - CP-SAT: [`ScheduleSolver.cpp` (L476–L499)](src/solver/ScheduleSolver.cpp#L476-L499)
  - MILP: [`ScheduleSolver.cpp` (L1895–L1921)](src/solver/ScheduleSolver.cpp#L1895-L1921)
  - Heurística Gulosa: [`ScheduleSolver.cpp` (L1107–L1115)](src/solver/ScheduleSolver.cpp#L1107-L1115)

  

### R12. Aulas consecutivas / geminadas

  

Aqui é importante distinguir **o que está implementado hoje no código** da **formulação exata de adjacência em MILP**:

- **Referência na literatura:**
  - Agrupamento por dia ativo ($w_{q,d}$) e aulas geminadas (*Double Lessons*): [[Santos et al., 2012](#16-referências-bibliográficas); [Dorneles et al., 2014](#16-referências-bibliográficas)].
  - Formulação por enumeração de padrões/configurações de blocos consecutivos ($\beta_{q,f}$): [[Qualizza & Serafini, 2005](#16-referências-bibliográficas); [Daskalaki et al., 2004](#16-referências-bibliográficas)].
- **Implementação no código (versão por agrupamento diário $w_{q,d}$):**
  - CP-SAT: [`ScheduleSolver.cpp` (L501–L573)](src/solver/ScheduleSolver.cpp#L501-L573)
  - MILP: [`ScheduleSolver.cpp` (L1923–L1955)](src/solver/ScheduleSolver.cpp#L1923-L1955)
  - Heurística Gulosa: [`ScheduleSolver.cpp` (L1119–L1127)](src/solver/ScheduleSolver.cpp#L1119-L1127)

   

#### Como garantir consecutividade real no modelo MILP (Formulação Exata)

Para garantir que as aulas de um bloco $\mathcal{A}_{q,j} = (a_1, \ldots, a_n)$ ocorram **seguidas e sem intervalo entre elas**, enumeram-se previamente as configurações válidas $f \in \mathcal{F}_q$ do bloco no mesmo dia $d_f$, tais que o início de cada aula coincida com o término da anterior:

  

$$

t_{c_f(a_{i+1})} = t_{c_f(a_i)} + L_{a_i} \qquad \forall i = 1, \ldots, n-1.

$$

  

Cria-se uma variável binária $\beta_{q,f} \in \{0,1\}$ para cada configuração de bloco e impõe-se:

  

$$

\sum_{f \in \mathcal{F}_q:\, j(f) = j} \beta_{q,f} = 1 \quad \forall j, \qquad

x_{a,c} = \sum_{f \in \mathcal{F}_q:\, j(f) = j,\ c_f(a) = c} \beta_{q,f} \quad \forall a \in \mathcal{A}_{q,j},\ \forall c \in \mathcal{C}_a,

$$

  

$$

\sum_{f \in \mathcal{F}_q:\, d_f = d} \beta_{q,f} = w_{q,d} \quad \forall d \in \mathcal{D}.

$$

  

---

  

## 10. Indicadores de Dias e Turnos Trabalhados pelos Professores

  

Para saber se o professor $p$ trabalha no dia $d$ ($y_{p,d,s} = 1$) ou no turno $h$ do dia $d$ ($v_{p,d,h,s} = 1$), vinculamos esses indicadores às variáveis $x_{a,c}$:

  

$$

x_{a,c} \le y_{p,d,s} \quad \forall (a,c) \in \mathcal{X}_{p,d,s}, \qquad

y_{p,d,s} \le \sum_{(a,c) \in \mathcal{X}_{p,d,s}} x_{a,c}.

$$

  

$$

x_{a,c} \le v_{p,d,h,s} \quad \forall (a,c) \in \mathcal{X}_{p,d,h,s}, \qquad

v_{p,d,h,s} \le \sum_{(a,c) \in \mathcal{X}_{p,d,h,s}} x_{a,c}.

$$

  

Assim, basta uma única aula ser alocada para forçar o indicador a $1$; se nenhuma aula for alocada no dia/turno, a segunda desigualdade força o indicador a $0$.

- **Referência na literatura:** Variáveis indicadoras de dia ativo do professor (`ClusterBusyTimesConstraint` no XHSTT) [[Santos et al., 2012](#16-referências-bibliográficas); [Dorneles et al., 2014](#16-referências-bibliográficas); [Post et al., 2012](#16-referências-bibliográficas)].
- **Implementação no código:**
  - Indicador de dia trabalhado ($y_{p,d}$): CP-SAT em [`ScheduleSolver.cpp` (L587–L603)](src/solver/ScheduleSolver.cpp#L587-L603) e MILP em [`ScheduleSolver.cpp` (L1959–L1995)](src/solver/ScheduleSolver.cpp#L1959-L1995).
  - Indicador de turno trabalhado ($v_{p,d,h}$ / `y_shift`): CP-SAT em [`ScheduleSolver.cpp` (L652–L664)](src/solver/ScheduleSolver.cpp#L652-L664) e MILP em [`ScheduleSolver.cpp` (L2015–L2034)](src/solver/ScheduleSolver.cpp#L2015-L2034).

  

---

  

## 11. Cálculo Linear de Janelas (Horários Vagos entre Aulas)

  

Uma **janela** é um slot vazio situado **entre** duas aulas no mesmo período. Horários livres antes da primeira aula ou depois da última aula **não** são janelas.

  

- **Para professores:** As janelas são medidas **dentro de cada turno** $i = (p,d,h,s)$, nos slots $\mathcal{E}_i = \mathcal{K}_{d,h}$. Assim, o intervalo entre o fim do turno da manhã e o início do turno da tarde/noite não é penalizado como janela.

- **Para turmas:** As janelas (lacunas) são medidas nos slots letivos da jornada $i = (g,d,s)$, $\mathcal{E}_i = \mathcal{F}_{g,d}$ (excluindo os recreios obrigatórios).

  

Para cada slot $k \in \mathcal{E}_i$, queremos que a variável $j_{i,k}$ valha $1$ se, e somente se:

1. Houver alguma aula **antes** de $k$ ($u^-_{i,k} = 1$);

2. Houver alguma aula **depois** de $k$ ($u^+_{i,k} = 1$); e

3. O slot $k$ estiver **vazio** ($o_{i,k} = 0$).

  

Isso é linearizado pelas inequações:

  

$$

u^-_{i,k} \ge o_{i,l} \quad \forall l \in \mathcal{E}_i \text{ com } l < k, \qquad

u^-_{i,k} \le \sum_{l \in \mathcal{E}_i:\, l < k} o_{i,l},

$$

  

$$

u^+_{i,k} \ge o_{i,l} \quad \forall l \in \mathcal{E}_i \text{ com } l > k, \qquad

u^+_{i,k} \le \sum_{l \in \mathcal{E}_i:\, l > k} o_{i,l},

$$

  

$$

j_{i,k} \ge u^-_{i,k} + u^+_{i,k} - o_{i,k} - 1, \qquad

j_{i,k} \le u^-_{i,k}, \qquad j_{i,k} \le u^+_{i,k}, \qquad j_{i,k} \le 1 - o_{i,k}.

$$

- **Referência na literatura:** Detecção linear de janelas/buracos internos (*Idle Times* / `LimitIdleTimesConstraint`) via variáveis binárias de precedência e sucessão [[Burke et al., 2010](#16-referências-bibliográficas); [Post et al., 2012](#16-referências-bibliográficas); [Fonseca, Santos & Carrano, 2016](#16-referências-bibliográficas)].
- **Implementação no código:**
  - **Janelas de professores ($J$):** CP-SAT em [`ScheduleSolver.cpp` (L667–L710)](src/solver/ScheduleSolver.cpp#L667-L710) e MILP (com encadeamento monotônico $u_{k-1} \le u_k$ e $v_{k+1} \le v_k$) em [`ScheduleSolver.cpp` (L2036–L2124)](src/solver/ScheduleSolver.cpp#L2036-L2124).
  - **Lacunas de turmas ($J_G$):** CP-SAT em [`ScheduleSolver.cpp` (L712–L755)](src/solver/ScheduleSolver.cpp#L712-L755) e MILP em [`ScheduleSolver.cpp` (L2126–L2215)](src/solver/ScheduleSolver.cpp#L2126-L2215).

---

  

## 12. Função Objetivo (Critérios de Qualidade)

  

O objetivo é minimizar a soma ponderada das penalidades de qualidade da grade:

  

### 12.1. Concentração de dias dos professores ($D_{prof}$)

Soma quantos dias na semana cada professor precisa ir à instituição:

  

$$

D_{prof} = \sum_{s \in \mathcal{S}} \rho_s \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} y_{p,d,s}.

$$

- **Referência:** Compactação da jornada docente em menos dias (`ClusterBusyTimesConstraint`) [[Santos et al., 2012](#16-referências-bibliográficas); [Dorneles et al., 2014](#16-referências-bibliográficas); [Souza, Ochi & Maculan, 2003](#16-referências-bibliográficas)].
- **Código:** CP-SAT em [`ScheduleSolver.cpp` (L605–L606)](src/solver/ScheduleSolver.cpp#L605-L606); MILP em [`ScheduleSolver.cpp` (L1968)](src/solver/ScheduleSolver.cpp#L1968).

  

### 12.2. Preferência de horários ($P_{hora}$)

Penaliza aulas alocadas em horários menos desejáveis (pré-calculados em $c^{hora}_{a,c}$):

  

$$

P_{hora} = \sum_{a \in \mathcal{A}} \sum_{c \in \mathcal{C}_a} \left(\sum_{s \in \mathcal{S}_a} \rho_s\right) c^{hora}_{a,c}\, x_{a,c}.

$$

- **Referência:** Custos de preferência de horários (`PreferTimesConstraint` — Soft) [[Schaerf, 1999](#16-referências-bibliográficas); [Post et al., 2012](#16-referências-bibliográficas)].
- **Código:** Cálculo de `custo_preferencia` em [`Preprocessor.cpp` (L421–L430)](src/solver/Preprocessor.cpp#L421-L430); CP-SAT em [`ScheduleSolver.cpp` (L624–L629)](src/solver/ScheduleSolver.cpp#L624-L629); MILP em [`ScheduleSolver.cpp` (L1656–L1658)](src/solver/ScheduleSolver.cpp#L1656-L1658).

  

### 12.3. Preferência de dias dos professores ($W$)

Penaliza alocar aulas em dias nos quais o professor tem nota de preferência $\omega_{p,d} < 10$. Possui dois modos configuráveis :

- **Por dia trabalhado :** cobra $(10 - \omega_{p,d})$ uma vez se o professor trabalhar no dia:

$$W_{dia} = \sum_{s \in \mathcal{S}} \rho_s \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} (10 - \omega_{p,d})\, y_{p,d,s}.$$

- **Por minuto de aula :** cobra proporcionalmente aos minutos de aula no dia:

$$W_{min} = \sum_{s \in \mathcal{S}} \rho_s \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} (10 - \omega_{p,d})\, m_{p,d,s}.$$

- **Referência:** Ponderação de preferências docentes por dia da semana [ [Santos et al., 2012](#16-referências-bibliográficas)].
- **Código:** CP-SAT em [`ScheduleSolver.cpp` (L608–L619)](src/solver/ScheduleSolver.cpp#L608-L619); MILP em [`ScheduleSolver.cpp` (L1659–L1667 e L1969–L1971)](src/solver/ScheduleSolver.cpp#L1659-L1667).

  

### 12.4. Turnos pretendidos pelos professores ($T_{prof}$)

Penaliza quando um professor precisa dar aula em um turno no qual não pretendia trabalhar ($\pi_{p,d,h} = 0$):

  

$$

T_{prof} = \sum_{s \in \mathcal{S}} \rho_s \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} \sum_{h \in \mathcal{H}} (1 - \pi_{p,d,h})\, v_{p,d,h,s}.

$$


- **Código:** CP-SAT em [`ScheduleSolver.cpp` (L651–L665)](src/solver/ScheduleSolver.cpp#L651-L665); MILP em [`ScheduleSolver.cpp` (L2015–L2034)](src/solver/ScheduleSolver.cpp#L2015-L2034).

  

### 12.5. Janelas de professores ($J$) e lacunas de turmas ($J_G$)

Soma os slots de janelas dos professores (ponderados por $\alpha_p = \text{pesoJanela}_p$) e os slots de lacunas das turmas:

  

$$

J_{slots} = \sum_{s \in \mathcal{S}} \rho_s \sum_{p,d,h} \alpha_p \sum_{k \in \mathcal{K}_{d,h}} j_{(p,d,h,s),k}, \qquad

J_{G,slots} = \sum_{s \in \mathcal{S}} \rho_s \sum_{g \in \mathcal{G}} \sum_{d \in \mathcal{D}_g} \sum_{k \in \mathcal{F}_{g,d}} j_{(g,d,s),k}.

$$

- **Referência:** `LimitIdleTimesConstraint` para docentes e turmas [[Post et al., 2012](#16-referências-bibliográficas); [Burke et al., 2010](#16-referências-bibliográficas)].
- **Código:** CP-SAT em [`ScheduleSolver.cpp` (L667–L755)](src/solver/ScheduleSolver.cpp#L667-L755); MILP em [`ScheduleSolver.cpp` (L2036–L2215)](src/solver/ScheduleSolver.cpp#L2036-L2215).

  

### 12.6. Expressão Global da Função Objetivo

No código implementado ([`ScheduleSolver.cpp` (L758–L766)](src/solver/ScheduleSolver.cpp#L758-L766) para CP-SAT e [`ScheduleSolver.cpp` (L1655–L2215)](src/solver/ScheduleSolver.cpp#L1655-L2215) para MILP), a função objetivo minimizada pelo solver é exatamente:

  

$$

\boxed{\min Z = \lambda_J\, J_{slots} + \lambda_D\, D_{prof} + \lambda_P\, P_{hora} + \lambda_W\, W + 10\,\lambda_T\, T_{prof} + 5\,\lambda_G\, J_{G,slots}}

$$

  

onde os pesos correspondem aos campos de `OptimizationConfig`:
- $\lambda_J$: `lambda_janelas`
- $\lambda_D$: `lambda_dias`
- $\lambda_P$: `lambda_preferencias`
- $\lambda_W$: `lambda_preferencia_dias`
- $\lambda_T$: `lambda_preferencia_turnos` (multiplicado por $10$ no código para equilibrar a escala)
- $\lambda_G$: `lambda_lacunas_turma` (multiplicado por $\Delta = 5$ no código para converter slots em minutos)



## 13. Exemplo Prático de Slots, Intervalos e Janelas

  

Suponha que os slots sejam de 5 minutos desde `00:00`:

- **Aula 1 (45 min = 9 slots):** começa às `07:00` (slot 84) e ocupa $[84, 93)$, terminando às `07:45` (slot 93).

- **Aula 2 (50 min = 10 slots):** começa às `07:45` (slot 93) e ocupa $[93, 103)$, terminando às `08:35` (slot 103).

- Como a Aula 2 começa exatamente no slot em que a Aula 1 termina ($93 = 84 + 9$), elas são **consecutivas** e têm **0 minutos de janela**.

- Se a Aula 2 começasse às `08:00` (slot 96), os slots $93, 94, 95$ ficariam vagos entre as duas aulas, gerando uma **janela de 3 slots (15 minutos)**.

- Se a turma tiver um intervalo obrigatório das `08:20` às `08:35` (slots $[100, 103)$), a Aula 2 começando às `07:45` é **eliminada no pré-processamento**, pois seu intervalo $[93, 103)$ invadiria o recreio da turma.

  

---

  

## 14. Resumo Completo do Modelo de Otimização (Formulação Integrada)

  

Abaixo está o modelo matemático completo reunido em um único bloco (função objetivo, expressões auxiliares, restrições obrigatórias, restrições opcionais, indicadores, janelas e domínios das variáveis):

  

### Função Objetivo

  

$$

\min Z = \lambda_J \sum_{s \in \mathcal{S}} \rho_s \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} \sum_{h \in \mathcal{H}} \alpha_p \sum_{k \in \mathcal{K}_{d,h}} j_{(p,d,h,s),k}

+ \lambda_D \sum_{s \in \mathcal{S}} \rho_s \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} y_{p,d,s}

+ \lambda_P \sum_{a \in \mathcal{A}} \sum_{c \in \mathcal{C}_a} \left(\sum_{s \in \mathcal{S}_a} \rho_s\right) c^{hora}_{a,c}\, x_{a,c}

+ \lambda_W W

+ 10\,\lambda_T \sum_{s \in \mathcal{S}} \rho_s \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} \sum_{h \in \mathcal{H}} (1 - \pi_{p,d,h})\, v_{p,d,h,s}

+ 5\,\lambda_G \sum_{s \in \mathcal{S}} \rho_s \sum_{g \in \mathcal{G}} \sum_{d \in \mathcal{D}_g} \sum_{k \in \mathcal{F}_{g,d}} j_{(g,d,s),k}

+ \lambda_E \sum_{a \in \mathcal{A}^{ant}} e_a

+ \lambda_R \sum_{a \in \mathcal{A}} \sum_{c \in \mathcal{C}_a} \left(\sum_{s \in \mathcal{S}_a} \rho_s\right) c^{sala}_{a,c}\, x_{a,c}

$$

  

onde o termo de preferência de dias $W$ é escolhido conforme a configuração:

  

$$

W = \begin{cases}

\displaystyle \sum_{s \in \mathcal{S}} \rho_s \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} (10 - \omega_{p,d})\, y_{p,d,s}, & \text{(modo por dia trabalhado)} \\[10pt]

\displaystyle \sum_{s \in \mathcal{S}} \rho_s \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} (10 - \omega_{p,d})\, m_{p,d,s}, & \text{(modo por minuto de aula)}

\end{cases}

$$

  

---

  

### Sujeito às Restrições

  

#### 1. Expressões auxiliares de ocupação e carga diária

$$

\begin{aligned}

o^P_{p,d,k,s} &= \sum_{a \in \mathcal{A}:\, p \in \mathcal{P}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,k}\, x_{a,c} && \forall p \in \mathcal{P},\ d \in \mathcal{D},\ k \in \mathcal{K}_d,\ s \in \mathcal{S} \\

o^G_{g,d,k,s} &= \sum_{a \in \mathcal{A}:\, g \in \mathcal{G}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,k}\, x_{a,c} && \forall g \in \mathcal{G},\ d \in \mathcal{D},\ k \in \mathcal{K}_d,\ s \in \mathcal{S} \\

m_{p,d,s} &= \sum_{(a,c) \in \mathcal{X}_{p,d,s}} \ell_a\, x_{a,c} && \forall p \in \mathcal{P},\ d \in \mathcal{D},\ s \in \mathcal{S}

\end{aligned}

$$

  

#### 2. Restrições obrigatórias (R1 a R9)

$$

\begin{aligned}

\text{(R1) Alocação única:} \quad & \sum_{c \in \mathcal{C}_a} x_{a,c} = 1 && \forall a \in \mathcal{A} \\[6pt]

\text{(R2) Não sobreposição de professores:} \quad & o^P_{p,d,k,s} \le 1 && \forall p \in \mathcal{P},\ d \in \mathcal{D},\ k \in \mathcal{K}_d,\ s \in \mathcal{S} \\[6pt]

\text{(R3) Não sobreposição de turmas:} \quad & o^G_{g,d,k,s} \le 1 && \forall g \in \mathcal{G},\ d \in \mathcal{D},\ k \in \mathcal{K}_d,\ s \in \mathcal{S} \\[6pt]

\text{(R4) Não sobreposição de salas:} \quad & \sum_{a \in \mathcal{A}:\, s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a:\, r \in b_c} H_{a,c,d,k}\, x_{a,c} \le 1 && \forall r \in \mathcal{R},\ d \in \mathcal{D},\ k \in \mathcal{K}_d,\ s \in \mathcal{S} \\[6pt]

\text{(R5a) Escolha da sala teórica fixa:} \quad & \sum_{r \in \mathcal{R}_g^{teo}} z_{g,r} = 1 && \forall g \in \mathcal{G} \\[6pt]

\text{(R5b) Vínculo da aula teórica à sala fixa:} \quad & x_{a,c} \le z_{g,r} && \forall a \in \mathcal{A}^{teo},\ c \in \mathcal{C}_a \text{ com } b_c=\{r\},\ g \in \mathcal{G}_a \\[6pt]

\text{(R5c) Sala teórica predefinida:} \quad & z_{g,\bar{r}_g} = 1 && \forall g \in \mathcal{G} \text{ com sala predefinida } \bar{r}_g \\[6pt]

\text{(R6) Carga máxima diária do professor:} \quad & m_{p,d,s} \le M_{p,d} && \forall p \in \mathcal{P} \text{ (com } M_{p,d}>0\text{)},\ d \in \mathcal{D},\ s \in \mathcal{S} \\[6pt]

\text{(R7) Fixação manual (Pinned):} \quad & \sum_{c \in \mathcal{C}_a^{fix}} x_{a,c} = 1 && \forall a \in \mathcal{A}^{fix} \\[6pt]

\text{(R8) Sincronização de aulas anuais:} \quad & \mathcal{S}_a = \mathcal{S} \text{ (mesma variável } x_{a,c}\text{)} && \forall a \in \mathcal{A}^{anual} \\[6pt]

\text{(R9) Início obrigatório na abertura da jornada:} \quad & x_{a,c} \le \sum_{\substack{a' \in \mathcal{A}:\, g \in \mathcal{G}_{a'},\, s \in \mathcal{S}_{a'} \\ c' \in \mathcal{C}_{a'}:\, d_{c'}=d,\, t_{c'}=T^{ini}_{g,d}}} x_{a',c'} && \forall g \in \mathcal{G},\ d \in \mathcal{D}_g,\ s \in \mathcal{S},\ \forall (a,c) \text{ com } g \in \mathcal{G}_a,\ s \in \mathcal{S}_a,\ d_c=d,\ t_c > T^{ini}_{g,d}

\end{aligned}

$$

  

#### 3. Restrições opcionais (R10 a R12 — incluídas quando ativadas)

$$

\begin{aligned}

\text{(R10) Preenchimento integral da jornada:} \quad & o^G_{g,d,k,s} = 1 && \forall g \in \mathcal{G},\ d \in \mathcal{D}_g,\ k \in \mathcal{F}_{g,d},\ s \in \mathcal{S} \\[6pt]

\text{(R11) Máximo diário da mesma disciplina:} \quad & \sum_{a \in \mathcal{A}_q:\, g \in \mathcal{G}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a:\, d_c = d} x_{a,c} \le U_{q,g} && \forall q \in \mathcal{Q},\ g \in \mathcal{G},\ d \in \mathcal{D},\ s \in \mathcal{S} \\[6pt]

\text{(R12a) Escolha de configuração de bloco:} \quad & \sum_{f \in \mathcal{F}_q:\, j(f) = j} \beta_{q,f} = 1 && \forall q \in \mathcal{Q}^{consec},\ \forall \text{bloco } j \\[6pt]

\text{(R12b) Acoplamento candidato–bloco:} \quad & x_{a,c} = \sum_{f \in \mathcal{F}_q:\, j(f) = j,\ c_f(a) = c} \beta_{q,f} && \forall q \in \mathcal{Q}^{consec},\ \forall j,\ \forall a \in \mathcal{A}_{q,j},\ \forall c \in \mathcal{C}_a \\[6pt]

\text{(R12c) No máximo um bloco por dia:} \quad & \sum_{f \in \mathcal{F}_q:\, d_f = d} \beta_{q,f} = w_{q,d} && \forall q \in \mathcal{Q}^{consec},\ \forall d \in \mathcal{D} \\[6pt]

\text{(R12 — Versão simplificada do código):} \quad & \sum_{a \in \mathcal{A}_q} \sum_{c \in \mathcal{C}_a:\, d_c = d} x_{a,c} = B_q\, w_{q,d},\quad \sum_{d \in \mathcal{D}} w_{q,d} = N_q && \forall q \in \mathcal{Q}^{consec},\ d \in \mathcal{D} \quad \big((B_q, N_q) \in \{(2,1), (4,1), (4,2)\}\big)

\end{aligned}

$$

  

#### 4. Indicadores de dias trabalhados, turnos trabalhados e estabilidade

$$

\begin{aligned}

x_{a,c} \le y_{p,d,s}, \quad y_{p,d,s} &\le \sum_{(a',c') \in \mathcal{X}_{p,d,s}} x_{a',c'} && \forall p \in \mathcal{P},\ d \in \mathcal{D},\ s \in \mathcal{S},\ \forall (a,c) \in \mathcal{X}_{p,d,s} \\[6pt]

x_{a,c} \le v_{p,d,h,s}, \quad v_{p,d,h,s} &\le \sum_{(a',c') \in \mathcal{X}_{p,d,h,s}} x_{a',c'} && \forall p \in \mathcal{P},\ d \in \mathcal{D},\ h \in \mathcal{H},\ s \in \mathcal{S},\ \forall (a,c) \in \mathcal{X}_{p,d,h,s} \\[6pt]

e_a &= 1 - \sum_{c \in \mathcal{C}_a^{ant}} x_{a,c} && \forall a \in \mathcal{A}^{ant}

\end{aligned}

$$

  

#### 5. Linearização de janelas (para cada sequência $i = (p,d,h,s)$ com $\mathcal{E}_i = \mathcal{K}_{d,h}$ e $i = (g,d,s)$ com $\mathcal{E}_i = \mathcal{F}_{g,d}$)

$$

\begin{aligned}

u^-_{i,k} &\ge o_{i,l} && \forall k \in \mathcal{E}_i,\ \forall l \in \mathcal{E}_i \text{ com } l < k \\

u^-_{i,k} &\le \sum_{l \in \mathcal{E}_i:\, l < k} o_{i,l} && \forall k \in \mathcal{E}_i \\

u^+_{i,k} &\ge o_{i,l} && \forall k \in \mathcal{E}_i,\ \forall l \in \mathcal{E}_i \text{ com } l > k \\

u^+_{i,k} &\le \sum_{l \in \mathcal{E}_i:\, l > k} o_{i,l} && \forall k \in \mathcal{E}_i \\

j_{i,k} &\ge u^-_{i,k} + u^+_{i,k} - o_{i,k} - 1 && \forall k \in \mathcal{E}_i \\

j_{i,k} &\le u^-_{i,k},\quad j_{i,k} \le u^+_{i,k},\quad j_{i,k} \le 1 - o_{i,k} && \forall k \in \mathcal{E}_i

\end{aligned}

$$

  

#### 6. Domínio das variáveis

$$

\begin{aligned}

x_{a,c} &\in \{0,1\} && \forall a \in \mathcal{A},\ c \in \mathcal{C}_a \\

z_{g,r} &\in \{0,1\} && \forall g \in \mathcal{G},\ r \in \mathcal{R}_g^{teo} \\

y_{p,d,s} &\in \{0,1\} && \forall p \in \mathcal{P},\ d \in \mathcal{D},\ s \in \mathcal{S} \\

v_{p,d,h,s} &\in \{0,1\} && \forall p \in \mathcal{P},\ d \in \mathcal{D},\ h \in \mathcal{H},\ s \in \mathcal{S} \\

w_{q,d} &\in \{0,1\} && \forall q \in \mathcal{Q}^{consec},\ d \in \mathcal{D} \\

\beta_{q,f} &\in \{0,1\} && \forall q \in \mathcal{Q}^{consec},\ f \in \mathcal{F}_q \\

u^-_{i,k},\ u^+_{i,k} &\in \{0,1\} && \forall i,\ \forall k \in \mathcal{E}_i \\

o^P_{p,d,k,s},\ o^G_{g,d,k,s},\ j_{i,k},\ e_a &\in [0,1] && \forall p, g, d, k, s, i, a \\

m_{p,d,s} &\ge 0 && \forall p \in \mathcal{P},\ d \in \mathcal{D},\ s \in \mathcal{S}

\end{aligned}

$$

---

## 15. Matriz de Rastreabilidade (Modelo $\leftrightarrow$ Literatura $\leftrightarrow$ Código-Fonte)

A tabela abaixo consolida a origem teórica na literatura científica e a localização exata da implementação de cada componente do modelo matemático nos arquivos do projeto:

| Elemento / Restrição | Padrão XHSTT / Literatura Base | Implementação no Pré-processador (`Preprocessor.cpp`) | Implementação CP-SAT (`ScheduleSolver.cpp`) | Implementação MILP — SCIP, HiGHS e cuOpt (`ScheduleSolver.cpp`) |
| :--- | :--- | :--- | :--- | :--- |
| **Discretização em slots ($\Delta = 5\text{ min}$)** | *Time-Indexed Formulation* [[Sousa & Wolsey, 1992](#16-referências-bibliográficas); [Daskalaki et al., 2004](#16-referências-bibliográficas)] | [`TimeInterval.h` (L16–L35)](include/model/TimeInterval.h#L16-L35) | [`ScheduleSolver.cpp` (L215–L220)](src/solver/ScheduleSolver.cpp#L215-L220) | [`ScheduleSolver.cpp` (L1652–L1672)](src/solver/ScheduleSolver.cpp#L1652-L1672) |
| **Filtro $\mathcal{C}_a$ — Jornada e intervalos** | Pré-filtragem de domínio [[Schaerf, 1999](#16-referências-bibliográficas)] | [`Preprocessor.cpp` (L199–L214, L361–L396)](src/solver/Preprocessor.cpp#L199-L214) | Implícito em $\mathcal{C}_a$ | Implícito em $\mathcal{C}_a$ |
| **Filtro $\mathcal{C}_a$ — Disponibilidade docente** | `PreferTimesConstraint` (Hard) [[Post et al., 2012](#16-referências-bibliográficas)] | [`Preprocessor.cpp` (L410–L419)](src/solver/Preprocessor.cpp#L410-L419) | Implícito em $\mathcal{C}_a$ | Implícito em $\mathcal{C}_a$ |
| **Filtro $\mathcal{C}_a$ — Turnos e Salas** | `AssignResourceConstraint` [[Daskalaki et al., 2004](#16-referências-bibliográficas)] | [`Preprocessor.cpp` (L223–L240, L302–L336)](src/solver/Preprocessor.cpp#L223-L240) | Implícito em $\mathcal{C}_a$ | Implícito em $\mathcal{C}_a$ |
| **R1. Alocação única de cada aula** | `AssignTimeConstraint` [[Post et al., 2012](#16-referências-bibliográficas); [Schaerf, 1999](#16-referências-bibliográficas)] | — | [`L282–L290`](src/solver/ScheduleSolver.cpp#L282-L290) | [`L1736–L1745`](src/solver/ScheduleSolver.cpp#L1736-L1745) |
| **R2. Não sobreposição de professores** | `AvoidClashesConstraint` [[Post et al., 2012](#16-referências-bibliográficas); [Laborie & Rogerie, 2008](#16-referências-bibliográficas)] | — | [`L292–L321`](src/solver/ScheduleSolver.cpp#L292-L321) (`AddNoOverlap`) | [`L1747–L1791`](src/solver/ScheduleSolver.cpp#L1747-L1791) (Cliques por slot) |
| **R3. Não sobreposição de turmas** | `AvoidClashesConstraint` [[Post et al., 2012](#16-referências-bibliográficas); [Santos et al., 2012](#16-referências-bibliográficas)] | — | [`L352–L379`](src/solver/ScheduleSolver.cpp#L352-L379) (`AddNoOverlap`) | [`L1747–L1791`](src/solver/ScheduleSolver.cpp#L1747-L1791) (Cliques por slot) |
| **R4. Não sobreposição de salas** | `AvoidClashesConstraint` [[Post et al., 2012](#16-referências-bibliográficas); [Daskalaki et al., 2004](#16-referências-bibliográficas)] | — | [`L323–L350`](src/solver/ScheduleSolver.cpp#L323-L350) (`AddNoOverlap`) | [`L1747–L1791`](src/solver/ScheduleSolver.cpp#L1747-L1791) (Cliques por slot) |
| **R5. Sala teórica fixa ($z_{g,r}$)** | *Room Stability* [[Daskalaki et al., 2004](#16-referências-bibliográficas); [Post et al., 2012](#16-referências-bibliográficas)] | [`Preprocessor.cpp` (L304–L315)](src/solver/Preprocessor.cpp#L304-L315) | [`L245–L278`](src/solver/ScheduleSolver.cpp#L245-L278) | [`L1696–L1734`](src/solver/ScheduleSolver.cpp#L1696-L1734) |
| **R6. Carga máxima diária ($M_{p,d}$)** | `LimitWorkloadConstraint` [[Post et al., 2012](#16-referências-bibliográficas); [Santos et al., 2012](#16-referências-bibliográficas)] | — | [`L420–L441`](src/solver/ScheduleSolver.cpp#L420-L441) | [`L1836–L1860`](src/solver/ScheduleSolver.cpp#L1836-L1860) |
| **R7. Aulas fixadas (*Pinned*)** | *Preassignments* [[Schaerf, 1999](#16-referências-bibliográficas); [Post et al., 2012](#16-referências-bibliográficas)] | [`Preprocessor.cpp` (L338–L351)](src/solver/Preprocessor.cpp#L338-L351) | Garantido por $\lvert\mathcal{C}_a\rvert = 1$ + R1 | Garantido por $\lvert\mathcal{C}_a\rvert = 1$ + R1 |
| **R8. Sincronização anual entre semestres** | Acoplamento multi-semestral [[Daskalaki et al., 2004](#16-referências-bibliográficas)] | — | [`L180–L243`](src/solver/ScheduleSolver.cpp#L180-L243) | [`L1616–L1694`](src/solver/ScheduleSolver.cpp#L1616-L1694) |
| **R9. Início na abertura da jornada ($T^{ini}_{g,d}$)** | Quebra de simetria / início pontual [[Santos et al., 2012](#16-referências-bibliográficas); [Dorneles et al., 2014](#16-referências-bibliográficas)] | — | [`L381–L418`](src/solver/ScheduleSolver.cpp#L381-L418) | [`L1793–L1834`](src/solver/ScheduleSolver.cpp#L1793-L1834) |
| **R10. Preenchimento integral da jornada** | *Compact Class Schedules* [[Schaerf, 1999](#16-referências-bibliográficas); [Santos et al., 2012](#16-referências-bibliográficas)] | [`Preprocessor.cpp` (L155–L184)](src/solver/Preprocessor.cpp#L155-L184) | [`L443–L474`](src/solver/ScheduleSolver.cpp#L443-L474) | [`L1862–L1893`](src/solver/ScheduleSolver.cpp#L1862-L1893) |
| **R11. Limite diário da disciplina ($U_{q,g}$)** | `DistributeSplitEventsConstraint` [[Post et al., 2012](#16-referências-bibliográficas); [Dorneles et al., 2014](#16-referências-bibliográficas)] | — | [`L476–L499`](src/solver/ScheduleSolver.cpp#L476-L499) | [`L1895–L1921`](src/solver/ScheduleSolver.cpp#L1895-L1921) |
| **R12. Aulas consecutivas / geminadas** | *Double Lessons* / Blocos [[Santos et al., 2012](#16-referências-bibliográficas); [Qualizza & Serafini, 2005](#16-referências-bibliográficas)] | — | [`L501–L573`](src/solver/ScheduleSolver.cpp#L501-L573) | [`L1923–L1955`](src/solver/ScheduleSolver.cpp#L1923-L1955) |
| **Seção 10 & 12.1. Dias trabalhados ($y_{p,d,s}, D_{prof}$)** | `ClusterBusyTimesConstraint` [[Santos et al., 2012](#16-referências-bibliográficas); [Dorneles et al., 2014](#16-referências-bibliográficas)] | — | [`L587–L606`](src/solver/ScheduleSolver.cpp#L587-L606) | [`L1959–L1995`](src/solver/ScheduleSolver.cpp#L1959-L1995) |
| **Seção 12.2. Preferência de horários ($P_{hora}$)** | `PreferTimesConstraint` (Soft) [[Post et al., 2012](#16-referências-bibliográficas)] | [`Preprocessor.cpp` (L421–L430)](src/solver/Preprocessor.cpp#L421-L430) | [`L624–L629`](src/solver/ScheduleSolver.cpp#L624-L629) | [`L1656–L1658`](src/solver/ScheduleSolver.cpp#L1656-L1658) |
| **Seção 12.3. Preferência de dias ($W_{dia}, W_{min}$)** | Preferências docentes | [`L608–L619`](src/solver/ScheduleSolver.cpp#L608-L619) | [`L1659–L1667, L1969–L1971`](src/solver/ScheduleSolver.cpp#L1659-L1667) |
| **Seção 12.4. Turnos pretendidos ($v_{p,d,h,s}, T_{prof}$)** | Preferência por turno [[Souza, Ochi & Maculan, 2003](#16-referências-bibliográficas)] | — | [`L651–L665`](src/solver/ScheduleSolver.cpp#L651-L665) | [`L2015–L2034`](src/solver/ScheduleSolver.cpp#L2015-L2034) |
| **Seção 11 & 12.5. Janelas ($J_{slots}$ e $J_{G,slots}$)** | `LimitIdleTimesConstraint` [[Burke et al., 2010](#16-referências-bibliográficas); [Post et al., 2012](#16-referências-bibliográficas)] | — | [`L667–L755`](src/solver/ScheduleSolver.cpp#L667-L755) | [`L2036–L2215`](src/solver/ScheduleSolver.cpp#L2036-L2215) |
| **Estabilidade / Warm Start ($e_a$)** | *Minimal Perturbation Problem* [[Phillips et al., 2017](#16-referências-bibliográficas)] | — | [`L768–L805`](src/solver/ScheduleSolver.cpp#L768-L805) (`AddHint`) | [`L2218–L2275`](src/solver/ScheduleSolver.cpp#L2218-L2275) (`warm_start_vals`) |

---

## 16. Referências Bibliográficas

1. **Burke, E. K., Mareček, J., Parkes, A. J., & Rudová, H. (2010).** *Decomposition, reformulation, and diving in university course timetabling.* Computers & Operations Research, 37(3), 582–597. https://doi.org/10.1016/j.cor.2009.02.023 *(Linearização em MILP de janelas ociosas e compacidade de grade horária).*
2. **Daskalaki, S., Birbas, T., & Housos, E. (2004).** *An integer programming formulation for a case study in university timetabling.* European Journal of Operational Research, 153(1), 117–135. https://doi.org/10.1016/S0377-2217(03)00103-6 *(Modelagem inteira para aulas com durações distintas, subconjuntos de salas fixas vs. laboratórios e blocos consecutivos).*
3. **Dorneles, Á. P., de Araújo, O. C. B., & Buriol, L. S. (2014).** *A fix-and-optimize heuristic for the high school timetabling problem.* Computers & Operations Research, 52, 29–38. https://doi.org/10.1016/j.cor.2014.06.023 *(Formulação MILP para o problema brasileiro de horários escolares: minimização de dias de trabalho $y_{p,d}$, janelas de professores, aulas geminadas e limite diário por disciplina).*
4. **Even, S., Itai, A., & Shamir, A. (1976).** *On the complexity of timetable and multicommodity flow problems.* SIAM Journal on Computing, 5(4), 691–703. https://doi.org/10.1137/0205048 *(Prova clássica da NP-completude/NP-dificuldade do problema de programação de horários escolares com restrições de disponibilidade docente).*
5. **Fonseca, G. H. G., Santos, H. G., & Carrano, E. G. (2016).** *Integrating matheuristics and metaheuristics for timetabling.* Computers & Operations Research, 74, 108–117. https://doi.org/10.1016/j.cor.2016.04.016 *(Modelagem híbrida MILP para o problema XHSTT / ITC 2011).*
6. **Laborie, P., & Rogerie, J. (2008).** *Reasoning with conditional time-intervals.* In *Proceedings of the Twenty-First International Florida Artificial Intelligence Research Society Conference (FLAIRS 2008)* (pp. 555–560). AAAI Press. *(Fundamentação teórica das variáveis de intervalo opcionais `OptionalIntervalVar` e propagadores `NoOverlap` usados no CP-SAT).*
7. **Perron, L., & Furnon, V. (Google).** *OR-Tools CP-SAT Solver Documentation — Employee & School Scheduling.* Google Developers. Disponível em: https://developers.google.com/optimization/scheduling *(Documentação técnica do solver CP-SAT utilizado em `ScheduleSolver::solveCpSat`).*
8. **Phillips, A. E., Waterer, H., Ehrgott, M., & Ryan, D. M. (2017).** *Integer programming for minimal perturbation problems in university course timetabling.* Annals of Operations Research, 252(2), 283–304. https://doi.org/10.1007/s10479-015-2094-z *(Modelagem de estabilidade e perturbação mínima $e_a$ na reotimização de grades horárias).*
9. **Post, G., Ahmadi, S., Daskalaki, S., Kingston, J. H., Kyngas, J., Nurmi, C., & Ranson, D. (2012).** *An XML format for benchmarks in High School Timetabling.* Annals of Operations Research, 194(1), 385–397. https://doi.org/10.1007/s10479-010-0699-9 *(Especificação oficial do padrão internacional XHSTT e catálogo canônico de restrições rígidas e flexíveis).*
10. **Qualizza, A., & Serafini, P. (2005).** *A column generation scheme for faculty timetabling.* In *Practice and Theory of Automated Timetabling V (PATAT 2004)*, Lecture Notes in Computer Science, vol. 3616 (pp. 161–173). Springer. https://doi.org/10.1007/11593577_10 *(Enumeração de configurações válidas de blocos/padrões de aulas no mesmo dia).*
11. **Santos, H. G., Uchoa, E., Ochi, L. S., & Maculan, N. (2012).** *Strong bounds with cut and column generation for class-teacher timetabling.* Annals of Operations Research, 194(1), 399–412. https://doi.org/10.1007/s10479-010-0778-y *(Referência fundamental para a formulação de horários escolares no Brasil com minimização de janelas, dias trabalhados $y_{p,d}$ e aulas geminadas).*
12. **Schaerf, A. (1999).** *A survey of automated timetabling.* Artificial Intelligence Review, 13(2), 87–127. https://doi.org/10.1023/A:1006576209967 *(Revisão clássica sobre problemas de programação de horários escolares e universitários, pré-alocações, indisponibilidades e preferências).*
13. **Sousa, J. P., & Wolsey, L. A. (1992).** *A time indexed formulation of non-preemptive single machine scheduling problems.* Mathematical Programming, 54(1), 353–367. https://doi.org/10.1007/BF01586059 *(Formulação indexada no tempo discretizada em slots elementares e restrições de clique $\sum H_{a,t,k} x_{a,t} \le 1$).*
