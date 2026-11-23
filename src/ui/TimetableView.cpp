/**
 * @file TimetableView.cpp
 * @brief Implementação da visualização interativa e gestão visual da grade semanal.
 */

#include "ui/TimetableView.h"
#include "ui/SmartTableWidgetItem.h"
#include "ui/ModernTheme.h"
#include "ui/MoveLessonDialog.h"
#include "ui/SwapLessonDialog.h"
#include "ui/ChangeRoomDialog.h"
#include "ui/AddLessonToScheduleDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <set>

namespace scheduler::ui {

/**
 * @brief Construtor da visão de grade horária.
 * @param data Referência para a base de dados da instituição.
 * @param parent Widget pai na hierarquia Qt.
 */
TimetableView::TimetableView(model::InstitutionData& data, QWidget* parent)
    : QWidget(parent), data_(data) {
    setupUi();
}

/**
 * @brief Constrói a interface com barra de filtros, seletores de modo e grade interativa.
 */
void TimetableView::setupUi() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(12);

    // Barra superior de seleção e filtros
    auto* top_panel = new QHBoxLayout();
    top_panel->setSpacing(8);

    auto* lbl_mode = new QLabel("Modo:", this);
    lbl_mode->setStyleSheet("font-weight: bold; color: palette(highlight);");
    top_panel->addWidget(lbl_mode);

    combo_view_type_ = new QComboBox(this);
    combo_view_type_->addItem(ModernTheme::icon("turma"), "Por Turma", 0);
    combo_view_type_->addItem(ModernTheme::icon("professor"), "Por Professor", 1);
    combo_view_type_->addItem(ModernTheme::icon("room"), "Por Sala", 2);
    combo_view_type_->addItem(ModernTheme::icon("calendar"), "Todas as Disciplinas e Turmas", 3);
    combo_view_type_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    combo_view_type_->setMinimumWidth(180);
    top_panel->addWidget(combo_view_type_);

    top_panel->addSpacing(8);

    lbl_entity_ = new QLabel("Selecionar Alvo:", this);
    lbl_entity_->setStyleSheet("font-weight: bold; color: palette(highlight);");
    top_panel->addWidget(lbl_entity_);

    combo_target_ = new QComboBox(this);
    combo_target_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    combo_target_->setMinimumWidth(180);
    combo_target_->setMaximumWidth(320);
    top_panel->addWidget(combo_target_);

    top_panel->addSpacing(8);

    auto* lbl_semestre = new QLabel("Semestre:", this);
    lbl_semestre->setStyleSheet("font-weight: bold; color: palette(highlight);");
    top_panel->addWidget(lbl_semestre);

    combo_semestre_filtro_ = new QComboBox(this);
    combo_semestre_filtro_->addItem("Todos os Semestres (Geral)", 0);
    combo_semestre_filtro_->addItem("1º Semestre", 1);
    combo_semestre_filtro_->addItem("2º Semestre", 2);
    combo_semestre_filtro_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    combo_semestre_filtro_->setMinimumWidth(160);
    top_panel->addWidget(combo_semestre_filtro_);

    top_panel->addStretch();

    btn_toggle_orientation_ = new QPushButton(ModernTheme::icon("calendar"), "Grade: Horário × Dia", this);
    btn_toggle_orientation_->setObjectName("btnSecondary");
    btn_toggle_orientation_->setCursor(Qt::PointingHandCursor);
    btn_toggle_orientation_->setToolTip("Alterna a visualização da grade semanal entre Horário por Dia (linhas = horários, colunas = dias) e Dia por Horário (linhas = dias, colunas = horários)");
    top_panel->addWidget(btn_toggle_orientation_);

    btn_swap_lesson_ = new QPushButton(ModernTheme::icon("swap"), "Trocar Aulas (Swap)...", this);
    btn_swap_lesson_->setObjectName("btnSecondary");
    btn_swap_lesson_->setCursor(Qt::PointingHandCursor);
    btn_swap_lesson_->setToolTip("Permuta os horários entre a aula selecionada e outra aula da grade semanal");
    top_panel->addWidget(btn_swap_lesson_);

    btn_add_lesson_ = new QPushButton(ModernTheme::icon("add"), "Alocar Disciplina...", this);
    btn_add_lesson_->setObjectName("btnPrimary");
    btn_add_lesson_->setCursor(Qt::PointingHandCursor);
    btn_add_lesson_->setToolTip("Adiciona uma ocorrência de disciplina manualmente na grade semanal");
    top_panel->addWidget(btn_add_lesson_);

    btn_clear_timetable_ = new QPushButton(ModernTheme::icon("delete"), "Limpar Grade...", this);
    btn_clear_timetable_->setObjectName("btnSecondary");
    btn_clear_timetable_->setCursor(Qt::PointingHandCursor);
    btn_clear_timetable_->setToolTip("Remove todas as alocações da grade semanal para iniciar uma nova configuração inicial");
    top_panel->addWidget(btn_clear_timetable_);

    main_layout->addLayout(top_panel);

    // Barra secundária de informações do alvo e busca rápida
    auto* sub_panel = new QHBoxLayout();
    sub_panel->setSpacing(8);

    lbl_info_banner_ = new QLabel(this);
    lbl_info_banner_->setObjectName("lblInfoBanner");
    lbl_info_banner_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sub_panel->addWidget(lbl_info_banner_, 1);

    btn_clear_selection_ = new QPushButton("✕ Limpar seleção", this);
    btn_clear_selection_->setObjectName("btnSecondary");
    btn_clear_selection_->setCursor(Qt::PointingHandCursor);
    btn_clear_selection_->setToolTip("Remove a seleção atual e oculta os indicadores de disponibilidade nos horários (Esc)");
    btn_clear_selection_->setVisible(false);
    sub_panel->addWidget(btn_clear_selection_);

    btn_conflict_summary_ = new QPushButton("✅ Sem choques", this);
    btn_conflict_summary_->setCursor(Qt::PointingHandCursor);
    btn_conflict_summary_->setToolTip("Clique para listar os choques de turma, professor, sala ou indisponibilidades na grade");
    sub_panel->addWidget(btn_conflict_summary_);

    sub_panel->addSpacing(8);

    search_lesson_ = new QLineEdit(this);
    search_lesson_->addAction(ModernTheme::icon("search"), QLineEdit::LeadingPosition);
    search_lesson_->setPlaceholderText("Destacar aula/prof/sala...");
    search_lesson_->setClearButtonEnabled(true);
    search_lesson_->setFixedWidth(220);
    search_lesson_->setToolTip("Digite para localizar e destacar aulas na grade semanal");
    sub_panel->addWidget(search_lesson_);

    lbl_search_count_ = new QLabel(this);
    lbl_search_count_->setObjectName("lblSearchCount");
    lbl_search_count_->setVisible(false);
    sub_panel->addWidget(lbl_search_count_);

    updateBannerTheme();

    main_layout->addLayout(sub_panel);

    // Tabela da Grade Semanal com Drag & Drop
    table_ = new TimetableGridWidget(this);
    table_->setColumnCount(5);
    QStringList headers;
    for (auto d : model::allDays()) {
        headers << QString::fromStdString(model::dayToLongString(d));
    }
    table_->setHorizontalHeaderLabels(headers);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);

    main_layout->addWidget(table_);

    connect(combo_view_type_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TimetableView::onViewTypeChanged);
    connect(combo_target_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TimetableView::onTargetEntityChanged);
    connect(combo_semestre_filtro_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TimetableView::onSemestreFiltroChanged);
    connect(table_, &QTableWidget::customContextMenuRequested,
            this, &TimetableView::onCellContextMenu);
    connect(table_, &QTableWidget::cellDoubleClicked,
            this, &TimetableView::onCellDoubleClicked);
    connect(table_, &TimetableGridWidget::lessonDropRequested,
            this, &TimetableView::onLessonDropRequested);
    connect(table_, &TimetableGridWidget::lessonSelected,
            this, &TimetableView::onLessonSelected);
    connect(table_, &TimetableGridWidget::emptyCellClicked,
            this, &TimetableView::onEmptyCellClicked);
    connect(btn_clear_selection_, &QPushButton::clicked,
            this, &TimetableView::onEmptyCellClicked);
    connect(btn_conflict_summary_, &QPushButton::clicked,
            this, &TimetableView::onShowConflictsMenu);
    connect(new QShortcut(QKeySequence(Qt::Key_Escape), this), &QShortcut::activated,
            this, &TimetableView::onEmptyCellClicked);
    connect(table_, &QTableWidget::cellClicked, this, [this](int row, int col) {
        auto* it = table_->item(row, col);
        if (it) {
            QStringList all_ids = it->data(Qt::UserRole + 3).toStringList();
            if (all_ids.size() > 1) {
                QPoint mouse_pos = table_->viewport()->mapFromGlobal(QCursor::pos());
                QRect cell_rect = table_->visualItemRect(it);
                int rel_y = mouse_pos.y() - cell_rect.top();
                int idx = std::clamp((rel_y * static_cast<int>(all_ids.size())) / std::max(1, cell_rect.height()), 0, static_cast<int>(all_ids.size()) - 1);
                onLessonSelected(all_ids[idx]);
            } else {
                QString aid = it->data(Qt::UserRole).toString();
                if (!aid.isEmpty()) {
                    onLessonSelected(aid);
                }
            }
        }
    });
    connect(search_lesson_, &QLineEdit::textChanged,
            this, &TimetableView::onSearchLessonChanged);
    connect(btn_toggle_orientation_, &QPushButton::clicked,
            this, &TimetableView::onToggleGridOrientation);
    connect(btn_swap_lesson_, &QPushButton::clicked, this, [this]() {
        trocarAulaManual(selected_aula_id_);
    });
    connect(btn_add_lesson_, &QPushButton::clicked, this, [this]() {
        alocarAulaManual(-1, -1);
    });
    connect(btn_clear_timetable_, &QPushButton::clicked, this, &TimetableView::limparGradeCompleta);

    populateTargetCombo();
}

/**
 * @brief Preenche o dropdown de seleção com as turmas, professores ou salas conforme o modo ativo.
 */
void TimetableView::populateTargetCombo() {
    combo_target_->blockSignals(true);
    combo_target_->clear();

    int mode = combo_view_type_->currentData().toInt();
    if (lbl_entity_) {
        lbl_entity_->setText(mode == 3 ? "Filtro de Turma:" : "Selecionar Alvo:");
    }

    if (mode == 0) { // Por Turma
        for (const auto& t : data_.turmas()) {
            QString label = QString::fromStdString(t.nome());
            combo_target_->addItem(label, QString::fromStdString(t.id()));
        }
    } else if (mode == 1) { // Por Professor
        for (const auto& p : data_.professores()) {
            QString label = QString::fromStdString(p.nome());
            combo_target_->addItem(label, QString::fromStdString(p.id()));
        }
    } else if (mode == 2) { // Por Sala
        for (const auto& s : data_.salas()) {
            QString label = QString::fromStdString(s.nome());
            combo_target_->addItem(label, QString::fromStdString(s.id()));
        }
    } else if (mode == 3) { // Todas as Disciplinas e Turmas
        combo_target_->addItem(ModernTheme::icon("calendar"), "Todas as Turmas (Visão Geral)", "ALL");
        for (const auto& t : data_.turmas()) {
            combo_target_->addItem(QString("Filtrar: %1").arg(QString::fromStdString(t.nome())), QString::fromStdString(t.id()));
        }
    }

    combo_target_->blockSignals(false);
    renderTimetable();
}

/**
 * @brief Atualiza o estilo visual e o fundo do banner informativo de acordo com o tema ativo.
 */
void TimetableView::updateBannerTheme() {
    if (!lbl_info_banner_) {
        return;
    }
    if (ModernTheme::isDarkTheme()) {
        lbl_info_banner_->setStyleSheet(
            "QLabel#lblInfoBanner { background-color: #1e293b; color: #f8fafc; border: 1px solid #334155; "
            "border-radius: 6px; padding: 4px 10px; font-size: 12px; font-weight: 500; }");
        if (lbl_search_count_ && !lbl_search_count_->isVisible()) {
            lbl_search_count_->setStyleSheet(
                "QLabel#lblSearchCount { background-color: #1e293b; color: #f8fafc; border: 1px solid #334155; "
                "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: 500; }");
        }
    } else {
        lbl_info_banner_->setStyleSheet(
            "QLabel#lblInfoBanner { background-color: palette(base); color: palette(window-text); border: 1px solid palette(mid); "
            "border-radius: 6px; padding: 4px 10px; font-size: 12px; font-weight: 500; }");
        if (lbl_search_count_ && !lbl_search_count_->isVisible()) {
            lbl_search_count_->setStyleSheet(
                "QLabel#lblSearchCount { background-color: palette(base); color: palette(window-text); border: 1px solid palette(mid); "
                "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: 500; }");
        }
    }
}

/**
 * @brief Atualiza os alvos e redesenha a grade na tela.
 */
void TimetableView::refreshView() {
    updateBannerTheme();
    populateTargetCombo();
}

/**
 * @brief Slot acionado na mudança do tipo de visão (Turma, Professor ou Sala).
 * @param index Novo índice do combo.
 */
void TimetableView::onViewTypeChanged(int /*index*/) {
    populateTargetCombo();
}

/**
 * @brief Slot acionado na escolha de outro item no dropdown de entidade alvo.
 * @param index Novo índice da entidade.
 */
void TimetableView::onTargetEntityChanged(int /*index*/) {
    renderTimetable();
}

/**
 * @brief Slot acionado ao alterar o filtro de semestres letivos (1º, 2º ou Ambos).
 * @param index Novo índice do semestre.
 */
void TimetableView::onSemestreFiltroChanged(int /*index*/) {
    renderTimetable();
}

/**
 * @brief Alterna a orientação da grade semanal entre Horário por Dia e Dia por Horário.
 */
void TimetableView::onToggleGridOrientation() {
    transpose_grid_ = !transpose_grid_;
    if (btn_toggle_orientation_) {
        btn_toggle_orientation_->setText(transpose_grid_ ? "Grade: Dia × Horário" : "Grade: Horário × Dia");
    }
    renderTimetable();
}

/**
 * @brief Renderiza visualmente as células da grade com os horários, intervalos e aulas alocadas.
 */
void TimetableView::renderTimetable() {
    table_->clearSpans();
    table_->setRowCount(0);

    int mode = combo_view_type_->currentData().toInt();
    QString target_id = combo_target_->currentData().toString();
    if (target_id.isEmpty() && mode != 3) {
        lbl_info_banner_->setText("Nenhum item selecionado.");
        return;
    }

    std::string id_str = target_id.toStdString();

    int sem_filtro = combo_semestre_filtro_ ? combo_semestre_filtro_->currentData().toInt() : 0;
    auto match_semestre = [sem_filtro](const model::AllocatedLesson& a) {
        if (sem_filtro <= 0) return true;
        if (a.regime == model::RegimeDisciplina::ANUAL) return true;
        if (sem_filtro == 1) {
            return a.semestre == model::SemestreLetivo::SEMESTRE_1 || a.semestre_alocado == 1;
        } else if (sem_filtro == 2) {
            return a.semestre == model::SemestreLetivo::SEMESTRE_2 || a.semestre_alocado == 2;
        }
        return true;
    };

    // Coletar todos os horários únicos ocupados
    std::set<std::pair<int, int>> horarios_set;

    if (mode == 0) { // Turma
        const auto* turma = data_.findTurma(id_str);
        if (turma) {
            std::string sala_info = turma->temSalaTeoricaPredefinida()
                ? turma->salaTeoricaId()
                : data_.grade().salaTeoricaTurma(turma->id());
            QString sem_suffix = (sem_filtro == 1) ? " [1º Sem]" : (sem_filtro == 2 ? " [2º Sem]" : "");
            lbl_info_banner_->setText(QString("Turno: %1 | Sala Teórica Fixa: %2%3")
                .arg(QString::fromStdString(model::shiftToString(turma->turno())))
                .arg(QString::fromStdString(sala_info.empty() ? "(não definida)" : sala_info))
                .arg(sem_suffix));

            // Adicionar slots letivos e intervalos da turma para todos os dias letivos
            for (auto d : turma->diasLetivos()) {
                auto j_dia = turma->jornadaParaDia(d);
                for (const auto& s : j_dia.slots_letivos) {
                    horarios_set.insert({s.start_min, s.end_min});
                }
                for (const auto& inter : j_dia.intervalosAtivos()) {
                    horarios_set.insert({inter.start_min, inter.end_min});
                }
            }
        }
        for (const auto& a : data_.grade().alocacoes()) {
            if (match_semestre(a) && std::find(a.turmas.begin(), a.turmas.end(), id_str) != a.turmas.end()) {
                horarios_set.insert({a.intervalo.start_min, a.intervalo.end_min});
            }
        }
    } else if (mode == 1) { // Professor
        const auto* prof = data_.findProfessor(id_str);
        if (prof) {
            QString pref_str = "Pesos semanais: ";
            for (auto d : model::allDays()) {
                pref_str += QString("%1=%2 ").arg(QString::fromStdString(model::dayToString(d))).arg(prof->pesoDia(d));
            }
            if (sem_filtro == 1) pref_str += " [1º Sem]";
            else if (sem_filtro == 2) pref_str += " [2º Sem]";
            lbl_info_banner_->setText(pref_str);
        }
        for (const auto& a : data_.grade().alocacoes()) {
            if (match_semestre(a) && std::find(a.professores.begin(), a.professores.end(), id_str) != a.professores.end()) {
                horarios_set.insert({a.intervalo.start_min, a.intervalo.end_min});
            }
        }
    } else if (mode == 2) { // Sala
        const auto* sala = data_.findSala(id_str);
        if (sala) {
            QString info = QString("Tipo: %1 | Capacidade: %2 alunos")
                .arg(QString::fromStdString(sala->tipo()))
                .arg(sala->capacidade());
            if (sem_filtro == 1) info += " [1º Sem]";
            else if (sem_filtro == 2) info += " [2º Sem]";
            lbl_info_banner_->setText(info);
        }
        for (const auto& a : data_.grade().alocacoes()) {
            if (match_semestre(a) && std::find(a.salas.begin(), a.salas.end(), id_str) != a.salas.end()) {
                horarios_set.insert({a.intervalo.start_min, a.intervalo.end_min});
            }
        }
    } else if (mode == 3) { // Todas as Disciplinas e Turmas
        int total_aloc = 0;
        std::set<std::string> turmas_ativas;
        std::set<std::string> disciplinas_ativas;

        bool filtrar_turma = (!target_id.isEmpty() && target_id != "ALL");
        for (const auto& a : data_.grade().alocacoes()) {
            if (match_semestre(a)) {
                if (!filtrar_turma || std::find(a.turmas.begin(), a.turmas.end(), id_str) != a.turmas.end()) {
                    horarios_set.insert({a.intervalo.start_min, a.intervalo.end_min});
                    total_aloc++;
                    for (const auto& tid : a.turmas) turmas_ativas.insert(tid);
                    disciplinas_ativas.insert(a.disciplina_id);
                }
            }
        }

        // Também incluir os slots letivos e intervalos cadastrados das turmas
        for (const auto& t : data_.turmas()) {
            if (filtrar_turma && t.id() != id_str) continue;
            for (auto d : t.diasLetivos()) {
                auto j_dia = t.jornadaParaDia(d);
                for (const auto& s : j_dia.slots_letivos) {
                    horarios_set.insert({s.start_min, s.end_min});
                }
                for (const auto& inter : j_dia.intervalosAtivos()) {
                    horarios_set.insert({inter.start_min, inter.end_min});
                }
            }
        }

        QString sem_suffix = (sem_filtro == 1) ? " [1º Sem]" : (sem_filtro == 2 ? " [2º Sem]" : "");
        if (filtrar_turma) {
            const auto* t = data_.findTurma(id_str);
            lbl_info_banner_->setText(QString("Visão Filtrada: Turma %1 | %2 aulas alocadas | %3 disciplinas distintas%4")
                .arg(QString::fromStdString(t ? t->nome() : id_str))
                .arg(total_aloc)
                .arg(disciplinas_ativas.size())
                .arg(sem_suffix));
        } else {
            lbl_info_banner_->setText(QString("Visão Geral: %1 aulas alocadas | %2 turmas atendidas | %3 disciplinas distintas%4")
                .arg(total_aloc)
                .arg(turmas_ativas.size())
                .arg(disciplinas_ativas.size())
                .arg(sem_suffix));
        }
    }

    if (!selected_aula_id_.isEmpty() && !data_.grade().findAula(selected_aula_id_.toStdString())) {
        selected_aula_id_.clear();
    }
    if (btn_clear_selection_) {
        btn_clear_selection_->setVisible(!selected_aula_id_.isEmpty());
    }

    auto conflitos_map = data_.verificarConflitosGradeDetalhado();
    updateConflictSummaryPill(conflitos_map);

    const model::AllocatedLesson* sel_aloc = selected_aula_id_.isEmpty()
        ? nullptr
        : data_.grade().findAula(selected_aula_id_.toStdString());

    if (sel_aloc) {
        QString t_name;
        if (!sel_aloc->turmas.empty()) {
            const auto* t = data_.findTurma(sel_aloc->turmas.front());
            t_name = QString::fromStdString(t ? t->nome() : sel_aloc->turmas.front());
        }
        QString p_name;
        if (!sel_aloc->professores.empty()) {
            const auto* p = data_.findProfessor(sel_aloc->professores.front());
            p_name = QString::fromStdString(p ? p->nome() : sel_aloc->professores.front());
        }
        QString sel_tag = QString(" | 🎯 <b>Aula Selecionada:</b> [%1] %2 — Prof. %3 (%4 às %5)")
            .arg(t_name.isEmpty() ? "Turma" : t_name)
            .arg(QString::fromStdString(sel_aloc->disciplina_nome))
            .arg(p_name.isEmpty() ? "Sem Prof" : p_name)
            .arg(QString::fromStdString(sel_aloc->diaString()))
            .arg(QString::fromStdString(sel_aloc->horarioFormatado()));
        lbl_info_banner_->setText(lbl_info_banner_->text() + sel_tag);
    }

    QStringList day_headers;
    for (auto d : model::allDays()) {
        day_headers << QString::fromStdString(model::dayToLongString(d));
    }

    row_intervals_.clear();
    if (horarios_set.empty()) {
        if (!transpose_grid_) {
            table_->setColumnCount(5);
            table_->setHorizontalHeaderLabels(day_headers);
            table_->setRowCount(1);
            table_->setVerticalHeaderLabels({""});
            auto* empty_item = new QTableWidgetItem("Nenhuma aula gerada ou alocada para este registro neste semestre.");
            empty_item->setTextAlignment(Qt::AlignCenter);
            empty_item->setForeground(QColor("#94a3b8"));
            table_->setItem(0, 0, empty_item);
            table_->setSpan(0, 0, 1, 5);
        } else {
            table_->setRowCount(5);
            table_->setVerticalHeaderLabels(day_headers);
            table_->setColumnCount(1);
            table_->setHorizontalHeaderLabels({"Horários"});
            auto* empty_item = new QTableWidgetItem("Nenhuma aula gerada ou alocada para este registro neste semestre.");
            empty_item->setTextAlignment(Qt::AlignCenter);
            empty_item->setForeground(QColor("#94a3b8"));
            table_->setItem(0, 0, empty_item);
            table_->setSpan(0, 0, 5, 1);
        }
        return;
    }

    QStringList time_headers;
    for (const auto& [start_s, end_s] : horarios_set) {
        row_intervals_.emplace_back(start_s, end_s);
        time_headers << QString::fromStdString(model::TimeInterval(start_s, end_s).toString());
    }

    if (!transpose_grid_) {
        table_->setColumnCount(5);
        table_->setHorizontalHeaderLabels(day_headers);
        table_->setRowCount(static_cast<int>(horarios_set.size()));
        table_->setVerticalHeaderLabels(time_headers);
    } else {
        table_->setRowCount(5);
        table_->setVerticalHeaderLabels(day_headers);
        table_->setColumnCount(static_cast<int>(horarios_set.size()));
        table_->setHorizontalHeaderLabels(time_headers);
    }

    int time_idx = 0;
    for (const auto& [start_s, end_s] : horarios_set) {
        for (int day_idx = 0; day_idx < 5; ++day_idx) {
            int cell_row = transpose_grid_ ? day_idx : time_idx;
            int cell_col = transpose_grid_ ? time_idx : day_idx;
            auto dia = static_cast<model::DayOfWeek>(day_idx);

            // Buscar se tem aula nesse dia e horário
            std::vector<const model::AllocatedLesson*> found_aulas;
            for (const auto& a : data_.grade().alocacoes()) {
                if (a.dia == dia && a.intervalo.start_min == start_s && a.intervalo.end_min == end_s && match_semestre(a)) {
                    if (mode == 0) {
                        if (std::find(a.turmas.begin(), a.turmas.end(), id_str) != a.turmas.end()) {
                            found_aulas.push_back(&a);
                        }
                    } else if (mode == 1) {
                        if (std::find(a.professores.begin(), a.professores.end(), id_str) != a.professores.end()) {
                            found_aulas.push_back(&a);
                        }
                    } else if (mode == 2) {
                        if (std::find(a.salas.begin(), a.salas.end(), id_str) != a.salas.end()) {
                            found_aulas.push_back(&a);
                        }
                    } else if (mode == 3) {
                        if (target_id.isEmpty() || target_id == "ALL") {
                            found_aulas.push_back(&a);
                        } else if (std::find(a.turmas.begin(), a.turmas.end(), id_str) != a.turmas.end()) {
                            found_aulas.push_back(&a);
                        }
                    }
                }
            }

            if (found_aulas.empty()) {
                bool is_recess_for_day = false;
                bool is_outside_day_jornada = false;
                bool is_day_off = false;

                if (mode == 0 || (mode == 3 && !target_id.isEmpty() && target_id != "ALL")) {
                    const auto* turma = data_.findTurma(id_str);
                    if (turma) {
                        if (!turma->funcionaNoDia(dia)) {
                            is_day_off = true;
                        } else {
                            auto j_dia = turma->jornadaParaDia(dia);
                            for (const auto& inter : j_dia.intervalosAtivos()) {
                                if (start_s == inter.start_min && end_s == inter.end_min) {
                                    is_recess_for_day = true;
                                    break;
                                }
                            }
                            if (!is_recess_for_day) {
                                if (!j_dia.contemSlotExato(model::TimeInterval(start_s, end_s))) {
                                    is_outside_day_jornada = true;
                                }
                            }
                        }
                    }
                } else if (mode == 3 && (target_id.isEmpty() || target_id == "ALL") && !data_.turmas().empty()) {
                    bool all_recess = true;
                    int working_turmas = 0;
                    for (const auto& t : data_.turmas()) {
                        if (!t.funcionaNoDia(dia)) continue;
                        working_turmas++;
                        auto j_dia = t.jornadaParaDia(dia);
                        bool t_recess = false;
                        for (const auto& inter : j_dia.intervalosAtivos()) {
                            if (start_s == inter.start_min && end_s == inter.end_min) {
                                t_recess = true;
                                break;
                            }
                        }
                        if (!t_recess) {
                            all_recess = false;
                            break;
                        }
                    }
                    if (working_turmas > 0 && all_recess) {
                        is_recess_for_day = true;
                    }
                }

                if (is_recess_for_day) {
                    int dur = end_s - start_s;
                    auto* item = new QTableWidgetItem(ModernTheme::icon("coffee"), QString("☕ INTERVALO (%1 min)").arg(dur));
                    item->setTextAlignment(Qt::AlignCenter);
                    item->setBackground(QColor(245, 158, 11, 40));
                    item->setForeground(QColor("#f59e0b"));
                    item->setFont(QFont("Segoe UI", 10, QFont::Bold));
                    table_->setItem(cell_row, cell_col, item);
                    continue;
                }

                if (is_day_off) {
                    auto* item = new QTableWidgetItem("—");
                    item->setToolTip("Dia não letivo para a turma");
                    item->setTextAlignment(Qt::AlignCenter);
                    item->setForeground(QColor("#475569"));
                    item->setBackground(QColor(15, 23, 42, 60));
                    table_->setItem(cell_row, cell_col, item);
                    continue;
                }

                if (is_outside_day_jornada) {
                    auto* item = new QTableWidgetItem("—");
                    item->setToolTip("Fora da jornada letiva da turma neste dia");
                    item->setTextAlignment(Qt::AlignCenter);
                    item->setForeground(QColor("#475569"));
                    item->setBackground(QColor(15, 23, 42, 60));
                    table_->setItem(cell_row, cell_col, item);
                    continue;
                }

                bool is_dark = ModernTheme::isDarkTheme();
                if (sel_aloc) {
                    model::TimeInterval cand_interval(start_s, start_s + sel_aloc->intervalo.durationMinutes());
                    std::vector<std::string> salas_override;
                    if (mode == 2 && !id_str.empty()) {
                        salas_override = { id_str };
                    }
                    auto avail_confs = data_.avaliarAulaNoHorario(
                        sel_aloc->aula_id, dia, cand_interval, {sel_aloc->aula_id}, salas_override);

                    if (avail_confs.empty()) {
                        QString free_label = (mode == 2) ? "🟢 Livre (nesta sala)" : "🟢 Livre";
                        auto* item = new QTableWidgetItem(free_label);
                        item->setTextAlignment(Qt::AlignCenter);
                        item->setBackground(is_dark ? QColor(22, 163, 74, 45) : QColor(220, 252, 231, 210));
                        item->setForeground(is_dark ? QColor("#4ade80") : QColor("#166534"));
                        QFont font = item->font();
                        font.setBold(true);
                        item->setFont(font);
                        item->setToolTip(QString("<b>🟢 Horário Livre para [%1]</b><br>"
                                                 "Professor, turma e sala disponíveis neste horário.<br>"
                                                 "<i>Arraste a aula selecionada para cá ou clique para limpar a seleção.</i>")
                                             .arg(QString::fromStdString(sel_aloc->disciplina_nome)));
                        item->setData(Qt::UserRole + 1, item->background());
                        item->setData(Qt::UserRole + 2, item->foreground());
                        table_->setItem(cell_row, cell_col, item);
                        continue;
                    } else {
                        QStringList short_lines;
                        QStringList detail_lines;
                        bool has_hard_clash = false;
                        for (const auto& c : avail_confs) {
                            QString icon_prefix = "🟠 ";
                            if (c.type == model::SlotConflictType::INDISPONIBILIDADE_PROFESSOR ||
                                c.type == model::SlotConflictType::TURMA_INATIVA ||
                                c.type == model::SlotConflictType::INTERVALO_RECREIO ||
                                c.type == model::SlotConflictType::FORA_JORNADA) {
                                icon_prefix = "🚫 ";
                                has_hard_clash = true;
                            } else if (c.type == model::SlotConflictType::CHOQUE_PROFESSOR ||
                                       c.type == model::SlotConflictType::CHOQUE_SALA ||
                                       c.type == model::SlotConflictType::CHOQUE_TURMA ||
                                       c.type == model::SlotConflictType::INCOMPATIBILIDADE_SALA) {
                                icon_prefix = "🔴 ";
                                has_hard_clash = true;
                            }
                            QString tag = icon_prefix + QString::fromStdString(c.short_label);
                            if (!short_lines.contains(tag)) {
                                short_lines << tag;
                            }
                            QString det = QString::fromStdString(c.detail);
                            if (!detail_lines.contains(det)) {
                                detail_lines << det;
                            }
                        }

                        QString display_text;
                        if (short_lines.size() > 2) {
                            display_text = short_lines.mid(0, 2).join("\n") +
                                           QString("\n+%1 outro(s)").arg(short_lines.size() - 2);
                        } else {
                            display_text = short_lines.join("\n");
                        }

                        auto* item = new QTableWidgetItem(display_text);
                        item->setTextAlignment(Qt::AlignCenter);
                        if (has_hard_clash) {
                            item->setBackground(is_dark ? QColor(234, 88, 12, 55) : QColor(254, 215, 170, 215));
                            item->setForeground(is_dark ? QColor("#fdba74") : QColor("#9a3412"));
                        } else {
                            item->setBackground(is_dark ? QColor(217, 119, 6, 40) : QColor(254, 243, 199, 200));
                            item->setForeground(is_dark ? QColor("#fcd34d") : QColor("#92400e"));
                        }
                        item->setToolTip(QString("<b>⚠️ Ocupação / Restrições para [%1]:</b><br>• %2")
                                             .arg(QString::fromStdString(sel_aloc->disciplina_nome),
                                                  detail_lines.join("<br>• ")));
                        item->setData(Qt::UserRole + 1, item->background());
                        item->setData(Qt::UserRole + 2, item->foreground());
                        table_->setItem(cell_row, cell_col, item);
                        continue;
                    }
                }

                if (mode == 1) {
                    const auto* prof = data_.findProfessor(id_str);
                    if (prof && !prof->isDisponivelNoIntervalo(dia, start_s, end_s)) {
                        auto* item = new QTableWidgetItem("🚫 Indisponível");
                        item->setTextAlignment(Qt::AlignCenter);
                        item->setBackground(is_dark ? QColor(100, 116, 139, 35) : QColor(226, 232, 240, 180));
                        item->setForeground(is_dark ? QColor("#94a3b8") : QColor("#64748b"));
                        item->setToolTip(QString("Prof. %1 possui indisponibilidade cadastrada neste horário.")
                                             .arg(QString::fromStdString(prof->nome())));
                        item->setData(Qt::UserRole + 1, item->background());
                        item->setData(Qt::UserRole + 2, item->foreground());
                        table_->setItem(cell_row, cell_col, item);
                        continue;
                    }
                }

                auto* item = new QTableWidgetItem("-");
                item->setTextAlignment(Qt::AlignCenter);
                item->setForeground(is_dark ? QColor("#64748b") : QColor("#94a3b8"));
                item->setData(Qt::UserRole + 1, item->background());
                item->setData(Qt::UserRole + 2, item->foreground());
                table_->setItem(cell_row, cell_col, item);
                continue;
            }

            if (mode == 3) {
                std::sort(found_aulas.begin(), found_aulas.end(), [this](const model::AllocatedLesson* l1, const model::AllocatedLesson* l2) {
                    std::string t1 = l1->turmas.empty() ? "" : l1->turmas.front();
                    std::string t2 = l2->turmas.empty() ? "" : l2->turmas.front();
                    const auto* turma1 = data_.findTurma(t1);
                    const auto* turma2 = data_.findTurma(t2);
                    std::string n1 = turma1 ? turma1->nome() : t1;
                    std::string n2 = turma2 ? turma2->nome() : t2;
                    if (n1 != n2) return n1 < n2;
                    return l1->disciplina_nome < l2->disciplina_nome;
                });
            }

            QStringList aula_blocks;
            QString main_aula_id;
            bool has_lab = false;
            bool has_special = false;
            bool cell_has_conflict = false;
            QStringList cell_conflict_tooltips;

            for (const auto* aula : found_aulas) {
                if (main_aula_id.isEmpty()) {
                    main_aula_id = QString::fromStdString(aula->aula_id);
                }
                if (aula->tipo == "LABORATORIO") has_lab = true;
                if (aula->tipo == "ESPECIAL") has_special = true;

                bool aula_has_conflict = false;
                QString aula_conflict_inline;
                auto it_conf = conflitos_map.find(aula->aula_id);
                if (it_conf != conflitos_map.end() && !it_conf->second.empty()) {
                    cell_has_conflict = true;
                    aula_has_conflict = true;
                    for (const auto& c_info : it_conf->second) {
                        cell_conflict_tooltips << QString("• [%1] %2")
                            .arg(QString::fromStdString(aula->disciplina_nome),
                                 QString::fromStdString(c_info.detail));
                    }
                    aula_conflict_inline = "⚠️ " + formatShortConflictTags(it_conf->second, 2);
                }

                QString reg_tag;
                if (aula->regime == model::RegimeDisciplina::ANUAL) {
                    reg_tag = "[Anual]";
                } else if (aula->semestre == model::SemestreLetivo::SEMESTRE_1 || aula->semestre_alocado == 1) {
                    reg_tag = "[1º Sem]";
                } else {
                    reg_tag = "[2º Sem]";
                }

                QString title = QString("%1 %2").arg(reg_tag, QString::fromStdString(aula->disciplina_nome));
                bool is_this_selected = (!selected_aula_id_.isEmpty() && aula->aula_id == selected_aula_id_.toStdString());
                if (is_this_selected) {
                    title = "👉 [SELECIONADA] " + title;
                }
                if (aula_has_conflict) {
                    title = "⚠️ " + title;
                }
                if (aula->is_pinned) {
                    title = "📌 " + title;
                }

                QString line2, line3;
                if (mode == 0) { // Turma view -> show professor & room
                    QString prof_names;
                    for (size_t i = 0; i < aula->professores.size(); ++i) {
                        if (i > 0) prof_names += ", ";
                        const auto* p = data_.findProfessor(aula->professores[i]);
                        prof_names += QString::fromStdString(p ? p->nome() : aula->professores[i]);
                    }
                    QString sala_names;
                    for (size_t i = 0; i < aula->salas.size(); ++i) {
                        if (i > 0) sala_names += ", ";
                        const auto* s = data_.findSala(aula->salas[i]);
                        sala_names += QString::fromStdString(s ? s->nome() : aula->salas[i]);
                    }
                    line2 = prof_names;
                    line3 = "Sala: " + sala_names;
                } else if (mode == 1) { // Professor view -> show turma & room
                    QString turma_names;
                    for (size_t i = 0; i < aula->turmas.size(); ++i) {
                        if (i > 0) turma_names += ", ";
                        const auto* t = data_.findTurma(aula->turmas[i]);
                        turma_names += QString::fromStdString(t ? t->nome() : aula->turmas[i]);
                    }
                    QString sala_names;
                    for (size_t i = 0; i < aula->salas.size(); ++i) {
                        if (i > 0) sala_names += ", ";
                        const auto* s = data_.findSala(aula->salas[i]);
                        sala_names += QString::fromStdString(s ? s->nome() : aula->salas[i]);
                    }
                    line2 = "Turma: " + turma_names;
                    line3 = "Sala: " + sala_names;
                } else if (mode == 2) { // Sala view -> show turma & professor
                    QString turma_names;
                    for (size_t i = 0; i < aula->turmas.size(); ++i) {
                        if (i > 0) turma_names += ", ";
                        const auto* t = data_.findTurma(aula->turmas[i]);
                        turma_names += QString::fromStdString(t ? t->nome() : aula->turmas[i]);
                    }
                    QString prof_names;
                    for (size_t i = 0; i < aula->professores.size(); ++i) {
                        if (i > 0) prof_names += ", ";
                        const auto* p = data_.findProfessor(aula->professores[i]);
                        prof_names += QString::fromStdString(p ? p->nome() : aula->professores[i]);
                    }
                    line2 = "Turma: " + turma_names;
                    line3 = prof_names;
                } else if (mode == 3) { // Todas as Disciplinas e Turmas
                    QString turma_names;
                    for (size_t i = 0; i < aula->turmas.size(); ++i) {
                        if (i > 0) turma_names += ", ";
                        const auto* t = data_.findTurma(aula->turmas[i]);
                        turma_names += QString::fromStdString(t ? t->nome() : aula->turmas[i]);
                    }
                    QString prof_names;
                    for (size_t i = 0; i < aula->professores.size(); ++i) {
                        if (i > 0) prof_names += ", ";
                        const auto* p = data_.findProfessor(aula->professores[i]);
                        prof_names += QString::fromStdString(p ? p->nome() : aula->professores[i]);
                    }
                    QString sala_names;
                    for (size_t i = 0; i < aula->salas.size(); ++i) {
                        if (i > 0) sala_names += ", ";
                        const auto* s = data_.findSala(aula->salas[i]);
                        sala_names += QString::fromStdString(s ? s->nome() : aula->salas[i]);
                    }
                    line2 = "Turma: " + (turma_names.isEmpty() ? "(sem turma)" : turma_names);
                    line3 = QString("Prof: %1 | Sala: %2")
                        .arg(prof_names.isEmpty() ? "(sem prof)" : prof_names,
                             sala_names.isEmpty() ? "(sem sala)" : sala_names);
                }

                QString card_text = QString("%1\n%2\n%3").arg(title, line2, line3);
                if (!aula_conflict_inline.isEmpty()) {
                    card_text += "\n" + aula_conflict_inline;
                }
                aula_blocks << card_text;
            }

            QStringList all_aulas_ids;
            for (const auto* aula : found_aulas) {
                all_aulas_ids << QString::fromStdString(aula->aula_id);
            }

            bool is_dark = ModernTheme::isDarkTheme();
            bool cell_has_selected = (!selected_aula_id_.isEmpty() &&
                std::find(all_aulas_ids.begin(), all_aulas_ids.end(), selected_aula_id_) != all_aulas_ids.end());

            bool cell_has_swap_conflict = false;
            QString sel_status_footer;
            QString sel_status_tooltip;
            if (sel_aloc && !cell_has_selected) {
                if (found_aulas.size() == 1) {
                    auto swap_confs = data_.avaliarTrocaAulas(sel_aloc->aula_id, found_aulas.front()->aula_id);
                    if (swap_confs.empty()) {
                        sel_status_footer = "🟢 Troca livre";
                        sel_status_tooltip = QString("<b>🎯 Simulação de troca com [%1]:</b><br>"
                                                     "✅ Troca livre (professor, sala e turma compatíveis nos dois horários).")
                                                 .arg(QString::fromStdString(sel_aloc->disciplina_nome));
                    } else {
                        cell_has_swap_conflict = true;
                        sel_status_footer = "⚠️ Troca: " + formatShortConflictTags(swap_confs, 2);
                        QStringList dets;
                        for (const auto& c : swap_confs) {
                            QString d = QString::fromStdString(c.detail);
                            if (!dets.contains(d)) dets << d;
                        }
                        sel_status_tooltip = QString("<b>🎯 Simulação de troca com [%1]:</b><br>• %2")
                                                 .arg(QString::fromStdString(sel_aloc->disciplina_nome),
                                                      dets.join("<br>• "));
                    }
                } else {
                    model::TimeInterval cand_interval(start_s, start_s + sel_aloc->intervalo.durationMinutes());
                    auto move_confs = data_.avaliarAulaNoHorario(
                        sel_aloc->aula_id, dia, cand_interval, {sel_aloc->aula_id});
                    if (move_confs.empty()) {
                        sel_status_footer = "🟢 Prof./Sala livres";
                        sel_status_tooltip = QString("<b>🎯 Disponibilidade para [%1] neste horário:</b><br>"
                                                     "✅ Professor, sala e turma da aula selecionada estão livres.")
                                                 .arg(QString::fromStdString(sel_aloc->disciplina_nome));
                    } else {
                        cell_has_swap_conflict = true;
                        sel_status_footer = "🔴 " + formatShortConflictTags(move_confs, 2);
                        QStringList dets;
                        for (const auto& c : move_confs) {
                            QString d = QString::fromStdString(c.detail);
                            if (!dets.contains(d)) dets << d;
                        }
                        sel_status_tooltip = QString("<b>🎯 Ocupação para [%1] neste horário:</b><br>• %2")
                                                 .arg(QString::fromStdString(sel_aloc->disciplina_nome),
                                                      dets.join("<br>• "));
                    }
                }
            }

            QString cell_text = aula_blocks.join("\n───────────────\n");
            if (!sel_status_footer.isEmpty()) {
                cell_text += "\n" + sel_status_footer;
            }
            auto* item = new QTableWidgetItem(cell_text);
            item->setData(Qt::UserRole, main_aula_id);
            item->setData(Qt::UserRole + 3, all_aulas_ids);

            if (cell_has_conflict) {
                // Destaque em cor vermelha para aulas que geraram conflito
                item->setBackground(is_dark ? QColor(153, 27, 27, 180) : QColor(254, 202, 202, 230));
                item->setForeground(is_dark ? QColor("#fca5a5") : QColor("#991b1b"));
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);

                cell_conflict_tooltips.removeDuplicates();
                QString tooltip = QString("⚠️ <b>AULA COM CONFLITO DE RESTRIÇÕES:</b><br>%1")
                    .arg(cell_conflict_tooltips.join("<br>"));
                if (!sel_status_tooltip.isEmpty()) {
                    tooltip += "<br><br>" + sel_status_tooltip;
                }
                item->setToolTip(tooltip);
            } else if (cell_has_selected) {
                // Destaque visual suave para a célula contendo a aula selecionada
                item->setBackground(is_dark ? QColor(30, 64, 175, 140) : QColor(191, 219, 254, 220));
                item->setForeground(is_dark ? QColor("#bfdbfe") : QColor("#1e3a8a"));
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
            } else if (cell_has_swap_conflict) {
                item->setBackground(is_dark ? QColor(217, 119, 6, 55) : QColor(254, 243, 199, 220));
                item->setForeground(is_dark ? QColor("#fcd34d") : QColor("#92400e"));
            } else if (has_lab) {
                item->setBackground(is_dark ? QColor(236, 72, 153, 40) : QColor(251, 207, 232, 160));
                item->setForeground(is_dark ? QColor("#f472b6") : QColor("#9d174d"));
            } else if (has_special) {
                item->setBackground(is_dark ? QColor(139, 92, 246, 40) : QColor(233, 213, 255, 160));
                item->setForeground(is_dark ? QColor("#c4b5fd") : QColor("#6b21a8"));
            } else {
                item->setBackground(is_dark ? QColor(37, 99, 235, 40) : QColor(219, 234, 254, 160));
                item->setForeground(is_dark ? QColor("#60a5fa") : QColor("#1e40af"));
            }

            if (!cell_has_conflict && found_aulas.size() > 1) {
                QStringList tip_lines;
                tip_lines << QString("<b>📋 %1 aulas alocadas neste horário:</b>").arg(found_aulas.size());
                for (const auto* a : found_aulas) {
                    QString t_names, p_names, s_names;
                    for (const auto& tid : a->turmas) {
                        const auto* t = data_.findTurma(tid);
                        t_names += (t_names.isEmpty() ? "" : ", ") + QString::fromStdString(t ? t->nome() : tid);
                    }
                    for (const auto& pid : a->professores) {
                        const auto* p = data_.findProfessor(pid);
                        p_names += (p_names.isEmpty() ? "" : ", ") + QString::fromStdString(p ? p->nome() : pid);
                    }
                    for (const auto& sid : a->salas) {
                        const auto* s = data_.findSala(sid);
                        s_names += (s_names.isEmpty() ? "" : ", ") + QString::fromStdString(s ? s->nome() : sid);
                    }
                    tip_lines << QString("• <b>[%1] %2</b><br>&nbsp;&nbsp;&nbsp;&nbsp;👤 Prof: %3 | 🏢 Sala: %4")
                        .arg(t_names.isEmpty() ? "Sem Turma" : t_names,
                             QString::fromStdString(a->disciplina_nome),
                             p_names.isEmpty() ? "—" : p_names,
                             s_names.isEmpty() ? "—" : s_names);
                }
                if (!sel_status_tooltip.isEmpty()) {
                    tip_lines << "" << sel_status_tooltip;
                }
                item->setToolTip(tip_lines.join("<br>"));
            } else if (!cell_has_conflict && !sel_status_tooltip.isEmpty()) {
                item->setToolTip(sel_status_tooltip);
            }

            item->setData(Qt::UserRole + 1, item->background());
            item->setData(Qt::UserRole + 2, item->foreground());
            table_->setItem(cell_row, cell_col, item);
        }
        time_idx++;
    }

    if (search_lesson_ && !search_lesson_->text().trimmed().isEmpty()) {
        onSearchLessonChanged(search_lesson_->text());
    }
}

/**
 * @brief Destaca as células cujas aulas correspondem ao texto digitado no campo de busca.
 * @param query Texto pesquisado.
 */
void TimetableView::onSearchLessonChanged(const QString& query) {
    QString clean = removeAccents(query).trimmed().toLower();
    int matches = 0;

    for (int r = 0; r < table_->rowCount(); ++r) {
        for (int c = 0; c < table_->columnCount(); ++c) {
            auto* it = table_->item(r, c);
            if (!it) continue;
            if (it->data(Qt::UserRole).toString().isEmpty()) continue;
            QString text = it->text();
            if (text == "-" || text.startsWith("☕") || text.startsWith("Nenhuma")) continue;

            QBrush origBg = it->data(Qt::UserRole + 1).value<QBrush>();
            QBrush origFg = it->data(Qt::UserRole + 2).value<QBrush>();

            bool is_dark = ModernTheme::isDarkTheme();
            if (!clean.isEmpty()) {
                if (removeAccents(text).toLower().contains(clean)) {
                    matches++;
                    it->setBackground(is_dark ? QColor(245, 158, 11, 140) : QColor(254, 240, 138, 220));
                    it->setForeground(is_dark ? QColor("#fef08a") : QColor("#854d0e"));
                } else {
                    it->setBackground(is_dark ? QColor(15, 23, 42, 160) : QColor(241, 245, 249, 180));
                    it->setForeground(is_dark ? QColor("#475569") : QColor("#94a3b8"));
                }
            } else {
                if (origBg.style() != Qt::NoBrush) {
                    it->setBackground(origBg);
                } else {
                    it->setBackground(QBrush());
                }
                if (origFg.style() != Qt::NoBrush) {
                    it->setForeground(origFg);
                }
            }
        }
    }

    if (clean.isEmpty()) {
        lbl_search_count_->setVisible(false);
    } else {
        lbl_search_count_->setVisible(true);
        if (matches == 0) {
            lbl_search_count_->setText("Nenhuma aula encontrada");
            lbl_search_count_->setStyleSheet(
                "background-color: rgba(239, 68, 68, 0.15); color: #dc2626; border: 1px solid rgba(239, 68, 68, 0.4); "
                "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
        } else {
            lbl_search_count_->setText(QString("%1 %2").arg(matches).arg(matches == 1 ? "aula encontrada" : "aulas encontradas"));
            lbl_search_count_->setStyleSheet(
                "background-color: rgba(2, 132, 199, 0.15); color: #0284c7; border: 1px solid rgba(2, 132, 199, 0.4); "
                "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
        }
    }
}

/**
 * @brief Abre o menu de contexto com ações de fixação, troca de sala e movimentação ao clicar na célula.
 * @param pos Ponto do clique do mouse.
 */
void TimetableView::onCellContextMenu(const QPoint& pos) {
    auto* item = table_->itemAt(pos);
    if (!item) return;

    int row = table_->rowAt(pos.y());
    int col = table_->columnAt(pos.x());
    QString aula_id = item->data(Qt::UserRole).toString();

    // Se clicou em célula vazia (ou sem aula alocada)
    if (aula_id.isEmpty()) {
        QString text = item->text();
        // Não oferecer alocação se for dia não letivo ("—")
        if (text == "—") return;

        QMenu menu(this);
        auto* add_act = menu.addAction(ModernTheme::icon("add"), "Alocar Disciplina Neste Horário...");
        connect(add_act, &QAction::triggered, [this, row, col]() {
            alocarAulaManual(row, col);
        });

        menu.addSeparator();
        auto* clear_act = menu.addAction(ModernTheme::icon("delete"), "Limpar Toda a Grade...");
        connect(clear_act, &QAction::triggered, this, &TimetableView::limparGradeCompleta);

        menu.exec(table_->viewport()->mapToGlobal(pos));
        return;
    }

    QStringList all_aulas_ids = item->data(Qt::UserRole + 3).toStringList();
    if (all_aulas_ids.isEmpty() && !aula_id.isEmpty()) {
        all_aulas_ids << aula_id;
    }

    if (!all_aulas_ids.isEmpty()) {
        QRect cell_rect = table_->visualItemRect(item);
        int rel_y = pos.y() - cell_rect.top();
        int idx = std::clamp((rel_y * static_cast<int>(all_aulas_ids.size())) / std::max(1, cell_rect.height()), 0, static_cast<int>(all_aulas_ids.size()) - 1);
        selected_aula_id_ = all_aulas_ids[idx];
    }

    if (all_aulas_ids.size() > 1) {
        QMenu menu(this);
        for (const auto& aid_q : all_aulas_ids) {
            std::string cur_aid = aid_q.toStdString();
            auto* cur_aloc = data_.grade().findAula(cur_aid);
            auto* cur_model = data_.findAula(cur_aid);
            if (!cur_aloc || !cur_model) continue;

            QString t_name;
            if (!cur_aloc->turmas.empty()) {
                const auto* t = data_.findTurma(cur_aloc->turmas.front());
                t_name = QString::fromStdString(t ? t->nome() : cur_aloc->turmas.front());
            }
            QString sub_title = QString("[%1] %2").arg(t_name.isEmpty() ? "Turma" : t_name, QString::fromStdString(cur_aloc->disciplina_nome));
            if (cur_aloc->is_pinned) sub_title = "📌 " + sub_title;

            auto* sub = menu.addMenu(ModernTheme::icon(cur_aloc->tipo == "LABORATORIO" ? "room" : "disciplina"), sub_title);

            auto* swap_act = sub->addAction(ModernTheme::icon("swap"), "Trocar Horário Com Outra Aula (Swap)...");
            connect(swap_act, &QAction::triggered, [this, aid_q]() {
                selected_aula_id_ = aid_q;
                trocarAulaManual(aid_q);
            });

            auto* m_act = sub->addAction(ModernTheme::icon("swap"), "Mover Horário / Sala...");
            connect(m_act, &QAction::triggered, [this, aid_q]() {
                selected_aula_id_ = aid_q;
                moverAulaManual(aid_q);
            });

            auto* r_act = sub->addAction(ModernTheme::icon("room"), "Alterar Sala de Aula...");
            connect(r_act, &QAction::triggered, [this, aid_q]() {
                mudarSalaManual(aid_q);
            });

            sub->addSeparator();

            if (cur_model->isPinned()) {
                auto* unpin_act = sub->addAction(ModernTheme::icon("unpin"), "Desafixar Horário Desta Aula");
                connect(unpin_act, &QAction::triggered, [this, cur_aid]() {
                    auto* a_m = data_.findAula(cur_aid);
                    auto* a_a = data_.grade().findAula(cur_aid);
                    if (a_m && a_a) {
                        a_m->unpin();
                        a_a->is_pinned = false;
                        renderTimetable();
                        emit lessonPinChanged(QString::fromStdString(a_m->id()), false);
                    }
                });
            } else {
                auto* pin_act = sub->addAction(ModernTheme::icon("pin"), "Fixar Horário Desta Aula (Bloquear para Reotimização)");
                connect(pin_act, &QAction::triggered, [this, cur_aid]() {
                    auto* a_m = data_.findAula(cur_aid);
                    auto* a_a = data_.grade().findAula(cur_aid);
                    if (a_m && a_a) {
                        a_m->setPinned(true);
                        a_m->setPinnedDay(a_a->dia);
                        a_m->setPinnedInterval(a_a->intervalo);
                        a_m->setPinnedRooms(a_a->salas);
                        a_a->is_pinned = true;
                        renderTimetable();
                        emit lessonPinChanged(QString::fromStdString(a_m->id()), true);
                    }
                });
            }

            auto* desalocar_act = sub->addAction(ModernTheme::icon("delete"), "Desalocar Aula da Grade");
            connect(desalocar_act, &QAction::triggered, [this, cur_aid]() {
                auto resp = QMessageBox::question(this, "Confirmar Desalocação",
                    "Deseja remover esta aula da grade de horários?\n(A aula ficará como não alocada até a próxima otimização)",
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (resp == QMessageBox::Yes) {
                    auto& alocs = data_.grade().alocacoes();
                    alocs.erase(std::remove_if(alocs.begin(), alocs.end(), [&](const model::AllocatedLesson& a) {
                        return a.aula_id == cur_aid;
                    }), alocs.end());
                    auto* a_model = data_.findAula(cur_aid);
                    if (a_model) a_model->unpin();
                    auto stats = data_.grade().estatisticas();
                    stats.total_aulas_alocadas = static_cast<int>(alocs.size());
                    data_.grade().setEstatisticas(stats);
                    data_.grade().addMensagemDiagnostico("[MANUAL] Aula " + cur_aid + " desalocada da grade pelo usuário.");
                    renderTimetable();
                    emit timetableModified();
                    emit lessonMoved(QString::fromStdString(cur_aid), "Desalocada da grade");
                }
            });
        }

        menu.addSeparator();

        auto* add_act = menu.addAction(ModernTheme::icon("add"), "Alocar Outra Disciplina na Grade...");
        connect(add_act, &QAction::triggered, [this]() {
            alocarAulaManual(-1, -1);
        });

        menu.addSeparator();

        auto* pin_all_act = menu.addAction(ModernTheme::icon("pin"), "Fixar Todas as Aulas da Grade Atual (Bloquear para Próximo Solver)");
        connect(pin_all_act, &QAction::triggered, [this]() {
            for (auto& aloc : data_.grade().alocacoes()) {
                auto* a_model = data_.findAula(aloc.aula_id);
                if (a_model) {
                    a_model->setPinned(true);
                    a_model->setPinnedDay(aloc.dia);
                    a_model->setPinnedInterval(aloc.intervalo);
                    a_model->setPinnedRooms(aloc.salas);
                    aloc.is_pinned = true;
                }
            }
            renderTimetable();
        });

        auto* unpin_all_act = menu.addAction(ModernTheme::icon("unpin"), "Desafixar Todas as Aulas da Grade Atual");
        connect(unpin_all_act, &QAction::triggered, [this]() {
            for (auto& aloc : data_.grade().alocacoes()) {
                auto* a_model = data_.findAula(aloc.aula_id);
                if (a_model) {
                    a_model->unpin();
                    aloc.is_pinned = false;
                }
            }
            renderTimetable();
        });

        menu.addSeparator();
        auto* clear_act = menu.addAction(ModernTheme::icon("delete"), "Limpar Toda a Grade...");
        connect(clear_act, &QAction::triggered, this, &TimetableView::limparGradeCompleta);

        menu.exec(table_->viewport()->mapToGlobal(pos));
        return;
    }

    std::string aid = aula_id.toStdString();
    auto* aula_aloc = data_.grade().findAula(aid);
    auto* aula_model = data_.findAula(aid);
    if (!aula_aloc || !aula_model) return;

    QMenu menu(this);

    auto* swap_act = menu.addAction(ModernTheme::icon("swap"), "Trocar Horário Com Outra Aula (Swap)...");
    connect(swap_act, &QAction::triggered, [this, aula_id]() {
        selected_aula_id_ = aula_id;
        trocarAulaManual(aula_id);
    });

    auto* move_act = menu.addAction(ModernTheme::icon("swap"), "Mover Horário / Sala...");
    connect(move_act, &QAction::triggered, [this, aula_id]() {
        selected_aula_id_ = aula_id;
        moverAulaManual(aula_id);
    });

    auto* room_act = menu.addAction(ModernTheme::icon("room"), "Alterar Sala de Aula...");
    connect(room_act, &QAction::triggered, [this, aula_id]() {
        mudarSalaManual(aula_id);
    });

    menu.addSeparator();

    auto* add_act = menu.addAction(ModernTheme::icon("add"), "Alocar Outra Disciplina na Grade...");
    connect(add_act, &QAction::triggered, [this]() {
        alocarAulaManual(-1, -1);
    });

    menu.addSeparator();

    if (aula_model->isPinned()) {
        auto* unpin_act = menu.addAction(ModernTheme::icon("unpin"), "Desafixar Horário Desta Aula");
        connect(unpin_act, &QAction::triggered, [this, aid]() {
            auto* a_model = data_.findAula(aid);
            auto* a_aloc = data_.grade().findAula(aid);
            if (a_model && a_aloc) {
                a_model->unpin();
                a_aloc->is_pinned = false;
                renderTimetable();
                emit lessonPinChanged(QString::fromStdString(a_model->id()), false);
            }
        });
    } else {
        auto* pin_act = menu.addAction(ModernTheme::icon("pin"), "Fixar Horário Desta Aula (Bloquear para Reotimização)");
        connect(pin_act, &QAction::triggered, [this, aid]() {
            auto* a_model = data_.findAula(aid);
            auto* a_aloc = data_.grade().findAula(aid);
            if (a_model && a_aloc) {
                a_model->setPinned(true);
                a_model->setPinnedDay(a_aloc->dia);
                a_model->setPinnedInterval(a_aloc->intervalo);
                a_model->setPinnedRooms(a_aloc->salas);
                a_aloc->is_pinned = true;
                renderTimetable();
                emit lessonPinChanged(QString::fromStdString(a_model->id()), true);
            }
        });
    }

    auto* desalocar_act = menu.addAction(ModernTheme::icon("delete"), "Desalocar Aula da Grade");
    connect(desalocar_act, &QAction::triggered, [this, aid]() {
        auto resp = QMessageBox::question(this, "Confirmar Desalocação",
            "Deseja remover esta aula da grade de horários?\n(A aula ficará como não alocada até a próxima otimização)",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (resp == QMessageBox::Yes) {
            auto& alocs = data_.grade().alocacoes();
            alocs.erase(std::remove_if(alocs.begin(), alocs.end(), [&](const model::AllocatedLesson& a) {
                return a.aula_id == aid;
            }), alocs.end());
            auto* a_model = data_.findAula(aid);
            if (a_model) a_model->unpin();
            auto stats = data_.grade().estatisticas();
            stats.total_aulas_alocadas = static_cast<int>(alocs.size());
            data_.grade().setEstatisticas(stats);
            data_.grade().addMensagemDiagnostico("[MANUAL] Aula " + aid + " desalocada da grade pelo usuário.");
            renderTimetable();
            emit timetableModified();
            emit lessonMoved(QString::fromStdString(aid), "Desalocada da grade");
        }
    });

    menu.addSeparator();
    auto* pin_all_act = menu.addAction(ModernTheme::icon("pin"), "Fixar Todas as Aulas da Grade Atual (Bloquear para Próximo Solver)");
    connect(pin_all_act, &QAction::triggered, [this]() {
        for (auto& aloc : data_.grade().alocacoes()) {
            auto* a_model = data_.findAula(aloc.aula_id);
            if (a_model) {
                a_model->setPinned(true);
                a_model->setPinnedDay(aloc.dia);
                a_model->setPinnedInterval(aloc.intervalo);
                a_model->setPinnedRooms(aloc.salas);
                aloc.is_pinned = true;
            }
        }
        renderTimetable();
    });

    auto* unpin_all_act = menu.addAction(ModernTheme::icon("unpin"), "Desafixar Todas as Aulas da Grade Atual");
    connect(unpin_all_act, &QAction::triggered, [this]() {
        for (auto& aloc : data_.grade().alocacoes()) {
            auto* a_model = data_.findAula(aloc.aula_id);
            if (a_model) {
                a_model->unpin();
                aloc.is_pinned = false;
            }
        }
        renderTimetable();
    });

    menu.addSeparator();
    auto* clear_act = menu.addAction(ModernTheme::icon("delete"), "Limpar Toda a Grade...");
    connect(clear_act, &QAction::triggered, this, &TimetableView::limparGradeCompleta);

    menu.exec(table_->viewport()->mapToGlobal(pos));
}

/**
 * @brief Abre o diálogo de movimentação rápida ou alocação manual ao executar clique duplo em uma célula.
 * @param row Linha da célula.
 * @param col Coluna da célula.
 */
void TimetableView::onCellDoubleClicked(int row, int col) {
    auto* item = table_->item(row, col);
    if (!item) return;

    QStringList all_aulas_ids = item->data(Qt::UserRole + 3).toStringList();
    if (all_aulas_ids.size() > 1) {
        QMenu menu(this);
        menu.setTitle("Ações para a Aula:");
        for (const auto& aid_q : all_aulas_ids) {
            std::string aid = aid_q.toStdString();
            auto* a_aloc = data_.grade().findAula(aid);
            if (!a_aloc) continue;
            QString t_name;
            if (!a_aloc->turmas.empty()) {
                const auto* t = data_.findTurma(a_aloc->turmas.front());
                t_name = QString::fromStdString(t ? t->nome() : a_aloc->turmas.front());
            }
            auto* sub = menu.addMenu(ModernTheme::icon("disciplina"), QString("[%1] %2").arg(t_name.isEmpty() ? "Turma" : t_name, QString::fromStdString(a_aloc->disciplina_nome)));
            auto* act_swap = sub->addAction(ModernTheme::icon("swap"), "🔄 Trocar Esta Aula com Outra (Swap)...");
            connect(act_swap, &QAction::triggered, [this, aid_q]() {
                selected_aula_id_ = aid_q;
                trocarAulaManual(aid_q);
            });
            auto* act_move = sub->addAction(ModernTheme::icon("swap"), "✏️ Mover Horário / Sala...");
            connect(act_move, &QAction::triggered, [this, aid_q]() {
                selected_aula_id_ = aid_q;
                moverAulaManual(aid_q);
            });
            auto* act_room = sub->addAction(ModernTheme::icon("room"), "🏢 Alterar Sala de Aula...");
            connect(act_room, &QAction::triggered, [this, aid_q]() {
                selected_aula_id_ = aid_q;
                mudarSalaManual(aid_q);
            });
        }
        menu.exec(QCursor::pos());
        return;
    }

    QString aid = item->data(Qt::UserRole).toString();
    if (!aid.isEmpty()) {
        selected_aula_id_ = aid;
        trocarAulaManual(aid);
    } else {
        QString text = item->text();
        if (!text.startsWith("☕") && text != "—") {
            alocarAulaManual(row, col);
        }
    }
}

/**
 * @brief Abre o diálogo de movimentação manual de horário e sala para a aula indicada.
 * @param aula_id ID da aula a movimentar.
 */
void TimetableView::moverAulaManual(const QString& aula_id) {
    std::string aid = aula_id.toStdString();
    auto* a_aloc = data_.grade().findAula(aid);
    auto* a_model = data_.findAula(aid);
    if (!a_aloc || !a_model) return;

    MoveLessonDialog dlg(data_, aid, row_intervals_, this);
    if (dlg.exec() == QDialog::Accepted) {
        auto target_day = dlg.selectedDay();
        auto target_interval = dlg.selectedInterval();
        std::string target_room = dlg.selectedRoomId();

        bool room_changed = false;
        std::string s_nome;
        if (!target_room.empty() && (a_aloc->salas.empty() || a_aloc->salas.front() != target_room)) {
            a_aloc->salas = { target_room };
            a_model->setPinnedRooms(a_aloc->salas);
            const auto* s = data_.findSala(target_room);
            s_nome = s ? s->nome() : target_room;
            room_changed = true;
        }

        int time_idx = -1;
        for (size_t r = 0; r < row_intervals_.size(); ++r) {
            if (row_intervals_[r] == target_interval) {
                time_idx = static_cast<int>(r);
                break;
            }
        }
        int day_idx = static_cast<int>(target_day);
        int target_row = transpose_grid_ ? day_idx : time_idx;
        int target_col = transpose_grid_ ? time_idx : day_idx;
        if (target_row >= 0 && target_col >= 0) {
            if (a_aloc->dia != target_day || a_aloc->intervalo != target_interval) {
                onLessonDropRequested(aula_id, target_row, target_col);
            } else if (room_changed) {
                // Verificar se a alteração de sala causará choque ou incompatibilidade
                std::vector<std::string> room_conflitos;
                const auto* s_novo = data_.findSala(target_room);
                if (a_aloc->tipo == "LABORATORIO" && s_novo && s_novo->isTeorica()) {
                    room_conflitos.push_back("Incompatibilidade de sala: A aula requer Laboratório, mas a Sala '" + s_nome + "' é Teórica.");
                }
                for (const auto& other_a : data_.grade().alocacoes()) {
                    if (other_a.aula_id != aid && other_a.dia == a_aloc->dia &&
                        std::max(other_a.intervalo.start_min, a_aloc->intervalo.start_min) < std::min(other_a.intervalo.end_min, a_aloc->intervalo.end_min)) {
                        if (std::find(other_a.salas.begin(), other_a.salas.end(), target_room) != other_a.salas.end()) {
                            room_conflitos.push_back("Choque de sala: A Sala '" + s_nome + "' já está ocupada pela aula '" +
                                                    other_a.disciplina_nome + "' (" + other_a.aula_id + ") neste horário.");
                        }
                    }
                }
                if (!room_conflitos.empty()) {
                    QString msg = QString("Foram detectados %1 conflito(s) ao alterar para a sala '%2':\n\n")
                        .arg(room_conflitos.size()).arg(QString::fromStdString(s_nome));
                    for (const auto& c : room_conflitos) {
                        msg += "• " + QString::fromStdString(c) + "\n";
                    }
                    msg += "\n⚠️ As aulas que geraram conflito serão destacadas em cor VERMELHA na grade horária.\n\nDeseja permissão para confirmar a alteração mesmo com os conflitos informados?";
                    auto resp = QMessageBox::question(this, "Permissão: Conflito de Sala", msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                    if (resp != QMessageBox::Yes) {
                        // Reverter sala
                        if (a_model->isPinned() && !a_model->pinnedRooms().empty()) {
                            a_aloc->salas = a_model->pinnedRooms();
                        }
                        return;
                    }
                }
                a_aloc->is_pinned = true;
                a_model->setPinned(true);
                data_.grade().addMensagemDiagnostico("[MANUAL] Sala da aula " + aid + " (" + a_aloc->disciplina_nome + ") alterada para " + s_nome + ".");
                renderTimetable();
                emit timetableModified();
                emit lessonMoved(aula_id, QString("Sala alterada para %1").arg(QString::fromStdString(s_nome)));
            }
        }
    }
}

/**
 * @brief Abre o diálogo para alterar ou trocar (swap) a sala de aula da aula informada.
 * @param aula_id ID da aula.
 */
void TimetableView::mudarSalaManual(const QString& aula_id) {
    std::string aid = aula_id.toStdString();
    auto* a_aloc = data_.grade().findAula(aid);
    auto* a_model = data_.findAula(aid);
    if (!a_aloc || !a_model) return;

    ChangeRoomDialog dlg(data_, aid, this);
    if (dlg.exec() == QDialog::Accepted) {
        std::string nova_sala_id = dlg.selectedRoomId();
        if (nova_sala_id.empty()) return;

        if (!a_aloc->salas.empty() && a_aloc->salas.front() == nova_sala_id) {
            return;
        }

        std::string sala_antiga_nome;
        if (!a_aloc->salas.empty()) {
            const auto* s_ant = data_.findSala(a_aloc->salas.front());
            sala_antiga_nome = s_ant ? s_ant->nome() : a_aloc->salas.front();
        } else {
            sala_antiga_nome = "Nenhuma";
        }

        const auto* s_nova = data_.findSala(nova_sala_id);
        std::string sala_nova_nome = s_nova ? s_nova->nome() : nova_sala_id;

        std::string conflito_aid = dlg.conflictingAulaId();
        if (!conflito_aid.empty() && dlg.isSwapRequested()) {
            auto* conflito_aloc = data_.grade().findAula(conflito_aid);
            auto* conflito_model = data_.findAula(conflito_aid);
            if (conflito_aloc) {
                auto salas_antigas = a_aloc->salas;
                a_aloc->salas = { nova_sala_id };
                conflito_aloc->salas = salas_antigas;

                if (dlg.isPinned()) {
                    a_aloc->is_pinned = true;
                    a_model->setPinned(true);
                    a_model->setPinnedRooms(a_aloc->salas);
                    conflito_aloc->is_pinned = true;
                    if (conflito_model) {
                        conflito_model->setPinned(true);
                        conflito_model->setPinnedRooms(conflito_aloc->salas);
                    }
                }

                data_.grade().addMensagemDiagnostico("[MANUAL] Troca de salas: Aula " + aid + " (" + a_aloc->disciplina_nome + ") trocou para " + sala_nova_nome +
                                                     ", e Aula " + conflito_aid + " (" + conflito_aloc->disciplina_nome + ") trocou para " + sala_antiga_nome + ".");

                renderTimetable();
                emit timetableModified();
                emit lessonMoved(QString::fromStdString(aid), QString("Sala trocada para %1 (Swap com %2)").arg(QString::fromStdString(sala_nova_nome), QString::fromStdString(conflito_aloc->disciplina_nome)));
                return;
            }
        }

        a_aloc->salas = { nova_sala_id };
        if (dlg.isPinned()) {
            a_aloc->is_pinned = true;
            a_model->setPinned(true);
            a_model->setPinnedRooms(a_aloc->salas);
        }

        data_.grade().addMensagemDiagnostico("[MANUAL] Sala da aula " + aid + " (" + a_aloc->disciplina_nome + ") alterada de " + sala_antiga_nome + " para " + sala_nova_nome + ".");

        renderTimetable();
        emit timetableModified();
        emit lessonMoved(QString::fromStdString(aid), QString("Sala alterada para %1").arg(QString::fromStdString(sala_nova_nome)));
    }
}

/**
 * @brief Abre o diálogo de alocação manual de disciplina na grade horária.
 * @param row Linha da célula da tabela selecionada (-1 se nenhuma).
 * @param col Coluna da célula da tabela selecionada (-1 se nenhuma).
 */
void TimetableView::alocarAulaManual(int row, int col) {
    if (data_.disciplinas().empty()) {
        QMessageBox::information(this, "Sem Disciplinas",
            "Nenhuma disciplina cadastrada na instituição.\nCadastre primeiro as disciplinas na aba '📚 Disciplinas' para poder alocá-las na grade.");
        return;
    }

    model::DayOfWeek default_day = model::DayOfWeek::SEG;
    model::TimeInterval default_interval;
    int time_idx = transpose_grid_ ? col : row;
    int day_idx = transpose_grid_ ? row : col;
    if (day_idx >= 0 && day_idx < static_cast<int>(model::allDays().size())) {
        default_day = model::allDays()[day_idx];
    }
    if (time_idx >= 0 && time_idx < static_cast<int>(row_intervals_.size())) {
        default_interval = row_intervals_[time_idx];
    }

    std::string def_turma, def_prof, def_sala;
    int mode = combo_view_type_->currentData().toInt();
    std::string target_id = combo_target_->currentData().toString().toStdString();
    if (mode == 0) def_turma = target_id;
    else if (mode == 1) def_prof = target_id;
    else if (mode == 2) def_sala = target_id;
    else if (mode == 3 && !target_id.empty() && target_id != "ALL") {
        def_turma = target_id;
    }

    AddLessonToScheduleDialog dlg(data_, this, default_day, default_interval, def_turma, def_prof, def_sala);
    if (dlg.exec() == QDialog::Accepted) {
        auto aloc = dlg.allocatedLesson();
        bool pinned = dlg.isPinned();
        std::string aid = aloc.aula_id;

        // Se a ocorrência já existia alocada na grade, removemos a alocação anterior para atualizar
        auto& alocs = data_.grade().alocacoes();
        alocs.erase(std::remove_if(alocs.begin(), alocs.end(), [&](const model::AllocatedLesson& a) {
            return a.aula_id == aid;
        }), alocs.end());

        // Adiciona a nova alocação na grade
        aloc.is_pinned = pinned;
        data_.grade().addAllocatedLesson(aloc);

        // Assegura consistência com a lista de ocorrências Lesson em data_.aulas()
        auto* a_model = data_.findAula(aid);
        if (!a_model) {
            const auto* disc = data_.findDisciplina(aloc.disciplina_id);
            if (disc) {
                model::Lesson nova_aula(aid, disc->id(), disc->nome(), disc->tipo(),
                                        disc->duracaoMinutos(), disc->professores(), disc->turmas(),
                                        disc->regime(), disc->semestre());
                nova_aula.setTurnosPermitidos(disc->turnosPermitidos());
                nova_aula.setSalasAlternativas(disc->salasAlternativas());
                if (pinned) {
                    nova_aula.setPinned(true);
                    nova_aula.setPinnedDay(aloc.dia);
                    nova_aula.setPinnedInterval(aloc.intervalo);
                    nova_aula.setPinnedRooms(aloc.salas);
                }
                data_.aulas().push_back(std::move(nova_aula));
            }
        } else {
            if (pinned) {
                a_model->setPinned(true);
                a_model->setPinnedDay(aloc.dia);
                a_model->setPinnedInterval(aloc.intervalo);
                a_model->setPinnedRooms(aloc.salas);
            } else {
                a_model->unpin();
            }
        }

        // Atualiza métricas estatísticas da grade
        auto stats = data_.grade().estatisticas();
        stats.total_aulas_alocadas = static_cast<int>(data_.grade().alocacoes().size());
        data_.grade().setEstatisticas(stats);

        data_.grade().addMensagemDiagnostico("[MANUAL] Aula " + aid + " (" + aloc.disciplina_nome + ") alocada manualmente na grade em " +
                                             aloc.diaString() + " às " + aloc.horarioFormatado() + (pinned ? " [FIXADA]" : "") + ".");

        renderTimetable();
        emit timetableModified();
        emit lessonMoved(QString::fromStdString(aid), QString("Alocada manualmente em %1 %2")
            .arg(QString::fromStdString(aloc.diaString()), QString::fromStdString(aloc.horarioFormatado())));
    }
}

/**
 * @brief Limpa todas as alocações da grade horária permitindo reiniciar a montagem da grade.
 */
void TimetableView::limparGradeCompleta() {
    if (data_.grade().alocacoes().empty()) {
        QMessageBox::information(this, "Grade Vazia", "A grade horária já está vazia.");
        return;
    }

    auto resp = QMessageBox::question(this, "Limpar Grade Horária",
        QString("Deseja realmente remover todas as %1 aulas alocadas na grade semanal?\n\n"
                "• Todas as aulas voltarão para o estado não alocado.\n"
                "• As fixações manuais (pins) na grade serão liberadas.\n"
                "• A estrutura de turmas, professores, salas e disciplinas será mantida intacta.")
            .arg(data_.grade().alocacoes().size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (resp == QMessageBox::Yes) {
        data_.grade().alocacoes().clear();
        for (auto& a : data_.aulas()) {
            a.unpin();
        }
        auto stats = data_.grade().estatisticas();
        stats.total_aulas_alocadas = 0;
        stats.status = "NAO_EXECUTADO";
        data_.grade().setEstatisticas(stats);
        data_.grade().addMensagemDiagnostico("[MANUAL] Grade horária limpa completamente pelo usuário.");

        renderTimetable();
        emit timetableModified();
        QMessageBox::information(this, "Grade Limpa", "A grade horária foi limpa com sucesso. Agora você pode alocar disciplinas manualmente para compor uma grade inicial.");
    }
}

/**
 * @brief Audita se a movimentação para um dia e horário gera choque com turmas, docentes ou salas.
 * @param aid ID da aula a movimentar.
 * @param dia Dia de destino pretendido.
 * @param intervalo Intervalo de destino pretendido.
 * @return Vetor de mensagens descrevendo conflitos encontrados.
 */
std::vector<std::string> TimetableView::verificarConflitosMovimentacao(
    const std::string& aid,
    model::DayOfWeek dia,
    const model::TimeInterval& intervalo) const {

    std::vector<std::string> conflitos;
    auto infos = data_.avaliarAulaNoHorario(aid, dia, intervalo, {aid});
    conflitos.reserve(infos.size());
    for (const auto& info : infos) {
        conflitos.push_back(info.detail);
    }
    return conflitos;
}

/**
 * @brief Trata a soltura (drop) de uma aula arrastada sobre uma nova célula da grade.
 * @param aula_id ID da aula arrastada.
 * @param target_row Linha da célula de destino.
 * @param target_col Coluna da célula de destino.
 */
void TimetableView::onLessonDropRequested(const QString& aula_id, int target_row, int target_col) {
    int time_idx = transpose_grid_ ? target_col : target_row;
    int day_idx = transpose_grid_ ? target_row : target_col;
    if (time_idx < 0 || time_idx >= static_cast<int>(row_intervals_.size())) return;
    if (day_idx < 0 || day_idx >= 5) return;

    auto target_day = static_cast<model::DayOfWeek>(day_idx);
    const auto& target_interval = row_intervals_[time_idx];

    auto* target_item = table_->item(target_row, target_col);
    if (target_item) {
        QString text = target_item->text();
        if (text.startsWith("☕")) {
            QMessageBox::warning(this, "Horário Inválido", "Não é possível alocar aulas dentro de um intervalo.");
            return;
        }
        if (text == "—") {
            QMessageBox::warning(this, "Horário Inválido", "O horário selecionado está fora da jornada letiva da turma neste dia.");
            return;
        }
    }

    std::string aid = aula_id.toStdString();
    auto* a_aloc = data_.grade().findAula(aid);
    auto* a_model = data_.findAula(aid);
    if (!a_aloc || !a_model) return;

    // Se soltou no mesmo lugar
    if (a_aloc->dia == target_day && a_aloc->intervalo == target_interval) {
        return;
    }

    // Verificar se o slot destino já possui uma aula alocada para o alvo atual
    QStringList target_ids = target_item ? target_item->data(Qt::UserRole + 3).toStringList() : QStringList();
    if (target_ids.isEmpty() && target_item && !target_item->data(Qt::UserRole).toString().isEmpty()) {
        target_ids << target_item->data(Qt::UserRole).toString();
    }
    target_ids.removeAll(QString::fromStdString(aid));

    std::string target_aid;
    if (target_ids.size() > 1) {
        QMessageBox box(this);
        box.setWindowTitle("Trocar ou Mover Aula");
        box.setIcon(QMessageBox::Question);
        box.setText(QString("O horário de destino (%1 às %2) possui %3 aulas alocadas.\n\n"
                            "Com qual aula deseja trocar os horários (Swap)?")
                    .arg(QString::fromStdString(model::dayToLongString(target_day)))
                    .arg(QString::fromStdString(target_interval.toString()))
                    .arg(target_ids.size()));

        std::vector<QAbstractButton*> swap_btns;
        for (const auto& tid_q : target_ids) {
            std::string tid = tid_q.toStdString();
            const auto* al = data_.grade().findAula(tid);
            if (al) {
                QString t_name;
                if (!al->turmas.empty()) {
                    const auto* t = data_.findTurma(al->turmas.front());
                    t_name = QString::fromStdString(t ? t->nome() : al->turmas.front());
                }
                QString btn_text = QString("🔄 Trocar com: [%1] %2").arg(t_name.isEmpty() ? "Turma" : t_name, QString::fromStdString(al->disciplina_nome));
                swap_btns.push_back(box.addButton(btn_text, QMessageBox::ActionRole));
            }
        }
        auto* btn_force = box.addButton("➡️ Alocar Juntas", QMessageBox::ActionRole);
        auto* btn_cancel = box.addButton("Cancelar", QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() == btn_cancel) return;
        if (box.clickedButton() != btn_force) {
            for (size_t i = 0; i < swap_btns.size(); ++i) {
                if (box.clickedButton() == swap_btns[i]) {
                    target_aid = target_ids[i].toStdString();
                    break;
                }
            }
        }
    } else if (target_ids.size() == 1) {
        target_aid = target_ids.front().toStdString();
        auto* target_aloc = data_.grade().findAula(target_aid);
        if (target_aloc && target_aid != aid) {
            auto swap_confs = data_.avaliarTrocaAulas(aid, target_aid);
            QString swap_warn;
            if (!swap_confs.empty()) {
                swap_warn = QString("\n\n⚠️ Atenção: a troca gerará %1 choque(s):\n").arg(swap_confs.size());
                for (size_t i = 0; i < std::min<size_t>(swap_confs.size(), 5); ++i) {
                    swap_warn += "• " + QString::fromStdString(swap_confs[i].detail) + "\n";
                }
                if (swap_confs.size() > 5) {
                    swap_warn += QString("... e mais %1 outro(s).\n").arg(swap_confs.size() - 5);
                }
            }

            QMessageBox box(this);
            box.setWindowTitle("Trocar ou Mover Aula");
            box.setIcon(swap_confs.empty() ? QMessageBox::Question : QMessageBox::Warning);
            box.setText(QString("O horário de destino (%1 às %2) já possui a aula:\n\n"
                                "  📌 %3%4\n\n"
                                "Deseja trocar os horários entre as duas aulas (Swap) ou cancelar?")
                        .arg(QString::fromStdString(model::dayToLongString(target_day)))
                        .arg(QString::fromStdString(target_interval.toString()))
                        .arg(QString::fromStdString(target_aloc->disciplina_nome))
                        .arg(swap_warn));

            auto* btn_swap = box.addButton("🔄 Trocar Horários (Swap)", QMessageBox::ActionRole);
            auto* btn_force = box.addButton("➡️ Alocar Juntas", QMessageBox::ActionRole);
            auto* btn_cancel = box.addButton("Cancelar", QMessageBox::RejectRole);
            box.setDefaultButton(btn_swap);
            box.exec();

            if (box.clickedButton() == btn_cancel) return;
            if (box.clickedButton() != btn_swap) {
                target_aid.clear();
            }
        }
    }

    if (!target_aid.empty()) {
        auto* target_aloc = data_.grade().findAula(target_aid);
        auto* target_model = data_.findAula(target_aid);
        if (target_aloc) {
            auto old_dia = a_aloc->dia;
            auto old_inter = a_aloc->intervalo;

            // Aula A -> Target
            a_aloc->dia = target_day;
            a_aloc->intervalo = model::TimeInterval(target_interval.start_min, target_interval.start_min + a_aloc->intervalo.durationMinutes());
            a_aloc->is_pinned = true;
            a_model->setPinned(true);
            a_model->setPinnedDay(a_aloc->dia);
            a_model->setPinnedInterval(a_aloc->intervalo);
            a_model->setPinnedRooms(a_aloc->salas);

            // Aula B -> Old
            target_aloc->dia = old_dia;
            target_aloc->intervalo = model::TimeInterval(old_inter.start_min, old_inter.start_min + target_aloc->intervalo.durationMinutes());
            target_aloc->is_pinned = true;
            if (target_model) {
                target_model->setPinned(true);
                target_model->setPinnedDay(target_aloc->dia);
                target_model->setPinnedInterval(target_aloc->intervalo);
                target_model->setPinnedRooms(target_aloc->salas);
            }

            data_.grade().addMensagemDiagnostico("[MANUAL] Troca de horários: Aula " + aid + " (" + a_aloc->disciplina_nome + ") trocada com Aula " + target_aid + " (" + target_aloc->disciplina_nome + ").");

            selected_aula_id_ = QString::fromStdString(aid);
            renderTimetable();
            emit timetableModified();
            emit lessonMoved(QString::fromStdString(aid), QString("Trocada com %1").arg(QString::fromStdString(target_aloc->disciplina_nome)));
            return;
        }
    }

    // Slot vazio ou "Alocar Juntas"
    model::TimeInterval new_interval(target_interval.start_min, target_interval.start_min + a_aloc->intervalo.durationMinutes());
    auto conflitos = verificarConflitosMovimentacao(aid, target_day, new_interval);
    if (!conflitos.empty()) {
        QString msg = QString("Foram detectados %1 conflito(s) com as restrições cadastradas ao movimentar esta aula:\n\n").arg(conflitos.size());
        for (size_t i = 0; i < std::min<size_t>(conflitos.size(), 8); ++i) {
            msg += "• " + QString::fromStdString(conflitos[i]) + "\n";
        }
        if (conflitos.size() > 8) {
            msg += QString("... e mais %1 outro(s).\n").arg(conflitos.size() - 8);
        }
        msg += "\n⚠️ As aulas que geraram conflito serão destacadas em cor VERMELHA na grade horária.\n\nDeseja permissão para confirmar a alocação mesmo com os conflitos informados?";
        auto resp = QMessageBox::question(this, "Permissão: Conflito de Restrições", msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (resp != QMessageBox::Yes) return;
    }

    int mode = combo_view_type_->currentData().toInt();
    if (mode == 2 && combo_target_->currentIndex() >= 0) {
        std::string room_id = combo_target_->currentData().toString().toStdString();
        if (!room_id.empty()) {
            a_aloc->salas = { room_id };
        }
    }

    a_aloc->dia = target_day;
    a_aloc->intervalo = new_interval;
    a_aloc->is_pinned = true;
    a_model->setPinned(true);
    a_model->setPinnedDay(a_aloc->dia);
    a_model->setPinnedInterval(a_aloc->intervalo);
    a_model->setPinnedRooms(a_aloc->salas);

    std::string diag_entry = "[MANUAL] Aula '" + a_aloc->disciplina_nome + "' (" + aid + ") movida manualmente para " + model::dayToLongString(target_day) + " às " + a_aloc->intervalo.toString() + ".";
    if (!conflitos.empty()) {
        diag_entry += "\n   ↳ Conflito(s) confirmado(s) pelo usuário:";
        for (const auto& c : conflitos) {
            diag_entry += "\n     • " + c;
        }
    }
    data_.grade().addMensagemDiagnostico(diag_entry);

    selected_aula_id_ = QString::fromStdString(aid);
    renderTimetable();
    emit timetableModified();
    emit lessonMoved(QString::fromStdString(aid), QString::fromStdString(model::dayToLongString(target_day) + " às " + a_aloc->intervalo.toString()));
}

/**
 * @brief Trata a seleção individual de uma aula pelo usuário na grade horária.
 * @param aula_id ID da aula selecionada.
 */
void TimetableView::onLessonSelected(const QString& aula_id) {
    if (selected_aula_id_ == aula_id) {
        return;
    }
    selected_aula_id_ = aula_id;
    renderTimetable();
}

void TimetableView::onEmptyCellClicked() {
    if (selected_aula_id_.isEmpty()) {
        return;
    }
    selected_aula_id_.clear();
    renderTimetable();
}

QString TimetableView::formatShortConflictTags(const std::vector<model::SlotConflictInfo>& infos, int max_tags) const {
    QStringList tags;
    for (const auto& info : infos) {
        QString label = QString::fromStdString(info.short_label);
        if (!label.isEmpty() && !tags.contains(label)) {
            tags << label;
        }
    }
    if (tags.isEmpty()) return {};
    if (tags.size() > max_tags) {
        int extra = static_cast<int>(tags.size()) - max_tags;
        return tags.mid(0, max_tags).join(" | ") + QString(" (+%1)").arg(extra);
    }
    return tags.join(" | ");
}

void TimetableView::updateConflictSummaryPill(
    const std::map<std::string, std::vector<model::SlotConflictInfo>>& conflitos_map) {
    if (!btn_conflict_summary_) return;

    if (conflitos_map.empty()) {
        btn_conflict_summary_->setText("✅ Sem choques");
        btn_conflict_summary_->setStyleSheet(
            "QPushButton { background-color: rgba(22, 163, 74, 0.14); color: #16a34a; "
            "border: 1px solid rgba(22, 163, 74, 0.35); border-radius: 10px; padding: 3px 10px; "
            "font-size: 11px; font-weight: bold; }");
        btn_conflict_summary_->setToolTip("Nenhum choque ou violação de restrição detectada na grade atual.");
    } else {
        int n = static_cast<int>(conflitos_map.size());
        btn_conflict_summary_->setText(QString("⚠️ %1 %2 com choque").arg(n).arg(n == 1 ? "aula" : "aulas"));
        btn_conflict_summary_->setStyleSheet(
            "QPushButton { background-color: rgba(220, 38, 38, 0.16); color: #dc2626; "
            "border: 1px solid rgba(220, 38, 38, 0.45); border-radius: 10px; padding: 3px 10px; "
            "font-size: 11px; font-weight: bold; }"
            "QPushButton:hover { background-color: rgba(220, 38, 38, 0.25); }");
        btn_conflict_summary_->setToolTip("Clique para visualizar a lista de choques e navegar até a aula.");
    }
}

void TimetableView::onShowConflictsMenu() {
    auto conflitos_map = data_.verificarConflitosGradeDetalhado();
    if (conflitos_map.empty()) {
        QMessageBox::information(this, "Auditoria de Choques",
            "Nenhum choque de turma, professor, sala ou indisponibilidade foi detectado na grade atual.");
        return;
    }

    QMenu menu(this);
    menu.setTitle("Choques Ativos na Grade:");
    for (const auto& [aid, infos] : conflitos_map) {
        const auto* aloc = data_.grade().findAula(aid);
        if (!aloc) continue;
        QString t_name;
        if (!aloc->turmas.empty()) {
            const auto* t = data_.findTurma(aloc->turmas.front());
            t_name = QString::fromStdString(t ? t->nome() : aloc->turmas.front());
        }
        QString entry = QString("⚠️ [%1] %2 (%3 %4) — %5")
            .arg(t_name.isEmpty() ? "Turma" : t_name,
                 QString::fromStdString(aloc->disciplina_nome),
                 QString::fromStdString(aloc->diaString()),
                 QString::fromStdString(aloc->horarioFormatado()),
                 formatShortConflictTags(infos, 2));
        auto* act = menu.addAction(entry);
        QString aid_q = QString::fromStdString(aid);
        QString first_turma = aloc->turmas.empty() ? QString() : QString::fromStdString(aloc->turmas.front());
        connect(act, &QAction::triggered, this, [this, aid_q, first_turma]() {
            if (search_lesson_ && !search_lesson_->text().isEmpty()) {
                search_lesson_->clear();
            }
            if (!first_turma.isEmpty()) {
                combo_view_type_->blockSignals(true);
                combo_view_type_->setCurrentIndex(0);
                combo_view_type_->blockSignals(false);
                populateTargetCombo();
                int idx = combo_target_->findData(first_turma);
                if (idx >= 0) {
                    combo_target_->blockSignals(true);
                    combo_target_->setCurrentIndex(idx);
                    combo_target_->blockSignals(false);
                }
            }
            selected_aula_id_ = aid_q;
            renderTimetable();
        });
    }
    menu.exec(btn_conflict_summary_->mapToGlobal(QPoint(0, btn_conflict_summary_->height())));
}

/**
 * @brief Abre o diálogo de permuta direta de horários (Swap) entre a aula e outra aula da grade.
 * @param aula_id ID da aula de origem.
 */
void TimetableView::trocarAulaManual(const QString& aula_id) {
    if (data_.grade().alocacoes().size() < 2) {
        QMessageBox::information(this, "Aulas Insuficientes",
            "É necessário ter pelo menos 2 aulas alocadas na grade para realizar a permuta (troca de horários).");
        return;
    }

    std::string initial_id = aula_id.toStdString();
    if (initial_id.empty() && !selected_aula_id_.isEmpty()) {
        initial_id = selected_aula_id_.toStdString();
    }

    SwapLessonDialog dlg(data_, initial_id, this);
    if (dlg.exec() == QDialog::Accepted) {
        std::string aid_a = dlg.aulaOrigemId();
        std::string aid_b = dlg.aulaDestinoId();

        auto* aloc_a = data_.grade().findAula(aid_a);
        auto* aloc_b = data_.grade().findAula(aid_b);
        auto* model_a = data_.findAula(aid_a);
        auto* model_b = data_.findAula(aid_b);

        if (!aloc_a || !aloc_b) return;

        auto dia_a = aloc_a->dia;
        auto inter_a = aloc_a->intervalo;
        auto dia_b = aloc_b->dia;
        auto inter_b = aloc_b->intervalo;

        // Troca os horários
        aloc_a->dia = dia_b;
        aloc_a->intervalo = model::TimeInterval(inter_b.start_min, inter_b.start_min + aloc_a->intervalo.durationMinutes());

        aloc_b->dia = dia_a;
        aloc_b->intervalo = model::TimeInterval(inter_a.start_min, inter_a.start_min + aloc_b->intervalo.durationMinutes());

        if (dlg.isPinned()) {
            aloc_a->is_pinned = true;
            aloc_b->is_pinned = true;
            if (model_a) {
                model_a->setPinned(true);
                model_a->setPinnedDay(aloc_a->dia);
                model_a->setPinnedInterval(aloc_a->intervalo);
                model_a->setPinnedRooms(aloc_a->salas);
            }
            if (model_b) {
                model_b->setPinned(true);
                model_b->setPinnedDay(aloc_b->dia);
                model_b->setPinnedInterval(aloc_b->intervalo);
                model_b->setPinnedRooms(aloc_b->salas);
            }
        }

        data_.grade().addMensagemDiagnostico(
            "[MANUAL] Permuta (Swap) realizada entre Aula " + aid_a + " (" + aloc_a->disciplina_nome + ") e Aula " +
            aid_b + " (" + aloc_b->disciplina_nome + ").");

        selected_aula_id_ = QString::fromStdString(aid_a);
        renderTimetable();
        emit timetableModified();
        emit lessonMoved(QString::fromStdString(aid_a),
                         QString("Trocada com %1").arg(QString::fromStdString(aloc_b->disciplina_nome)));

        QMessageBox::information(this, "Troca Realizada",
            QString("A permuta de horários entre as aulas foi concluída com sucesso:\n\n"
                    "• %1 agora está em %2 às %3\n"
                    "• %4 agora está em %5 às %6")
                .arg(QString::fromStdString(aloc_a->disciplina_nome),
                     QString::fromStdString(aloc_a->diaString()),
                     QString::fromStdString(aloc_a->horarioFormatado()),
                     QString::fromStdString(aloc_b->disciplina_nome),
                     QString::fromStdString(aloc_b->diaString()),
                     QString::fromStdString(aloc_b->horarioFormatado())));
    }
}

} // namespace scheduler::ui
