# Revisão, Correções e Versão Melhorada do `modelo.md`

Este documento consolida todas as correções técnicas, matemáticas, estruturais, gramaticais e bibliográficas identificadas na revisão de [`modelo.md`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/modelo.md), confrontando-o com o código-fonte atual do projeto, seguido da **versão completa revisada e corrigida** pronta para uso.

---

## Parte I — Relatório de Correções e Melhorias

### 1. Correções Fatuais e de Referência ao Código-Fonte

1. **Arquivo inexistente (`TimeInterval.h` $\to$ `TimeSlot.h`):**
   - `modelo.md` referenciava `include/model/TimeInterval.h#L16-L35`, arquivo que não existe no repositório.
   - **Correção:** As constantes `SLOT_MINUTES = 5`, `SLOTS_PER_DAY = 288` e a `struct TimeInterval` estão em [`include/model/TimeSlot.h`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/include/model/TimeSlot.h#L14-L21) (`L14-L21` e `L106-L200`).
2. **Número de intervalos na jornada da turma (`JornadaTurma`):**
   - O texto dizia *"pode ter até 3 intervalos"*, mas na frase seguinte contradizia dizendo *"Retirando os dois intervalos, a jornada se divide em até três blocos"*, e a fórmula de $\mathcal{F}_{g,d}$ mostrava apenas $[b^1_{g,d}, e^1_{g,d}) \cup [b^2_{g,d}, e^2_{g,d})$.
   - **Correção:** Em [`Turma.h`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/include/model/Turma.h#L23-L30) e [`Turma.cpp`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/model/Turma.cpp#L82-L156), o sistema suporta até **3 intervalos ativos** ($m \in \mathcal{M}_{g,d} \subseteq \{1, 2, 3\}$), dividindo a jornada em até **4 blocos letivos contínuos**.
3. **Coluna deslocada na Matriz de Rastreabilidade:**
   - Na tabela da Seção 15, a linha de *Preferência de dias ($W$)* omitia a coluna `Preprocessor.cpp` (`—`), deslocando as referências de CP-SAT e MILP para as colunas erradas.
4. **Intervalos de linhas do Warm Start:**
   - Corrigidos de `L768–L805` para [`L768–L834`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L768-L834) (CP-SAT) e de `L2218–L2275` para [`L2222–L2346`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L2222-L2346) (MILP).
5. **Definição exata de turnos ($\mathcal{K}_{d,h}$) e custo horário ($c^{hora}_{a,c}$):**
   - Explicitado que `slotToShift` ([`TimeSlot.cpp`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/model/TimeSlot.cpp#L120-L126)) particiona o dia em Manhã (`< 12:30`), Tarde (`12:30–18:00`) e Noite (`>= 18:00`), enquanto `shiftAcomodaAula` ([`Preprocessor.cpp`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L223-L240)) valida faixas permitidas com sobreposição de bordas (`07:00–13:00`, `12:00–19:00`, `18:00–23:00`, `07:00–23:00`).
   - Esclarecido que $c^{hora}_{a,c}$ atualmente aplica penalidade unitária (`+1`) para aulas iniciadas antes das `07:30` ou após as `11:00` ([`Preprocessor.cpp`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L421-L430)).

### 2. Correções Matemáticas e Aderência ao Modelo Implementado

1. **Contradição entre a Seção 12.6 e a Seção 14 na Função Objetivo:**
   - A Seção 12.6 definia a função objetivo exata do código com **6 termos** ($\lambda_J, \lambda_D, \lambda_P, \lambda_W, \lambda_T, \lambda_G$), enquanto a Seção 14 adicionava $\lambda_E \sum e_a$, $\lambda_R \sum c^{sala}_{a,c} x_{a,c}$ e pesos semestrais $\rho_s$.
   - **Correção:** Em [`OptimizationConfig.h`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/include/model/OptimizationConfig.h#L162-L192) e [`ScheduleSolver.cpp`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L758-L766), não existem $\lambda_E$, $e_a$, $\lambda_R$, $c^{sala}_{a,c}$ nem $\rho_s$. A estabilidade da grade atual é tratada exclusivamente como **Warm Start** (`AddHint` no CP-SAT, `warm_start_vals` no MILP e bônus/penalidade na Heurística Gulosa). Os termos não implementados foram removidos da formulação principal para eliminar a autocontradição.
2. **Escopo do índice semestral $s$:**
   - As restrições rígidas **R2, R3, R4, R6, R9 e R10** são efetivamente particionadas por semestre ($s \in \{1, 2\}$) quando o modo multi-semestre está ativo (`is_multi_sem == true`).
   - Entretanto, os indicadores suaves ($y_{p,d}$, $v_{p,d,h}$, $J$, $J_G$ e $W$) no código atual ([`ScheduleSolver.cpp`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L588-L755) e [`L1960–L2220`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1960-L2220)) operam sobre `vars_prof_dia` e `vars_turma_dia` sem particionamento por $s$. Isso foi documentado explicitamente na versão revisada (apontando a indexação por $s$ nos termos suaves como refinamento futuro para o modo `AMBOS_INTEGRADO`).
3. **Separação entre o implementado e extensões propostas (R12):**
   - A formulação com variáveis $\beta_{q,f}$ (consecutividade estrita em MILP) não está implementada no código, que usa apenas agrupamento no mesmo dia via $w_{q,d}$ com $(B_q, N_q) \in \{(2,1), (4,1), (4,2)\}$.
   - **Correção:** $\beta_{q,f}$ foi retirado do resumo das restrições ativas (evitando misturar variáveis inexistentes no solver com as implementadas) e mantido isoladamente como **extensão futura**, com alerta sobre a necessidade de respeitar os blocos letivos contínuos caso um bloco de 4 aulas exceda o tamanho de um único bloco entre recreios.
4. **Completude da notação e explicitação de detalhes do solver:**
   - Adicionados na tabela de conjuntos/parâmetros símbolos que apareciam sem definição prévia ($\mathcal{A}^{teo}$, $\mathcal{A}^{anual}$, $\mathcal{A}^{fix}$, $\mathcal{Q}^{consec}$, $\bar{r}_g$, $B_q$, $N_q$, $o_{i,k}$ e valores padrão de $\lambda$).
   - Precisado em **R5** que o vínculo se aplica a aulas teóricas de turma única ($|\mathcal{G}_a| = 1$, $|b_c| = 1$) e que o MILP usa a forma agregada fortalecida $\sum_{c \in \mathcal{C}_a:\, b_c=\{r\}} x_{a,c} \le z_{g,r}$.
   - Explicitado na linearização de janelas que, sob minimização com pesos não negativos, o encadeamento monotônico $O(|\mathcal{E}_i|)$ ($u^-_{i,k-1} \le u^-_{i,k}$ e $u^+_{i,k+1} \le u^+_{i,k}$) implementado no MILP substitui com equivalência ótima as $O(|\mathcal{E}_i|^2)$ restrições diretas e torna as desigualdades de teto redundantes.

### 3. Correções Estruturais, Markdown e Redação

1. **Numeração de seções e hierarquia de títulos:**
   - O documento original pulava as seções **4** e **9** (indo de 3 para 5 e de 8 para 10) e alternava `## 1.` com `# 2.`. A numeração foi corrigida sequencialmente de **1 a 14**.
2. **Blocos matemáticos `$$ ... $$`:**
   - Removidas dezenas de linhas em branco dentro de blocos `$$ ... $$`, que quebram a renderização em KaTeX e visualizadores GitHub Flavored Markdown.
3. **Revisão gramatical e ortográfica em português:**
   - Corrigidos erros como: *"Apesar de um ser"* $\to$ *"Apesar de ser"*; *"a baixo"* $\to$ *"abaixo"*; *"Professores tem"* $\to$ *"Professores têm"*; *"Salas pode"* $\to$ *"Salas podem"*; *"e etc"* $\to$ *"etc."*; *"gerado, e um dia"* $\to$ *"gerados: em um dia"*; *"esta sintetizado"* $\to$ *"estão sintetizados"*; parêntese faltando em `$\mathcal{S} = \{1, 2\}$`; espaços antes de dois-pontos; e redação da Seção 5.

### 4. Redução das Referências Bibliográficas

- O documento original listava **13 referências** na Seção 16 além de **1 citação órfã** (`[Souza, Ochi & Maculan, 2003]`, citada no texto sem constar na lista bibliográfica).
- **Redução aplicada:** A bibliografia foi reduzida para **5 artigos científicos essenciais** (redução superior a 60%), eliminando citações redundantes, citações ligadas a extensões não implementadas (`Phillips et al., 2017`; `Qualizza & Serafini, 2005`), documentação de software na lista acadêmica (`Perron & Furnon`) e a citação órfã (`Souza, Ochi & Maculan, 2003`):
  1. **Daskalaki, Birbas & Housos (2004)** — formulação inteira, durações distintas, subconjuntos/estabilidade de salas e acoplamento multi-período.
  2. **Post et al. (2012)** — padrão internacional XHSTT e taxonomia de restrições rígidas e flexíveis.
  3. **Santos et al. (2012)** — formulação MILP para horários escolares no Brasil ($y_{p,d}$, janelas, quebra de simetria inicial e aulas geminadas).
  4. **Schaerf (1999)** — revisão clássica do problema, classificação NP-difícil, pré-filtragem de domínio, pré-alocações e compacidade.
  5. **Sousa & Wolsey (1992)** — formulação indexada no tempo em slots discretos e desigualdades de clique de não sobreposição.

---

## Parte II — Versão Revisada do Modelo (`modelo.md`)

# Modelo Matemático do Escalonador de Horários

## 1. Descrição do Problema

O problema de escalonamento de horários em instituições de ensino é amplamente estudado na literatura e conta com diversas ferramentas comerciais, como [aSc TimeTables](https://www.asctimetables.com/timetables_pt.html), [PowerCubus](https://powercubus.com.br/features) e [CronosTimeTable](https://www.cronostimetable.com/). Apesar de recorrente, o problema apresenta particularidades operacionais significativas entre diferentes instituições. O **Problema de Programação de Horários Universitário e Escolar** (*High School and University Timetabling Problem*) é classificado na literatura de complexidade computacional como **NP-difícil** (*NP-hard*) [[Schaerf, 1999](#14-referências-bibliográficas)].

Visando atender às necessidades reais da instituição de ensino, o sistema contempla os seguintes requisitos:
- Aulas com duração de **45 minutos** ou **50 minutos** (generalizável para qualquer múltiplo de 5 minutos).
- Professores com disponibilidade horária definida por dia da semana e turno.
- Professores com preferências de dias da semana, turnos pretendidos e sensibilidade individual a janelas.
- Turmas com aulas nos turnos manhã/tarde ou tarde/noite, podendo ter jornadas e intervalos distintos por dia da semana.
- Salas cadastradas e classificadas por tipo (salas teóricas, laboratórios — mecânica, elétrica, informática, química, física — e ambientes especiais como ginásio ou sala de música), com capacidade máxima de alunos.
- Disciplinas semestrais ou anuais, com restrições de sala ou conjunto de salas alternativas, limite diário de aulas por turma e opção de agrupamento em bloco.

---

## 2. Definição do Modelo

### 2.1. Representação do Tempo

Como as aulas podem durar 45 ou 50 minutos, o dia é discretizado em *slots* de $\Delta = \operatorname{MDC}(45, 50) = 5$ minutos, numerados a partir da meia-noite (`00:00` = slot $0$):
- Uma aula de **45 minutos** ocupa $L_a = 9$ slots consecutivos.
- Uma aula de **50 minutos** ocupa $L_a = 10$ slots consecutivos.
- Uma aula que começa no slot $t$ e dura $L_a$ slots ocupa o intervalo semiaberto $[t, t + L_a)$, isto é, os slots $\{t, t+1, \ldots, t+L_a-1\}$. Assim, uma aula que termina às `08:45` (slot $105$) não conflita com outra que começa exatamente às `08:45` (slot $105$).

Essa abordagem permite representar qualquer duração múltipla de 5 minutos ao custo de gerenciar 288 slots diários ($0 \le k < 288$). Trata-se da **Formulação Indexada no Tempo** (*Time-Indexed Formulation*) [[Sousa & Wolsey, 1992](#14-referências-bibliográficas); [Daskalaki et al., 2004](#14-referências-bibliográficas)], implementada em [`TimeSlot.h`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/include/model/TimeSlot.h#L14-L21) (`SLOT_MINUTES = 5`, `SLOTS_PER_DAY = 288`).

### 2.2. Disciplinas Anuais e Semestrais

Todas as disciplinas atuam sobre um horizonte semestral ($\mathcal{S} \subseteq \{1, 2\}$):
- **Disciplinas anuais:** Ocorrem nos dois semestres ($\mathcal{S}_a = \{1, 2\}$) exatamente no mesmo dia, horário e sala. Utilizam uma única variável de decisão $x_{a,c}$ compartilhada entre as restrições do 1º e do 2º semestre.
- **Disciplinas semestrais:** Ocupam professor, turma e sala apenas no semestre em que estão ativas ($\mathcal{S}_a = \{1\}$ ou $\mathcal{S}_a = \{2\}$), permitindo que uma aula do 1º semestre e outra do 2º semestre reutilizem o mesmo horário e a mesma sala física sem conflito.

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
| $\mathcal{K}_d,\ k$ | Slots de 5 minutos do dia $d$ ($0 \le k < 288$) |
| $\mathcal{H},\ h$ | Turnos de trabalho (`MANHA`: $< 12\text{h}30$; `TARDE`: $12\text{h}30\text{–}18\text{h}00$; `NOITE`: $\ge 18\text{h}00$) |
| $\mathcal{K}_{d,h} \subseteq \mathcal{K}_d$ | Slots do dia $d$ pertencentes ao turno $h$ |
| $\mathcal{P}_a \subseteq \mathcal{P},\ \mathcal{G}_a \subseteq \mathcal{G}$ | Professores e turmas que participam da aula $a$ |
| $\mathcal{D}_g \subseteq \mathcal{D}$ | Dias da semana em que a turma $g$ tem aula |
| $\mathcal{S}_a \subseteq \mathcal{S}$ | Semestres em que a aula $a$ está ativa |
| $\mathcal{R}_g^{teo} \subseteq \mathcal{R}$ | Salas elegíveis para serem a sala teórica fixa da turma $g$ |
| $\mathcal{B}_a$ | Combinações de salas válidas para a aula $a$ |
| $\mathcal{C}_a,\ c$ | Conjunto e índice de candidatos viáveis gerados para a aula $a$ |
| $\mathcal{C}_a^{fix} \subseteq \mathcal{C}_a$ | Subconjunto unitário do candidato fixado para a aula $a \in \mathcal{A}^{fix}$ |

**O que é uma ocorrência de aula ($a \in \mathcal{A}$)?**
Se a disciplina de Matemática tem 4 aulas por semana, o sistema gera 4 ocorrências individuais ($a_1, a_2, a_3, a_4 \in \mathcal{A}_q$). Cada ocorrência recebe um dia, um horário inicial e uma sala. Caso as aulas ocorram sempre em pares inseparáveis, a disciplina pode ser cadastrada com duração de 90 ou 100 minutos, gerando metade das ocorrências.

**O que é um candidato ($c \in \mathcal{C}_a$)?**
Cada candidato $c \in \mathcal{C}_a$ representa uma tupla admissível $(d_c, t_c, b_c)$ para a aula $a$:
- $d_c \in \mathcal{D}$: dia da semana;
- $t_c \in \mathcal{K}_d$: slot inicial da aula;
- $b_c \subseteq \mathcal{R}$: conjunto de salas usadas simultaneamente pela aula.

---

## 3. Parâmetros de Entrada

| Símbolo | Unidade / Domínio | Significado |
|---|---|---|
| $L_a$ | Slots ($\mathbb{Z}_{>0}$) | Duração da aula $a$ em slots de 5 minutos |
| $\ell_a = 5 L_a$ | Minutos | Duração da aula $a$ em minutos |
| $T^{ini}_{g,d},\ T^{fim}_{g,d}$ | Slots | Início e fim da jornada diária da turma $g$ no dia $d$ |
| $[b^m_{g,d}, e^m_{g,d}),\ m \in \mathcal{M}_{g,d}$ | Slots | Intervalos ativos de recreio/almoço da turma $g$ no dia $d$ ($\mathcal{M}_{g,d} \subseteq \{1,2,3\}$) |
| $\mathcal{F}_{g,d} \subseteq \mathcal{K}_d$ | Conjunto de slots | Slots letivos úteis da turma $g$ no dia $d$ (jornada menos intervalos ativos) |
| $\mathcal{V}_{p,d} \subseteq \mathcal{K}_d$ | Conjunto de slots | Slots de disponibilidade do professor $p$ no dia $d$ |
| $M_{p,d}$ | Minutos ($\mathbb{Z}_{\ge 0}$) | Carga máxima diária do professor $p$ no dia $d$ ($0$ = sem limite) |
| $\omega_{p,d}$ | $\{0, \ldots, 10\}$ | Nota de preferência do professor $p$ pelo dia $d$ ($10$ = prefere muito) |
| $\alpha_p$ | $\{0, \ldots, 10\}$ | Sensibilidade individual do professor $p$ a janelas entre aulas (`pesoJanela`) |
| $\pi_{p,d,h}$ | $\{0, 1\}$ | Vale $1$ se o professor $p$ pretende trabalhar no turno $h$ do dia $d$ |
| $\bar{r}_g \in \mathcal{R}$ | Sala | Sala teórica fixa predefinida no cadastro da turma $g$ (quando houver) |
| $U_{q,g}$ | $\mathbb{Z}_{>0}$ | Limite diário de aulas da disciplina $q$ para a turma $g$ (padrão global: $2$) |
| $B_q,\ N_q$ | $\mathbb{Z}_{>0}$ | Tamanho do bloco diário ($2$ ou $4$) e nº de dias ativos ($1$ ou $2$) para $q \in \mathcal{Q}^{consec}$ |
| $c^{hora}_{a,c}$ | $\mathbb{Z}_{\ge 0}$ | Penalidade de horário do candidato $c$ ($1$ se $t_c < 07\text{h}30$ ou $t_c > 11\text{h}00$; $0$ caso contrário) |
| $\lambda_J, \lambda_D, \lambda_P, \lambda_W, \lambda_T, \lambda_G$ | $\mathbb{Z}_{\ge 0}$ | Pesos da função objetivo (padrões: $8, 5, 3, 7, 8, 2$) |

> **Importante:** Preferência baixa ($\omega_{p,d} = 0$ ou $\pi_{p,d,h} = 0$) gera apenas **penalidade** na função objetivo, não proibindo a alocação. Horários estritamente proibidos para o docente permanecem fora de $\mathcal{V}_{p,d}$.

---

## 4. Geração de Horários Candidatos ($\mathcal{C}_a$)

A jornada da turma $g$ no dia $d$ obedece à ordem cronológica entre $T^{ini}_{g,d}$ e $T^{fim}_{g,d}$, podendo conter até **3 intervalos ativos** ($\mathcal{M}_{g,d} \subseteq \{1, 2, 3\}$) que satisfazem:
$$
T^{ini}_{g,d} < b^1_{g,d} < e^1_{g,d} \le b^2_{g,d} < e^2_{g,d} \le b^3_{g,d} < e^3_{g,d} < T^{fim}_{g,d}.
$$
Retirando os intervalos ativos, a jornada se particiona em até **4 blocos letivos contínuos**, cuja união define os slots letivos úteis $\mathcal{F}_{g,d}$:
$$
\mathcal{F}_{g,d} = \{T^{ini}_{g,d}, \ldots, T^{fim}_{g,d}-1\} \setminus \bigcup_{m \in \mathcal{M}_{g,d}} [b^m_{g,d}, e^m_{g,d}).
$$

Para uma aula $a$, um candidato $c = (d_c, t_c, b_c)$ que ocupa os slots $\mathcal{I}_{a,c} = \{t_c, \ldots, t_c + L_a - 1\}$ só é gerado se satisfizer **todas** as regras abaixo (pré-filtragem de domínio segundo [[Schaerf, 1999](#14-referências-bibliográficas)], implementada em [`Preprocessor::gerarCandidatosParaAula`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L249-L481)):

1. **Acomodação na jornada sem cortar intervalos:** Todas as turmas $g \in \mathcal{G}_a$ funcionam no dia $d_c$ ($d_c \in \mathcal{D}_g$) e $\mathcal{I}_{a,c}$ cabe inteiramente dentro de um único bloco letivo contínuo de $\mathcal{F}_{g,d_c}$ ([`turmaAcomodaAula`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L199-L214)).
2. **Disponibilidade docente:** Todos os professores $p \in \mathcal{P}_a$ estão disponíveis em todo o intervalo ($\mathcal{I}_{a,c} \subseteq \mathcal{V}_{p,d_c}$), correspondendo à `PreferTimesConstraint` (Hard) no padrão XHSTT [[Post et al., 2012](#14-referências-bibliográficas)] ([`Preprocessor.cpp`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L410-L419)).
3. **Turno permitido:** $\mathcal{I}_{a,c}$ está contido em ao menos um dos turnos autorizados para a disciplina (`MANHA`: `07:00–13:00`, `TARDE`: `12:00–19:00`, `NOITE`: `18:00–23:00`, `INTEGRAL`: `07:00–23:00`), ou a lista de turnos permitidos é vazia ([`shiftAcomodaAula`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L223-L240)).
4. **Salas compatíveis:** A combinação $b_c \in \mathcal{B}_a$ é válida para o tipo da aula; em aulas teóricas, $b_c = \{r\}$ pertence às salas teóricas elegíveis da turma ([Daskalaki et al., 2004](#14-referências-bibliográficas); [`Preprocessor.cpp`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L302-L336)).
5. **Fixação manual (*Pinned*):** Para $a \in \mathcal{A}^{fix}$, gera-se exclusivamente o candidato correspondente ao dia, horário e sala fixados pelo usuário ([`Preprocessor.cpp`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L338-L351)).

---

## 5. Variáveis de Decisão

| Variável | Domínio | O que representa |
|---|---|---|
| $x_{a,c}$ | $\{0,1\}$ | Vale $1$ se o candidato $c \in \mathcal{C}_a$ for escolhido para a aula $a$ |
| $z_{g,r}$ | $\{0,1\}$ | Vale $1$ se a sala $r \in \mathcal{R}_g^{teo}$ for a sala teórica fixa da turma $g$ |
| $y_{p,d}$ | $\{0,1\}$ | Vale $1$ se o professor $p$ lecionar ao menos uma aula no dia $d$ |
| $v_{p,d,h}$ | $\{0,1\}$ | Vale $1$ se o professor $p$ trabalhar no turno $h$ do dia $d$ |
| $o^P_{p,d,k,s}$ | $[0,1]$ | Ocupação do professor $p$ no slot $k$, dia $d$ e semestre $s$ |
| $o^G_{g,d,k,s}$ | $[0,1]$ | Ocupação da turma $g$ no slot $k$, dia $d$ e semestre $s$ |
| $m_{p,d,s}$ | $\mathbb{R}_{\ge 0}$ | Carga em minutos do professor $p$ no dia $d$ e semestre $s$ |
| $u^-_{i,k},\ u^+_{i,k}$ | $\{0,1\}$ | Indicam se há aula **antes** ($u^-$) e **depois** ($u^+$) do slot $k$ na sequência $i$ |
| $j_{i,k}$ | $[0,1]$ | Vale $1$ se o slot $k$ for uma **janela** (slot vago entre aulas) na sequência $i$ |
| $w_{q,d}$ | $\{0,1\}$ | Vale $1$ se a disciplina $q \in \mathcal{Q}^{consec}$ tiver seu bloco alocado no dia $d$ |

---

## 6. Expressões Auxiliares de Ocupação

Para indicar se o candidato $c \in \mathcal{C}_a$ ocupa o slot $k$ do dia $d$, define-se a constante:
$$
H_{a,c,d,k} = \begin{cases}
1, & \text{se } d_c = d \text{ e } t_c \le k < t_c + L_a, \\
0, & \text{caso contrário}.
\end{cases}
$$

As ocupações do professor $p$ e da turma $g$ no dia $d$, slot $k$ e semestre $s$ são dadas por somas lineares:
$$
o^P_{p,d,k,s} = \sum_{a \in \mathcal{A}:\, p \in \mathcal{P}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,k}\, x_{a,c},
\qquad
o^G_{g,d,k,s} = \sum_{a \in \mathcal{A}:\, g \in \mathcal{G}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,k}\, x_{a,c}.
$$

Definem-se ainda o conjunto de pares $(a,c)$ do professor $p$ no dia $d$:
$$
\mathcal{X}_{p,d} = \{(a,c) : a \in \mathcal{A},\ p \in \mathcal{P}_a,\ c \in \mathcal{C}_a,\ d_c = d\},
$$
seu subconjunto por semestre $\mathcal{X}_{p,d,s} = \{(a,c) \in \mathcal{X}_{p,d} : s \in \mathcal{S}_a\}$ e seu subconjunto por turno $\mathcal{X}_{p,d,h} = \{(a,c) \in \mathcal{X}_{p,d} : t_c \in \mathcal{K}_{d,h}\}$.

---

## 7. Restrições do Modelo

### R1. Alocação única de cada aula
$$
\sum_{c \in \mathcal{C}_a} x_{a,c} = 1 \qquad \forall a \in \mathcal{A}.
$$
- **Literatura:** `AssignTimeConstraint` e `AssignResourceConstraint` no padrão XHSTT [[Post et al., 2012](#14-referências-bibliográficas); [Schaerf, 1999](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`ScheduleSolver.cpp` (L282–L290)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L282-L290); MILP [`ScheduleSolver.cpp` (L1736–L1745)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1736-L1745).

### R2. Não sobreposição de professores
$$
o^P_{p,d,k,s} \le 1 \qquad \forall p \in \mathcal{P},\ d \in \mathcal{D},\ k \in \mathcal{K}_d,\ s \in \mathcal{S}.
$$
- **Literatura:** `AvoidClashesConstraint` [[Post et al., 2012](#14-referências-bibliográficas)] e cliques indexadas no tempo [[Sousa & Wolsey, 1992](#14-referências-bibliográficas)].
- **Código:** CP-SAT (`NewOptionalIntervalVar` + `AddNoOverlap`) [`L292–L321`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L292-L321); MILP (`add_no_overlap_cliques`) [`L1747–L1791`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1747-L1791); Gulosa [`L1094–L1100`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1094-L1100).

### R3. Não sobreposição de turmas
$$
o^G_{g,d,k,s} \le 1 \qquad \forall g \in \mathcal{G},\ d \in \mathcal{D},\ k \in \mathcal{K}_d,\ s \in \mathcal{S}.
$$
- **Literatura:** `AvoidClashesConstraint` [[Post et al., 2012](#14-referências-bibliográficas); [Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`L352–L379`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L352-L379); MILP [`L1747–L1791`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1747-L1791); Gulosa [`L1102–L1106`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1102-L1106).

### R4. Não sobreposição de salas
$$
\sum_{a \in \mathcal{A}:\, s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a:\, r \in b_c} H_{a,c,d,k}\, x_{a,c} \le 1 \qquad \forall r \in \mathcal{R},\ d \in \mathcal{D},\ k \in \mathcal{K}_d,\ s \in \mathcal{S}.
$$
- **Literatura:** `AvoidClashesConstraint` [[Post et al., 2012](#14-referências-bibliográficas); [Daskalaki et al., 2004](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`L323–L350`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L323-L350); MILP [`L1747–L1791`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1747-L1791); Gulosa [`L1153–L1159`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1153-L1159).

### R5. Sala teórica fixa por turma
Cada turma $g$ utiliza uma única sala teórica fixa durante a semana para suas aulas teóricas de turma única ($|\mathcal{G}_a|=1$, $|b_c|=1$):
$$
\sum_{r \in \mathcal{R}_g^{teo}} z_{g,r} = 1 \quad \forall g \in \mathcal{G}, \qquad
\sum_{c \in \mathcal{C}_a:\, b_c = \{r\}} x_{a,c} \le z_{g,r} \quad \forall a \in \mathcal{A}^{teo}\ (|\mathcal{G}_a|=1),\ g \in \mathcal{G}_a,\ r \in \mathcal{R}_g^{teo}.
$$
Se a turma possuir sala predefinida $\bar{r}_g$, impõe-se $z_{g,\bar{r}_g} = 1$. Aulas práticas ou de laboratório não estão sujeitas a esta regra.
- **Literatura:** Estabilidade e particionamento de salas teóricas vs. laboratórios [[Daskalaki et al., 2004](#14-referências-bibliográficas); [Post et al., 2012](#14-referências-bibliográficas)].
- **Código:** Pré-processador [`L304–L315`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L304-L315); CP-SAT [`L245–L278`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L245-L278); MILP (forma agregada fortalecida) [`L1696–L1734`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1696-L1734).

### R6. Limite diário de minutos por professor
Para todo professor com teto $M_{p,d} > 0$:
$$
m_{p,d,s} = \sum_{(a,c) \in \mathcal{X}_{p,d,s}} \ell_a\, x_{a,c} \le M_{p,d} \qquad \forall p \in \mathcal{P},\ d \in \mathcal{D},\ s \in \mathcal{S}.
$$
- **Literatura:** `LimitWorkloadConstraint` no padrão XHSTT [[Post et al., 2012](#14-referências-bibliográficas); [Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`L420–L441`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L420-L441); MILP [`L1836–L1860`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1836-L1860).

### R7. Aulas fixadas manualmente (*Pinned*)
Para aulas fixadas pelo usuário ($a \in \mathcal{A}^{fix}$), $\mathcal{C}_a = \mathcal{C}_a^{fix}$ contém apenas o candidato fixado, garantindo por R1:
$$
\sum_{c \in \mathcal{C}_a^{fix}} x_{a,c} = 1 \qquad \forall a \in \mathcal{A}^{fix}.
$$
- **Literatura:** *Preassignments* [[Schaerf, 1999](#14-referências-bibliográficas); [Post et al., 2012](#14-referências-bibliográficas)].
- **Código:** Pré-processador [`L338–L351`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L338-L351); Gulosa [`L1031`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1031).

### R8. Sincronização de disciplinas anuais entre semestres
Para $a \in \mathcal{A}^{anual}$, tem-se $\mathcal{S}_a = \mathcal{S}$ e a mesma variável $x_{a,c}$ participa simultaneamente das restrições de ambos os semestres.
- **Literatura:** Acoplamento multi-período por compartilhamento de variáveis [[Daskalaki et al., 2004](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`L180–L243`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L180-L243); MILP [`L1616–L1694`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1616-L1694).

### R9. Primeira aula do dia deve começar no início da jornada
Se uma turma $g$ tem aula no dia $d$ e semestre $s$, sua primeira aula deve iniciar em $T^{ini}_{g,d}$. Seja:
$$
B_{g,d,s} = \sum_{a \in \mathcal{A}:\, g \in \mathcal{G}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a:\, d_c = d,\ t_c = T^{ini}_{g,d}} x_{a,c}.
$$
Para cada aula $a$ da turma no dia que inicie após $T^{ini}_{g,d}$:
$$
\sum_{c \in \mathcal{C}_a:\, d_c = d,\ t_c > T^{ini}_{g,d}} x_{a,c} \le B_{g,d,s} \qquad \forall g \in \mathcal{G},\ d \in \mathcal{D}_g,\ s \in \mathcal{S},\ \forall a \text{ com } g \in \mathcal{G}_a,\ s \in \mathcal{S}_a.
$$
- **Literatura:** Quebra de simetria temporal e eliminação de ociosidade inicial [[Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`L381–L418`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L381-L418); MILP [`L1793–L1834`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1793-L1834); Gulosa [`L1129–L1151`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1129-L1151).

### R10. Preenchimento integral da jornada *(opcional)*
Quando `preenchimento_integral_jornada` está ativo, todos os slots letivos $\mathcal{F}_{g,d}$ da turma devem estar ocupados:
$$
o^G_{g,d,k,s} = 1 \qquad \forall g \in \mathcal{G},\ d \in \mathcal{D}_g,\ k \in \mathcal{F}_{g,d},\ s \in \mathcal{S}.
$$
- **Literatura:** *Complete / Compact Class Timetables* [[Schaerf, 1999](#14-referências-bibliográficas); [Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** Validação [`Preprocessor.cpp` (L155–L184)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L155-L184); CP-SAT [`L443–L474`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L443-L474); MILP [`L1862–L1893`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1862-L1893).

### R11. Limite máximo de aulas da mesma disciplina por dia *(opcional)*
Quando `distribuir_disciplinas_na_semana` está ativo:
$$
\sum_{a \in \mathcal{A}_q} \sum_{c \in \mathcal{C}_a:\, d_c = d} x_{a,c} \le U_{q,g} \qquad \forall q \in \mathcal{Q},\ g \in \mathcal{G}_q,\ d \in \mathcal{D}.
$$
- **Literatura:** `DistributeSplitEventsConstraint` / `SpreadEventsConstraint` no XHSTT [[Post et al., 2012](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`L476–L499`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L476-L499); MILP [`L1895–L1921`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1895-L1921); Gulosa [`L1107–L1115`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1107-L1115).

### R12. Agrupamento de aulas em bloco *(opcional)*
Para disciplinas com `exigirAulasConsecutivas = true` e $|\mathcal{A}_q| \in \{2, 4, 8\}$, define-se $(B_q, N_q) \in \{(2,1), (4,1), (4,2)\}$ e impõe-se o agrupamento em $N_q$ dias com exatamente $B_q$ aulas no mesmo dia:
$$
\sum_{a \in \mathcal{A}_q} \sum_{c \in \mathcal{C}_a:\, d_c = d} x_{a,c} = B_q\, w_{q,d} \quad \forall d \in \mathcal{D}, \qquad
\sum_{d \in \mathcal{D}} w_{q,d} = N_q \qquad \forall q \in \mathcal{Q}^{consec}.
$$
- **Literatura:** *Double Lessons* e agrupamento diário [[Santos et al., 2012](#14-referências-bibliográficas); [Daskalaki et al., 2004](#14-referências-bibliográficas)].
- **Código:** CP-SAT [`L501–L573`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L501-L573); MILP [`L1923–L1955`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1923-L1955); Gulosa [`L1119–L1127`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1119-L1127).

> **Nota sobre extensão futura (consecutividade estrita em slots):** A implementação atual garante que as $B_q$ aulas ocorram no mesmo dia ($w_{q,d}$), sendo a adjacência dentro do dia incentivada pela minimização de lacunas ($J_G$) e janelas ($J$). Caso se deseje impor adjacência estrita em MILP via variáveis de configuração de bloco $\beta_{q,f} \in \{0,1\}$, deve-se enumerar apenas sequências que respeitem os blocos letivos contínuos de $\mathcal{F}_{g,d}$ (ou permitir passagem pelo intervalo obrigatório quando $B_q \cdot \ell_a$ exceder o maior bloco contínuo da turma).

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

> **Nota de escopo multi-semestral:** No código atual, esses indicadores agregam sobre $\mathcal{X}_{p,d}$ e $\mathcal{X}_{p,d,h}$ sem separação por semestre $s$. Em uma extensão multi-semestral rigorosa para os termos suaves, basta indexar $y_{p,d,s}$ e $v_{p,d,h,s}$ sobre $\mathcal{X}_{p,d,s}$ e $\mathcal{X}_{p,d,h,s}$.

- **Literatura:** `ClusterBusyTimesConstraint` no padrão XHSTT [[Post et al., 2012](#14-referências-bibliográficas); [Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** $y_{p,d}$ em [`L587–L603`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L587-L603) (CP-SAT) e [`L1959–L1995`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1959-L1995) (MILP); $v_{p,d,h}$ (`y_shift`) em [`L652–L664`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L652-L664) (CP-SAT) e [`L2015–L2034`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L2015-L2034) (MILP).

---

## 9. Cálculo Linear de Janelas (Horários Vagos entre Aulas)

Uma **janela** é um slot vazio situado estritamente **entre** duas aulas no mesmo período de análise. Slots livres antes da primeira aula ou após a última não são janelas:
- **Para professores:** As janelas são medidas dentro de cada turno $i = (p, d, h)$ nos slots $\mathcal{E}_i = \mathcal{K}_{d,h}$, com ocupação $o_{i,k} = \sum_{(a,c) \in \mathcal{X}_{p,d,h}} H_{a,c,d,k}\, x_{a,c}$. Assim, o intervalo entre turnos distintos não é penalizado.
- **Para turmas:** As lacunas são medidas nos slots letivos da jornada $i = (g, d)$ em $\mathcal{E}_i = \mathcal{F}_{g,d}$ (excluindo os recreios obrigatórios), com ocupação $o_{i,k} = \sum_{a:\, g \in \mathcal{G}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,k}\, x_{a,c}$.

Para que $j_{i,k} = 1$ quando houver aula antes de $k$ ($u^-_{i,k} = 1$), aula depois de $k$ ($u^+_{i,k} = 1$) e o slot $k$ estiver vazio ($o_{i,k} = 0$), utiliza-se a linearização:
$$
u^-_{i,k} \ge o_{i,l} \quad \forall l \in \mathcal{E}_i:\, l < k, \qquad
u^+_{i,k} \ge o_{i,l} \quad \forall l \in \mathcal{E}_i:\, l > k,
$$
$$
j_{i,k} \ge u^-_{i,k} + u^+_{i,k} - o_{i,k} - 1 \qquad \forall k \in \mathcal{E}_i.
$$
Como o objetivo **minimiza** $j_{i,k}$ com coeficiente não negativo e $j_{i,k} \ge 0$, essas desigualdades bastam para garantir que $j_{i,k}$ assuma o valor exato na solução ótima. No MILP, as desigualdades diretas sobre $u^-$ e $u^+$ são substituídas de forma equivalente pelo encadeamento monotônico $O(|\mathcal{E}_i|)$:
$$
u^-_{i,k-1} \le u^-_{i,k}, \qquad u^+_{i,k+1} \le u^+_{i,k},
$$
reduzindo drasticamente o número de linhas na matriz esparsa.

- **Literatura:** `LimitIdleTimesConstraint` e linearização de janelas [[Post et al., 2012](#14-referências-bibliográficas); [Santos et al., 2012](#14-referências-bibliográficas)].
- **Código:** Janelas docentes ($J$) em [`L667–L710`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L667-L710) (CP-SAT) e [`L2036–L2124`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L2036-L2124) (MILP); Lacunas de turmas ($J_G$) em [`L712–L755`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L712-L755) (CP-SAT) e [`L2126–L2220`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L2126-L2220) (MILP).

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

### 10.5. Janelas de professores ($J_{slots}$) e lacunas de turmas ($J_{G,slots}$)
$$
J_{slots} = \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} \sum_{h \in \mathcal{H}} \alpha_p \sum_{k \in \mathcal{K}_{d,h}} j_{(p,d,h),k}, \qquad
J_{G,slots} = \sum_{g \in \mathcal{G}} \sum_{d \in \mathcal{D}_g} \sum_{k \in \mathcal{F}_{g,d}} j_{(g,d),k}.
$$

### 10.6. Expressão Global da Função Objetivo

Implementada identicamente em CP-SAT ([`ScheduleSolver.cpp` (L758–L766)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L758-L766)) e nos motores MILP — SCIP, HiGHS e cuOpt ([`ScheduleSolver.cpp` (L1655–L2220)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1655-L2220)):
$$
\boxed{\min Z = \lambda_J\, J_{slots} + \lambda_D\, D_{prof} + \lambda_P\, P_{hora} + \lambda_W\, W + 10\,\lambda_T\, T_{prof} + 5\,\lambda_G\, J_{G,slots}}
$$
onde o fator $10$ equilibra a escala da penalidade por turno e o fator $\Delta = 5$ converte os slots de lacuna da turma em minutos. A preservação da grade atual em tela (`usar_distribuicao_como_entrada`) atua como solução inicial (*Warm Start*) sem alterar os pesos de $Z$.

---

## 11. Exemplo Prático de Slots, Intervalos e Janelas

Considerando slots de 5 minutos a partir de `00:00`:
- **Aula 1 (45 min = 9 slots):** começa às `07:00` (slot $84$) e ocupa $[84, 93)$, terminando às `07:45` (slot $93$).
- **Aula 2 (50 min = 10 slots):** se começar às `07:45` (slot $93$), ocupa $[93, 103)$ e termina às `08:35` (slot $103$). Como $93 = 84 + 9$, as aulas são consecutivas com **0 minutos de janela**.
- Se a Aula 2 começar às `08:00` (slot $96$), os slots $93, 94, 95$ ficam vagos entre elas, gerando **3 slots (15 minutos) de janela**.
- Se a turma possuir intervalo obrigatório das `08:20` às `08:35` (slots $[100, 103)$), o candidato das `07:45` é **eliminado no pré-processamento**, pois $[93, 103)$ invade o intervalo da turma.

---

## 12. Formulação Matemática Integrada (Resumo Completo)

$$
\min Z = \lambda_J \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} \sum_{h \in \mathcal{H}} \alpha_p \sum_{k \in \mathcal{K}_{d,h}} j_{(p,d,h),k}
+ \lambda_D \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} y_{p,d}
+ \lambda_P \sum_{a \in \mathcal{A}} \sum_{c \in \mathcal{C}_a} c^{hora}_{a,c}\, x_{a,c}
+ \lambda_W W
+ 10\,\lambda_T \sum_{p \in \mathcal{P}} \sum_{d \in \mathcal{D}} \sum_{h \in \mathcal{H}} (1 - \pi_{p,d,h})\, v_{p,d,h}
+ 5\,\lambda_G \sum_{g \in \mathcal{G}} \sum_{d \in \mathcal{D}_g} \sum_{k \in \mathcal{F}_{g,d}} j_{(g,d),k}
$$

sujeito a:
$$
\begin{aligned}
o^P_{p,d,k,s} &= \sum_{a \in \mathcal{A}:\, p \in \mathcal{P}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,k}\, x_{a,c} && \forall p, d, k, s \\
o^G_{g,d,k,s} &= \sum_{a \in \mathcal{A}:\, g \in \mathcal{G}_a,\ s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a} H_{a,c,d,k}\, x_{a,c} && \forall g, d, k, s \\
m_{p,d,s} &= \sum_{(a,c) \in \mathcal{X}_{p,d,s}} \ell_a\, x_{a,c} && \forall p, d, s \\[6pt]
\sum_{c \in \mathcal{C}_a} x_{a,c} &= 1 && \forall a \in \mathcal{A} \\
o^P_{p,d,k,s} &\le 1 && \forall p, d, k, s \\
o^G_{g,d,k,s} &\le 1 && \forall g, d, k, s \\
\sum_{a \in \mathcal{A}:\, s \in \mathcal{S}_a} \sum_{c \in \mathcal{C}_a:\, r \in b_c} H_{a,c,d,k}\, x_{a,c} &\le 1 && \forall r, d, k, s \\
\sum_{r \in \mathcal{R}_g^{teo}} z_{g,r} &= 1 && \forall g \in \mathcal{G} \\
\sum_{c \in \mathcal{C}_a:\, b_c = \{r\}} x_{a,c} &\le z_{g,r} && \forall a \in \mathcal{A}^{teo}\ (|\mathcal{G}_a|=1),\ g \in \mathcal{G}_a,\ r \in \mathcal{R}_g^{teo} \\
z_{g,\bar{r}_g} &= 1 && \forall g \in \mathcal{G} \text{ com sala predefinida } \bar{r}_g \\
m_{p,d,s} &\le M_{p,d} && \forall p \text{ com } M_{p,d}>0,\ d, s \\
\sum_{c \in \mathcal{C}_a:\, d_c = d,\ t_c > T^{ini}_{g,d}} x_{a,c} &\le \sum_{a' \in \mathcal{A}:\, g \in \mathcal{G}_{a'},\, s \in \mathcal{S}_{a'}} \sum_{c' \in \mathcal{C}_{a'}:\, d_{c'}=d,\ t_{c'}=T^{ini}_{g,d}} x_{a',c'} && \forall g, d \in \mathcal{D}_g, s,\ \forall a \text{ com } g \in \mathcal{G}_a,\ s \in \mathcal{S}_a \\[6pt]
o^G_{g,d,k,s} &= 1 && \forall g, d \in \mathcal{D}_g, k \in \mathcal{F}_{g,d}, s \quad \text{(se R10 ativa)} \\
\sum_{a \in \mathcal{A}_q} \sum_{c \in \mathcal{C}_a:\, d_c = d} x_{a,c} &\le U_{q,g} && \forall q, g \in \mathcal{G}_q, d \quad \text{(se R11 ativa)} \\
\sum_{a \in \mathcal{A}_q} \sum_{c \in \mathcal{C}_a:\, d_c = d} x_{a,c} &= B_q\, w_{q,d},\quad \sum_{d \in \mathcal{D}} w_{q,d} = N_q && \forall q \in \mathcal{Q}^{consec},\ d \in \mathcal{D} \quad \text{(se R12 ativa)} \\[6pt]
x_{a,c} \le y_{p,d}, \quad y_{p,d} &\le \sum_{(a',c') \in \mathcal{X}_{p,d}} x_{a',c'} && \forall p, d,\ \forall (a,c) \in \mathcal{X}_{p,d} \\
x_{a,c} \le v_{p,d,h}, \quad v_{p,d,h} &\le \sum_{(a',c') \in \mathcal{X}_{p,d,h}} x_{a',c'} && \forall p, d, h,\ \forall (a,c) \in \mathcal{X}_{p,d,h} \\
u^-_{i,k} \ge o_{i,l}\ (\forall l < k), \quad u^+_{i,k} &\ge o_{i,l}\ (\forall l > k), \quad j_{i,k} \ge u^-_{i,k} + u^+_{i,k} - o_{i,k} - 1 && \forall i,\ \forall k \in \mathcal{E}_i \\[6pt]
x_{a,c}, z_{g,r}, y_{p,d}, v_{p,d,h}, w_{q,d}, u^-_{i,k}, u^+_{i,k} &\in \{0, 1\}, \qquad j_{i,k}, o^P_{p,d,k,s}, o^G_{g,d,k,s} \in [0, 1], \qquad m_{p,d,s} \ge 0.
\end{aligned}
$$

---

## 13. Matriz de Rastreabilidade (Modelo $\leftrightarrow$ Literatura $\leftrightarrow$ Código-Fonte)

| Elemento / Restrição | Padrão XHSTT / Literatura Base | Pré-processador (`Preprocessor.cpp` / `TimeSlot.h`) | CP-SAT (`ScheduleSolver.cpp`) | MILP — SCIP, HiGHS e cuOpt (`ScheduleSolver.cpp`) |
| :--- | :--- | :--- | :--- | :--- |
| **Discretização em slots ($\Delta = 5\text{ min}$)** | *Time-Indexed* [[Sousa & Wolsey, 1992](#14-referências-bibliográficas); [Daskalaki et al., 2004](#14-referências-bibliográficas)] | [`TimeSlot.h` (L14–L21)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/include/model/TimeSlot.h#L14-L21) | [`L215–L220`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L215-L220) | [`L1652–L1672`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1652-L1672) |
| **Filtro $\mathcal{C}_a$ — Jornada e intervalos** | Pré-filtragem de domínio [[Schaerf, 1999](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L199–L214, L361–L396)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L199-L214) | Implícito em $\mathcal{C}_a$ | Implícito em $\mathcal{C}_a$ |
| **Filtro $\mathcal{C}_a$ — Disponibilidade docente** | `PreferTimesConstraint` (Hard) [[Post et al., 2012](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L410–L419)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L410-L419) | Implícito em $\mathcal{C}_a$ | Implícito em $\mathcal{C}_a$ |
| **Filtro $\mathcal{C}_a$ — Turnos e salas** | `AssignResourceConstraint` [[Daskalaki et al., 2004](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L223–L240, L302–L336)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L223-L240) | Implícito em $\mathcal{C}_a$ | Implícito em $\mathcal{C}_a$ |
| **R1. Alocação única de cada aula** | `AssignTimeConstraint` [[Post et al., 2012](#14-referências-bibliográficas); [Schaerf, 1999](#14-referências-bibliográficas)] | — | [`L282–L290`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L282-L290) | [`L1736–L1745`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1736-L1745) |
| **R2. Não sobreposição de professores** | `AvoidClashesConstraint` [[Post et al., 2012](#14-referências-bibliográficas); [Sousa & Wolsey, 1992](#14-referências-bibliográficas)] | — | [`L292–L321`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L292-L321) (`AddNoOverlap`) | [`L1747–L1791`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1747-L1791) (Cliques) |
| **R3. Não sobreposição de turmas** | `AvoidClashesConstraint` [[Post et al., 2012](#14-referências-bibliográficas); [Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L352–L379`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L352-L379) (`AddNoOverlap`) | [`L1747–L1791`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1747-L1791) (Cliques) |
| **R4. Não sobreposição de salas** | `AvoidClashesConstraint` [[Post et al., 2012](#14-referências-bibliográficas); [Daskalaki et al., 2004](#14-referências-bibliográficas)] | — | [`L323–L350`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L323-L350) (`AddNoOverlap`) | [`L1747–L1791`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1747-L1791) (Cliques) |
| **R5. Sala teórica fixa ($z_{g,r}$)** | *Room Stability* [[Daskalaki et al., 2004](#14-referências-bibliográficas); [Post et al., 2012](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L304–L315)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L304-L315) | [`L245–L278`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L245-L278) | [`L1696–L1734`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1696-L1734) |
| **R6. Carga máxima diária ($M_{p,d}$)** | `LimitWorkloadConstraint` [[Post et al., 2012](#14-referências-bibliográficas); [Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L420–L441`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L420-L441) | [`L1836–L1860`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1836-L1860) |
| **R7. Aulas fixadas (*Pinned*)** | *Preassignments* [[Schaerf, 1999](#14-referências-bibliográficas); [Post et al., 2012](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L338–L351)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L338-L351) | Garantido por $\lvert\mathcal{C}_a\rvert = 1$ + R1 | Garantido por $\lvert\mathcal{C}_a\rvert = 1$ + R1 |
| **R8. Sincronização anual** | Acoplamento multi-período [[Daskalaki et al., 2004](#14-referências-bibliográficas)] | — | [`L180–L243`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L180-L243) | [`L1616–L1694`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1616-L1694) |
| **R9. Início na abertura ($T^{ini}_{g,d}$)** | Quebra de simetria [[Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L381–L418`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L381-L418) | [`L1793–L1834`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1793-L1834) |
| **R10. Preenchimento integral** | *Compact Class Schedules* [[Schaerf, 1999](#14-referências-bibliográficas); [Santos et al., 2012](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L155–L184)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L155-L184) | [`L443–L474`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L443-L474) | [`L1862–L1893`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1862-L1893) |
| **R11. Limite diário ($U_{q,g}$)** | `DistributeSplitEventsConstraint` [[Post et al., 2012](#14-referências-bibliográficas)] | — | [`L476–L499`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L476-L499) | [`L1895–L1921`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1895-L1921) |
| **R12. Agrupamento em bloco ($w_{q,d}$)** | *Double Lessons* [[Santos et al., 2012](#14-referências-bibliográficas); [Daskalaki et al., 2004](#14-referências-bibliográficas)] | — | [`L501–L573`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L501-L573) | [`L1923–L1955`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1923-L1955) |
| **Seção 8 & 10.1. Dias ($y_{p,d}, D_{prof}$)** | `ClusterBusyTimesConstraint` [[Post et al., 2012](#14-referências-bibliográficas); [Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L587–L606`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L587-L606) | [`L1959–L1995`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1959-L1995) |
| **Seção 10.2. Preferência horária ($P_{hora}$)** | `PreferTimesConstraint` (Soft) [[Post et al., 2012](#14-referências-bibliográficas)] | [`Preprocessor.cpp` (L421–L430)](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/Preprocessor.cpp#L421-L430) | [`L624–L629`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L624-L629) | [`L1656–L1658`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1656-L1658) |
| **Seção 10.3. Preferência de dias ($W$)** | Preferências docentes [[Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L608–L619`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L608-L619) | [`L1659–L1667, L1969–L1971`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L1659-L1667) |
| **Seção 10.4. Turnos ($v_{p,d,h}, T_{prof}$)** | Preferência por turno [[Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L651–L665`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L651-L665) | [`L2015–L2034`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L2015-L2034) |
| **Seção 9 & 10.5. Janelas ($J, J_G$)** | `LimitIdleTimesConstraint` [[Post et al., 2012](#14-referências-bibliográficas); [Santos et al., 2012](#14-referências-bibliográficas)] | — | [`L667–L755`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L667-L755) | [`L2036–L2220`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L2036-L2220) |
| **Warm Start (grade atual em tela)** | Solução inicial primal | — | [`L768–L834`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L768-L834) (`AddHint`) | [`L2222–L2346`](file:///home/riad-mattos-nassiffe/projetos/escalonadorHorario/src/solver/ScheduleSolver.cpp#L2222-L2346) (`warm_start_vals`) |

---

## 14. Referências Bibliográficas

1. **Daskalaki, S., Birbas, T., & Housos, E. (2004).** *An integer programming formulation for a case study in university timetabling.* European Journal of Operational Research, 153(1), 117–135. https://doi.org/10.1016/S0377-2217(03)00103-6
2. **Post, G., Ahmadi, S., Daskalaki, S., Kingston, J. H., Kyngas, J., Nurmi, C., & Ranson, D. (2012).** *An XML format for benchmarks in High School Timetabling.* Annals of Operations Research, 194(1), 385–397. https://doi.org/10.1007/s10479-010-0699-9
3. **Santos, H. G., Uchoa, E., Ochi, L. S., & Maculan, N. (2012).** *Strong bounds with cut and column generation for class-teacher timetabling.* Annals of Operations Research, 194(1), 399–412. https://doi.org/10.1007/s10479-010-0778-y
4. **Schaerf, A. (1999).** *A survey of automated timetabling.* Artificial Intelligence Review, 13(2), 87–127. https://doi.org/10.1023/A:1006576209967
5. **Sousa, J. P., & Wolsey, L. A. (1992).** *A time indexed formulation of non-preemptive single machine scheduling problems.* Mathematical Programming, 54(1), 353–367. https://doi.org/10.1007/BF01586059
