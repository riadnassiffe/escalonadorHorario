# Modelo Matemático do Escalonador de Horários

## 1. Descrição do Problema

O problema de escalonamento de horários em instituições de ensino é amplamente estudado na literatura e conta com diversas ferramentas comerciais, como [aSc TimeTables](https://www.asctimetables.com/timetables_pt.html), [PowerCubus](https://powercubus.com.br/features) e [CronosTimeTable](https://www.cronostimetable.com/). Apesar de recorrente, o problema apresenta particularidades operacionais significativas entre diferentes instituições. O **Problema de Programação de Horários Universitário e Escolar** (*High School and University Timetabling Problem*) é classificado na literatura de complexidade computacional como **NP-difícil** (*NP-hard*) [[Daskalaki et al., 2004](#14-referências-bibliográficas)].

Visando atender às necessidades reais da instituição de ensino com total flexibilidade de grade, o sistema contempla os seguintes requisitos:
- **Slots de tempo definidos pelo usuário:** para cada turma e dia da semana, o usuário informa a lista de slots letivos com horário exato de início e fim (por exemplo, `07:30–08:20`, `08:20–09:10`, `09:25–10:10`), podendo variar livremente entre turmas, turnos e dias.
- **Alocação exata por duração:** cada aula possui duração $\ell_a$ em minutos (45, 50, 60 ou qualquer valor positivo) e só pode ser alocada em um slot cadastrado cuja duração seja exatamente igual à sua ($\text{fim} - \text{início} = \ell_a$).
- **Intervalos implícitos:** apenas os slots letivos são cadastrados; qualquer lacuna entre o término de um slot e o início do seguinte representa automaticamente um intervalo de recreio ou almoço.
- **Sobreposição em tempo real:** professores, turmas e salas podem atuar em turmas com grades distintas e desalinhadas; conflitos são auditados pela interseção real de intervalos em minutos $[t^{ini}, t^{fim})$.
- **Preferências e regras acadêmicas:** disponibilidade docente por intervalos contínuos de horário, preferências de dias/turnos, aversão individual a janelas, sala teórica fixa por turma, disciplinas anuais/semestrais, limite diário por disciplina e agrupamento em bloco.

---

## 2. Definição do Modelo

### 2.1. Representação do Tempo e Slots Configuráveis

O dia é representado em linha do tempo contínua de minutos inteiros a partir da meia-noite (`00:00` = $0$, `24:00` = $1440$, constante `MINUTES_PER_DAY = 1440` em [`TimeSlot.h`](include/model/TimeSlot.h#L15)):

- Para cada turma $g \in \mathcal{G}$ e dia letivo $d \in \mathcal{D}_g$, o usuário cadastra uma sequência ordenada de **slots letivos** $\mathcal{K}_{g,d} = \{1, \ldots, |\mathcal{K}_{g,d}|\}$.
- Cada slot $k \in \mathcal{K}_{g,d}$ corresponde ao intervalo semiaberto $[t^{ini}_{g,d,k}, t^{fim}_{g,d,k}) \subseteq [0, 1440)$ em minutos, com duração:
$$
\Delta_{g,d,k} = t^{fim}_{g,d,k} - t^{ini}_{g,d,k} > 0,
\qquad
t^{fim}_{g,d,k} \le t^{ini}_{g,d,k+1} \quad \forall k < |\mathcal{K}_{g,d}|.
$$
- Como os intervalos são semiabertos, um slot que termina às `08:20` ($500$) não conflita com outro que inicia às `08:20` ($500$).
- Quando $t^{fim}_{g,d,k} < t^{ini}_{g,d,k+1}$, o intervalo $[t^{fim}_{g,d,k}, t^{ini}_{g,d,k+1})$ constitui automaticamente um **intervalo de recreio/almoço** da turma $g$ no dia $d$.

Essa abordagem combina a **Formulação Baseada em Eventos/Intervalos** com **Geração Prévia de Candidatos** [[Daskalaki et al., 2004](#14-referências-bibliográficas)], eliminando a explosão combinatória de uma grade fixa de 5 minutos e suportando horários arbitrários com precisão de 1 minuto (implementada em [`TimeSlot.h`](include/model/TimeSlot.h#L100-L185) e [`Turma.h`](include/model/Turma.h#L16-L115)).

### 2.2. Disciplinas Anuais e Semestrais

Todas as disciplinas atuam sobre um horizonte semestral ($\mathcal{S} \subseteq \{1, 2\}$):
- **Disciplinas anuais:** ocorrem nos dois semestres ($\mathcal{S}_a = \{1, 2\}$) exatamente no mesmo dia, slot horário e sala. Utilizam uma única variável de decisão $x_{a,c}$ compartilhada entre as restrições do 1º e do 2º semestre.
- **Disciplinas semestrais:** ocupam professor, turma e sala apenas no semestre em que estão ativas ($\mathcal{S}_a = \{1\}$ ou $\mathcal{S}_a = \{2\}$), permitindo que uma aula do 1º semestre e outra do 2º semestre reutilizem o mesmo slot horário e a mesma sala física sem conflito.

### 2.3. Conjuntos e Índices

Na tabela abaixo estão sintetizados os conjuntos e índices do modelo:

| Símbolo | Significado |
|---|---|
| $\mathcal{P},\ p$ | Conjunto e índice de professores |
| $\mathcal{G},\ g$ | Conjunto e índice de turmas |
| $\mathcal{R},\ r$ | Conjunto e índice de salas e laboratórios |
| $\mathcal{Q},\ q$ | Conjunto e índice de disciplinas |
| $\mathcal{Q}^{consec} \subseteq \mathcal{Q}$ | Disciplinas configuradas para exigir agrupamento em bloco |
| $\mathcal{A},\ a$ | Ocorrências semanais de aulas a serem alocadas |
| $\mathcal{A}_q \subseteq \mathcal{A}$ | Ocorrências de aula pertencentes à disciplina $q$ |
| $\mathcal{A}^{teo},\ \mathcal{A}^{anual},\ \mathcal{A}^{fix}$ | Subconjuntos de aulas teóricas, anuais e fixadas manualmente (*pinned*) |
| $\mathcal{D},\ d$ | Dias letivos da semana (`SEG` a `SEX`) |
| $\mathcal{S},\ s$ | Semestres ativos na otimização ($\{1\}$, $\{2\}$ ou $\{1, 2\}$) |
| $\mathcal{H},\ h$ | Turnos de trabalho (`MANHA`: $< 12\text{h}30$; `TARDE`: $12\text{h}30\text{–}18\text{h}00$; `NOITE`: $\ge 18\text{h}00$) |
| $\mathcal{K}_{g,d},\ k$ | Slots letivos ordenados cadastrados para a turma $g$ no dia $d$ |
| $\mathcal{P}_a \subseteq \mathcal{P},\ \mathcal{G}_a \subseteq \mathcal{G}$ | Professores e turmas que participam da aula $a$ |
| $\mathcal{D}_g \subseteq \mathcal{D}$ | Dias da semana em que a turma $g$ tem aula |
| $\mathcal{S}_a \subseteq \mathcal{S}$ | Semestres em que a aula $a$ está ativa |
| $\mathcal{R}_g^{teo} \subseteq \mathcal{R}$ | Salas elegíveis para serem a sala teórica fixa da turma $g$ |
| $\mathcal{B}_a$ | Combinações de salas válidas para a aula $a$ |
| $\mathcal{C}_a,\ c$ | Conjunto e índice de candidatos viáveis gerados para a aula $a$ |
| $\mathcal{C}_a^{fix} \subseteq \mathcal{C}_a$ | Subconjunto unitário do candidato fixado para a aula $a \in \mathcal{A}^{fix}$ |

**O que é um candidato ($c \in \mathcal{C}_a$)?**
Cada candidato $c \in \mathcal{C}_a$ representa uma tupla admissível $(d_c, t^{ini}_c, t^{fim}_c, b_c)$ para a aula $a$:
- $d_c \in \mathcal{D}$: dia da semana;
- $[t^{ini}_c, t^{fim}_c) \subseteq [0, 1440)$: intervalo exato de um slot cadastrado com $t^{fim}_c - t^{ini}_c = \ell_a$;
- $b_c \subseteq \mathcal{R}$: conjunto de salas usadas simultaneamente pela aula.

---

## 3. Parâmetros de Entrada

| Símbolo | Unidade / Domínio | Significado |
|---|---|---|
| $\ell_a$ | Minutos ($\mathbb{Z}_{>0}$) | Duração da aula $a$ em minutos |
| $[t^{ini}_{g,d,k}, t^{fim}_{g,d,k})$ | Minutos $[0, 1440)$ | Início e fim do $k$-ésimo slot letivo da turma $g$ no dia $d$ |
| $\Delta_{g,d,k} = t^{fim}_{g,d,k} - t^{ini}_{g,d,k}$ | Minutos ($\mathbb{Z}_{>0}$) | Duração do $k$-ésimo slot letivo da turma $g$ no dia $d$ |
| $\mathcal{V}_{p,d} \subseteq [0, 1440)$ | União de intervalos | Horários de disponibilidade do professor $p$ no dia $d$ |
| $M_{p,d}$ | Minutos ($\mathbb{Z}_{\ge 0}$) | Carga máxima diária do professor $p$ no dia $d$ ($0$ = sem limite) |
| $\omega_{p,d}$ | $\{0, \ldots, 10\}$ | Nota de preferência do professor $p$ pelo dia $d$ ($10$ = prefere muito) |
| $\alpha_p$ | $\{0, \ldots, 10\}$ | Sensibilidade individual do professor $p$ a janelas entre aulas (`pesoJanela`) |
| $\pi_{p,d,h}$ | $\{0, 1\}$ | Vale $1$ se o professor $p$ pretende trabalhar no turno $h$ do dia $d$ |
| $\bar{r}_g \in \mathcal{R}$ | Sala | Sala teórica fixa predefinida no cadastro da turma $g$ (quando houver) |
| $U_{q,g}$ | $\mathbb{Z}_{>0}$ | Limite diário de aulas da disciplina $q$ para a turma $g$ (padrão global: $2$) |
| $B_q,\ N_q$ | $\mathbb{Z}_{>0}$ | Tamanho do bloco diário ($B_q \in \{2, 3, 4\}$, respeitando $B_q \le U_{q,g}$) e nº de dias ativos ($N_q = |\mathcal{A}_q| / B_q$) para $q \in \mathcal{Q}^{consec}$ |
| $c^{hora}_{a,c}$ | $\mathbb{Z}_{\ge 0}$ | Penalidade de horário do candidato $c$ ($1$ se $t^{ini}_c < 07\text{h}30$ ou $t^{ini}_c > 11\text{h}00$; $0$ caso contrário) |
| $\lambda_J, \lambda_D, \lambda_P, \lambda_W, \lambda_T, \lambda_G$ | $\mathbb{Z}_{\ge 0}$ | Pesos da função objetivo (padrões: $8, 5, 3, 7, 8, 2$) |

> **Importante:** Preferência baixa ($\omega_{p,d} = 0$ ou $\pi_{p,d,h} = 0$) gera apenas **penalidade** na função objetivo, não proibindo a alocação. Horários estritamente proibidos para o docente permanecem fora de $\mathcal{V}_{p,d}$.

---

## 4. Geração de Horários Candidatos ($\mathcal{C}_a$)

Para uma aula $a$, um candidato $c = (d_c, t^{ini}_c, t^{fim}_c, b_c)$ só é gerado se satisfizer **todas** as regras abaixo (pré-filtragem de domínio implementada em [`Preprocessor::gerarCandidatosParaAula`](src/solver/Preprocessor.cpp#L223-L444)):

1. **Correspondência exata de slot e duração:** Todas as turmas $g \in \mathcal{G}_a$ funcionam no dia $d_c$ ($d_c \in \mathcal{D}_g$), possuem o **mesmo slot cadastrado** $[t^{ini}_c, t^{fim}_c) = [t^{ini}_{g,d_c,k}, t^{fim}_{g,d_c,k})$ para algum $k \in \mathcal{K}_{g,d_c}$, e a duração do slot coincide exatamente com a da aula:
$$
t^{fim}_c - t^{ini}_c = \ell_a.
$$
2. **Disponibilidade docente:** Todos os professores $p \in \mathcal{P}_a$ estão disponíveis em todo o intervalo ($[t^{ini}_c, t^{fim}_c) \subseteq \mathcal{V}_{p,d_c}$) ([`Preprocessor.cpp`](src/solver/Preprocessor.cpp#L375-L384)).
3. **Turno permitido:** $[t^{ini}_c, t^{fim}_c)$ está contido em ao menos um dos turnos autorizados para a disciplina (`MANHA`: `07:00–13:00`, `TARDE`: `12:00–19:00`, `NOITE`: `18:00–23:00`, `INTEGRAL`: `07:00–23:00`), ou a lista de turnos permitidos é vazia ([`shiftAcomodaAula`](src/solver/Preprocessor.cpp#L205-L221)).
4. **Salas compatíveis:** A combinação $b_c \in \mathcal{B}_a$ é válida para o tipo da aula; em aulas teóricas, $b_c = \{r\}$ pertence às salas teóricas elegíveis da turma ([Daskalaki et al., 2004](#14-referências-bibliográficas); [`Preprocessor.cpp`](src/solver/Preprocessor.cpp#L276-L300)).
5. **Fixação manual (*Pinned*):** Para $a \in \mathcal{A}^{fix}$, gera-se exclusivamente o candidato correspondente ao dia, intervalo e sala fixados pelo usuário ([`Preprocessor.cpp`](src/solver/Preprocessor.cpp#L308-L319)).

---

## 5. Variáveis de Decisão

| Variável | Domínio | O que representa |
|---|---|---|
| $x_{a,c}$ | $\{0,1\}$ | Vale $1$ se o candidato $c \in \mathcal{C}_a$ for escolhido para a aula $a$ |
| $z_{g,r}$ | $\{0,1\}$ | Vale $1$ se a sala $r \in \mathcal{R}_g^{teo}$ for a sala teórica fixa da turma $g$ |
| $y_{p,d}$ | $\{0,1\}$ | Vale $1$ se o professor $p$ lecionar ao menos uma aula no dia $d$ |
| $v_{p,d,h}$ | $\{0,1\}$ | Vale $1$ se o professor $p$ trabalhar no turno $h$ do dia $d$ |
| $o^P_{p,d,t,s}$ | $[0,1]$ | Ocupação instantânea do professor $p$ no minuto $t$, dia $d$ e semestre $s$ |
| $o^G_{g,d,t,s}$ | $[0,1]$ | Ocupação instantânea da turma $g$ no minuto $t$, dia $d$ e semestre $s$ |
| $m_{p,d,s}$ | $\mathbb{R}_{\ge 0}$ | Carga em minutos do professor $p$ no dia $d$ e semestre $s$ |
| $u^-_{i,m},\ u^+_{i,m}$ | $\{0,1\}$ | Indicam se há aula **antes** ($u^-$) e **depois** ($u^+$) do elemento $m$ na sequência $i$ |
| $j_{i,m}$ | $[0,1]$ | Vale $1$ se o elemento $m$ for uma **janela/lacuna** (vago entre aulas) na sequência $i$ |
| $w_{q,d}$ | $\{0,1\}$ | Vale $1$ se a disciplina $q \in \mathcal{Q}^{consec}$ tiver seu bloco alocado no dia $d$ |

---

## 6. Expressões Auxiliares de Ocupação e Cobertura

Para indicar se o candidato $c \in \mathcal{C}_a$ cobre o instante $t \in [0, 1440)$ no dia $d$, define-se a função indicadora:
$$
H_{a,c,d,t} = \begin{cases}
1, & \text{se } d_c = d \text{ e } t^{ini}_c \le t < t^{fim}_c, \\
0, & \text{caso contrário}.
\end{cases}
$$

As ocupações instantâneas do professor $p$ e da turma $g$ no dia $d$, instante $t$ e semestre $s$ são dadas por somas lineares:
$$
o^P_{p,d,t,s} = \sum_{a \in \mathcal{A}:\, p \in \mathcal{P}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,t}\, x_{a,c},
\qquad
o^G_{g,d,t,s} = \sum_{a \in \mathcal{A}:\, g \in \mathcal{G}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,t}\, x_{a,c}.
$$

Analogamente, define-se a ocupação do slot letivo $k \in \mathcal{K}_{g,d}$ da turma $g$ no dia $d$ e semestre $s$:
$$
\tilde{o}^G_{g,d,k,s} = \sum_{a \in \mathcal{A}:\, g \in \mathcal{G}_a,\ s \in \mathcal{S}_a} \sum_{\substack{c \in \mathcal{C}_a:\, d_c = d \\ t^{ini}_c < t^{fim}_{g,d,k} \text{ e } t^{ini}_{g,d,k} < t^{fim}_c}} x_{a,c}.
$$

Definem-se ainda o conjunto de pares $(a,c)$ do professor $p$ no dia $d$:
$$
\mathcal{X}_{p,d} = \{(a,c) : a \in \mathcal{A},\ p \in \mathcal{P}_a,\ c \in \mathcal{C}_a,\ d_c = d\},
$$
seu subconjunto por semestre $\mathcal{X}_{p,d,s} = \{(a,c) \in \mathcal{X}_{p,d} : s \in \mathcal{S}_a\}$ e seu subconjunto por turno $\mathcal{X}_{p,d,h} = \{(a,c) \in \mathcal{X}_{p,d} : \text{turno}(t^{ini}_c) = h\}$.

---

## 7. Restrições do Modelo

### R1. Alocação única de cada aula
$$
\sum_{c \in \mathcal{C}_a} x_{a,c} = 1 \qquad \forall a \in \mathcal{A}.
$$
- **Literatura:** Alocação única de evento e recurso [[Daskalaki et al., 2004](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`CpSatSolver.cpp` (L187–L195)](src/solver/CpSatSolver.cpp#L187-L195); MILP [`MilpSolver.cpp` (L216–L225)](src/solver/MilpSolver.cpp#L216-L225).

### R2. Não sobreposição de professores (interseção real em minutos)
$$
o^P_{p,d,t,s} \le 1 \qquad \forall p \in \mathcal{P},\ d \in \mathcal{D},\ t \in \mathcal{T}^{ini}_{p,d,s},\ s \in \mathcal{S},
$$
onde $\mathcal{T}^{ini}_{p,d,s} = \{t^{ini}_c : (a,c) \in \mathcal{X}_{p,d,s}\}$ é o conjunto finito dos instantes de início dos candidatos. Pelo Teorema das Cliques em Grafos de Intervalo, impor a não sobreposição sobre $\mathcal{T}^{ini}_{p,d,s}$ garante exatamente não sobreposição em todo $t \in [0, 1440)$, mesmo quando o professor atende turmas com horários desalinhados.
- **Literatura:** Cliques de não sobreposição em grafos de intervalo [[Sousa & Wolsey, 1992](#14-referências-bibliográficas); [Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** CP-SAT (`NewOptionalIntervalVar(start_min, dur, end_min)` + `AddNoOverlap`) [`CpSatSolver.cpp` (L197–L226)](src/solver/CpSatSolver.cpp#L197-L226); MILP (`add_no_overlap_cliques` por pontos de início) [`MilpSolver.cpp` (L227–L274)](src/solver/MilpSolver.cpp#L227-L274); Gulosa [`GreedyHeuristicSolver.cpp` (L94–L101, L142–L148)](src/solver/GreedyHeuristicSolver.cpp#L94-L101).

### R3. Não sobreposição de turmas
$$
o^G_{g,d,t,s} \le 1 \qquad \forall g \in \mathcal{G},\ d \in \mathcal{D},\ t \in \mathcal{T}^{ini}_{g,d,s},\ s \in \mathcal{S}.
$$
- **Literatura:** Prevenção de conflitos de turmas [[Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`CpSatSolver.cpp` (L257–L284)](src/solver/CpSatSolver.cpp#L257-L284); MILP [`MilpSolver.cpp` (L227–L274)](src/solver/MilpSolver.cpp#L227-L274); Gulosa [`GreedyHeuristicSolver.cpp` (L150–L154)](src/solver/GreedyHeuristicSolver.cpp#L150-L154).

### R4. Não sobreposição de salas (interseção real em minutos)
$$
\sum_{a \in \mathcal{A}:\, s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a:\, r \in b_c} H_{a,c,d,t}\, x_{a,c} \le 1 \qquad \forall r \in \mathcal{R},\ d \in \mathcal{D},\ t \in \mathcal{T}^{ini}_{r,d,s},\ s \in \mathcal{S}.
$$
- **Literatura:** Prevenção de conflitos de salas físicas [[Daskalaki et al., 2004](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`CpSatSolver.cpp` (L228–L255)](src/solver/CpSatSolver.cpp#L228-L255); MILP [`MilpSolver.cpp` (L227–L274)](src/solver/MilpSolver.cpp#L227-L274); Gulosa [`GreedyHeuristicSolver.cpp` (L205–L211)](src/solver/GreedyHeuristicSolver.cpp#L205-L211).

### R5. Sala teórica fixa por turma
Cada turma $g$ utiliza uma única sala teórica fixa durante a semana para suas aulas teóricas de turma única ($|\mathcal{G}_a|=1$, $|b_c|=1$):
$$
\sum_{r \in \mathcal{R}_g^{teo}} z_{g,r} = 1 \quad \forall g \in \mathcal{G}, \qquad
\sum_{c \in \mathcal{C}_a:\, b_c = \{r\}} x_{a,c} \le z_{g,r} \quad \forall a \in \mathcal{A}^{teo}\ (|\mathcal{G}_a|=1),\ g \in \mathcal{G}_a,\ r \in \mathcal{R}_g^{teo}.
$$
Se a turma possuir sala predefinida $\bar{r}_g$, impõe-se $z_{g,\bar{r}_g} = 1$. Aulas práticas ou de laboratório não estão sujeitas a esta regra.
- **Literatura:** Estabilidade e particionamento de salas teóricas vs. laboratórios [[Daskalaki et al., 2004](#14-referências-bibliográficas)].
- **Código:** Pré-processador [`Preprocessor.cpp` (L278–L289)](src/solver/Preprocessor.cpp#L278-L289); CP-SAT [`CpSatSolver.cpp` (L150–L183)](src/solver/CpSatSolver.cpp#L150-L183); MILP [`MilpSolver.cpp` (L176–L214)](src/solver/MilpSolver.cpp#L176-L214).

### R6. Limite diário de minutos por professor
Para todo professor com teto $M_{p,d} > 0$:
$$
m_{p,d,s} = \sum_{(a,c) \in \mathcal{X}_{p,d,s}} \ell_a\, x_{a,c} \le M_{p,d} \qquad \forall p \in \mathcal{P},\ d \in \mathcal{D},\ s \in \mathcal{S}.
$$
- **Literatura:** Limite diário de carga horária docente [[Santos et al., 2012](#14-referências-bibliográficas); [Daskalaki et al., 2004](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`CpSatSolver.cpp` (L326–L347)](src/solver/CpSatSolver.cpp#L326-L347); MILP [`MilpSolver.cpp` (L320–L344)](src/solver/MilpSolver.cpp#L320-L344).

### R7. Aulas fixadas manualmente (*Pinned*)
Para aulas fixadas pelo usuário ($a \in \mathcal{A}^{fix}$), $\mathcal{C}_a = \mathcal{C}_a^{fix}$ contém apenas o candidato fixado, garantindo por R1:
$$
\sum_{c \in \mathcal{C}_a^{fix}} x_{a,c} = 1 \qquad \forall a \in \mathcal{A}^{fix}.
$$
- **Literatura:** Pré-alocações (*preassignments*) [[Daskalaki et al., 2004](#14-referências-bibliográficas)].
- **Código:** Pré-processador [`Preprocessor.cpp` (L308–L319)](src/solver/Preprocessor.cpp#L308-L319); Gulosa [`GreedyHeuristicSolver.cpp` (L79)](src/solver/GreedyHeuristicSolver.cpp#L79).

### R8. Sincronização de disciplinas anuais entre semestres
Para $a \in \mathcal{A}^{anual}$, tem-se $\mathcal{S}_a = \mathcal{S}$ e a mesma variável $x_{a,c}$ participa simultaneamente das restrições de ambos os semestres.
- **Literatura:** Acoplamento multi-período por compartilhamento de variáveis [[Daskalaki et al., 2004](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`CpSatSolver.cpp` (L84–L148)](src/solver/CpSatSolver.cpp#L84-L148); MILP [`MilpSolver.cpp` (L96–L174)](src/solver/MilpSolver.cpp#L96-L174).

### R9. Primeira aula do dia deve começar no primeiro slot letivo
Se uma turma $g$ tem aula no dia $d$ e semestre $s$, alguma aula deve ocupar o primeiro slot cadastrado da turma no dia ($t^{ini}_{g,d,1}$). Seja:
$$
B_{g,d,s} = \sum_{a \in \mathcal{A}:\, g \in \mathcal{G}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a:\, d_c = d,\ t^{ini}_c = t^{ini}_{g,d,1}} x_{a,c}.
$$
Para cada aula $a$ da turma no dia que inicie após $t^{ini}_{g,d,1}$:
$$
\sum_{c \in \mathcal{C}_a:\, d_c = d,\ t^{ini}_c > t^{ini}_{g,d,1}} x_{a,c} \le B_{g,d,s} \qquad \forall g \in \mathcal{G},\ d \in \mathcal{D}_g,\ s \in \mathcal{S},\ \forall a \text{ com } g \in \mathcal{G}_a,\ s \in \mathcal{S}_a.
$$
- **Literatura:** Quebra de simetria temporal e eliminação de ociosidade inicial [[Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`CpSatSolver.cpp` (L286–L324)](src/solver/CpSatSolver.cpp#L286-L324); MILP [`MilpSolver.cpp` (L276–L318)](src/solver/MilpSolver.cpp#L276-L318); Gulosa [`GreedyHeuristicSolver.cpp` (L178–L203)](src/solver/GreedyHeuristicSolver.cpp#L178-L203).

### R10. Preenchimento integral dos slots letivos *(opcional)*
Quando `preenchimento_integral_jornada` está ativo, todos os slots letivos cadastrados $k \in \mathcal{K}_{g,d}$ da turma devem estar ocupados:
$$
\tilde{o}^G_{g,d,k,s} = 1 \qquad \forall g \in \mathcal{G},\ d \in \mathcal{D}_g,\ k \in \mathcal{K}_{g,d},\ s \in \mathcal{S}.
$$
- **Literatura:** *Complete / Compact Class Timetables* [[Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** Validação por duração [`Preprocessor.cpp` (L117–L194)](src/solver/Preprocessor.cpp#L117-L194); CP-SAT [`CpSatSolver.cpp` (L349–L377)](src/solver/CpSatSolver.cpp#L349-L377); MILP [`MilpSolver.cpp` (L346–L374)](src/solver/MilpSolver.cpp#L346-L374).

### R11. Limite máximo de aulas da mesma disciplina por dia *(opcional)*
Quando `distribuir_disciplinas_na_semana` está ativo:
$$
\sum_{a \in \mathcal{A}_q} \sum_{c \in \mathcal{C}_a:\, d_c = d} x_{a,c} \le U_{q,g} \qquad \forall q \in \mathcal{Q},\ g \in \mathcal{G}_q,\ d \in \mathcal{D}.
$$
- **Literatura:** Distribuição semanal de aulas da mesma disciplina [[Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`CpSatSolver.cpp` (L379–L403)](src/solver/CpSatSolver.cpp#L379-L403); MILP [`MilpSolver.cpp` (L376–L402)](src/solver/MilpSolver.cpp#L376-L402); Gulosa [`GreedyHeuristicSolver.cpp` (L155–L165)](src/solver/GreedyHeuristicSolver.cpp#L155-L165).

### R12. Agrupamento de aulas em bloco *(opcional)*
Para disciplinas com `exigirAulasConsecutivas = true`, define-se o tamanho do bloco diário como o maior divisor $B_q \in \{2, 3, 4\}$ de $|\mathcal{A}_q|$ compatível com o limite diário $U_{q,g}$ ($B_q \le U_{q,g}$ quando R11 está ativa) e com a quantidade de dias letivos ($N_q = |\mathcal{A}_q| / B_q \le |\mathcal{D}|$) — por exemplo, $(B_q, N_q) = (2,1)$ para $2$ aulas, $(2,2)$ para $4$ aulas com $U_{q,g}=2$, $(4,1)$ para $4$ aulas com $U_{q,g}\ge 4$, e $(4,2)$ para $8$ aulas com $U_{q,g}\ge 4$ —, impondo-se o agrupamento em $N_q$ dias com exatamente $B_q$ aulas no mesmo dia:
$$
\sum_{a \in \mathcal{A}_q} \sum_{c \in \mathcal{C}_a:\, d_c = d} x_{a,c} = B_q\, w_{q,d} \quad \forall d \in \mathcal{D}, \qquad
\sum_{d \in \mathcal{D}} w_{q,d} = N_q \qquad \forall q \in \mathcal{Q}^{consec}.
$$
- **Literatura:** *Double Lessons* e agrupamento diário [[Santos et al., 2012](#14-referências-bibliográficas); [Daskalaki et al., 2004](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`CpSatSolver.cpp` (L417–L457)](src/solver/CpSatSolver.cpp#L417-L457); MILP [`MilpSolver.cpp` (L406–L448)](src/solver/MilpSolver.cpp#L406-L448); Gulosa [`GreedyHeuristicSolver.cpp` (L167–L175)](src/solver/GreedyHeuristicSolver.cpp#L167-L175).

---

## 8. Indicadores de Dias e Turnos Trabalhados pelos Professores

Para identificar se o professor $p$ trabalha no dia $d$ ($y_{p,d} = 1$) ou no turno $h$ do dia $d$ ($v_{p,d,h} = 1$), vinculam-se os indicadores às variáveis $x_{a,c}$:
$$
x_{a,c} \le y_{p,d} \quad \forall (a,c) \in \mathcal{X}_{p,d}, \qquad
y_{p,d} \le \sum_{(a,c) \in \mathcal{X}_{p,d}} x_{a,c},
$$
$$
x_{a,c} \le v_{p,d,h} \quad \forall (a,c) \in \mathcal{X}_{p,d,h}, \qquad
v_{p,d,h} \le \sum_{(a,c) \in \mathcal{X}_{p,d,h}} x_{a,c}.
$$

> **Nota de escopo multi-semestral:** Assim como na versão anterior, esses indicadores operam sobre $\mathcal{X}_{p,d}$ e $\mathcal{X}_{p,d,h}$ agregando os semestres. Uma futura extensão multi-semestral para os termos suaves pode indexar $y_{p,d,s}$ e $v_{p,d,h,s}$ por semestre $s$.

- **Literatura:** Concentração de dias e turnos docentes [[Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** $y_{p,d}$ em [`CpSatSolver.cpp` (L491–L526)](src/solver/CpSatSolver.cpp#L491-L526) e [`MilpSolver.cpp` (L440–L476)](src/solver/MilpSolver.cpp#L440-L476); $v_{p,d,h}$ (`y_shift`) em [`CpSatSolver.cpp` (L535–L569)](src/solver/CpSatSolver.cpp#L535-L569) e [`MilpSolver.cpp` (L478–L515)](src/solver/MilpSolver.cpp#L478-L515).

---

## 9. Cálculo Linear de Janelas e Lacunas (em Minutos)

Uma **janela** é um tempo ocioso situado estritamente **entre** duas aulas no mesmo período de análise. Horários livres antes da primeira aula ou após a última não são penalizados:

- **Para professores ($J$, por segmentos elementares no turno):**
  Como um professor pode lecionar em turmas com slots de horários e durações distintos dentro do mesmo turno $i = (p, d, h)$, ordenam-se todos os pontos críticos distintos $\{t_0 < t_1 < \dots < t_{M_i}\}$ de início e término dos candidatos de $\mathcal{X}_{p,d,h}$. Esses pontos particionam a linha do tempo em $M_i$ segmentos elementares $E_{i,m} = [t_m, t_{m+1})$ de duração $d_{i,m} = t_{m+1} - t_m$ minutos ($m \in \mathcal{E}_i = \{0, \ldots, M_i-1\}$), com ocupação linear binária:
  $$
  o_{i,m} = \sum_{(a,c) \in \mathcal{X}_{p,d,h}:\, t^{ini}_c \le t_m \text{ e } t_{m+1} \le t^{fim}_c} x_{a,c}.
  $$
- **Para turmas ($J_G$, diretamente sobre os slots letivos cadastrados):**
  As lacunas da turma $i = (g, d)$ são medidas diretamente sobre os slots letivos cadastrados $\mathcal{E}_i = \mathcal{K}_{g,d}$ (de duração $\Delta_{g,d,k}$ minutos), com ocupação $o_{i,k} = \tilde{o}^G_{g,d,k}$. Dessa forma, os intervalos de recreio/almoço implícitos entre slots **nunca** são penalizados como lacuna.

Para que $j_{i,m} = 1$ quando houver aula antes do elemento $m$ ($u^-_{i,m} = 1$), aula depois de $m$ ($u^+_{i,m} = 1$) e o elemento $m$ estiver vago ($o_{i,m} = 0$), utiliza-se a linearização:
$$
u^-_{i,m} \ge o_{i,l} \quad \forall l \in \mathcal{E}_i:\, l < m, \qquad
u^+_{i,m} \ge o_{i,l} \quad \forall l \in \mathcal{E}_i:\, l > m,
$$
$$
j_{i,m} \ge u^-_{i,m} + u^+_{i,m} - o_{i,m} - 1 \qquad \forall m \in \mathcal{E}_i.
$$
Sob minimização com pesos não negativos em minutos ($d_{i,m}$ ou $\Delta_{g,d,k}$), essas desigualdades garantem que $j_{i,m}$ assuma o valor exato na solução ótima. No MILP, as desigualdades diretas sobre $u^-$ e $u^+$ são substituídas de forma equivalente pelo encadeamento monotônico $O(|\mathcal{E}_i|)$:
$$
u^-_{i,m-1} \le u^-_{i,m}, \qquad u^+_{i,m+1} \le u^+_{i,m}.
$$

- **Literatura:** Minimização e linearização de janelas [[Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** Janelas docentes ($J$) em [`CpSatSolver.cpp` (L571–L620)](src/solver/CpSatSolver.cpp#L571-L620) e [`MilpSolver.cpp` (L517–L617)](src/solver/MilpSolver.cpp#L517-L617); Lacunas de turmas ($J_G$) em [`CpSatSolver.cpp` (L622–L664)](src/solver/CpSatSolver.cpp#L622-L664) e [`MilpSolver.cpp` (L619–L711)](src/solver/MilpSolver.cpp#L619-L711).

---

## 10. Função Objetivo (Critérios de Qualidade)

O solver minimiza a soma ponderada de seis critérios de qualidade:

### 10.1. Concentração de dias dos professores ($D_{prof}$)
$$
D_{prof} = \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} y_{p,d}.
$$

### 10.2. Preferência de horários ($P_{hora}$)
$$
P_{hora} = \sum_{a \in \mathcal{A}} \sum_{c \in \mathcal{C}_a} c^{hora}_{a,c}\, x_{a,c}.
$$

### 10.3. Preferência de dias dos professores ($W$)
Configurável em dois modos (`ModeloPreferenciaDias`):
- **Por dia trabalhado (`POR_DIA_TRABALHADO`):**
$$W_{dia} = \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} (10 - \omega_{p,d})\, y_{p,d}.$$
- **Por minuto de aula (`POR_MINUTO_AULA`):**
$$W_{min} = \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} (10 - \omega_{p,d}) \sum_{(a,c) \in \mathcal{X}_{p,d}} \ell_a\, x_{a,c}.$$

### 10.4. Turnos pretendidos pelos professores ($T_{prof}$)
$$
T_{prof} = \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} \sum_{h \in \mathcal{H}} (1 - \pi_{p,d,h})\, v_{p,d,h}.
$$

### 10.5. Janelas de professores ($J_{min}$) e lacunas de turmas ($J_{G,min}$) em minutos
$$
J_{min} = \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} \sum_{h \in \mathcal{H}} \alpha_p \sum_{m \in \mathcal{E}_{(p,d,h)}} d_{(p,d,h),m}\, j_{(p,d,h),m}, \qquad
J_{G,min} = \sum_{g \in \mathcal{G}} \sum_{d \in \mathcal{D}_g} \sum_{k \in \mathcal{K}_{g,d}} \Delta_{g,d,k}\, j_{(g,d),k}.
$$

### 10.6. Expressão Global da Função Objetivo

Implementada identicamente em CP-SAT ([`CpSatSolver.cpp` (L666–L675)](src/solver/CpSatSolver.cpp#L666-L675)) e nos motores MILP — SCIP, HiGHS e cuOpt ([`MilpSolver.cpp` (L135–L711)](src/solver/MilpSolver.cpp#L135-L711)):
$$
\boxed{\min Z = \lambda_J\, J_{min} + \lambda_D\, D_{prof} + \lambda_P\, P_{hora} + \lambda_W\, W + 10\,\lambda_T\, T_{prof} + \lambda_G\, J_{G,min}}
$$
onde tanto $J_{min}$ quanto $J_{G,min}$ já são expressos diretamente em **minutos reais**. A preservação da grade atual em tela (`usar_distribuicao_como_entrada`) atua como solução inicial (*Warm Start*) sem alterar os pesos de $Z$.

---

## 11. Exemplo Prático de Slots Configuráveis, Intervalos e Choques

Suponha duas turmas com grades distintas informadas pelo usuário na segunda-feira:
- **Turma A:** Slot 1 = `07:30–08:12` (42 min), Slot 2 = `08:12–09:02` (50 min), *[lacuna implícita de recreio `09:02–09:17` (15 min)]*, Slot 3 = `09:17–10:02` (45 min).
  - Uma disciplina de **42 min** recebe candidatos apenas no Slot 1; uma de **50 min**, apenas no Slot 2; uma de **45 min**, apenas no Slot 3.
  - Se as três aulas forem alocadas, o preenchimento integral (R10) é satisfeito com **0 minutos** de lacuna da turma ($J_{G,min} = 0$), pois o recreio `09:02–09:17` não é um slot letivo.
- **Turma B (desalinhada):** Slot 1 = `07:45–08:30` (45 min), Slot 2 = `09:10–09:55` (45 min).
  - Se o mesmo professor lecionar na Turma A às `07:30–08:12` (Slot 1) e na Turma B, o Slot 1 da Turma B (`07:45–08:30`) é impedido por R2 pois $[450, 492) \cap [465, 510) = [465, 492) \ne \emptyset$. A aula da Turma B é alocada no Slot 2 (`09:10–09:55`), gerando ao docente apenas a janela correspondente aos minutos ociosos entre `08:12` e `09:10`.

---

## 12. Formulação Matemática Integrada (Resumo Completo)

$$
\min Z = \lambda_J \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} \sum_{h \in \mathcal{H}} \alpha_p \sum_{m \in \mathcal{E}_{(p,d,h)}} d_{(p,d,h),m}\, j_{(p,d,h),m}
+ \lambda_D \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} y_{p,d}
+ \lambda_P \sum_{a \in \mathcal{A}} \sum_{c \in \mathcal{C}_a} c^{hora}_{a,c}\, x_{a,c}
+ \lambda_W W
+ 10\,\lambda_T \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} \sum_{h \in \mathcal{H}} (1 - \pi_{p,d,h})\, v_{p,d,h}
+ \lambda_G \sum_{g \in \mathcal{G}} \sum_{d \in \mathcal{D}_g} \sum_{k \in \mathcal{K}_{g,d}} \Delta_{g,d,k}\, j_{(g,d),k}
$$

sujeito a:
$$
\begin{aligned}
o^P_{p,d,t,s} &= \sum_{a \in \mathcal{A}:\, p \in \mathcal{P}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,t}\, x_{a,c} && \forall p, d, t, s \\
o^G_{g,d,t,s} &= \sum_{a \in \mathcal{A}:\, g \in \mathcal{G}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,t}\, x_{a,c} && \forall g, d, t, s \\
m_{p,d,s} &= \sum_{(a,c) \in \mathcal{X}_{p,d,s}} \ell_a\, x_{a,c} && \forall p, d, s \\[6pt]
\sum_{c \in \mathcal{C}_a} x_{a,c} &= 1 && \forall a \in \mathcal{A} \\
o^P_{p,d,t,s} &\le 1 && \forall p, d,\ t \in \mathcal{T}^{ini}_{p,d,s},\ s \\
o^G_{g,d,t,s} &\le 1 && \forall g, d,\ t \in \mathcal{T}^{ini}_{g,d,s},\ s \\
\sum_{a \in \mathcal{A}:\, s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a:\, r \in b_c} H_{a,c,d,t}\, x_{a,c} &\le 1 && \forall r, d,\ t \in \mathcal{T}^{ini}_{r,d,s},\ s \\
\sum_{r \in \mathcal{R}_g^{teo}} z_{g,r} &= 1 && \forall g \in \mathcal{G} \\
\sum_{c \in \mathcal{C}_a:\, b_c = \{r\}} x_{a,c} &\le z_{g,r} && \forall a \in \mathcal{A}^{teo}\ (|\mathcal{G}_a|=1),\ g \in \mathcal{G}_a,\ r \in \mathcal{R}_g^{teo} \\
z_{g,\bar{r}_g} &= 1 && \forall g \in \mathcal{G} \text{ com sala predefinida } \bar{r}_g \\
m_{p,d,s} &\le M_{p,d} && \forall p \text{ com } M_{p,d}>0,\ d, s \\
\sum_{c \in \mathcal{C}_a:\, d_c = d,\ t^{ini}_c > t^{ini}_{g,d,1}} x_{a,c} &\le \sum_{a' \in \mathcal{A}:\, g \in \mathcal{G}_{a'},\, s \in \mathcal{S}_{a'}} \sum_{c' \in \mathcal{C}_{a'}:\, d_{c'}=d,\ t^{ini}_{c'}=t^{ini}_{g,d,1}} x_{a',c'} && \forall g, d \in \mathcal{D}_g, s,\ \forall a \text{ com } g \in \mathcal{G}_a,\ s \in \mathcal{S}_a \\[6pt]
\tilde{o}^G_{g,d,k,s} &= 1 && \forall g, d \in \mathcal{D}_g, k \in \mathcal{K}_{g,d}, s \quad \text{(se R10 ativa)} \\
\sum_{a \in \mathcal{A}_q} \sum_{c \in \mathcal{C}_a:\, d_c = d} x_{a,c} &\le U_{q,g} && \forall q, g \in \mathcal{G}_q, d \quad \text{(se R11 ativa)} \\
\sum_{a \in \mathcal{A}_q} \sum_{c \in \mathcal{C}_a:\, d_c = d} x_{a,c} &= B_q\, w_{q,d},\quad \sum_{d \in \mathcal{D}} w_{q,d} = N_q && \forall q \in \mathcal{Q}^{consec},\ d \in \mathcal{D} \quad \text{(se R12 ativa)} \\[6pt]
x_{a,c} \le y_{p,d}, \quad y_{p,d} &\le \sum_{(a',c') \in \mathcal{X}_{p,d}} x_{a',c'} && \forall p, d,\ \forall (a,c) \in \mathcal{X}_{p,d} \\
x_{a,c} \le v_{p,d,h}, \quad v_{p,d,h} &\le \sum_{(a',c') \in \mathcal{X}_{p,d,h}} x_{a',c'} && \forall p, d, h,\ \forall (a,c) \in \mathcal{X}_{p,d,h} \\
u^-_{i,m} \ge o_{i,l}\ (\forall l < m), \quad u^+_{i,m} &\ge o_{i,l}\ (\forall l > m), \quad j_{i,m} \ge u^-_{i,m} + u^+_{i,m} - o_{i,m} - 1 && \forall i,\ \forall m \in \mathcal{E}_i \\[6pt]
x_{a,c}, z_{g,r}, y_{p,d}, v_{p,d,h}, w_{q,d}, u^-_{i,m}, u^+_{i,m} &\in \{0, 1\}, \qquad j_{i,m}, o^P_{p,d,t,s}, o^G_{g,d,t,s} \in [0, 1], \qquad m_{p,d,s} \ge 0.
\end{aligned}
$$

---

## 13. Matriz de Rastreabilidade (Modelo $\leftrightarrow$ Literatura $\leftrightarrow$ Código-Fonte)

| Elemento / Restrição | Literatura Base | Pré-processador (`Preprocessor.cpp` / Modelo) | CP-SAT (`CpSatSolver.cpp`) | MILP — SCIP, HiGHS e cuOpt (`MilpSolver.cpp`) |
| :--- | :--- | :--- | :--- | :--- |
| **Tempo contínuo ($[0, 1440)$) e slots configuráveis** | Slots heterogêneos por turma [[Daskalaki et al., 2004](#14-referências-bibliográficas)] | [`TimeSlot.h` (L15–L185)](include/model/TimeSlot.h#L15-L185), [`Turma.h` (L24–L115)](include/model/Turma.h#L24-L115) | [`L101–L148`](src/solver/CpSatSolver.cpp#L101-L148) | [`L114–L174`](src/solver/MilpSolver.cpp#L114-L174) |
| **Filtro $\mathcal{C}_a$ — Slot exato e duração ($\Delta_k = \ell_a$)** | Pré-filtragem de domínio [[Daskalaki et al., 2004](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L199–L203, L347–L363)](src/solver/Preprocessor.cpp#L199-L203) | Implícito em $\mathcal{C}_a$ | Implícito em $\mathcal{C}_a$ |
| **Filtro $\mathcal{C}_a$ — Disponibilidade docente** | Restrição rígida de horário [[Santos et al., 2012](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L375–L384)](src/solver/Preprocessor.cpp#L375-L384) | Implícito em $\mathcal{C}_a$ | Implícito em $\mathcal{C}_a$ |
| **Filtro $\mathcal{C}_a$ — Turnos e salas** | Compatibilidade de recursos [[Daskalaki et al., 2004](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L205–L221, L276–L300)](src/solver/Preprocessor.cpp#L205-L221) | Implícito em $\mathcal{C}_a$ | Implícito em $\mathcal{C}_a$ |
| **R1. Alocação única de cada aula** | Alocação única de evento [[Daskalaki et al., 2004](#14-referências-bibliográficas)] | — | [`L187–L195`](src/solver/CpSatSolver.cpp#L187-L195) | [`L216–L225`](src/solver/MilpSolver.cpp#L216-L225) |
| **R2. Não sobreposição de professores** | Cliques de não sobreposição [[Sousa & Wolsey, 1992](#14-referências-bibliográficas); [Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L197–L226`](src/solver/CpSatSolver.cpp#L197-L226) (`AddNoOverlap`) | [`L227–L274`](src/solver/MilpSolver.cpp#L227-L274) (Cliques em $\mathcal{T}^{ini}$) |
| **R3. Não sobreposição de turmas** | Prevenção de conflitos [[Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L257–L284`](src/solver/CpSatSolver.cpp#L257-L284) (`AddNoOverlap`) | [`L227–L274`](src/solver/MilpSolver.cpp#L227-L274) (Cliques em $\mathcal{T}^{ini}$) |
| **R4. Não sobreposição de salas** | Prevenção de conflitos de salas [[Daskalaki et al., 2004](#14-referências-bibliográficas)] | — | [`L228–L255`](src/solver/CpSatSolver.cpp#L228-L255) (`AddNoOverlap`) | [`L227–L274`](src/solver/MilpSolver.cpp#L227-L274) (Cliques em $\mathcal{T}^{ini}$) |
| **R5. Sala teórica fixa ($z_{g,r}$)** | *Room Stability* [[Daskalaki et al., 2004](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L278–L289)](src/solver/Preprocessor.cpp#L278-L289) | [`L150–L183`](src/solver/CpSatSolver.cpp#L150-L183) | [`L176–L214`](src/solver/MilpSolver.cpp#L176-L214) |
| **R6. Carga máxima diária ($M_{p,d}$)** | Limite diário de carga [[Santos et al., 2012](#14-referências-bibliográficas); [Daskalaki et al., 2004](#14-referências-bibliográficas)] | — | [`L326–L347`](src/solver/CpSatSolver.cpp#L326-L347) | [`L320–L344`](src/solver/MilpSolver.cpp#L320-L344) |
| **R7. Aulas fixadas (*Pinned*)** | Pré-alocações (*preassignments*) [[Daskalaki et al., 2004](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L308–L319)](src/solver/Preprocessor.cpp#L308-L319) | Garantido por $\lvert\mathcal{C}_a\rvert = 1$ + R1 | Garantido por $\lvert\mathcal{C}_a\rvert = 1$ + R1 |
| **R8. Sincronização anual** | Acoplamento multi-período [[Daskalaki et al., 2004](#14-referências-bibliográficas)] | — | [`L84–L148`](src/solver/CpSatSolver.cpp#L84-L148) | [`L96–L174`](src/solver/MilpSolver.cpp#L96-L174) |
| **R9. Início no 1º slot ($t^{ini}_{g,d,1}$)** | Quebra de simetria [[Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L286–L324`](src/solver/CpSatSolver.cpp#L286-L324) | [`L276–L318`](src/solver/MilpSolver.cpp#L276-L318) |
| **R10. Preenchimento integral dos slots** | *Compact Class Schedules* [[Santos et al., 2012](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L117–L194)](src/solver/Preprocessor.cpp#L117-L194) | [`L349–L377`](src/solver/CpSatSolver.cpp#L349-L377) | [`L346–L374`](src/solver/MilpSolver.cpp#L346-L374) |
| **R11. Limite diário ($U_{q,g}$)** | Distribuição semanal [[Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L379–L403`](src/solver/CpSatSolver.cpp#L379-L403) | [`L376–L402`](src/solver/MilpSolver.cpp#L376-L402) |
| **R12. Agrupamento em bloco ($w_{q,d}$)** | *Double Lessons* [[Santos et al., 2012](#14-referências-bibliográficas); [Daskalaki et al., 2004](#14-referências-bibliográficas)] | — | [`L417–L457`](src/solver/CpSatSolver.cpp#L417-L457) | [`L406–L448`](src/solver/MilpSolver.cpp#L406-L448) |
| **Seção 8 & 10.1. Dias ($y_{p,d}, D_{prof}$)** | Concentração de dias [[Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L491–L526`](src/solver/CpSatSolver.cpp#L491-L526) | [`L440–L476`](src/solver/MilpSolver.cpp#L440-L476) |
| **Seção 10.2. Preferência horária ($P_{hora}$)** | Preferência horária (Soft) [[Santos et al., 2012](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L386–L392)](src/solver/Preprocessor.cpp#L386-L392) | [`L528–L533`](src/solver/CpSatSolver.cpp#L528-L533) | [`L135–L156`](src/solver/MilpSolver.cpp#L135-L156) |
| **Seção 10.3. Preferência de dias ($W$)** | Preferências docentes [[Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L512–L525`](src/solver/CpSatSolver.cpp#L512-L525) | [`L139–L147, L449–L451`](src/solver/MilpSolver.cpp#L139-L147) |
| **Seção 10.4. Turnos ($v_{p,d,h}, T_{prof}$)** | Preferência por turno [[Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L535–L569`](src/solver/CpSatSolver.cpp#L535-L569) | [`L478–L515`](src/solver/MilpSolver.cpp#L478-L515) |
| **Seção 9 & 10.5. Janelas ($J_{min}, J_{G,min}$)** | Linearização de janelas [[Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L571–L664`](src/solver/CpSatSolver.cpp#L571-L664) | [`L517–L711`](src/solver/MilpSolver.cpp#L517-L711) |
| **Warm Start (grade atual em tela)** | Solução inicial primal | — | [`L677–L744`](src/solver/CpSatSolver.cpp#L677-L744) (`AddHint`) | [`L713–L837`](src/solver/MilpSolver.cpp#L713-L837) (`warm_start_vals`) |

---

## 14. Referências Bibliográficas

1. **Daskalaki, S., Birbas, T., & Housos, E. (2004).** *An integer programming formulation for a case study in university timetabling.* European Journal of Operational Research, 153(1), 117–135. https://doi.org/10.1016/S0377-2217(03)00103-6
2. **Santos, H. G., Uchoa, E., Ochi, L. S., & Maculan, N. (2012).** *Strong bounds with cut and column generation for class-teacher timetabling.* Annals of Operations Research, 194(1), 399–412. https://doi.org/10.1007/s10479-010-0778-y
3. **Sousa, J. P., & Wolsey, L. A. (1992).** *A time indexed formulation of non-preemptive single machine scheduling problems.* Mathematical Programming, 54(1), 353–367. https://doi.org/10.1007/BF01586059
