# Documentação da Interface Gráfica (ui.md)

Este documento descreve detalhadamente tudo o que foi implementado na **Interface Gráfica de Usuário (GUI)** do **Escalonador Inteligente de Horários Escolares**, construída sobre o framework **Qt 6** (C++20).

---

## 1. Arquitetura Visual e Sistema de Temas (`ModernTheme`)

A interface foi projetada para combinar elegância visual, usabilidade institucional e integração nativa com o ambiente de desktop.

### 1.1. Modos de Temas Visuais Suportados
O usuário pode alternar o estilo visual em tempo real através do menu superior **Exibir > Tema Visual**:
* **Padrão do Sistema (KDE Plasma):** Utiliza as cores, paletas de sistema e integração de fontes do ambiente desktop configurado pelo usuário.
* **KDE Breeze Nativo:** Aplica o estilo gráfico oficial Breeze do ecossistema KDE.
* **Qt Fusion:** Tema limpo, neutro e consistente entre diferentes distribuições Linux.
* **Escuro Moderno (Slate Dark / ModernDark):** Tema escuro estilizado sob medida, com tons de ardósia (*Slate* #0f172a / #1e293b), bordas sutis (#334155 / #475569) e destaque em azul índigo (#2563eb / #38bdf8).

### 1.2. Recursos de Design, Usabilidade e Diretório de Imagens (`img/`)
* **Detecção Automática de Dark Mode:** Reconhece a luminosidade da paleta do sistema para garantir legibilidade dos textos e contrastes.
* **Sistema de Ícones Vetoriais (`img/*.svg`):** Todos os ícones e imagens utilizados na interface gráfica estão centralizados na pasta `img/` (no mesmo nível de `src/`), carregados dinamicamente via `ModernTheme::icon()` e `ModernTheme::iconPath()`:
  * `app_icon.svg` (Ícone principal da janela e da aplicação)
  * `calendar.svg`, `professor.svg`, `turma.svg`, `room.svg`, `disciplina.svg`, `settings.svg`, `logs.svg` (Ícones das abas e entidades)
  * `optimize.svg`, `new_project.svg`, `open_file.svg`, `save_file.svg`, `example.svg`, `export_html.svg`, `export_csv.svg` (Ações da barra de ferramentas e menus)
  * `add.svg`, `edit.svg`, `delete.svg`, `pin.svg`, `unpin.svg`, `swap.svg`, `theme.svg`, `search.svg`, `copy.svg`, `coffee.svg` (Operações de edição, busca e grade)
  * `morning.svg`, `afternoon.svg`, `evening.svg`, `integral.svg` (Turnos letivos)
* **Feedback de Ações:** Diálogos modais, caixas de confirmação destrutiva (`QMessageBox::question`) e mensagens de status contextuais.

---

## 2. Janela Principal (`MainWindow`)

A `MainWindow` atua como contêiner central da aplicação, estruturada em:
1. **Barra de Menus Superior (MenuBar)**
2. **Barra de Ferramentas de Acesso Rápido (ToolBar)**
3. **Banner Superior de Indicadores-Chave (KPI Banner)**
4. **Painel de Abas de Navegação (TabWidget Central)**
5. **Barra de Status Inferior (StatusBar)**

---

## 3. Barra de Menus e Barra de Ferramentas

### 3.1. Estrutura de Menus
* **Menu Arquivo:**
  * *Novo Projeto* (`Ctrl+N`): Reinicializa os dados da instituição para um cenário vazio.
  * *Abrir JSON...* (`Ctrl+O`): Carrega dados acadêmicos e grade a partir de arquivo `.json`.
  * *Salvar JSON...* (`Ctrl+S`): Persiste o estado completo atual em disco.
  * *Carregar Exemplo Padrão:* Carrega cenário acadêmico demonstrativo completo (turmas de 45 e 50 min, laboratórios, professores e preferências).
  * *Exportar Grade HTML...:* Gera relatório visual responsivo para impressão ou compartilhamento.
  * *Exportar Grade CSV...:* Gera planilha tabular delimitada por ponto e vírgula.
  * *Sair* (`Ctrl+Q`): Encerra a aplicação.
* **Menu Grade:**
  * *➕ Alocar Disciplina Manualmente...* (`Ctrl+M`): Abre diálogo para inserção assistida de aula.
  * *📌 Fixar Todas as Aulas da Grade:* Bloqueia em lote todas as aulas alocadas para servirem de âncora fixa para o próximo solver.
  * *🔓 Desafixar Todas as Aulas da Grade:* Libera em lote todas as aulas alocadas para reotimização irrestrita.
  * *🗑️ Limpar Toda a Grade...:* Remove todas as alocações da grade após confirmação.
* **Menu Exibir:**
  * *🎨 Tema Visual:* Seleção exclusiva entre os 4 temas visuais disponíveis com recarregamento instantâneo.

### 3.2. Barra de Ferramentas (ToolBar)
Botões com ícones e tooltips descritivos para execução imediata:
* `⚡ OTIMIZAR GRADE` (Botão de destaque com estilo primário acentuado)
* `Novo Projeto`
* `Abrir JSON`
* `Salvar JSON`
* `Carregar Exemplo Padrão`
* `Exportar HTML`
* `Exportar CSV`

---

## 4. Banner Superior de Indicadores de Desempenho (KPI Banner)

Fixado logo acima das abas principais, exibe em tempo real o diagnóstico da solução atual através de cartões estilizados:
* **Status do Solver:** Exibe `OPTIMAL` (verde), `FEASIBLE` (azul), `INFEASIBLE` (vermelho) ou `NÃO EXECUTADO` (cinza neutro).
* **Tempo de Resolução:** Tempo computacional despendido em milissegundos (ex: `125.4 ms`).
* **Aulas Alocadas:** Fração e total de aulas posicionadas com sucesso (ex: `120 / 120`).
* **Janelas Professores (J):** Minutos totais de janelas ociosas entre aulas de docentes (ex: `0 min`).
* **Dias Trabalhados (D):** Somatório de dias de presença dos professores na instituição.
* **Penalidade Pref. Dias (W):** Custo de penalização por alocação em dias menos preferidos pelos docentes.

---

## 5. Visualizador Interativo da Grade Semanal (`TimetableView` & `TimetableGridWidget`)

É o componente central de visualização e manipulação da grade de horários gerada.

### 5.1. Perspectivas de Visualização
Um menu suspenso permite alternar a perspectiva da grade semanal instantaneamente:
1. **Visão por Turma:** Mostra a grade curricular da turma selecionada com suas disciplinas, professores, salas e intervalos de recreio.
2. **Visão por Professor:** Mostra a agenda semanal de aulas do docente selecionado, indicando as turmas atendidas e os laboratórios/salas atribuídos.
3. **Visão por Sala / Laboratório:** Mostra a taxa de ocupação física da sala selecionada ao longo da semana.
4. **Visão Geral (Todas as Disciplinas e Turmas):** Mostra todas as disciplinas e turmas alocadas simultaneamente na semana, com identificação clara de turma, docente e sala, oferecendo uma visão panorâmica global com suporte a filtro opcional por turma.

### 5.2. Filtros e Controles Rápidos da Grade
* **Seletor de Entidade / Filtro de Turma:** Dropdown dinâmico que lista turmas, professores, salas ou "Todas as Turmas (Visão Geral)" com opção de filtro pontual.
* **Filtro de Semestre Letivo:** Alterna a exibição entre:
  * `1º Semestre (Anuais + 1º Sem)`
  * `2º Semestre (Anuais + 2º Sem)`
  * `Ambos os Semestres`
* **Busca Textual em Tempo Real:**
  * Campo de busca com limpeza rápida (`search_lesson_`).
  * Destaca visualmente na grade as células cujas disciplinas, professores ou salas correspondam ao texto digitado.
  * Badge ao lado indicando o número de aulas encontradas (ex: `3 encontradas`).
* **Ações Diretas:**
  * Botão `🔄 Trocar Aulas (Swap)...`: Abre o diálogo de permuta rápida entre a aula selecionada e qualquer outra da grade.
  * Botão `➕ Alocar Disciplina...`: Adiciona uma ocorrência manual.
  * Botão `🗑️ Limpar Grade...`: Limpa a grade semanal.

### 5.3. Renderização Visual das Células e Seleção Individual
* **Seleção Precisa de Aula:** Em células com múltiplas aulas (como no modo de visualização global ou turmas concorrentes), o clique direto seleciona especificamente a disciplina e docente desejados. A aula ativa ganha a marcação visual `👉 [SELECIONADA]`, destaque em azul/dourado na célula e informações no banner superior.
* **Faixas Horárias Dinâmicas:** Linhas da tabela calculadas a partir dos horários reais de início/fim e intervalos dos blocos letivos.
* **Células de Recreio / Almoço:** Renderizadas com cor destacada (âmbar) e texto centralizado (ex: `☕ INTERVALO (09:40 - 09:55)`), bloqueadas para alocação.
* **Cartões de Aula:**
  * Borda lateral colorida conforme o tipo: Azul para `TEORICA`, Magenta para `LABORATORIO` e Roxo para `ESPECIAL`.
  * Nome da disciplina em negrito.
  * Informação formatada de horário e duração em minutos.
  * Identificação de turma, professor e sala.
  * **Badges de Regime e Semestre:** Indicadores compactos `Anual`, `1º Sem` ou `2º Sem`.
  * **Ícone de Fixação (Pin 📌):** Indica se a aula está travada contra alterações do solver.

### 5.4. Arrastar e Soltar Nativo (Drag & Drop com `TimetableGridWidget`)
* Permite clicar com o botão esquerdo em qualquer aula alocada e arrastá-la diretamente para outro horário ou dia da semana. Em células com mais de uma aula, arrasta especificamente a aula clicada.
* **Feedback Visual:** Gera miniatura gráfica flutuante com sombra durante o movimento do cursor (`QDrag`) contendo o nome e turma da aula arrastada.
* **Validação de Destino:** O cursor bloqueia solturas inválidas sobre células de recreio ou dias inativos da turma.
* **Confirmação e Auditoria:** Ao soltar a aula (`dropEvent`), se o horário já estiver ocupado, o sistema pergunta com qual aula daquele horário deseja trocar (Swap) ou se deseja alocar juntas.

### 5.5. Menu de Contexto Completo (Botão Direito do Mouse)
Ao clicar com o botão direito em uma **célula ocupada por aula**:
* `🔄 Trocar Horário Com Outra Aula (Swap)...`: Abre o diálogo de permuta direta de horários entre esta aula e qualquer outra aula da grade.
* `🔄 Mover Horário / Sala...`: Abre o diálogo de movimentação detalhada.
* `🏢 Alterar Sala de Aula...`: Abre o diálogo específico para troca ou permuta de espaço físico.
* `➕ Alocar Outra Disciplina na Grade...`: Abre o assistente de nova alocação.
* `📌 Fixar Horário Desta Aula / 🔓 Desafixar Horário`: Alterna individualmente o estado de bloqueio da aula para o solver.
* `❌ Desalocar Aula da Grade`: Remove a aula da grade mantendo o cadastro intacto.
* `📌 Fixar Todas as Aulas da Grade`: Bloqueia toda a grade atual.
* `🔓 Desafixar Todas as Aulas da Grade`: Desbloqueia toda a grade atual.
* `🗑️ Limpar Toda a Grade...`: Esvazia a grade semanal.

Ao clicar com o botão direito em uma **célula vazia**:
* `➕ Alocar Disciplina Neste Horário...`: Abre o assistente com o dia e horário da célula pré-selecionados.
* `🗑️ Limpar Toda a Grade...`: Esvazia a grade semanal.

### 5.6. Atalho por Clique Duplo
* Um duplo clique com o botão esquerdo sobre uma aula abre imediatamente o diálogo de permuta rápida (Swap) ou o menu de ações específicas daquela aula.

---

## 6. Diálogos de Edição Direta da Grade

### 6.1. Diálogo de Movimentação de Horário e Sala (`MoveLessonDialog`)
* Exibe os dados da aula selecionada (nome, turma, professor, duração).
* Dropdowns para escolha do **Novo Dia da Semana**, **Novo Horário Letivo** e **Nova Sala de Aula**.
* **Painel de Validação em Tempo Real:** Exibe um alerta verde caso o novo horário esteja totalmente livre, ou um alerta em vermelho/amarelo listando eventuais choques de professor, turma ou sala.
* **Permuta Automática (Swap):** Caso o horário de destino já esteja ocupado por outra aula da mesma turma, oferece a opção de trocar mutuamente os horários das duas aulas.
* Checkbox para **Fixar Aula (Pin)** após a movimentação.

### 6.2. Diálogo de Alteração de Sala de Aula (`ChangeRoomDialog`)
* Mostra o horário, dia e disciplina atuais.
* Dropdown com todas as salas e laboratórios cadastrados, exibindo a capacidade máxima e o estado de ocupação naquele horário.
* **Detecção de Conflitos e Permuta:** Se a sala escolhida já estiver reservada para outra aula no mesmo horário, o diálogo avisa qual é a aula conflitante e habilita o botão `🔄 Trocar Salas (Swap)`.
* Opção de fixar a atribuição da sala.

### 6.3. Diálogo de Alocação Manual de Disciplina (`AddLessonToScheduleDialog`)
* Permite alocar manualmente ocorrências de disciplinas que ainda não estejam na grade.
* Pré-preenche dia, horário e turma com base na célula ou filtros ativos da grade.
* Dropdown de disciplinas disponíveis com indicação de carga horária e aulas já alocadas.
* Seleção da ocorrência de aula semanal (ex: Aula 1 de 4, Aula 2 de 4).
* Seleção de semestre letivo (1º Semestre, 2º Semestre ou Anual).
* Validação dinâmica de choques e respeito aos intervalos de recreio.
* Opção de fixar imediatamente a aula alocada.

### 6.4. Diálogo de Permuta de Horários entre Aulas (`SwapLessonDialog`)
* Permite selecionar uma aula de origem e permutar diretamente o dia e horário com qualquer outra aula alocada na grade.
* **Busca e Filtragem em Tempo Real:** Permite encontrar a aula de destino por disciplina, professor, turma ou dia da semana.
* **Validação Mútua em Tempo Real:** Audita simultaneamente choques de turma, disponibilidade de docentes e ocupação de salas para ambas as aulas.
* **Fixação Automática:** Permite travar ambas as aulas com Pin (📌) para proteger a nova configuração contra alterações futuras do solver.

---

## 7. Componente Universal de Busca e Ordenação (`TableSearchSortBar` & `SmartTableWidgetItem`)

Presente no topo de todas as abas de cadastro (Professores, Turmas, Salas e Disciplinas), unifica a navegação e localização de registros.

### 7.1. Funcionalidades da Barra de Busca e Ordenação
* **Campo de Busca Textual:**
  * Filtragem instantânea conforme o usuário digita.
  * **Normalização e Remoção de Acentos (`removeAccents`):** Permite encontrar "Física" digitando "fisica", "Atenção" digitando "atencao", etc.
* **Contador de Registros:** Badge com contagem em tempo real (ex: `12 de 12 professores`, `3 de 12 encontrados`).
* **Seletor de Coluna para Ordenação:** Dropdown dinâmico populado com os cabeçalhos da tabela associada.
* **Botão de Alternância de Ordem:** Alterna com um clique entre ordem crescente (`▲ A-Z`) e decrescente (`▼ Z-A`).

### 7.2. Ordenação Natural Inteligente (`SmartTableWidgetItem`)
* Especialização de `QTableWidgetItem` utilizando `QCollator` com modo numérico ativado.
* Evita a ordenação puramente alfabética ingênua:
  * Em tabelas normais: `"Sala 10"` fica antes de `"Sala 2"`.
  * Com `SmartTableWidgetItem`: `"Sala 2"` fica corretamente antes de `"Sala 10"`.

---

## 8. Abas de Cadastro e Gerenciamento de Entidades (CRUD)

### 8.1. Aba de Professores (`createProfessoresTab`)
* **Tabela de Listagem:** Exibe ID, Nome Completo, Carga Horária Máxima Diária, Grau de Aversão a Janelas, Turnos Pretendidos e Preferência Semanal de Dias.
* **Botões de Ação:** `➕ Adicionar Professor`, `✏️ Editar Professor`, `🗑️ Remover Professor`.
* **Formulário Modal (`ProfessorEditorDialog`):**
  * *Campos Cadastrais:* Identificador (ID) e Nome do professor.
  * *Sliders de Preferência por Dia Útil:* 5 controles deslizantes para Segunda, Terça, Quarta, Quinta e Sexta com valores variando de -2 (Indesejado) a +2 (Muito Preferido), convertidos para escala matemática de 0 a 10.
  * *Slider de Aversão a Janelas:* Controle deslizante de 0 (Indiferente a horários vagos) a 10 (Aversão máxima a janelas).
  * *Turnos Pretendidos:* Checkboxes independentes para Manhã, Tarde e Noite.
  * *Tabela de Disponibilidade Semanal:* Tabela dinâmica onde o usuário pode adicionar e remover faixas exatas de horário de disponibilidade por dia com `QTimeEdit`.
  * *Limites Diários:* `QSpinBox` para carga máxima diária em minutos (ex: 240 min) e quantidade máxima de aulas/dia.

### 8.2. Aba de Turmas & Jornadas (`createTurmasTab`)
* **Tabela de Listagem:** Exibe ID, Nome, Turno Principal, Sala Teórica Fixa, Dias Letivos de Funcionamento, Horário da Jornada Padrão e Intervalos Ativos.
* **Botões de Ação:** `➕ Adicionar Turma`, `✏️ Editar Turma`, `🗑️ Remover Turma`.
* **Formulário Modal (`TurmaEditorDialog`):**
  * *Identificação e Turno:* ID, Nome da turma e Turno base (Manhã, Tarde, Noite, Integral).
  * *Dias Letivos:* Checkboxes para ativar os dias da semana em que a turma tem aulas (Segunda a Sexta).
  * *Abas de Horários Diários (Segunda a Sexta):*
    * Checkbox para ativar/desativar funcionamento no dia específico.
    * Campos `QTimeEdit` para Horário de Início e Horário de Término do turno.
    * Até **3 Intervalos de Descanso/Recreio/Almoço** por dia, cada um com checkbox de ativação e horários de início e fim.
    * **Cálculo Automático de Minutos Letivos:** Rótulo informativo em tempo real mostrando o total de minutos letivos disponíveis no dia e alertando caso haja inconsistências cronológicas.
  * *Política de Sala Teórica Fixa:*
    * Opção de selecionar uma Sala Teórica Fixa pré-definida.
    * Opção de definir lista de Salas Teóricas Permitidas para que o solver escolha automaticamente a melhor sala fixa para a turma.

### 8.3. Aba de Salas & Laboratórios (`createSalasTab`)
* **Tabela de Listagem:** Exibe ID, Nome da Sala, Tipo (Teórica, Laboratório, Especial), Capacidade de Alunos e Recursos/Equipamentos.
* **Botões de Ação:** `➕ Adicionar Sala`, `✏️ Editar Sala`, `🗑️ Remover Sala`.
* **Formulário Modal (`RoomEditorDialog`):**
  * ID e Nome descritivo da sala.
  * Dropdown de Tipo (`TEORICA`, `LABORATORIO`, `ESPECIAL`).
  * `QSpinBox` para capacidade máxima de estudantes.
  * Campo de texto para inclusão de recursos adicionais separados por vírgula (ex: `Projetor, Lousa Digital, 30 PCs`).

### 8.4. Aba de Disciplinas (`createDisciplinasTab`)
* **Tabela de Listagem:** Exibe ID, Nome da Disciplina, Regime/Semestre, Tipo, Duração da Aula (min), Aulas Semanais Requeridas, Turmas Atendidas, Professores Habilitados e Exigência de Bloco Consecutivo.
* **Botões de Ação:** `➕ Adicionar Disciplina`, `✏️ Editar Disciplina`, `🗑️ Remover Disciplina`.
* **Formulário Modal (`LessonEditorDialog`):**
  * ID e Nome da matéria.
  * Tipo de ambiente (`TEORICA` ou `LABORATORIO`).
  * **Regime Acadêmico:** Dropdown para alternar entre `Anual (Ano Todo)` e `Semestral`.
  * **Semestre de Oferta:** Dropdown com `1º Semestre`, `2º Semestre` ou `Ambos os Semestres`.
  * **Duração da Aula:** Dropdown com valores pré-definidos (`45 minutos`, `50 minutos`) ou valor customizado em minutos.
  * Quantidade de aulas semanais requeridas (`QSpinBox`).
  * Seleção de Professor responsável e Turmas atendidas.
  * **Exigência de Aulas Consecutivas em Bloco:** Checkbox para forçar que as aulas ocorram juntas no mesmo dia (blocos de 2, 4 ou 8 aulas).
  * Lista de Salas e Laboratórios alternativos admitidos para a disciplina.

---

## 9. Aba de Parâmetros do Solver (`ConfigWidget`)

Painel gráfico completo para ajuste fino da formulação matemática e restrições sem necessidade de alterar código:

### 9.1. Ponderação da Função Objetivo (Pesos Institucionais $\lambda_i$)
Sliders interativos com exibição numérica do valor configurado (0 a 20):
* **Reduzir Janelas de Professores ($\lambda_1$):** Controla o peso $J$.
* **Concentrar Dias Trabalhados ($\lambda_2$):** Controla o peso $D$.
* **Respeitar Preferências de Horários ($\lambda_3$):** Controla o peso $P$.
* **Priorizar Dias Preferidos dos Professores ($\lambda_4$):** Controla o peso $W$.
* **Priorizar Turnos Pretendidos ($\lambda_5$):** Controla o peso $T$.
* **Reduzir Lacunas na Grade das Turmas ($\lambda_6$):** Controla o peso $J_G$.

### 9.2. Estratégias e Restrições Adicionais
* **Modelo de Preferência de Dias ($W$):**
  * `Por Dia Trabalhado`: Penalidade aplicada por dia trabalhado com base em $(10 - \omega_{p,d})$.
  * `Por Minuto de Aula`: Penalidade proporcional aos minutos letivos lecionados no dia.
* **Preenchimento Integral da Jornada:** Checkbox que, quando ativado, proíbe slots vazios no horário das turmas.
* **Distribuir Disciplinas na Semana:** Checkbox e `QSpinBox` para limitar a quantidade máxima de aulas da mesma disciplina por dia para a turma (padrão: 2 aulas/dia).

### 9.3. Recursos Computacionais e Execução
* **Tempo Limite:** `QDoubleSpinBox` com timeout máximo em segundos (ou checkbox `Sem Limite de Tempo`).
* **Limite de Memória RAM:** `QSpinBox` com teto de consumo em Megabytes (padrão: 2048 MB = 2 GB) e opção `Sem Limite de Memória`.
* **Paralelismo de CPU:** `QSpinBox` com a quantidade de threads de processamento simultâneo (workers).
* **Warm-Start:** Checkbox para reutilizar a distribuição atual da grade em tela como semente inicial de resolução.
* **Log Interno de Busca:** Checkbox para habilitar a emissão do log de busca das cláusulas do solver no terminal.

---

## 10. Aba de Diagnóstico e Logs (`createLogsTab`)

* Contém um console monoespaçado de texto enriquecido (`QPlainTextEdit`) com barra de rolagem automática.
* Registra o histórico completo da execução:
  * Relatório de validação prévia de dados (erros impeditivos e avisos).
  * Quantidade de variáveis e cláusulas criadas pelo pré-processador.
  * Notificações de aplicação de Warm-Start.
  * Tempo de resolução e valor final da função objetivo.
  * Relatório da **Auditoria Pós-Resolução** apontando se a grade é 100% livre de conflitos ou detalhando eventuais sobreposições detectadas.

---

## 11. Diálogo de Execução e Otimização (`OptimizeDialog`)

Acionado pelo botão principal `⚡ OTIMIZAR GRADE`:
* **Seletor de Algoritmo:** Dropdown para escolher entre:
  1. `Google OR-Tools CP-SAT (Restrições / Exato) [Recomendado]`
  2. `NVIDIA cuOpt - Otimização Matemática Acelerada por GPU (MILP/LP) [GPU]`
  3. `SCIP - Programação Inteira Mista e por Restrições (Branch-and-Cut)`
  4. `HiGHS - Programação Linear Inteira Mista de Alta Performance (MILP)`
  5. `Heurística Construtiva Gulosa (Rápido / Prioridade)`
* **Painel Explicativo Dinâmico:** Atualiza um texto descritivo explicando a metodologia, garantias matemáticas e casos recomendados para o algoritmo selecionado.
* **Escopo Semestral de Otimização:** Dropdown para definir se a execução otimizará Todas as Disciplinas, 1º Semestre, 2º Semestre ou Ambos Integrado.
* **Sincronização Intersemestral:** Checkbox para garantir horários idênticos para disciplinas anuais.
* **Configuração de Recursos:** Ajuste direto de timeout e memória RAM.
* **Feedback de Execução Assíncrona:** A otimização roda em segundo plano sem congelar a interface, emitindo atualizações na barra de progresso da janela principal e permitindo cancelamento seguro.

---

## 12. Utilitários de Exportação de Relatórios (`ScheduleReportExporter`)

A interface oferece dois canais completos de exportação de dados:

### 12.1. Relatório em HTML Visual Moderno
* Gera arquivo `.html` autossuficiente com design responsivo estilizado via variáveis CSS.
* Painel de cabeçalho contendo os mesmos cards visuais de KPIs da aplicação (Status, Tempo de Resolução, Aulas Alocadas, Janelas, etc.).
* Tabelas de horários segmentadas e formatadas individualmente para:
  * **Todas as Turmas Cadastradas** (com indicação visual de recreios e salas).
  * **Todos os Professores** (com turmas e salas associadas).
  * **Todas as Salas Físicas e Laboratórios** (mostrando a ocupação horária).
* Badges coloridos para disciplinas teóricas, laboratórios, anuais e semestrais.
* Folha de estilo otimizada para impressão direta sem cortes (`@media print`).

### 12.2. Planilha Tabular em CSV
* Gera arquivo `.csv` delimitado por ponto e vírgula contendo todas as alocações da grade com 13 colunas estruturadas:
  `Aula_ID; Disciplina; Tipo; Regime; Semestre; Dia; Inicio; Fim; Duracao_Min; Professores; Turmas; Salas; Fixada`
* Compatível para abertura direta no Microsoft Excel, LibreOffice Calc e ferramentas de análise de dados.
