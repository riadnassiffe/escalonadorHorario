# Modelo 3 — Programação Linear Inteira Mista para a grade de horários

## 1. Objetivo e escopo

Este documento especifica uma formulação MILP para alocar todas as ocorrências semanais de aulas, respeitando professores, turmas, salas, jornadas, intervalos, turnos e semestres. A função objetivo melhora a qualidade da grade sem permitir violações das restrições obrigatórias.

A formulação usa variáveis binárias de seleção de candidatos e restrições lineares de ocupação do tempo. Não depende de primitivas de programação por restrições, como `NoOverlap`, nem de produtos entre variáveis.

Trata-se do **modelo proposto para implementação**, incluindo a correção das lacunas encontradas na revisão. No código atual, `ScheduleSolver::solveMilp` encaminha a resolução para `solveCpSat`; a opção MILP ainda não constitui uma implementação independente desta formulação.

## 2. Convenções temporais e semestrais

O tempo é discretizado em slots de $\Delta=5$ minutos, numerados desde a meia-noite. Uma aula de 45 minutos ocupa 9 slots; uma de 50 minutos ocupa 10. Todos os horários e durações devem ser múltiplos de cinco minutos.

Uma aula com início $t$ e duração $L_a$ ocupa o intervalo semiaberto $[t,t+L_a)$, isto é, os slots $t,\ldots,t+L_a-1$. Portanto, uma aula que termina às 08:00 pode ser seguida por outra que começa às 08:00.

O modelo integrado considera dois semestres. Uma ocorrência anual possui uma única decisão de dia, início e salas, compartilhada pelos dois semestres. Uma ocorrência semestral ocupa recursos somente nos semestres em que está ativa. Assim, aulas de semestres distintos podem compartilhar horário e sala.

Ao otimizar apenas um semestre, excluem-se as ocorrências inativas nesse semestre antes de construir o modelo.

## 3. Conjuntos e índices

| Símbolo | Descrição |
|---|---|
| $P$, $p$ | Professores |
| $G$, $g$ | Turmas |
| $R$, $r$ | Salas e laboratórios |
| $Q$, $q$ | Disciplinas |
| $A$, $a$ | Ocorrências semanais de aulas incluídas na otimização |
| $A_q$ | Ocorrências da disciplina $q$ |
| $D$, $d$ | Dias da semana, de segunda a sexta |
| $S$, $s$ | Semestres incluídos: $\{1\}$, $\{2\}$ ou $\{1,2\}$ |
| $K_d$, $k$ | Slots do horizonte diário, ordenados cronologicamente |
| $H$, $h$ | Turnos usados na avaliação de preferências e janelas |
| $K_{d,h}$ | Slots atribuídos ao turno $h$ para calcular janelas |
| $P_a$, $G_a$ | Professores e turmas participantes da aula $a$ |
| $D_g$ | Dias letivos da turma $g$ |
| $S_a\subseteq S$ | Semestres em que a ocorrência $a$ está ativa |
| $R_g^{teo}$ | Salas elegíveis para a sala teórica fixa da turma $g$ |
| $B_a$ | Combinações admissíveis de salas para a aula $a$ |
| $C_a$ | Candidatos admissíveis da ocorrência $a$ |

Uma disciplina com quatro aulas semanais gera quatro ocorrências em $A_q$. Essas ocorrências representam aulas distintas, mesmo que tenham duração e participantes iguais.

Cada candidato $c\in C_a$ é uma tupla $(d_c,t_c,b_c)$: dia, slot inicial e conjunto de salas simultaneamente reservadas. Usar um conjunto $b_c$ permite representar atividades que exigem várias salas ao mesmo tempo.

Os conjuntos $K_{d,h}$ devem formar uma partição do horizonte avaliado para não contar uma janela em dois turnos. Se os turnos administrativos se sobrepõem, é necessário definir uma classificação única para o cálculo das penalidades. Essa classificação não altera os intervalos de turnos autorizados para uma aula.

## 4. Parâmetros

| Símbolo | Unidade/domínio | Significado |
|---|---|---|
| $L_a$ | Slots inteiros positivos | Duração da ocorrência $a$ |
| $\ell_a=\Delta L_a$ | Minutos | Duração da ocorrência em minutos |
| $T^{ini}_{g,d},T^{fim}_{g,d}$ | Slots | Limites da jornada da turma |
| $[b^j_{g,d},e^j_{g,d})$ | Slots, $j=1,2$ | Dois intervalos obrigatórios da turma |
| $F_{g,d}\subseteq K_d$ | Conjunto de slots | Slots letivos da jornada, excluídos os intervalos |
| $V_{p,d}$ | Conjunto de slots | Disponibilidade do professor |
| $M_{p,d}$ | Minutos | Carga diária máxima do professor, quando definida |
| $\omega_{p,d}$ | Inteiro entre 0 e 10 | Preferência do professor pelo dia; 10 é a maior preferência |
| $\alpha_p$ | Real não negativo | Sensibilidade do professor a janelas |
| $\pi_{p,d,h}$ | Binário | 1 se o professor pretende trabalhar no turno $h$ do dia $d$ |
| $U_{q,g}$ | Inteiro positivo | Máximo de ocorrências da disciplina por dia e turma |
| $c^{hora}_{a,c}$ | Real não negativo | Custo de preferência horária do candidato |
| $c^{sala}_{a,c}$ | Real não negativo | Custo de uso das salas do candidato, se adotado |
| $\lambda_J,\lambda_D,\lambda_P,\lambda_W,\lambda_T,\lambda_G$ | Reais não negativos | Pesos institucionais dos seis critérios principais |
| $\lambda_E,\lambda_R$ | Reais não negativos | Pesos opcionais de estabilidade e preferência de salas |
| $\rho_s$ | Real positivo | Peso de cada semestre na avaliação da qualidade |

Pode-se usar $\alpha_p=\texttt{pesoJanela}_p/5$ para reproduzir a escala relativa do código. Usar a nota diretamente também é possível, desde que o peso global seja calibrado de forma consistente.

Para avaliar os dois semestres com a mesma importância, pode-se usar $\rho_1=\rho_2=1/2$. Para medir o custo acumulado das duas grades semanais, usa-se $\rho_1=\rho_2=1$. Na otimização de um único semestre, usa-se $\rho_s=1$.

Preferência zero não significa indisponibilidade. A proibição de um horário pertence a $V_{p,d}$, não a $\omega_{p,d}$ ou $\pi_{p,d,h}$.

## 5. Construção dos candidatos admissíveis

Para cada $a$, gerar $C_a$ antes de criar as variáveis. Defina $I_{a,c}=\{t_c,\ldots,t_c+L_a-1\}$. Um candidato só pode existir se satisfizer todas as condições abaixo:

1. $d_c\in D_g$ e $I_{a,c}\subseteq F_{g,d_c}$ para toda turma $g\in G_a$.
2. $I_{a,c}\subseteq V_{p,d_c}$ para todo professor $p\in P_a$.
3. O intervalo completo da aula cabe em pelo menos um dos turnos permitidos para a disciplina. Uma lista vazia de turnos permitidos pode representar ausência desse filtro, conforme a convenção do cadastro.
4. $b_c\in B_a$, com salas existentes, disponíveis e compatíveis com tipo, equipamentos e capacidade exigidos pela atividade.
5. Se houver horários de início autorizados, $t_c$ pertence à lista correspondente.
6. Para aula teórica, $b_c=\{r\}$ e $r\in\bigcap_{g\in G_a}R_g^{teo}$.

A capacidade deve considerar todos os estudantes presentes, inclusive em aulas conjuntas. Para atividades com várias salas, a combinação deve ser previamente validada segundo a distribuição dos estudantes; somar capacidades só é adequado quando essa distribuição é permitida.

Os dados de jornada devem satisfazer:

$$
T^{ini}_{g,d}<b^1_{g,d}<e^1_{g,d}\le b^2_{g,d}<e^2_{g,d}<T^{fim}_{g,d}.
$$

Os slots letivos são:

$$
F_{g,d}=\{T^{ini}_{g,d},\ldots,T^{fim}_{g,d}-1\}
\setminus\left([b^1_{g,d},e^1_{g,d})\cup[b^2_{g,d},e^2_{g,d})\right).
$$

Aqui, os intervalos na subtração representam seus slots inteiros. Uma ocorrência não pode ser dividida entre blocos letivos.

**Aulas fixadas passam pelos mesmos filtros.** Uma fixação inválida deve gerar diagnóstico ou inviabilidade; ela não autoriza inserir um candidato incompatível com a jornada ou disponibilidade.

## 6. Variáveis de decisão

| Variável | Domínio | Significado |
|---|---|---|
| $x_{a,c}$ | $\{0,1\}$ | Seleciona o candidato $c$ para a ocorrência $a$ |
| $z_{g,r}$ | $\{0,1\}$ | Designa $r$ como sala teórica fixa de $g$ |
| $y_{p,d,s}$ | $\{0,1\}$ | Professor trabalha no dia e semestre indicados |
| $v_{p,d,h,s}$ | $\{0,1\}$ | Professor trabalha no turno indicado |
| $o^P_{p,d,k,s}$ | $[0,1]$ | Ocupação do professor no slot |
| $o^G_{g,d,k,s}$ | $[0,1]$ | Ocupação da turma no slot |
| $m_{p,d,s}$ | $\mathbb R_{\ge0}$ | Minutos de aula do professor no dia |
| $u^-_{i,k},u^+_{i,k}$ | $\{0,1\}$ | Há ocupação antes/depois de $k$ na sequência $i$ |
| $j_{i,k}$ | $[0,1]$ | Slot vazio situado entre duas ocupações na sequência $i$ |
| $w_{q,d}$ | $\{0,1\}$ | Dia escolhido para um bloco consecutivo da disciplina |
| $\beta_{q,f}$ | $\{0,1\}$ | Seleciona uma configuração consecutiva $f$, definida na seção 9 |
| $e_a$ | $[0,1]$ | Indica mudança em relação à alocação anterior de $a$ |

As variáveis de ocupação, minutos, janelas e mudança podem ser contínuas: as igualdades e linearizações abaixo determinam seus valores a partir das variáveis binárias. As variáveis de blocos e estabilidade só são necessárias quando essas funcionalidades são usadas.

## 7. Expressões auxiliares

Defina o coeficiente constante:

$$
H_{a,c,d,k}=\begin{cases}
1,&d_c=d\text{ e }t_c\le k<t_c+L_a,\\
0,&\text{caso contrário}.
\end{cases}
$$

As ocupações são dadas por:

$$
o^P_{p,d,k,s}=\sum_{a:\,p\in P_a,\ s\in S_a}\sum_{c\in C_a}H_{a,c,d,k}x_{a,c},
$$

$$
o^G_{g,d,k,s}=\sum_{a:\,g\in G_a,\ s\in S_a}\sum_{c\in C_a}H_{a,c,d,k}x_{a,c}.
$$

Para abreviar outras restrições, seja:

$$
X_{p,d,s}=\{(a,c):p\in P_a,\ s\in S_a,\ c\in C_a,\ d_c=d\}.
$$

Defina ainda $X_{p,d,h,s}\subseteq X_{p,d,s}$ contendo os candidatos cuja aula intersecta algum slot de $K_{d,h}$. Uma aula que atravessa dois turnos ativa os dois indicadores de turno; alternativamente, a instituição pode proibir essa travessia na geração de candidatos.

Todas as somas sobre conjuntos vazios valem zero.

## 8. Restrições obrigatórias

### R1. Alocação única e completa

$$
\sum_{c\in C_a}x_{a,c}=1\qquad\forall a\in A.
$$

Todas as ocorrências são obrigatórias. Se $C_a=\varnothing$, a equação torna-se $0=1$, sinalizando inviabilidade. Uma grade parcial não é uma solução viável deste modelo.

### R2. Não sobreposição de professores

$$
o^P_{p,d,k,s}\le1\qquad\forall p,d,k,s.
$$

Professores de uma aula conjunta ficam todos ocupados durante sua duração completa.

### R3. Não sobreposição de turmas

$$
o^G_{g,d,k,s}\le1\qquad\forall g,d,k,s.
$$

Jornadas e intervalos são respeitados pela construção de $C_a$. A restrição acima impede conflitos entre aulas individualmente admissíveis.

### R4. Não sobreposição de salas

$$
\sum_{a:\,s\in S_a}\sum_{c\in C_a:\,r\in b_c}
H_{a,c,d,k}x_{a,c}\le1\qquad\forall r,d,k,s.
$$

Uma atividade que usa várias salas reserva cada uma delas. Uma aula conjunta é uma única ocorrência, evitando contar sua reserva várias vezes por turma participante.

### R5. Sala teórica fixa

$$
\sum_{r\in R_g^{teo}}z_{g,r}=1\qquad\forall g\in G.
$$

Para toda aula teórica, candidato $c$ com $b_c=\{r\}$ e turma participante:

$$
x_{a,c}\le z_{g,r}\qquad\forall g\in G_a.
$$

Se a sala já estiver predefinida como $\bar r_g$:

$$
z_{g,\bar r_g}=1.
$$

As mesmas variáveis $z$ valem para ambos os semestres. Aulas de laboratório não alteram a sala-base. Turmas distintas podem ter a mesma sala-base se suas aulas não se sobrepuserem.

Para aulas teóricas conjuntas, esta política exige a mesma sala-base para todas as turmas participantes. Se a instituição desejar uma sala especial para encontros conjuntos, essas ocorrências devem ter uma regra de sala própria, explicitamente excluída desta vinculação.

### R6. Carga máxima diária dos professores

$$
m_{p,d,s}=\sum_{(a,c)\in X_{p,d,s}}\ell_a x_{a,c},
$$

$$
m_{p,d,s}\le M_{p,d}\qquad\forall p,d,s\text{ com limite definido}.
$$

O limite é medido em minutos, separadamente por semestre. Ausência de limite significa omitir a desigualdade; um valor zero usado como sentinela pelo cadastro não deve ser interpretado como proibição de trabalhar.

### R7. Fixação manual

Para uma ocorrência fixada em um candidato admissível $\bar c_a$:

$$
x_{a,\bar c_a}=1.
$$

Se apenas dia e horário estiverem fixados, seja $C_a^{fix}=\{c\in C_a:d_c=\bar d_a,\ t_c=\bar t_a\}$. Impõe-se:

$$
\sum_{c\in C_a^{fix}}x_{a,c}=1.
$$

Se as salas também estiverem fixadas, acrescenta-se $b_c=\bar b_a$ ao filtro. Um conjunto filtrado vazio significa fixação incompatível. Não se relaxam outras restrições para preservá-la.

### R8. Sincronização de aulas anuais

Para uma aula anual, $S_a=S$ e a mesma variável $x_{a,c}$ participa das restrições de recursos de todos os semestres. Isso garante horários e salas idênticos sem equações adicionais.

Se a sincronização for desativada, criam-se cópias $a^{(s)}$ com decisões independentes e $S_{a^{(s)}}=\{s\}$. Em uma implementação com variáveis separadas mesmo no modo sincronizado, é necessário impor $x^{(1)}_{a,c}=x^{(2)}_{a,c}$ para candidatos comuns e eliminar candidatos sem correspondência.

### R9. Primeira aula no início da jornada

Para cada turma, dia letivo e semestre, defina:

$$
B_{g,d,s}=\sum_{a:\,g\in G_a,\ s\in S_a}
\sum_{c\in C_a:\,d_c=d,\ t_c=T^{ini}_{g,d}}x_{a,c}.
$$

Para todo candidato da turma ativo nesse semestre com $d_c=d$ e $t_c>T^{ini}_{g,d}$:

$$
x_{a,c}\le B_{g,d,s}.
$$

Se houver aulas no dia, alguma começa exatamente no início da jornada. Um dia sem aulas continua permitido, salvo quando o preenchimento integral estiver ativado. Se nenhum candidato puder começar no início, nenhuma aula poderá ser alocada nesse dia.

## 9. Restrições opcionais que se tornam obrigatórias quando ativadas

### R10. Preenchimento integral da jornada

$$
o^G_{g,d,k,s}=1\qquad\forall g,\ d\in D_g,\ k\in F_{g,d},\ s\in S.
$$

Devem ser criadas equações inclusive para dias e slots sem candidatos. Nesse caso, a igualdade $0=1$ identifica corretamente a impossibilidade de preencher a jornada. Intervalos obrigatórios não são preenchidos.

Esta versão exige preenchimento nos semestres selecionados para todas as turmas incluídas. Turmas sem funcionamento em determinado semestre devem ser excluídas desse conjunto de índices por um parâmetro explícito de funcionamento semestral.

### R11. Máximo diário da mesma disciplina

$$
\sum_{a\in A_q:\,g\in G_a,\ s\in S_a}
\sum_{c\in C_a:\,d_c=d}x_{a,c}\le U_{q,g}
\qquad\forall q,g,d,s.
$$

Usa-se o limite da disciplina quando definido; caso contrário, o padrão institucional. Esta é uma restrição rígida quando ativada. Torná-la flexível exige uma folga não negativa penalizada e uma política explícita para permitir o excesso.

### R12. Blocos realmente consecutivos

Exigir somente que várias aulas ocorram no mesmo dia não garante consecutividade. Para garantir adjacência, usam-se configurações completas de bloco.

Para cada disciplina com essa regra, particione suas ocorrências em grupos obrigatórios $A_{q,j}$, cada um representando um bloco. Exemplos: duas aulas formam um bloco de duas; quatro formam um de quatro; oito formam dois de quatro. Uma partição fixa pressupõe ocorrências intercambiáveis; se elas tiverem fixações ou atributos diferentes, a formação dos grupos deve ser definida explicitamente ou enumerada entre as configurações admissíveis.

Seja $\mathcal F_q$ o conjunto de configurações admissíveis de **um** bloco da disciplina. Cada configuração $f$ especifica o grupo $j(f)$ e um candidato $c_f(a)$ para cada $a\in A_{q,j(f)}$, todos no mesmo dia $d_f$, com uma ordem $(a_1,\ldots,a_n)$ tal que:

$$
t_{c_f(a_{i+1})}=t_{c_f(a_i)}+L_{a_i}\qquad i=1,\ldots,n-1.
$$

Todos os candidatos da configuração pertencem aos respectivos $C_a$. Portanto, o bloco inteiro precisa caber sem atravessar intervalos. Se for exigida a mesma combinação de salas para o bloco, esse requisito também deve ser usado na enumeração.

Escolhe-se uma configuração por grupo:

$$
\sum_{f\in\mathcal F_q:\,j(f)=j}\beta_{q,f}=1\qquad\forall j.
$$

Vinculam-se as escolhas de candidatos às configurações:

$$
x_{a,c}=\sum_{f\in\mathcal F_q:\,j(f)=j,\ c_f(a)=c}\beta_{q,f}
\qquad\forall a\in A_{q,j},\ c\in C_a.
$$

Para exigir no máximo um bloco por dia, como no caso de oito aulas em dois dias distintos:

$$
\sum_{f\in\mathcal F_q:\,d_f=d}\beta_{q,f}=w_{q,d}\qquad\forall d.
$$

Como $w_{q,d}$ é binária, dois blocos no mesmo dia são proibidos. As equações por grupo implicam $\sum_d w_{q,d}$ igual ao número de blocos.

Disciplinas com outras quantidades semanais precisam de uma partição pedagógica explícita; não se deve ignorar silenciosamente a regra. Se R11 permitir no máximo duas aulas por dia e R12 exigir quatro, há incompatibilidade: o cadastro precisa definir um limite compatível ou desativar R11 para essa disciplina.

## 10. Indicadores exatos de dias e turnos trabalhados

Para todo $(a,c)\in X_{p,d,s}$:

$$
x_{a,c}\le y_{p,d,s},\qquad
y_{p,d,s}\le\sum_{(a,c)\in X_{p,d,s}}x_{a,c}.
$$

A segunda desigualdade é imposta uma vez por $(p,d,s)$; a primeira, uma vez por candidato. Se o conjunto estiver vazio, $y_{p,d,s}=0$.

De modo análogo:

$$
x_{a,c}\le v_{p,d,h,s}\quad\forall(a,c)\in X_{p,d,h,s},\qquad
v_{p,d,h,s}\le\sum_{(a,c)\in X_{p,d,h,s}}x_{a,c}.
$$

As duas direções garantem indicadores corretos mesmo quando seus pesos na função objetivo forem zero.

## 11. Linearização exata de janelas

Uma janela é um slot vazio com pelo menos uma ocupação anterior e uma posterior no período avaliado. Tempo livre antes da primeira aula ou depois da última não conta como janela.

Para professores, cada sequência $i=(p,d,h,s)$ usa os slots ordenados $E_i=K_{d,h}$ e ocupações $o_{i,k}=o^P_{p,d,k,s}$. Isso evita contar a transição entre turnos como janela.

Para turmas, cada sequência $i=(g,d,s)$ usa $E_i=F_{g,d}$ e $o_{i,k}=o^G_{g,d,k,s}$. Os intervalos obrigatórios ficam fora da contagem, mas uma lacuna letiva antes ou depois deles ainda pode ser detectada.

Defina $E_i^-(k)=\{l\in E_i:l<k\}$ e $E_i^+(k)=\{l\in E_i:l>k\}$. Impõe-se:

$$
u^-_{i,k}\ge o_{i,l}\quad\forall l\in E_i^-(k),\qquad
u^-_{i,k}\le\sum_{l\in E_i^-(k)}o_{i,l},
$$

$$
u^+_{i,k}\ge o_{i,l}\quad\forall l\in E_i^+(k),\qquad
u^+_{i,k}\le\sum_{l\in E_i^+(k)}o_{i,l}.
$$

Por fim:

$$
\begin{aligned}
j_{i,k}&\ge u^-_{i,k}+u^+_{i,k}-o_{i,k}-1,\\
j_{i,k}&\le u^-_{i,k},\\
j_{i,k}&\le u^+_{i,k},\\
j_{i,k}&\le1-o_{i,k}.
\end{aligned}
$$

Essas desigualdades fazem $j_{i,k}=1$ exatamente quando há aula antes, aula depois e nenhuma aula em $k$. Nas extremidades, os conjuntos vazios fixam o indicador correspondente em zero.

Os totais, em minutos, são:

$$
J=\Delta\sum_s\rho_s\sum_{p,d,h}\alpha_p\sum_{k\in K_{d,h}}j_{(p,d,h,s),k},
$$

$$
J_G=\Delta\sum_s\rho_s\sum_g\sum_{d\in D_g}\sum_{k\in F_{g,d}}j_{(g,d,s),k}.
$$

Indisponibilidade entre duas aulas do professor é contada como tempo de espera nesta definição. Caso a instituição deseje excluí-la, deve definir um subconjunto explícito de slots contabilizáveis, mantendo a detecção de ocupações anteriores e posteriores.

A formulação acima é didática e pode gerar muitas desigualdades. Uma implementação pode substituir as relações de existência antes/depois por recorrências de prefixo e sufixo, preservando a mesma lógica e reduzindo o tamanho do modelo.

## 12. Função objetivo

### 12.1. Dias trabalhados

$$
D_{prof}=\sum_s\rho_s\sum_{p,d}y_{p,d,s}.
$$

Minimizar esse termo concentra a carga em menos dias por semestre.

### 12.2. Preferência de horários

$$
P_{hora}=\sum_{a\in A}\sum_{c\in C_a}
\left(\sum_{s\in S_a}\rho_s\right)c^{hora}_{a,c}x_{a,c}.
$$

O custo é um parâmetro pré-calculado. Ele pode representar horários pedagogicamente desfavoráveis ou preferências individuais agregadas dos participantes.

### 12.3. Preferência de dias

Escolher uma das duas versões:

$$
W_{dia}=\sum_s\rho_s\sum_{p,d}(10-\omega_{p,d})y_{p,d,s},
$$

ou

$$
W_{min}=\sum_s\rho_s\sum_{p,d}(10-\omega_{p,d})m_{p,d,s}.
$$

A primeira cobra uma vez por dia trabalhado; a segunda cobra proporcionalmente aos minutos. A troca de versão exige recalibrar $\lambda_W$, pois as unidades são diferentes.

### 12.4. Turnos pretendidos

$$
T_{prof}=\sum_s\rho_s\sum_{p,d,h}(1-\pi_{p,d,h})v_{p,d,h,s}.
$$

O custo é cobrado uma vez por turno trabalhado e não pretendido. Turno não pretendido continua permitido se houver disponibilidade.

### 12.5. Estabilidade e preferência de salas, opcionais

Para cada ocorrência com alocação anterior, seja $C_a^{ant}$ o conjunto de candidatos que preservam os atributos escolhidos como critério de estabilidade, por exemplo dia, início e salas:

$$
e_a=1-\sum_{c\in C_a^{ant}}x_{a,c},\qquad E=\sum_{a\text{ com alocação anterior}}e_a.
$$

Se a alocação anterior deixou de ser admissível, $C_a^{ant}=\varnothing$ e $e_a=1$. Para salas:

$$
C_{salas}=\sum_a\sum_{c\in C_a}
\left(\sum_{s\in S_a}\rho_s\right)c^{sala}_{a,c}x_{a,c}.
$$

Esses são critérios opcionais da formulação proposta. Fornecer a grade anterior como solução inicial (*warm start*) é uma estratégia de busca e, por si só, não cria uma preferência por preservá-la. Para isso, é necessário incluir $E$ no objetivo; para impedir mudanças, usa-se R7.

### 12.6. Objetivo completo

$$
\boxed{\min Z=\lambda_JJ+\lambda_DD_{prof}+\lambda_PP_{hora}
+\lambda_WW+\lambda_TT_{prof}+\lambda_GJ_G
+\lambda_EE+\lambda_RC_{salas}}
$$

Aqui, $W$ é $W_{dia}$ ou $W_{min}$. Os dois últimos termos podem ser omitidos. Os seis primeiros pesos correspondem, respectivamente, a `lambda_janelas`, `lambda_dias`, `lambda_preferencias`, `lambda_preferencia_dias`, `lambda_preferencia_turnos` e `lambda_lacunas_turma`.

Os termos têm unidades diferentes: minutos, dias, turnos e custos abstratos. Os pesos devem refletir trocas aceitáveis entre esses critérios. Peso zero desativa um critério de qualidade, mas não desativa uma restrição obrigatória.

## 13. Exemplo de consecutividade e duração

Uma aula de 45 minutos começando às 07:00 ocupa $[84,93)$; outra de 50 minutos começando às 07:45 ocupa $[93,103)$. Elas terminam às 07:45 e 08:35, respectivamente, e são consecutivas porque $93=84+9$.

Se a segunda começar às 08:00, não existe sobreposição, mas há uma janela de três slots, ou 15 minutos. As duas aulas não formam um bloco consecutivo, mesmo ocorrendo no mesmo dia.

Se houver intervalo obrigatório da turma às 08:20–08:35, o segundo candidato do primeiro exemplo é inadmissível porque atravessa o intervalo.

## 14. Validação, resolução e leitura do resultado

Antes da resolução, verificar referências de entidades, durações, jornadas, salas elegíveis, fixações e compatibilidade entre limites diários e blocos. Essas verificações ajudam a explicar inviabilidades, mas não substituem as restrições do modelo.

Para cada turma e semestre, a carga semanal total não pode superar a soma dos minutos letivos disponíveis. Com preenchimento integral, essas quantidades devem ser iguais. Essa condição é necessária, mas não suficiente: disponibilidade de professores, salas e encaixe das durações também podem impedir uma grade.

O MILP reúne R1–R9, as restrições opcionais ativadas, as equações auxiliares e os domínios das variáveis, minimizando $Z$. Todos os coeficientes são constantes e todas as expressões são lineares. O modelo é um MILP com variáveis binárias e auxiliares contínuas; também admite uma versão inteiramente inteira.

Tempo máximo, memória, número de threads e solução inicial são parâmetros de execução do resolvedor, não restrições da grade. Ao atingir um limite, é preciso distinguir:

- **Ótimo comprovado:** há solução viável e prova de otimalidade.
- **Viável sem prova de otimalidade:** há solução que satisfaz todas as restrições, com qualidade ainda potencialmente melhorável.
- **Inviável comprovado:** nenhuma solução satisfaz as restrições.
- **Sem solução encontrada:** a busca terminou sem solução e sem prova de inviabilidade.

Uma implementação deve registrar o valor da melhor solução, o melhor limite e a lacuna de otimalidade quando disponíveis. A validação final deve conferir todas as ocorrências, ocupações por semestre, cargas, salas-base, fixações, preenchimento e adjacência dos blocos, independentemente do algoritmo que produziu a grade.
