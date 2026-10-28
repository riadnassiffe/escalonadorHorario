/**
 * @file AddLessonToScheduleDialog.cpp
 * @brief Implementação do diálogo para alocação manual de disciplinas na grade semanal.
 */

#include "ui/AddLessonToScheduleDialog.h"
#include "ui/ModernTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <algorithm>
#include <set>

namespace scheduler::ui {

AddLessonToScheduleDialog::AddLessonToScheduleDialog(model::InstitutionData& data,
                                                     QWidget* parent,
                                                     model::DayOfWeek default_day,
                                                     const model::TimeInterval& default_interval,
                                                     const std::string& default_turma_id,
                                                     const std::string& default_professor_id,
                                                     const std::string& default_room_id)
    : QDialog(parent),
      data_(data),
      default_day_(default_day),
      default_interval_(default_interval),
      default_turma_id_(default_turma_id),
      default_professor_id_(default_professor_id),
      default_room_id_(default_room_id) {

    // Assegurar que as ocorrências de aulas estejam geradas no modelo de dados
    if (data_.aulas().empty() && !data_.disciplinas().empty()) {
        data_.gerarOcorrenciasAulas(true, data_.config().escopo_semestre);
    }

    setupUi();
    populateDisciplinas();
}

std::string AddLessonToScheduleDialog::aulaId() const {
    if (!combo_ocorrencia_ || combo_ocorrencia_->currentIndex() < 0) return {};
    return combo_ocorrencia_->currentData().toString().toStdString();
}

void AddLessonToScheduleDialog::setupUi() {
    setWindowTitle("Alocar Disciplina na Grade Semanal");
    setMinimumWidth(600);
    resize(640, 560);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(12);
    main_layout->setContentsMargins(16, 16, 16, 16);

    // Título e Descrição
    auto* lbl_title = new QLabel("<h3>➕ Alocar Disciplina Manualmente</h3>", this);
    main_layout->addWidget(lbl_title);

    auto* lbl_sub = new QLabel(
        "Selecione a disciplina, a ocorrência semanal e o horário para compor sua grade inicial.<br>"
        "As aulas fixadas serão preservadas durante a execução do otimizador automático.", this);
    lbl_sub->setStyleSheet("color: palette(window-text); font-size: 11px; margin-bottom: 4px;");
    main_layout->addWidget(lbl_sub);

    // Painel de Filtros
    auto* filter_box = new QHBoxLayout();
    filter_box->setSpacing(12);

    chk_filtro_turma_ = new QCheckBox("Apenas disciplinas da turma selecionada", this);
    chk_filtro_turma_->setChecked(!default_turma_id_.empty());
    chk_filtro_turma_->setEnabled(!default_turma_id_.empty());
    filter_box->addWidget(chk_filtro_turma_);

    chk_apenas_pendentes_ = new QCheckBox("Apenas com aulas pendentes de alocação", this);
    chk_apenas_pendentes_->setChecked(true);
    filter_box->addWidget(chk_apenas_pendentes_);

    filter_box->addStretch();
    main_layout->addLayout(filter_box);

    connect(chk_filtro_turma_, &QCheckBox::toggled, this, &AddLessonToScheduleDialog::onFiltrosChanged);
    connect(chk_apenas_pendentes_, &QCheckBox::toggled, this, &AddLessonToScheduleDialog::onFiltrosChanged);

    // Formulário de Seleção
    auto* form_group = new QGroupBox("Dados da Alocação", this);
    auto* form = new QFormLayout(form_group);
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight);

    // Disciplina
    combo_disciplina_ = new QComboBox(this);
    combo_disciplina_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    combo_disciplina_->setMinimumWidth(380);
    form->addRow("Disciplina:", combo_disciplina_);

    // Card informativo da disciplina selecionada
    lbl_disciplina_info_ = new QLabel(this);
    lbl_disciplina_info_->setStyleSheet(
        "background-color: palette(base); border: 1px solid palette(mid); border-radius: 6px; "
        "padding: 8px; font-size: 11px;");
    lbl_disciplina_info_->setWordWrap(true);
    form->addRow("", lbl_disciplina_info_);

    // Ocorrência da Aula
    combo_ocorrencia_ = new QComboBox(this);
    combo_ocorrencia_->setMinimumWidth(380);
    form->addRow("Ocorrência:", combo_ocorrencia_);

    // Dia da Semana
    combo_dia_ = new QComboBox(this);
    for (auto d : model::allDays()) {
        combo_dia_->addItem(QString::fromStdString(model::dayToLongString(d)), static_cast<int>(d));
    }
    int def_day_idx = combo_dia_->findData(static_cast<int>(default_day_));
    if (def_day_idx >= 0) combo_dia_->setCurrentIndex(def_day_idx);
    form->addRow("Dia da Semana:", combo_dia_);

    // Horário / Intervalo
    combo_horario_ = new QComboBox(this);
    combo_horario_->setMinimumWidth(380);
    form->addRow("Horário:", combo_horario_);

    // Sala de Aula
    combo_sala_ = new QComboBox(this);
    combo_sala_->setMinimumWidth(380);
    form->addRow("Sala de Aula:", combo_sala_);

    // Semestre de Vigência
    combo_semestre_ = new QComboBox(this);
    combo_semestre_->addItem("Ambos os Semestres (Ano Todo)", 3);
    combo_semestre_->addItem("1º Semestre", 1);
    combo_semestre_->addItem("2º Semestre", 2);
    form->addRow("Semestre:", combo_semestre_);

    // Checkbox de Fixação (Pin)
    chk_fixar_ = new QCheckBox("📌 Fixar aula na grade (bloquear para o otimizador automático)", this);
    chk_fixar_->setChecked(true);
    chk_fixar_->setStyleSheet("font-weight: 600; color: palette(highlight); margin-top: 4px;");
    form->addRow("", chk_fixar_);

    main_layout->addWidget(form_group);

    // Banner de Auditoria de Conflitos
    lbl_conflito_status_ = new QLabel(this);
    lbl_conflito_status_->setWordWrap(true);
    lbl_conflito_status_->setStyleSheet(
        "background-color: palette(base); border: 1px solid palette(mid); border-radius: 6px; "
        "padding: 8px 12px; font-size: 11px;");
    main_layout->addWidget(lbl_conflito_status_);

    main_layout->addStretch();

    // Botões de Ação
    auto* btn_layout = new QHBoxLayout();
    btn_layout->addStretch();

    btn_cancelar_ = new QPushButton("Cancelar", this);
    btn_cancelar_->setObjectName("btnSecondary");
    btn_layout->addWidget(btn_cancelar_);

    btn_alocar_ = new QPushButton("Alocar Aula na Grade", this);
    btn_alocar_->setObjectName("btnPrimary");
    btn_alocar_->setCursor(Qt::PointingHandCursor);
    btn_alocar_->setDefault(true);
    btn_layout->addWidget(btn_alocar_);

    main_layout->addLayout(btn_layout);

    // Conexões internas
    connect(combo_disciplina_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddLessonToScheduleDialog::onDisciplinaChanged);
    connect(combo_ocorrencia_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddLessonToScheduleDialog::onOcorrenciaChanged);
    connect(combo_dia_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddLessonToScheduleDialog::onDiaChanged);
    connect(combo_horario_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddLessonToScheduleDialog::onHorarioChanged);
    connect(combo_sala_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddLessonToScheduleDialog::onSalaChanged);
    connect(combo_semestre_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddLessonToScheduleDialog::updateValidationStatus);

    connect(btn_alocar_, &QPushButton::clicked, this, &AddLessonToScheduleDialog::onAcceptClicked);
    connect(btn_cancelar_, &QPushButton::clicked, this, &QDialog::reject);
}

void AddLessonToScheduleDialog::onFiltrosChanged() {
    populateDisciplinas();
}

void AddLessonToScheduleDialog::populateDisciplinas() {
    combo_disciplina_->blockSignals(true);
    combo_disciplina_->clear();

    bool filtrar_turma = chk_filtro_turma_->isChecked() && !default_turma_id_.empty();
    bool apenas_pendentes = chk_apenas_pendentes_->isChecked();

    int pre_select_idx = -1;

    for (const auto& d : data_.disciplinas()) {
        if (filtrar_turma) {
            if (std::find(d.turmas().begin(), d.turmas().end(), default_turma_id_) == d.turmas().end()) {
                continue;
            }
        }

        // Contar quantas aulas desta disciplina já estão na grade
        int alocadas = 0;
        for (const auto& a : data_.grade().alocacoes()) {
            if (a.disciplina_id == d.id()) {
                alocadas++;
            }
        }

        if (apenas_pendentes && alocadas >= d.aulasSemanais()) {
            continue;
        }

        QString prof_str;
        for (size_t i = 0; i < d.professores().size(); ++i) {
            if (i > 0) prof_str += ", ";
            const auto* p = data_.findProfessor(d.professores()[i]);
            prof_str += QString::fromStdString(p ? p->nome() : d.professores()[i]);
        }

        QString turma_str;
        for (size_t i = 0; i < d.turmas().size(); ++i) {
            if (i > 0) turma_str += ", ";
            const auto* t = data_.findTurma(d.turmas()[i]);
            turma_str += QString::fromStdString(t ? t->nome() : d.turmas()[i]);
        }

        QString item_text = QString("%1 (%2) | Turma: %3 | Prof: %4 [%5/%6 alocadas]")
            .arg(QString::fromStdString(d.nome()))
            .arg(QString::fromStdString(d.id()))
            .arg(turma_str.isEmpty() ? "Nenhuma" : turma_str)
            .arg(prof_str.isEmpty() ? "Sem professor" : prof_str)
            .arg(alocadas)
            .arg(d.aulasSemanais());

        combo_disciplina_->addItem(item_text, QString::fromStdString(d.id()));

        // Priorizar disciplina se combinava com turma ou professor prévio
        if (pre_select_idx < 0) {
            if (!default_turma_id_.empty() && std::find(d.turmas().begin(), d.turmas().end(), default_turma_id_) != d.turmas().end()) {
                pre_select_idx = combo_disciplina_->count() - 1;
            } else if (!default_professor_id_.empty() && std::find(d.professores().begin(), d.professores().end(), default_professor_id_) != d.professores().end()) {
                pre_select_idx = combo_disciplina_->count() - 1;
            }
        }
    }

    if (combo_disciplina_->count() == 0) {
        combo_disciplina_->addItem("(Nenhuma disciplina compatível encontrada)", "");
        combo_disciplina_->setEnabled(false);
        btn_alocar_->setEnabled(false);
    } else {
        combo_disciplina_->setEnabled(true);
        btn_alocar_->setEnabled(true);
        if (pre_select_idx >= 0) {
            combo_disciplina_->setCurrentIndex(pre_select_idx);
        } else {
            combo_disciplina_->setCurrentIndex(0);
        }
    }

    combo_disciplina_->blockSignals(false);
    onDisciplinaChanged(combo_disciplina_->currentIndex());
}

void AddLessonToScheduleDialog::onDisciplinaChanged(int /*index*/) {
    updateSummaryCard();
    populateOcorrencias();
    populateHorarios();
    populateSalas();
    updateValidationStatus();
}

void AddLessonToScheduleDialog::updateSummaryCard() {
    if (combo_disciplina_->currentIndex() < 0) {
        lbl_disciplina_info_->setText("Selecione uma disciplina.");
        return;
    }

    std::string disc_id = combo_disciplina_->currentData().toString().toStdString();
    const auto* disc = data_.findDisciplina(disc_id);
    if (!disc) {
        lbl_disciplina_info_->setText("Disciplina não localizada.");
        return;
    }

    QStringList t_names, p_names;
    for (const auto& tid : disc->turmas()) {
        const auto* t = data_.findTurma(tid);
        t_names.append(QString::fromStdString(t ? t->nome() : tid));
    }
    for (const auto& pid : disc->professores()) {
        const auto* p = data_.findProfessor(pid);
        p_names.append(QString::fromStdString(p ? p->nome() : pid));
    }

    QString regime_str = (disc->regime() == model::RegimeDisciplina::ANUAL)
        ? "Anual"
        : (disc->semestre() == model::SemestreLetivo::SEMESTRE_1 ? "1º Semestre" : "2º Semestre");

    int alocadas = 0;
    for (const auto& a : data_.grade().alocacoes()) {
        if (a.disciplina_id == disc->id()) alocadas++;
    }

    lbl_disciplina_info_->setText(
        QString("<b>Turma(s):</b> %1 | <b>Professor(es):</b> %2<br>"
                "<b>Tipo:</b> %3 | <b>Duração:</b> %4 min (%5 slots) | <b>Regime:</b> %6<br>"
                "<b>Status semanal:</b> %7 de %8 aulas alocadas na grade")
            .arg(t_names.join(", "))
            .arg(p_names.isEmpty() ? "Nenhum" : p_names.join(", "))
            .arg(QString::fromStdString(disc->tipo()))
            .arg(disc->duracaoMinutos())
            .arg(disc->duracaoSlots())
            .arg(regime_str)
            .arg(alocadas)
            .arg(disc->aulasSemanais())
    );

    // Ajustar combo semestre
    combo_semestre_->blockSignals(true);
    if (disc->regime() == model::RegimeDisciplina::ANUAL) {
        combo_semestre_->setEnabled(true);
        combo_semestre_->setCurrentIndex(combo_semestre_->findData(3));
    } else {
        combo_semestre_->setEnabled(false);
        int s_idx = (disc->semestre() == model::SemestreLetivo::SEMESTRE_2) ? 2 : 1;
        combo_semestre_->setCurrentIndex(combo_semestre_->findData(s_idx));
    }
    combo_semestre_->blockSignals(false);
}

void AddLessonToScheduleDialog::populateOcorrencias() {
    combo_ocorrencia_->blockSignals(true);
    combo_ocorrencia_->clear();

    std::string disc_id = combo_disciplina_->currentData().toString().toStdString();
    const auto* disc = data_.findDisciplina(disc_id);
    if (!disc) {
        combo_ocorrencia_->blockSignals(false);
        return;
    }

    int first_pending_idx = -1;

    for (int i = 1; i <= disc->aulasSemanais(); ++i) {
        std::string aula_id = disc->id() + "_" + std::to_string(i);
        const auto* a_aloc = data_.grade().findAula(aula_id);

        QString status_txt;
        if (a_aloc) {
            status_txt = QString("[Alocada: %1 %2 | Sala: %3]")
                .arg(QString::fromStdString(a_aloc->diaString()))
                .arg(QString::fromStdString(a_aloc->horarioFormatado()))
                .arg(QString::fromStdString(a_aloc->salas.empty() ? "-" : a_aloc->salas.front()));
        } else {
            status_txt = "[Pendente de alocação]";
            if (first_pending_idx < 0) {
                first_pending_idx = combo_ocorrencia_->count();
            }
        }

        QString item_text = QString("Aula %1 (%2) - %3")
            .arg(i)
            .arg(QString::fromStdString(aula_id))
            .arg(status_txt);

        combo_ocorrencia_->addItem(item_text, QString::fromStdString(aula_id));
    }

    // Opção para ocorrência extra se o usuário quiser acrescentar além da carga padrão
    std::string extra_id = disc->id() + "_" + std::to_string(disc->aulasSemanais() + 1);
    combo_ocorrencia_->addItem(QString("➕ Ocorrência Extra (%1)").arg(QString::fromStdString(extra_id)), QString::fromStdString(extra_id));

    if (first_pending_idx >= 0) {
        combo_ocorrencia_->setCurrentIndex(first_pending_idx);
    } else {
        combo_ocorrencia_->setCurrentIndex(0);
    }

    combo_ocorrencia_->blockSignals(false);
}

void AddLessonToScheduleDialog::populateHorarios() {
    combo_horario_->blockSignals(true);
    combo_horario_->clear();

    std::string disc_id = combo_disciplina_->currentData().toString().toStdString();
    const auto* disc = data_.findDisciplina(disc_id);

    int dur_slots = disc ? disc->duracaoSlots() : 10;
    if (dur_slots <= 0) dur_slots = 10;

    auto dia = static_cast<model::DayOfWeek>(combo_dia_->currentData().toInt());

    // Obter turma principal da disciplina
    const model::Turma* turma = nullptr;
    if (disc && !disc->turmas().empty()) {
        turma = data_.findTurma(disc->turmas().front());
    }

    std::set<std::pair<int, int>> slots_set;

    if (turma && turma->funcionaNoDia(dia)) {
        auto j = turma->jornadaParaDia(dia);
        for (const auto& b : j.blocosLetivos()) {
            for (int s = b.start_slot; s + dur_slots <= b.end_slot; s += dur_slots) {
                slots_set.emplace(s, s + dur_slots);
            }
        }
    }

    // Se a turma não tiver horários ou não couber, sugerir os intervalos gerais da instituição
    if (slots_set.empty()) {
        // Horários convencionais matutinos, vespertinos e noturnos
        std::vector<std::pair<std::string, std::string>> defaults = {
            {"07:30", "08:20"}, {"08:20", "09:10"}, {"09:30", "10:20"}, {"10:20", "11:10"}, {"11:10", "12:00"},
            {"13:00", "13:50"}, {"13:50", "14:40"}, {"15:00", "15:50"}, {"15:50", "16:40"}, {"16:40", "17:30"},
            {"19:00", "19:50"}, {"19:50", "20:40"}, {"20:50", "21:40"}, {"21:40", "22:30"}
        };
        for (const auto& p : defaults) {
            model::TimeInterval inter(p.first, p.second);
            slots_set.emplace(inter.start_slot, inter.start_slot + dur_slots);
        }
    }

    // Assegurar que default_interval seja incluído se fornecido
    if (default_interval_.durationSlots() > 0) {
        slots_set.emplace(default_interval_.start_slot, default_interval_.start_slot + dur_slots);
    }

    int pre_sel_idx = -1;
    int idx_count = 0;

    for (const auto& p : slots_set) {
        model::TimeInterval inter(p.first, p.second);
        QString item_text = QString("%1 (%2 min)")
            .arg(QString::fromStdString(inter.toString()))
            .arg(inter.durationMinutes());

        combo_horario_->addItem(item_text, QPoint(inter.start_slot, inter.end_slot));

        if (default_interval_.durationSlots() > 0 && inter.start_slot == default_interval_.start_slot) {
            pre_sel_idx = idx_count;
        }
        idx_count++;
    }

    if (pre_sel_idx >= 0) {
        combo_horario_->setCurrentIndex(pre_sel_idx);
    } else {
        combo_horario_->setCurrentIndex(0);
    }

    combo_horario_->blockSignals(false);
}

void AddLessonToScheduleDialog::populateSalas() {
    combo_sala_->blockSignals(true);
    combo_sala_->clear();

    std::string disc_id = combo_disciplina_->currentData().toString().toStdString();
    const auto* disc = data_.findDisciplina(disc_id);

    std::string suggested_room;

    // Se disciplina teórica, verificar sala teórica fixa da turma
    if (disc && disc->isTeorica() && !disc->turmas().empty()) {
        const auto* turma = data_.findTurma(disc->turmas().front());
        if (turma) {
            suggested_room = turma->temSalaTeoricaPredefinida()
                ? turma->salaTeoricaId()
                : data_.grade().salaTeoricaTurma(turma->id());
        }
    } else if (disc && !disc->isTeorica() && !disc->salasAlternativas().empty() && !disc->salasAlternativas().front().empty()) {
        suggested_room = disc->salasAlternativas().front().front();
    }

    if (suggested_room.empty() && !default_room_id_.empty()) {
        suggested_room = default_room_id_;
    }

    int pre_idx = -1;

    for (const auto& s : data_.salas()) {
        QString text = QString("%1 (%2 - %3 alunos)")
            .arg(QString::fromStdString(s.nome()))
            .arg(QString::fromStdString(s.tipo()))
            .arg(s.capacidade());

        combo_sala_->addItem(text, QString::fromStdString(s.id()));

        if (!suggested_room.empty() && s.id() == suggested_room) {
            pre_idx = combo_sala_->count() - 1;
        }
    }

    if (pre_idx >= 0) {
        combo_sala_->setCurrentIndex(pre_idx);
    } else if (combo_sala_->count() > 0) {
        combo_sala_->setCurrentIndex(0);
    }

    combo_sala_->blockSignals(false);
}

void AddLessonToScheduleDialog::onOcorrenciaChanged(int /*index*/) {
    updateValidationStatus();
}

void AddLessonToScheduleDialog::onDiaChanged(int /*index*/) {
    populateHorarios();
    updateValidationStatus();
}

void AddLessonToScheduleDialog::onHorarioChanged(int /*index*/) {
    updateValidationStatus();
}

void AddLessonToScheduleDialog::onSalaChanged(int /*index*/) {
    updateValidationStatus();
}

std::vector<std::string> AddLessonToScheduleDialog::verificarConflitos() const {
    std::vector<std::string> conflitos;

    if (combo_disciplina_->currentIndex() < 0 || combo_horario_->currentIndex() < 0) {
        return conflitos;
    }

    std::string disc_id = combo_disciplina_->currentData().toString().toStdString();
    const auto* disc = data_.findDisciplina(disc_id);
    if (!disc) return conflitos;

    std::string cur_aid = aulaId();
    auto dia = static_cast<model::DayOfWeek>(combo_dia_->currentData().toInt());

    QPoint p = combo_horario_->currentData().toPoint();
    model::TimeInterval intervalo(p.x(), p.y());

    std::string sel_sala = combo_sala_->currentIndex() >= 0
        ? combo_sala_->currentData().toString().toStdString() : "";

    int sem_alocado = combo_semestre_->currentData().toInt();

    auto time_overlaps = [](const model::TimeInterval& t1, const model::TimeInterval& t2) {
        return std::max(t1.start_slot, t2.start_slot) < std::min(t1.end_slot, t2.end_slot);
    };

    auto semestres_conflitam = [sem_alocado](const model::AllocatedLesson& a) {
        if (sem_alocado == 3 || a.regime == model::RegimeDisciplina::ANUAL) return true;
        int s1 = (sem_alocado == 2) ? 2 : 1;
        int s2 = (a.semestre == model::SemestreLetivo::SEMESTRE_2 || a.semestre_alocado == 2) ? 2 : 1;
        return s1 == s2;
    };

    // 1. Validar turmas
    for (const auto& t_id : disc->turmas()) {
        const auto* t = data_.findTurma(t_id);
        if (t) {
            if (!t->funcionaNoDia(dia)) {
                conflitos.push_back("A turma " + t->nome() + " não funciona na " + model::dayToLongString(dia) + ".");
            }
            auto j = t->jornadaParaDia(dia);
            for (const auto& inter : j.intervalosAtivos()) {
                if (time_overlaps(inter, intervalo)) {
                    conflitos.push_back("O horário sobrepõe o intervalo de recreio (" + inter.toString() + ") da turma " + t->nome() + ".");
                }
            }
            if (intervalo.start_slot < j.jornada_total.start_slot || intervalo.end_slot > j.jornada_total.end_slot) {
                conflitos.push_back("O horário ultrapassa a jornada diária da turma " + t->nome() + " (" + j.jornada_total.toString() + ").");
            }
        }

        // Choque com outras aulas da mesma turma
        for (const auto& a : data_.grade().alocacoes()) {
            if (a.aula_id != cur_aid && a.dia == dia && semestres_conflitam(a) && time_overlaps(a.intervalo, intervalo)) {
                if (std::find(a.turmas.begin(), a.turmas.end(), t_id) != a.turmas.end()) {
                    conflitos.push_back("Choque de turma: A Turma " + (t ? t->nome() : t_id) + " já possui a aula '" +
                                        a.disciplina_nome + "' (" + a.aula_id + ") neste horário.");
                }
            }
        }

        // Limite diário de aulas da disciplina na turma
        if (data_.config().distribuir_disciplinas_na_semana) {
            int count_dia = 0;
            for (const auto& a : data_.grade().alocacoes()) {
                if (a.aula_id != cur_aid && a.dia == dia && a.disciplina_id == disc->id() && semestres_conflitam(a)) {
                    if (std::find(a.turmas.begin(), a.turmas.end(), t_id) != a.turmas.end()) {
                        count_dia++;
                    }
                }
            }
            int max_lim = (disc->maxAulasPorDiaTurma() > 0) ? disc->maxAulasPorDiaTurma() : data_.config().max_aulas_disciplina_dia_turma;
            if (count_dia + 1 > max_lim) {
                conflitos.push_back("Limite diário: Excederá o limite de " + std::to_string(max_lim) +
                                    " aulas diárias desta disciplina para a Turma " + (t ? t->nome() : t_id) + ".");
            }
        }
    }

    // 2. Validar professores
    for (const auto& p_id : disc->professores()) {
        const auto* p = data_.findProfessor(p_id);
        if (p) {
            if (!p->isDisponivelNoIntervalo(dia, intervalo.start_slot, intervalo.end_slot)) {
                conflitos.push_back("Indisponibilidade docente: O Prof. " + p->nome() + " possui indisponibilidade cadastrada neste horário.");
            }
        }
        for (const auto& a : data_.grade().alocacoes()) {
            if (a.aula_id != cur_aid && a.dia == dia && semestres_conflitam(a) && time_overlaps(a.intervalo, intervalo)) {
                if (std::find(a.professores.begin(), a.professores.end(), p_id) != a.professores.end()) {
                    conflitos.push_back("Choque de professor: O Prof. " + (p ? p->nome() : p_id) + " já ministra a aula '" +
                                        a.disciplina_nome + "' (" + a.aula_id + ") neste horário.");
                }
            }
        }
    }

    // 3. Validar sala
    if (!sel_sala.empty()) {
        const auto* s = data_.findSala(sel_sala);
        for (const auto& a : data_.grade().alocacoes()) {
            if (a.aula_id != cur_aid && a.dia == dia && semestres_conflitam(a) && time_overlaps(a.intervalo, intervalo)) {
                if (std::find(a.salas.begin(), a.salas.end(), sel_sala) != a.salas.end()) {
                    conflitos.push_back("Choque de sala: A Sala " + (s ? s->nome() : sel_sala) + " já está ocupada pela aula '" +
                                        a.disciplina_nome + "' (" + a.aula_id + ") neste horário.");
                }
            }
        }
    }

    return conflitos;
}

void AddLessonToScheduleDialog::updateValidationStatus() {
    auto conflitos = verificarConflitos();

    if (conflitos.empty()) {
        lbl_conflito_status_->setText("✅ <b>Sem conflitos:</b> Horário, turmas, professores e sala livres para alocação.");
        lbl_conflito_status_->setStyleSheet(
            "background-color: rgba(34, 197, 94, 0.15); color: #15803d; border: 1px solid rgba(34, 197, 94, 0.4); "
            "border-radius: 6px; padding: 8px 12px; font-size: 11px;");
    } else {
        QStringList itens;
        for (const auto& c : conflitos) {
            itens.append("• " + QString::fromStdString(c));
        }
        lbl_conflito_status_->setText(QString("⚠️ <b>Atenção (%1 conflito%2 detectado%2):</b><br>%3")
            .arg(conflitos.size())
            .arg(conflitos.size() > 1 ? "s" : "")
            .arg(itens.join("<br>")));
        lbl_conflito_status_->setStyleSheet(
            "background-color: rgba(239, 68, 68, 0.15); color: #dc2626; border: 1px solid rgba(239, 68, 68, 0.4); "
            "border-radius: 6px; padding: 8px 12px; font-size: 11px;");
    }
}

void AddLessonToScheduleDialog::onAcceptClicked() {
    if (combo_disciplina_->currentIndex() < 0) {
        QMessageBox::warning(this, "Aviso", "Selecione uma disciplina para alocar.");
        return;
    }

    std::string disc_id = combo_disciplina_->currentData().toString().toStdString();
    const auto* disc = data_.findDisciplina(disc_id);
    if (!disc) return;

    std::string aid = aulaId();
    if (aid.empty()) {
        QMessageBox::warning(this, "Aviso", "Selecione a ocorrência da aula a ser alocada.");
        return;
    }

    auto conflitos = verificarConflitos();
    if (!conflitos.empty()) {
        QString msg = QString("Foram detectados %1 conflito(s) de horário com esta alocação:\n\n").arg(conflitos.size());
        for (size_t i = 0; i < std::min<size_t>(conflitos.size(), 6); ++i) {
            msg += QString("• %1\n").arg(QString::fromStdString(conflitos[i]));
        }
        if (conflitos.size() > 6) {
            msg += QString("... e mais %1 outro(s).\n").arg(conflitos.size() - 6);
        }
        msg += "\nDeseja alocar mesmo assim com os conflitos informados?";

        auto resp = QMessageBox::question(this, "Confirmar Alocação com Conflitos", msg,
                                          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (resp != QMessageBox::Yes) {
            return;
        }
    }

    auto dia = static_cast<model::DayOfWeek>(combo_dia_->currentData().toInt());
    QPoint p = combo_horario_->currentData().toPoint();
    model::TimeInterval inter(p.x(), p.y());

    std::string sel_sala = combo_sala_->currentIndex() >= 0
        ? combo_sala_->currentData().toString().toStdString() : "";

    int sem_alocado = combo_semestre_->currentData().toInt();

    result_allocation_.aula_id = aid;
    result_allocation_.disciplina_id = disc->id();
    result_allocation_.disciplina_nome = disc->nome();
    result_allocation_.tipo = disc->tipo();
    result_allocation_.dia = dia;
    result_allocation_.intervalo = inter;
    result_allocation_.professores = disc->professores();
    result_allocation_.turmas = disc->turmas();
    result_allocation_.salas = sel_sala.empty() ? std::vector<std::string>{} : std::vector<std::string>{ sel_sala };
    result_allocation_.is_pinned = chk_fixar_->isChecked();
    result_allocation_.regime = disc->regime();
    result_allocation_.semestre = disc->semestre();
    result_allocation_.semestre_alocado = sem_alocado;

    is_pinned_ = chk_fixar_->isChecked();

    accept();
}

} // namespace scheduler::ui
