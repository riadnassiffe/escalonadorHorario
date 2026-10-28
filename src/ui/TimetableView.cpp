/**
 * @file TimetableView.cpp
 * @brief Implementação da visualização interativa e gestão visual da grade semanal.
 */

#include "ui/TimetableView.h"
#include "ui/SmartTableWidgetItem.h"
#include "ui/ModernTheme.h"
#include "ui/MoveLessonDialog.h"
#include "ui/ChangeRoomDialog.h"
#include "ui/AddLessonToScheduleDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
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

    // Barra superior de seleção
    auto* top_panel = new QHBoxLayout();
    top_panel->setSpacing(10);

    auto* lbl_mode = new QLabel("Modo de Exibição:", this);
    lbl_mode->setStyleSheet("font-weight: bold; color: palette(highlight);");
    top_panel->addWidget(lbl_mode);

    combo_view_type_ = new QComboBox(this);
    combo_view_type_->addItem("Por Turma", 0);
    combo_view_type_->addItem("Por Professor", 1);
    combo_view_type_->addItem("Por Sala", 2);
    combo_view_type_->setMinimumWidth(160);
    top_panel->addWidget(combo_view_type_);

    auto* lbl_entity = new QLabel("Selecionar Alvo:", this);
    lbl_entity->setStyleSheet("font-weight: bold; color: palette(highlight); margin-left: 10px;");
    top_panel->addWidget(lbl_entity);

    combo_target_ = new QComboBox(this);
    combo_target_->setMinimumWidth(260);
    top_panel->addWidget(combo_target_);

    auto* lbl_semestre = new QLabel("Semestre:", this);
    lbl_semestre->setStyleSheet("font-weight: bold; color: palette(highlight); margin-left: 10px;");
    top_panel->addWidget(lbl_semestre);

    combo_semestre_filtro_ = new QComboBox(this);
    combo_semestre_filtro_->addItem("Todos os Semestres (Geral)", 0);
    combo_semestre_filtro_->addItem("1º Semestre", 1);
    combo_semestre_filtro_->addItem("2º Semestre", 2);
    combo_semestre_filtro_->setMinimumWidth(190);
    top_panel->addWidget(combo_semestre_filtro_);

    top_panel->addStretch();

    lbl_info_banner_ = new QLabel(this);
    lbl_info_banner_->setStyleSheet("font-size: 12px;");
    top_panel->addWidget(lbl_info_banner_);

    top_panel->addSpacing(15);

    search_lesson_ = new QLineEdit(this);
    search_lesson_->setPlaceholderText("🔍 Destacar aula/prof/sala...");
    search_lesson_->setClearButtonEnabled(true);
    search_lesson_->setFixedWidth(220);
    search_lesson_->setToolTip("Digite para localizar e destacar aulas na grade semanal");
    top_panel->addWidget(search_lesson_);

    lbl_search_count_ = new QLabel(this);
    lbl_search_count_->setStyleSheet(
        "background-color: palette(base); color: palette(window-text); border: 1px solid palette(mid); "
        "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: 500;");
    lbl_search_count_->setVisible(false);
    top_panel->addWidget(lbl_search_count_);

    top_panel->addSpacing(10);

    btn_add_lesson_ = new QPushButton("➕ Alocar Disciplina...", this);
    btn_add_lesson_->setObjectName("btnPrimary");
    btn_add_lesson_->setCursor(Qt::PointingHandCursor);
    btn_add_lesson_->setToolTip("Adiciona uma ocorrência de disciplina manualmente na grade semanal");
    top_panel->addWidget(btn_add_lesson_);

    btn_clear_timetable_ = new QPushButton("🗑️ Limpar Grade...", this);
    btn_clear_timetable_->setObjectName("btnSecondary");
    btn_clear_timetable_->setCursor(Qt::PointingHandCursor);
    btn_clear_timetable_->setToolTip("Remove todas as alocações da grade semanal para iniciar uma nova configuração inicial");
    top_panel->addWidget(btn_clear_timetable_);

    main_layout->addLayout(top_panel);

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
    connect(search_lesson_, &QLineEdit::textChanged,
            this, &TimetableView::onSearchLessonChanged);
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
    }

    combo_target_->blockSignals(false);
    renderTimetable();
}

/**
 * @brief Atualiza os alvos e redesenha a grade na tela.
 */
void TimetableView::refreshView() {
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
 * @brief Renderiza visualmente as células da grade com os horários, intervalos e aulas alocadas.
 */
void TimetableView::renderTimetable() {
    table_->setRowCount(0);

    QString target_id = combo_target_->currentData().toString();
    if (target_id.isEmpty()) {
        lbl_info_banner_->setText("Nenhum item selecionado.");
        return;
    }

    int mode = combo_view_type_->currentData().toInt();
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

            // Adicionar intervalos da turma para todos os dias letivos
            for (auto d : turma->diasLetivos()) {
                auto j_dia = turma->jornadaParaDia(d);
                for (const auto& inter : j_dia.intervalosAtivos()) {
                    horarios_set.insert({inter.start_slot, inter.end_slot});
                }
            }

            // Adicionar períodos letivos da turma para exibir slots vazios na grade
            int dur_slots = 10; // 50 minutos padrão
            for (const auto& d : data_.disciplinas()) {
                if (std::find(d.turmas().begin(), d.turmas().end(), id_str) != d.turmas().end()) {
                    if (d.duracaoSlots() > 0) {
                        dur_slots = d.duracaoSlots();
                        break;
                    }
                }
            }
            for (auto d : turma->diasLetivos()) {
                auto j_dia = turma->jornadaParaDia(d);
                for (const auto& b : j_dia.blocosLetivos()) {
                    for (int s = b.start_slot; s + dur_slots <= b.end_slot; s += dur_slots) {
                        horarios_set.insert({s, s + dur_slots});
                    }
                }
            }
        }
        for (const auto& a : data_.grade().alocacoes()) {
            if (match_semestre(a) && std::find(a.turmas.begin(), a.turmas.end(), id_str) != a.turmas.end()) {
                horarios_set.insert({a.intervalo.start_slot, a.intervalo.end_slot});
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
                horarios_set.insert({a.intervalo.start_slot, a.intervalo.end_slot});
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
                horarios_set.insert({a.intervalo.start_slot, a.intervalo.end_slot});
            }
        }
    }

    row_intervals_.clear();
    if (horarios_set.empty()) {
        table_->setRowCount(1);
        auto* empty_item = new QTableWidgetItem("Nenhuma aula gerada ou alocada para este registro neste semestre.");
        empty_item->setTextAlignment(Qt::AlignCenter);
        empty_item->setForeground(QColor("#94a3b8"));
        table_->setItem(0, 0, empty_item);
        table_->setSpan(0, 0, 1, 5);
        return;
    }

    for (const auto& [start_s, end_s] : horarios_set) {
        row_intervals_.emplace_back(start_s, end_s);
    }

    table_->setRowCount(static_cast<int>(horarios_set.size()));
    QStringList vertical_headers;

    int row = 0;
    for (const auto& [start_s, end_s] : horarios_set) {
        model::TimeInterval row_inter(start_s, end_s);
        vertical_headers << QString::fromStdString(row_inter.toString());

        for (int col = 0; col < 5; ++col) {
            auto dia = static_cast<model::DayOfWeek>(col);

            bool is_recess_for_day = false;
            bool is_outside_day_jornada = false;
            bool is_day_off = false;

            if (mode == 0) {
                const auto* turma = data_.findTurma(id_str);
                if (turma) {
                    if (!turma->funcionaNoDia(dia)) {
                        is_day_off = true;
                    } else {
                        auto j_dia = turma->jornadaParaDia(dia);
                        for (const auto& inter : j_dia.intervalosAtivos()) {
                            if (start_s == inter.start_slot && end_s == inter.end_slot) {
                                is_recess_for_day = true;
                                break;
                            }
                        }
                        if (!is_recess_for_day) {
                            if (start_s < j_dia.jornada_total.start_slot || end_s > j_dia.jornada_total.end_slot) {
                                is_outside_day_jornada = true;
                            }
                        }
                    }
                }
            }

            if (is_recess_for_day) {
                int dur = (end_s - start_s) * model::SLOT_MINUTES;
                auto* item = new QTableWidgetItem(QString("☕ INTERVALO (%1 min)").arg(dur));
                item->setTextAlignment(Qt::AlignCenter);
                item->setBackground(QColor(245, 158, 11, 40));
                item->setForeground(QColor("#f59e0b"));
                item->setFont(QFont("Segoe UI", 10, QFont::Bold));
                table_->setItem(row, col, item);
                continue;
            }

            if (is_day_off) {
                auto* item = new QTableWidgetItem("—");
                item->setToolTip("Dia não letivo para a turma");
                item->setTextAlignment(Qt::AlignCenter);
                item->setForeground(QColor("#475569"));
                item->setBackground(QColor(15, 23, 42, 60));
                table_->setItem(row, col, item);
                continue;
            }

            if (is_outside_day_jornada) {
                auto* item = new QTableWidgetItem("—");
                item->setToolTip("Fora da jornada letiva da turma neste dia");
                item->setTextAlignment(Qt::AlignCenter);
                item->setForeground(QColor("#475569"));
                item->setBackground(QColor(15, 23, 42, 60));
                table_->setItem(row, col, item);
                continue;
            }

            // Buscar se tem aula nesse dia e horário
            std::vector<const model::AllocatedLesson*> found_aulas;
            for (const auto& a : data_.grade().alocacoes()) {
                if (a.dia == dia && a.intervalo.start_slot == start_s && match_semestre(a)) {
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
                    }
                }
            }

            if (!found_aulas.empty()) {
                QStringList aula_blocks;
                QString main_aula_id;
                bool has_lab = false;
                bool has_special = false;

                for (const auto* aula : found_aulas) {
                    if (main_aula_id.isEmpty()) {
                        main_aula_id = QString::fromStdString(aula->aula_id);
                    }
                    if (aula->tipo == "LABORATORIO") has_lab = true;
                    if (aula->tipo == "ESPECIAL") has_special = true;

                    QString reg_tag;
                    if (aula->regime == model::RegimeDisciplina::ANUAL) {
                        reg_tag = "[Anual]";
                    } else if (aula->semestre == model::SemestreLetivo::SEMESTRE_1 || aula->semestre_alocado == 1) {
                        reg_tag = "[1º Sem]";
                    } else {
                        reg_tag = "[2º Sem]";
                    }

                    QString title = QString("%1 %2").arg(reg_tag, QString::fromStdString(aula->disciplina_nome));
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
                    }

                    aula_blocks << QString("%1\n%2\n%3").arg(title, line2, line3);
                }

                QString cell_text = aula_blocks.join("\n───────────────\n");
                auto* item = new QTableWidgetItem(cell_text);
                item->setData(Qt::UserRole, main_aula_id);

                bool is_dark = ModernTheme::isDarkTheme();
                if (has_lab) {
                    item->setBackground(is_dark ? QColor(236, 72, 153, 40) : QColor(251, 207, 232, 160));
                    item->setForeground(is_dark ? QColor("#f472b6") : QColor("#9d174d"));
                } else if (has_special) {
                    item->setBackground(is_dark ? QColor(139, 92, 246, 40) : QColor(233, 213, 255, 160));
                    item->setForeground(is_dark ? QColor("#c4b5fd") : QColor("#6b21a8"));
                } else {
                    item->setBackground(is_dark ? QColor(37, 99, 235, 40) : QColor(219, 234, 254, 160));
                    item->setForeground(is_dark ? QColor("#60a5fa") : QColor("#1e40af"));
                }

                item->setData(Qt::UserRole + 1, item->background());
                item->setData(Qt::UserRole + 2, item->foreground());
                table_->setItem(row, col, item);
            } else {
                auto* item = new QTableWidgetItem("-");
                item->setTextAlignment(Qt::AlignCenter);
                bool is_dark = ModernTheme::isDarkTheme();
                item->setForeground(is_dark ? QColor("#64748b") : QColor("#94a3b8"));
                item->setData(Qt::UserRole + 1, item->background());
                item->setData(Qt::UserRole + 2, item->foreground());
                table_->setItem(row, col, item);
            }
        }
        row++;
    }

    table_->setVerticalHeaderLabels(vertical_headers);

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
        auto* add_act = menu.addAction("➕ Alocar Disciplina Neste Horário...");
        connect(add_act, &QAction::triggered, [this, row, col]() {
            alocarAulaManual(row, col);
        });

        menu.addSeparator();
        auto* clear_act = menu.addAction("🗑️ Limpar Toda a Grade...");
        connect(clear_act, &QAction::triggered, this, &TimetableView::limparGradeCompleta);

        menu.exec(table_->viewport()->mapToGlobal(pos));
        return;
    }

    std::string aid = aula_id.toStdString();
    auto* aula_aloc = data_.grade().findAula(aid);
    auto* aula_model = data_.findAula(aid);
    if (!aula_aloc || !aula_model) return;

    QMenu menu(this);

    auto* move_act = menu.addAction("🔄 Mover Horário / Sala...");
    connect(move_act, &QAction::triggered, [this, aula_id]() {
        moverAulaManual(aula_id);
    });

    auto* room_act = menu.addAction("🏢 Alterar Sala de Aula...");
    connect(room_act, &QAction::triggered, [this, aula_id]() {
        mudarSalaManual(aula_id);
    });

    menu.addSeparator();

    auto* add_act = menu.addAction("➕ Alocar Outra Disciplina na Grade...");
    connect(add_act, &QAction::triggered, [this]() {
        alocarAulaManual(-1, -1);
    });

    menu.addSeparator();

    if (aula_model->isPinned()) {
        auto* unpin_act = menu.addAction("🔓 Desafixar Horário Desta Aula");
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
        auto* pin_act = menu.addAction("📌 Fixar Horário Desta Aula (Bloquear para Reotimização)");
        connect(pin_act, &QAction::triggered, [this, aid]() {
            auto* a_model = data_.findAula(aid);
            auto* a_aloc = data_.grade().findAula(aid);
            if (a_model && a_aloc) {
                a_model->setPinned(true);
                a_model->setPinnedDay(a_aloc->dia);
                a_model->setPinnedStartSlot(a_aloc->intervalo.start_slot);
                a_model->setPinnedRooms(a_aloc->salas);
                a_aloc->is_pinned = true;
                renderTimetable();
                emit lessonPinChanged(QString::fromStdString(a_model->id()), true);
            }
        });
    }

    auto* desalocar_act = menu.addAction("❌ Desalocar Aula da Grade");
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
    auto* pin_all_act = menu.addAction("📌 Fixar Todas as Aulas da Grade Atual (Bloquear para Próximo Solver)");
    connect(pin_all_act, &QAction::triggered, [this]() {
        for (auto& aloc : data_.grade().alocacoes()) {
            auto* a_model = data_.findAula(aloc.aula_id);
            if (a_model) {
                a_model->setPinned(true);
                a_model->setPinnedDay(aloc.dia);
                a_model->setPinnedStartSlot(aloc.intervalo.start_slot);
                a_model->setPinnedRooms(aloc.salas);
                aloc.is_pinned = true;
            }
        }
        renderTimetable();
    });

    auto* unpin_all_act = menu.addAction("🔓 Desafixar Todas as Aulas da Grade Atual");
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
    auto* clear_act = menu.addAction("🗑️ Limpar Toda a Grade...");
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
    QString aid = item->data(Qt::UserRole).toString();
    if (!aid.isEmpty()) {
        moverAulaManual(aid);
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

        int target_row = -1;
        for (size_t r = 0; r < row_intervals_.size(); ++r) {
            if (row_intervals_[r].start_slot == target_interval.start_slot) {
                target_row = static_cast<int>(r);
                break;
            }
        }
        int target_col = static_cast<int>(target_day);
        if (target_row >= 0 && target_col >= 0) {
            if (a_aloc->dia != target_day || a_aloc->intervalo.start_slot != target_interval.start_slot) {
                onLessonDropRequested(aula_id, target_row, target_col);
            } else if (room_changed) {
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
    if (col >= 0 && col < static_cast<int>(model::allDays().size())) {
        default_day = model::allDays()[col];
    }
    if (row >= 0 && row < static_cast<int>(row_intervals_.size())) {
        default_interval = row_intervals_[row];
    }

    std::string def_turma, def_prof, def_sala;
    int mode = combo_view_type_->currentData().toInt();
    std::string target_id = combo_target_->currentData().toString().toStdString();
    if (mode == 0) def_turma = target_id;
    else if (mode == 1) def_prof = target_id;
    else if (mode == 2) def_sala = target_id;

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
                    nova_aula.setPinnedStartSlot(aloc.intervalo.start_slot);
                    nova_aula.setPinnedRooms(aloc.salas);
                }
                data_.aulas().push_back(std::move(nova_aula));
            }
        } else {
            if (pinned) {
                a_model->setPinned(true);
                a_model->setPinnedDay(aloc.dia);
                a_model->setPinnedStartSlot(aloc.intervalo.start_slot);
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
    const auto* a_aloc = data_.grade().findAula(aid);
    if (!a_aloc) return conflitos;

    auto time_overlaps = [](const model::TimeInterval& t1, const model::TimeInterval& t2) {
        return std::max(t1.start_slot, t2.start_slot) < std::min(t1.end_slot, t2.end_slot);
    };

    auto semestres_conflitam = [](const model::AllocatedLesson& a1, const model::AllocatedLesson& a2) {
        if (a1.regime == model::RegimeDisciplina::ANUAL || a2.regime == model::RegimeDisciplina::ANUAL) return true;
        int s1 = (a1.semestre == model::SemestreLetivo::SEMESTRE_2 || a1.semestre_alocado == 2) ? 2 : 1;
        int s2 = (a2.semestre == model::SemestreLetivo::SEMESTRE_2 || a2.semestre_alocado == 2) ? 2 : 1;
        return s1 == s2;
    };

    // 1. Verificar turmas
    for (const auto& t_id : a_aloc->turmas) {
        const auto* t = data_.findTurma(t_id);
        if (t) {
            if (!t->funcionaNoDia(dia)) {
                conflitos.push_back("A turma " + t->nome() + " não possui aulas na " + model::dayToLongString(dia) + ".");
            }
            auto j = t->jornadaParaDia(dia);
            for (const auto& inter : j.intervalosAtivos()) {
                if (time_overlaps(inter, intervalo)) {
                    conflitos.push_back("O horário sobrepõe o intervalo (" + inter.toString() + ") da turma " + t->nome() + ".");
                }
            }
            if (intervalo.start_slot < j.jornada_total.start_slot || intervalo.end_slot > j.jornada_total.end_slot) {
                conflitos.push_back("O horário está fora da jornada total da turma " + t->nome() + " (" + j.jornada_total.toString() + ").");
            }
        }

        // Choque com outras aulas da mesma turma
        for (const auto& a : data_.grade().alocacoes()) {
            if (a.aula_id != aid && a.dia == dia && semestres_conflitam(*a_aloc, a) && time_overlaps(a.intervalo, intervalo)) {
                if (std::find(a.turmas.begin(), a.turmas.end(), t_id) != a.turmas.end()) {
                    conflitos.push_back("Choque de turma: A aula '" + a_aloc->disciplina_nome + "' (" + aid + ") conflita com a aula '" + a.disciplina_nome + "' (" + a.aula_id + ") da Turma " + (t ? t->nome() : t_id) + ".");
                }
            }
        }

        // Limite diário de aulas da disciplina na turma
        if (data_.config().distribuir_disciplinas_na_semana) {
            int count_dia = 0;
            std::string alocadas_str;
            for (const auto& a : data_.grade().alocacoes()) {
                if (a.aula_id != aid && a.dia == dia && a.disciplina_id == a_aloc->disciplina_id && semestres_conflitam(*a_aloc, a)) {
                    if (std::find(a.turmas.begin(), a.turmas.end(), t_id) != a.turmas.end()) {
                        count_dia++;
                        if (!alocadas_str.empty()) alocadas_str += ", ";
                        alocadas_str += "'" + a.disciplina_nome + "' (" + a.aula_id + " às " + a.horarioFormatado() + ")";
                    }
                }
            }
            const auto* disc = data_.findDisciplina(a_aloc->disciplina_id);
            int max_lim = (disc && disc->maxAulasPorDiaTurma() > 0) ? disc->maxAulasPorDiaTurma() : data_.config().max_aulas_disciplina_dia_turma;
            if (count_dia + 1 > max_lim) {
                conflitos.push_back("Limite diário: A aula '" + a_aloc->disciplina_nome + "' (" + aid + ") excederá o limite de " + std::to_string(max_lim) + " aulas/dia na Turma " + (t ? t->nome() : t_id) + " (aulas já alocadas hoje: " + alocadas_str + ").");
            }
        }
    }

    // 2. Verificar professores
    for (const auto& p_id : a_aloc->professores) {
        const auto* p = data_.findProfessor(p_id);
        if (p) {
            if (!p->isDisponivelNoIntervalo(dia, intervalo.start_slot, intervalo.end_slot)) {
                conflitos.push_back("Disponibilidade: O Prof. " + p->nome() + " possui indisponibilidade cadastrada neste horário na " + model::dayToLongString(dia) + ".");
            }
        }
        for (const auto& a : data_.grade().alocacoes()) {
            if (a.aula_id != aid && a.dia == dia && semestres_conflitam(*a_aloc, a) && time_overlaps(a.intervalo, intervalo)) {
                if (std::find(a.professores.begin(), a.professores.end(), p_id) != a.professores.end()) {
                    conflitos.push_back("Choque de professor: A aula '" + a_aloc->disciplina_nome + "' (" + aid + ") conflita com a aula '" + a.disciplina_nome + "' (" + a.aula_id + ") do Prof. " + (p ? p->nome() : p_id) + ".");
                }
            }
        }
    }

    // 3. Verificar salas
    for (const auto& sala_id : a_aloc->salas) {
        const auto* s = data_.findSala(sala_id);
        for (const auto& a : data_.grade().alocacoes()) {
            if (a.aula_id != aid && a.dia == dia && semestres_conflitam(*a_aloc, a) && time_overlaps(a.intervalo, intervalo)) {
                if (std::find(a.salas.begin(), a.salas.end(), sala_id) != a.salas.end()) {
                    conflitos.push_back("Choque de sala: A aula '" + a_aloc->disciplina_nome + "' (" + aid + ") conflita com a aula '" + a.disciplina_nome + "' (" + a.aula_id + ") na Sala " + (s ? s->nome() : sala_id) + ".");
                }
            }
        }
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
    if (target_row < 0 || target_row >= static_cast<int>(row_intervals_.size())) return;
    if (target_col < 0 || target_col >= 5) return;

    auto target_day = static_cast<model::DayOfWeek>(target_col);
    const auto& target_interval = row_intervals_[target_row];

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
    if (a_aloc->dia == target_day && a_aloc->intervalo.start_slot == target_interval.start_slot) {
        return;
    }

    // Verificar se o slot destino já possui uma aula alocada para o alvo atual
    QString target_aid_q = target_item ? target_item->data(Qt::UserRole).toString() : QString();
    std::string target_aid = target_aid_q.toStdString();
    auto* target_aloc = !target_aid.empty() ? data_.grade().findAula(target_aid) : nullptr;
    auto* target_model = !target_aid.empty() ? data_.findAula(target_aid) : nullptr;

    if (target_aloc && target_aid != aid) {
        QMessageBox box(this);
        box.setWindowTitle("Trocar ou Mover Aula");
        box.setIcon(QMessageBox::Question);
        box.setText(QString("O horário de destino (%1 às %2) já possui a aula:\n\n"
                            "  📌 %3\n\n"
                            "Deseja trocar os horários entre as duas aulas (Swap) ou cancelar?")
                    .arg(QString::fromStdString(model::dayToLongString(target_day)))
                    .arg(QString::fromStdString(target_interval.toString()))
                    .arg(QString::fromStdString(target_aloc->disciplina_nome)));

        auto* btn_swap = box.addButton("🔄 Trocar Horários (Swap)", QMessageBox::ActionRole);
        auto* btn_force = box.addButton("➡️ Alocar Juntas", QMessageBox::ActionRole);
        auto* btn_cancel = box.addButton("Cancelar", QMessageBox::RejectRole);
        box.setDefaultButton(btn_swap);
        box.exec();

        if (box.clickedButton() == btn_cancel) return;

        if (box.clickedButton() == btn_swap) {
            auto old_dia = a_aloc->dia;
            auto old_inter = a_aloc->intervalo;

            // Aula A -> Target
            a_aloc->dia = target_day;
            a_aloc->intervalo = model::TimeInterval(target_interval.start_slot, target_interval.start_slot + a_aloc->intervalo.durationSlots());
            a_aloc->is_pinned = true;
            a_model->setPinned(true);
            a_model->setPinnedDay(a_aloc->dia);
            a_model->setPinnedStartSlot(a_aloc->intervalo.start_slot);
            a_model->setPinnedRooms(a_aloc->salas);

            // Aula B -> Old
            target_aloc->dia = old_dia;
            target_aloc->intervalo = model::TimeInterval(old_inter.start_slot, old_inter.start_slot + target_aloc->intervalo.durationSlots());
            target_aloc->is_pinned = true;
            if (target_model) {
                target_model->setPinned(true);
                target_model->setPinnedDay(target_aloc->dia);
                target_model->setPinnedStartSlot(target_aloc->intervalo.start_slot);
                target_model->setPinnedRooms(target_aloc->salas);
            }

            data_.grade().addMensagemDiagnostico("[MANUAL] Troca de horários: Aula " + aid + " (" + a_aloc->disciplina_nome + ") trocada com Aula " + target_aid + " (" + target_aloc->disciplina_nome + ").");

            renderTimetable();
            emit timetableModified();
            emit lessonMoved(QString::fromStdString(aid), QString("Trocada com %1").arg(QString::fromStdString(target_aloc->disciplina_nome)));
            return;
        }
    }

    // Slot vazio ou "Alocar Juntas"
    model::TimeInterval new_interval(target_interval.start_slot, target_interval.start_slot + a_aloc->intervalo.durationSlots());
    auto conflitos = verificarConflitosMovimentacao(aid, target_day, new_interval);
    if (!conflitos.empty()) {
        QString msg = "Atenção: A movimentação manual causará as seguintes incompatibilidades:\n\n";
        for (const auto& c : conflitos) {
            msg += "• " + QString::fromStdString(c) + "\n";
        }
        msg += "\nDeseja confirmar a alocação manual mesmo assim?";
        auto resp = QMessageBox::warning(this, "Aviso de Conflitos", msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (resp != QMessageBox::Yes) return;
    }

    int mode = combo_view_type_->currentIndex();
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
    a_model->setPinnedStartSlot(a_aloc->intervalo.start_slot);
    a_model->setPinnedRooms(a_aloc->salas);

    std::string diag_entry = "[MANUAL] Aula '" + a_aloc->disciplina_nome + "' (" + aid + ") movida manualmente para " + model::dayToLongString(target_day) + " às " + a_aloc->intervalo.toString() + ".";
    if (!conflitos.empty()) {
        diag_entry += "\n   ↳ Conflito(s) confirmado(s) pelo usuário:";
        for (const auto& c : conflitos) {
            diag_entry += "\n     • " + c;
        }
    }
    data_.grade().addMensagemDiagnostico(diag_entry);

    renderTimetable();
    emit timetableModified();
    emit lessonMoved(QString::fromStdString(aid), QString::fromStdString(model::dayToLongString(target_day) + " às " + a_aloc->intervalo.toString()));
}

} // namespace scheduler::ui
