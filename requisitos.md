# Requisitos Implementados no Sistema (requisitos.md)

Este documento descreve detalhadamente todos os requisitos funcionais, não-funcionais, regras matemáticas e restrições implementados na base de código do **Escalonador Inteligente de Horários Escolares**.

---

## 1. Visão Geral e Paradigma do Sistema

O sistema tem como objetivo solucionar o **Problema de Programação de Horários Escolar e Universitário** (*High School and University Timetabling Problem*), classificado computacionalmente como **NP-difícil**.

A resolução combina formulações exatas baseadas em **Programação por Restrições (CP-SAT)** e **Programação Linear Inteira Mista (MILP)** com **Heurísticas Construtivas Gulosas** e **Meta-heurísticas Estocásticas (Algoritmo Genético)**.

---

## 2. Requisitos de Modelagem Temporal e Estruturas de Dados

### 2.1. Discretização Temporal em Slots de 5 Minutos
* **Granularidade Base ($\Delta = 5\text{ min}$):** Obtida a partir do Máximo Divisor Comum entre as durações típicas de aula:
  $$\Delta = \operatorname{MDC}(45, 50) = 5 \text{ minutos}$$
* **Mapeamento Diário:** O dia letivo (00:00 às 24:00) é particionado em $N_{\text{slots}} = \frac{24 \times 60}{5} = 288$ slots discretos, indexados de $0$ a $287$.
* **Conversão de Horários:**
  $$\text{slot}(h, m) = \frac{h \times 60 + m}{5}$$
* **Duração de Aulas:**
  * Aulas de 45 minutos: ocupam exatamente $9$ slots contínuos.
  * Aulas de 50 minutos: ocupam exatamente $10$ slots contínuos.
  * O sistema aceita qualquer duração múltipla de 5 minutos (ex: 90 min = 18 slots, 100 min = 20 slots).

### 2.2. Regimes Curriculares e Semestres Acadêmicos
* **Regimes Suportados:**
  * **ANUAL:** Disciplina que vigora durante todo o ano letivo (ambos os semestres).
  * **SEMESTRAL:** Disciplina ofertada especificamente no 1º Semestre ou no 2º Semestre.
* **Escopos de Otimização Semestral:**
  1. *Todas as Disciplinas (Geral):* Otimização irrestrita unificada.
  2. *1º Semestre:* Otimiza apenas disciplinas anuais e semestrais do 1º semestre.
  3. *2º Semestre:* Otimiza apenas disciplinas anuais e semestrais do 2º semestre.
  4. *Ambos os Semestres (Multi-Semestre Integrado):* Otimização simultânea dos dois semestres.
* **Sincronização Intersemestral de Disciplinas Anuais:**
  * Quando habilitada, garante que disciplinas anuais ocupem os mesmos horários, dias e salas no 1º e no 2º semestre, permitindo que as disciplinas semestrais preencham os horários complementares.

### 2.3. Modelagem de Turmas e Jornadas Diárias
* **Jornada Base por Turma:**
  * Intervalo total da jornada diária: $[T^{\text{ini}}, T^{\text{fim}})$.
  * Suporte a até 3 intervalos de descanso/recreio/almoço por dia: $[b^{(1)}, e^{(1)})$, $[b^{(2)}, e^{(2)})$, $[b^{(3)}, e^{(3)})$, com ativação e horários independentes.
  * **Condição de Consistência Temporal:**
    $$T^{\text{ini}} < b^{(1)} < e^{(1)} \le b^{(2)} < e^{(2)} \le b^{(3)} < e^{(3)} < T^{\text{fim}}$$
* **Blocos Letivos Contínuos:**
  * As aulas só podem ser posicionadas estritamente dentro dos blocos letivos contínuos $\mathcal{F}_{g,d}$, jamais atravessando ou sobrepondo intervalos de descanso.
* **Exceções de Jornada por Dia da Semana:**
  * Cada turma pode definir jornadas diferenciadas para dias específicos da semana (ex: contraturno, sextas com horário reduzido).
* **Dias Letivos:**
  * Configuração individual dos dias da semana em que cada turma tem atividades (Segunda a Sexta).
* **Política de Sala Teórica Fixa:**
  * Definição de sala teórica fixa pré-atribuída ou lista de salas teóricas permitidas para escolha automática pelo otimizador.

### 2.4. Modelagem de Professores
* **Identificação e Carga Horária:**
  * Identificador único e nome completo.
  * Teto de carga horária diária máxima em minutos ($M_{p,d}$).
  * Quantidade máxima de aulas lecionadas por dia.
* **Disponibilidade Explícita:**
  * Cadastro de intervalos de horários disponíveis por dia da semana.
* **Preferência Semanal por Dia Útil ($\omega_{p,d}$):**
  * Pesos inteiros de preferência de 0 a 10 para cada dia da semana (Segunda a Sexta).
  * Conversão em penalidade: $c_{p,d}^{\text{dia}} = 10 - \omega_{p,d}$.
* **Preferência de Turnos:**
  * Definição de turnos de interesse do docente por dia (Manhã, Tarde, Noite, Integral).
* **Aversão a Janelas Ociosas:**
  * Peso individual de aversão a janelas entre aulas de 0 a 10 (`pesoJanela`).

### 2.5. Modelagem de Salas e Espaços Físicos
* **Tipos de Ambiente:**
  * Salas Teóricas (`TEORICA`).
  * Laboratórios e Salas Especiais (`LABORATORIO`, `ESPECIAL`).
* **Capacidade e Recursos:**
  * Capacidade máxima de estudantes por sala.
  * Lista de recursos ou equipamentos específicos cadastrados (ex: projetor, bancada química).

### 2.6. Modelagem de Disciplinas e Aulas (Lesson)
* **Carga Horária Semanal:**
  * Número de aulas semanais requeridas ($N_{\text{aulas}}$).
  * O sistema gera automaticamente as ocorrências de aula individuais ($1, 2, \dots, N_{\text{aulas}}$).
* **Aulas Consecutivas em Bloco:**
  * Suporte à exigência de que aulas de uma mesma matéria ocorram agrupadas em bloco no mesmo dia (ex: 2 aulas juntas, bloco de 4 aulas, ou 8 aulas divididas em 2 blocos de 4).
* **Associação:**
  * Vínculo com turmas participantes e professores responsáveis.
  * Requisitos de salas alternativas permitidas.

---

## 3. Requisitos de Restrições Rígidas (Hard Constraints)

Todas as restrições a seguir são de cumprimento obrigatório para que uma solução seja considerada viável (`FEASIBLE` ou `OPTIMAL`):

1. **Alocação Exata de Cada Aula:**
   * Toda aula $a$ deve ser alocada em exatamente uma combinação de dia, horário e sala viáveis:
     $$\sum_{c \in C_a} x_{a,c} = 1, \quad \forall a \in A$$
2. **Não Sobreposição de Professores (`NoOverlap`):**
   * Nenhum professor pode ministrar duas ou mais aulas simultâneas em intervalos de tempo que se sobreponham no mesmo semestre letivo.
3. **Não Sobreposição de Turmas (`NoOverlap`):**
   * Nenhuma turma de alunos pode assistir a duas ou mais aulas simultâneas em horários sobrepostos no mesmo semestre letivo.
4. **Não Sobreposição de Salas (`NoOverlap`):**
   * Nenhuma sala ou laboratório físico pode acomodar duas ou mais aulas simultâneas em horários sobrepostos no mesmo semestre letivo.
5. **Acomodação Estrita nos Blocos Letivos da Turma:**
   * A aula deve caber inteiramente dentro de um único bloco contínuo da jornada da turma, respeitando início, término e intervalos de descanso.
6. **Respeito à Disponibilidade dos Professores:**
   * Aulas só podem ser alocadas em dias e horários em que o professor esteja explicitamente disponível.
7. **Respeito aos Turnos Permitidos:**
   * Aulas devem ocorrer estritamente dentro dos turnos permitidos para a turma e disciplina (Manhã, Tarde, Noite).
8. **Carga Horária Máxima Diária do Professor:**
   * A soma dos minutos de aulas alocadas para um professor $p$ no dia $d$ não pode ultrapassar o teto $M_{p,d}$ (quando configurado maior que zero).
9. **Sala Teórica Fixa por Turma:**
   * Cada turma mantém rigorosamente a mesma sala teórica ao longo de toda a semana para todas as suas disciplinas teóricas.
   * Se a turma não tiver sala pré-definida, o solver escolhe exatamente uma sala entre as elegíveis via restrição de unicidade:
     $$\sum_{r \in R_g^{\text{teo}}} z_{g,r} = 1$$
10. **Início Obrigatório da Primeira Aula no Início da Jornada:**
    * A primeira aula do dia de cada turma deve iniciar obrigatoriamente no horário inicial da jornada ($T^{\text{ini}}$).
11. **Compatibilidade Semestral:**
    * Disciplinas semestrais do 1º e 2º semestre podem reutilizar os mesmos professores, turmas e salas em horários coincidentes sem configurar choque.
    * Disciplinas anuais ocupam os recursos em ambos os semestres.
12. **Aulas Consecutivas em Bloco:**
    * Quando exigido pela disciplina, as aulas são forçadas a ocorrer agrupadas no mesmo dia através de variáveis binárias de dia ativo ($w_d$).
13. **Teto Diário de Aulas da Mesma Matéria por Turma:**
    * Quando ativado o controle de dispersão, limita o número máximo de aulas da mesma matéria em um único dia para a turma (padrão: 2 aulas/dia).
14. **Preenchimento Integral Opcional da Jornada:**
    * Quando ativado, exige que todos os slots letivos da jornada de cada turma tenham aula alocada, proibindo janelas ou horários vagos.
15. **Preservação de Aulas Fixadas (Pinning):**
    * Aulas fixadas manualmente pelo usuário têm seu dia, horário e sala travados com valor binário fixo $x_{a,c} = 1$, servindo de âncora para a reotimização das demais.

---

## 4. Requisitos de Restrições Suaves e Função Objetivo (Soft Constraints)

A função objetivo do solver minimiza a penalidade ponderada global $Z$:

$$\min Z = \lambda_1 J + \lambda_2 D + \lambda_3 P + \lambda_4 W + \lambda_5 T + \lambda_6 J_G$$

### 4.1. Pesos Institucionais Configuráveis ($\lambda_i$)
* $\lambda_1$ (`lambda_janelas`, padrão: 8): Peso para redução de janelas ociosas entre aulas de professores.
* $\lambda_2$ (`lambda_dias`, padrão: 5): Peso para concentração da jornada em menos dias na semana.
* $\lambda_3$ (`lambda_preferencias`, padrão: 3): Peso para respeito a preferências de horários dos professores.
* $\lambda_4$ (`lambda_preferencia_dias`, padrão: 7): Peso para priorização de dias mais desejados pelos docentes.
* $\lambda_5$ (`lambda_preferencia_turnos`, padrão: 8): Peso para respeito aos turnos pretendidos pelos professores.
* $\lambda_6$ (`lambda_lacunas_turma`, padrão: 2): Peso para minimização de lacunas/buracos na grade das turmas.

### 4.2. Componentes da Função Objetivo
* **Janelas de Professores ($J$):**
  * Calculadas dentro de cada turno de trabalho do docente.
  * Para cada slot intermediário vago entre a primeira e a última aula lecionada no turno, adiciona-se uma penalidade ponderada pelo peso individual de aversão do docente (`pesoJanela`).
* **Concentração de Dias de Trabalho ($D$):**
  * Minimização da soma de variáveis binárias $y_{p,d}$, onde $y_{p,d} = 1$ indica que o professor $p$ tem pelo menos uma aula no dia $d$.
* **Preferências de Horário ($P$):**
  * Soma dos custos de alocação fora dos horários preferenciais indicados nos candidatos de alocação.
* **Preferência Semanal por Dias ($W$):**
  * Suporta duas estratégias de cálculo configuráveis:
    1. *Por Dia Trabalhado:* $W = \sum_{p,d} (10 - \omega_{p,d}) \cdot y_{p,d}$
    2. *Por Minuto de Aula:* $W = \sum_{p,d} (10 - \omega_{p,d}) \cdot m_{p,d}$
* **Turnos Não Pretendidos ($T$):**
  * Penaliza a alocação de aulas para o professor em turnos que ele não sinalizou como pretendidos para aquele dia.
* **Lacunas nas Turmas ($J_G$):**
  * Quando o preenchimento integral estiver desligado, penaliza a ocorrência de horários vagos entre blocos de aulas dos alunos.

---

## 5. Requisitos dos Motores de Resolução (Solvers)

O sistema implementa algoritmos de resolução intercambiáveis:

1. **Google OR-Tools CP-SAT (Padrão / Recomendado):**
   * Programação por restrições com aprendizado de cláusulas guiado por conflitos (CDCL).
   * Variáveis intervalares opcionais (`NewOptionalIntervalVar`) e propagador nativo `AddNoOverlap`.
   * Parâmetros configuráveis: tempo limite em segundos (`tempo_limite_segundos`), limite de memória RAM em MB (`max_memoria_mb`), número de threads de CPU paralelas (`num_threads`) e log de progresso (`log_search_progress`).
2. **NVIDIA cuOpt — GPU-Accelerated Mathematical Optimization (`solveCuOpt`):**
   * Solver de otimização combinatória e Programação Linear Inteira Mista (MILP) acelerado por hardware NVIDIA GPU / CUDA.
   * Montagem de matriz esparsa comprimida (CSR) com vinculação direta à C API do cuOpt (`cuOptCreateRangedProblem`, `cuOptSolve`) via carregamento dinâmico (`libcuopt.so`).
   * Exportação automática da formulação matemática completa em formato padrão MPS (`grade_cuopt_model.mps`) para ingestão em microserviços cuOpt, scripts Python e containers NVIDIA NGC.
   * Telemetria e detecção automática de GPUs NVIDIA instaladas no sistema, com fallback para motor Dual Simplex B&C de alta performance com Warm Start.
3. **SCIP — Solving Constraint Integer Programs (`solveScip`):**
   * Solver nativo de Programação Inteira Mista e por Restrições (Branch-Cut-and-Price) integrado via API C do SCIP (`<scip/scip.h>`).
   * Formulação linear inteira completa com cortes de clique por slot para não sobreposição ($\sum_{c \text{ cobre } k} x_{a,c} \le 1$), variáveis de sala teórica fixa ($z_{g,r}$), início obrigatório da jornada ($T_{\text{ini}}$), blocos consecutivos e penalidades suaves ($D, W, P, T, J, J_G$).
   * Suporte a injeção de solução primal inicial (Warm Start / semente heurística) via `SCIPaddSolFree`.
4. **HiGHS — High Performance Optimization Software (`solveHighs`):**
   * Solver nativo de Programação Linear Inteira Mista de alta performance integrado via API C++ do HiGHS (`<Highs.h>`).
   * Construção direta da matriz esparsa comprimida (`HighsSparseMatrix`), pré-processamento algébrico, Dual Simplex paralelo e Branch-and-Cut.
   * Suporte a Warm Start via `Highs::setSolution`.
5. **Heurística Construtiva Gulosa (Greedy Heuristic):**
   * Resolução rápida baseada em ordenação por grau de restrição (número de candidatos viáveis e duração da aula).
   * Alocação gulosa priorizando aulas fixadas e minimizando penalidades imediatas.
6. **Auditoria Pós-Resolução:**
   * Verificação estrita de integridade executada após o solver para detectar eventuais sobreposições de professores, turmas ou salas, emitindo mensagens de diagnóstico imediatas.

---

## 6. Requisitos de Pré-Processamento e Validação

* **Validação Prévia de Dados:**
  * Detecção de identificadores vazios ou duplicados.
  * Validação de consistência temporal das jornadas de turmas (ordem dos intervalos e limites).
  * Verificação de professores sem disponibilidade ou turmas sem dias letivos.
  * Emissão de erros impeditivos (`errors`) e advertências informativas (`warnings`).
* **Geração do Domínio de Candidatos Viáveis ($C_a$):**
  * Pré-cálculo de todas as combinações válidas de dia, slot inicial e salas para cada aula antes da entrega ao solver.
  * Descarte antecipado de posições que violem jornadas, disponibilidades ou limites de turnos, reduzindo o espaço de busca do modelo.

---

## 7. Requisitos de Persistência e Exportação

* **Persistência em JSON (`JsonSerialization`):**
  * Salvar e carregar todos os dados da instituição (professores, turmas, salas, disciplinas, configurações e grade gerada).
  * Estrutura robusta baseada em `nlohmann/json`.
* **Exportação em HTML (`ScheduleReportExporter::exportarHtml`):**
  * Relatório visual completo com tema responsivo, cartões de KPIs e tabelas separadas por Turma, Professor e Sala.
  * Suporte nativo a impressão em navegadores.
* **Exportação em CSV (`ScheduleReportExporter::exportarCsv`):**
  * Arquivo tabular com delimitador `;` contendo 13 colunas detalhando cada aula alocada.

---

## 8. Requisitos de Linha de Comando (CLI / Headless)

* Suporte a execução sem interface gráfica via flags:
  * `--cli`, `--test`, `--benchmark`: Execução de testes automatizados e benchmark.
  * `--cuopt`, `--greedy`, `--cpsat`, `--scip`, `--highs`, `--milp`: Seleção do algoritmo.
  * `--log`: Ativação do log interno de progresso dos motores de busca.
  * `--max-memory <MB>`: Definição de teto de memória RAM.
  * `--unlimited-memory`: Execução sem limite de memória.
  * `--time-limit <SEG>`: Tempo limite de resolução.
  * `--warmstart-chain`: Encadeamento guloso seguido de CP-SAT/Genético.
  * `--render-screenshots`: Renderização offscreen de diálogos da interface para documentação.
  * `--help`, `-h`: Exibição do manual de ajuda.
