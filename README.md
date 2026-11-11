# Escalonador Inteligente de Horários Escolares

Sistema avançado de geração de grade horária semanal para professores e turmas escolares, construído em **C++20**, **Google OR-Tools (Solver CP-SAT)**, interface gráfica com **Qt 6** e gerenciamento de dependências com **Conan 2.x**.

O software implementa rigorosamente todas as especificações e formulações matemáticas descritas em `instrucoes.md`.

---

## 🚀 Características Principais

1. **Discretização Temporal em Intervalos de 5 Minutos (Seção 1.1)**
   - $\operatorname{MDC}(45, 50) = 5$ min.
   - Suporte nativo a aulas de **45 minutos** (9 slots) e **50 minutos** (10 slots) ou múltiplos de 5 min na mesma instituição e turma sem fragmentação indevida.
   - Respeito à transição precisa de horários (ex: 08:00–07:45, 07:45–08:35, etc.).

2. **Jornada Diária e Dois Intervalos Obrigatórios por Turma (Seções 2.4 & 3.5)**
   - Configuração de início ($T^{ini}$) e término ($T^{fim}$) da jornada.
   - **Dois intervalos de descanso obrigatórios** ordenados por dia:
     $$T^{ini} < b^{(1)} < e^{(1)} \le b^{(2)} < e^{(2)} < T^{fim}$$
   - Nenhuma aula pode atravessar ou sobrepor os intervalos de descanso; as aulas cabem inteiramente nos blocos letivos contínuos:
     $$\mathcal{F}_{g,d} = \{ [T^{ini}, b^{(1)}),\ [e^{(1)}, b^{(2)}),\ [e^{(2)}, T^{fim}) \}$$

3. **Preferência Semanal por Dia dos Professores (Seções 2.3 & 4.4)**
   - Cada professor define **5 pesos inteiros** $\omega_{p,d} \in \{0, 1, \dots, 10\}$ (Segunda a Sexta).
   - Conversão em penalidade: $c^{dia}_{p,d} = 10 - \omega_{p,d}$.
   - Otimização ponderada na função objetivo ($W$).

4. **Sala Teórica Fixa por Turma (Seção 3.6)**
   - Cada turma mantém a mesma sala teórica ao longo de toda a semana.
   - Suporte a sala pré-definida no cadastro ou seleção automática pelo solver via variáveis binárias $z_{g,r}$ dentre as salas elegíveis ($R_g^{teo}$).
   - Aulas de laboratório e atividades especiais utilizam salas alternativas ($B_a$) sem alterar a sala-base da turma.

5. **Restrições Hard (Obrigatórias)**
   - Cada aula alocada exatamente uma vez ($\sum x = 1$).
   - Não sobreposição de professores (`NoOverlap` via CP-SAT).
   - Não sobreposição de turmas (`NoOverlap`).
   - Não sobreposição de salas (`NoOverlap`).
   - Início obrigatório da primeira aula no início da jornada da turma ($T^{ini}$).
   - Respeito à disponibilidade de horários dos professores ($V_{p,d}$).
   - Respeito aos turnos permitidos (Manhã, Tarde, Noite, Integral).
   - Carga máxima diária dos professores ($M_{p,d}$).
   - Preenchimento integral opcional da jornada (Seção 3.10).

6. **Função Objetivo Ponderada Multicritério (Seção 4 & 8)**
   $$\min Z = \lambda_1 J + \lambda_2 D + \lambda_3 P + \lambda_4 J_G + \lambda_5 W$$
   - $J$: Minimização de janelas ociosas entre aulas dos professores.
   - $D$: Concentração de aulas em menos dias presenciais de trabalho.
   - $P$: Respeito às preferências de faixas horárias no dia.
   - $W$: Priorização dos dias de maior preferência dos professores.
   - $J_G$: Minimização de lacunas internas na grade das turmas.
   - Pesos institucionais $\lambda_i$ totalmente configuráveis pela UI e via JSON.

7. **Fixação Manual de Aulas (Pinning / Reotimização)**
   - Permite fixar o dia, horário e sala de qualquer aula selecionada com o botão direito na grade.
   - O solver respeita as aulas bloqueadas e reotimiza as demais ao redor.

8. **Exportação de Relatórios**
   - Exportação completa em **HTML moderno** pronto para impressão com layout responsivo e cartões visuais.
   - Exportação em planilha **CSV** para integração com Excel e LibreOffice.

---

## 🛠️ Tecnologias Utilizadas

- **Linguagem**: C++20 (GCC / Clang).
- **Otimizador**: Google OR-Tools (Solver CP-SAT com `CpModelBuilder`).
- **Interface Gráfica**: Qt 6 (Widgets, Layouts, QTableWidget, Temas Modernos).
- **Gerenciador de Dependências**: Conan 2.x (`conanfile.txt`).
- **Build System**: CMake 3.20+ / Ninja.
- **Serialização**: JSON via `nlohmann_json`.

---

## 📂 Estrutura do Código

```
.
├── CMakeLists.txt                # Configuração do build CMake com C++20 e Qt6
├── conanfile.txt                 # Dependências Conan (or-tools/9.15, nlohmann_json)
├── instrucoes.md                 # Documento de especificação original
├── examples/
│   └── exemplo_instituicao.json  # Cenário escolar completo de exemplo
├── include/
│   ├── model/                    # Modelos de domínio OOP
│   │   ├── TimeSlot.h            # Discretização de 5 min, turnos e dias
│   │   ├── Professor.h           # Professor, disponibilidade e pesos 0-10
│   │   ├── Room.h                # Sala teórica, laboratório, capacidade
│   │   ├── Turma.h               # Turma, jornada e 2 intervalos obrigatórios
│   │   ├── Lesson.h              # Disciplina e ocorrência de aula (45/50 min)
│   │   ├── OptimizationConfig.h  # Pesos lambda_i e parâmetros do CP-SAT
│   │   ├── Timetable.h           # Grade gerada, alocações e estatísticas
│   │   └── InstitutionData.h     # Repositório central de dados acadêmicos
│   ├── solver/
│   │   ├── CandidateAllocation.h # Tupla (d, t, b) admissível
│   │   ├── Preprocessor.h        # Validação e geração de candidatos C_a
│   │   └── ScheduleSolver.h      # Formulação e resolução no CP-SAT
│   ├── serialization/
│   │   └── JsonSerialization.h   # Leitura e escrita completa em JSON
│   └── ui/
│       ├── ModernTheme.h         # Estilos e paleta moderna (Dark/Slate/Cyan)
│       ├── TimetableView.h       # Tabela semanal (por turma, professor, sala)
│       ├── ProfessorEditorDialog.h # Editor de professor e pesos semanais
│       ├── TurmaEditorDialog.h   # Editor de turma, 2 intervalos e sala fixa
│       ├── RoomEditorDialog.h    # Editor de salas e laboratórios
│       ├── LessonEditorDialog.h  # Editor de disciplinas e salas alternativas
│       ├── ConfigWidget.h        # Painel de ajuste dos pesos lambda
│       ├── ScheduleReportExporter.h # Gerador de HTML e CSV
│       └── MainWindow.h          # Janela principal e abas de controle
└── src/                          # Implementação dos arquivos C++
    ├── main.cpp                  # Ponto de entrada (Modo GUI e Modo --test CLI)
    ├── model/
    ├── solver/
    ├── serialization/
    └── ui/
```

---

## ⚙️ Compilação e Execução

### 1. Instalar Dependências com Conan

```bash
conan install . --build=missing -s compiler.cppstd=20
```

### 2. Configurar e Compilar com CMake

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 3. Executar o Software com Interface Gráfica

```bash
./build/escalonadorHorarios
```

### 4. Executar em Modo de Teste Automatizado / CLI

O executável possui suporte direto a execução em linha de comando para validação rápida, benchmarks e integração contínua (CI):

```bash
# Executa com o cenário acadêmico padrão:
./build/escalonadorHorarios --test

# Executa definindo algoritmo, limite de tempo e teto de memória RAM (em MB):
./build/escalonadorHorarios --test --cpsat --time-limit 30 --max-memory 4096

# Ou com um arquivo JSON específico:
./build/escalonadorHorarios --test examples/exemplo_instituicao.json
```

Ao finalizar, os relatórios `grade_horarios.html` e `grade_horarios.csv` são gerados automaticamente.

### 5. Parâmetros de Recursos e Memória

- **Limite Máximo de Memória RAM (`max_memoria_mb`)**: Permite limitar a quantidade máxima de memória a ser consumida pelo solver (padrão: 2048 MB = 2 GB), evitando esgotamento de recursos em instâncias extensas.
- **Opção Sem Restrição (`sem_limite_memoria`)**: Desativa a trava de memória para permitir uso livre da memória disponível no sistema.
- **Configuração**: Ajustável diretamente na aba de parâmetros do sistema, no diálogo de execução de otimização, via JSON institucional ou por linha de comando (`--max-memory <MB>` / `--unlimited-memory`).
