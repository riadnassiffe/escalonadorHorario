#include "ui/TimetableView.h"
#include "ui/SmartTableWidgetItem.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <set>

namespace scheduler::ui {

TimetableView::TimetableView(model::InstitutionData& data, QWidget* parent)
    : QWidget(parent), data_(data) {
    setupUi();
}

void TimetableView::setupUi() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(12);

    // Barra superior de seleção
    auto* top_panel = new QHBoxLayout();
    top_panel->setSpacing(10);

    auto* lbl_mode = new QLabel("Modo de Exibição:", this);
    lbl_mode->setStyleSheet("font-weight: bold; color: #38bdf8;");
    top_panel->addWidget(lbl_mode);

    combo_view_type_ = new QComboBox(this);
    combo_view_type_->addItem("Por Turma", 0);
    combo_view_type_->addItem("Por Professor", 1);
    combo_view_type_->addItem("Por Sala", 2);
    combo_view_type_->setMinimumWidth(160);
    top_panel->addWidget(combo_view_type_);

    auto* lbl_entity = new QLabel("Selecionar Alvo:", this);
    lbl_entity->setStyleSheet("font-weight: bold; color: #38bdf8; margin-left: 10px;");
    top_panel->addWidget(lbl_entity);

    combo_target_ = new QComboBox(this);
    combo_target_->setMinimumWidth(260);
    top_panel->addWidget(combo_target_);

    auto* lbl_semestre = new QLabel("Semestre:", this);
    lbl_semestre->setStyleSheet("font-weight: bold; color: #38bdf8; margin-left: 10px;");
    top_panel->addWidget(lbl_semestre);

    combo_semestre_filtro_ = new QComboBox(this);
    combo_semestre_filtro_->addItem("Todos os Semestres (Geral)", 0);
    combo_semestre_filtro_->addItem("1º Semestre", 1);
    combo_semestre_filtro_->addItem("2º Semestre", 2);
    combo_semestre_filtro_->setMinimumWidth(190);
    top_panel->addWidget(combo_semestre_filtro_);

    top_panel->addStretch();

    lbl_info_banner_ = new QLabel(this);
    lbl_info_banner_->setStyleSheet("color: #94a3b8; font-size: 12px;");
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
        "background-color: #1e293b; color: #94a3b8; border: 1px solid #334155; "
        "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: 500;");
    lbl_search_count_->setVisible(false);
    top_panel->addWidget(lbl_search_count_);

    main_layout->addLayout(top_panel);

    // Tabela da Grade Semanal
    table_ = new QTableWidget(this);
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
    connect(search_lesson_, &QLineEdit::textChanged,
            this, &TimetableView::onSearchLessonChanged);

    populateTargetCombo();
}

void TimetableView::populateTargetCombo() {
    combo_target_->blockSignals(true);
    combo_target_->clear();

    int mode = combo_view_type_->currentData().toInt();
    if (mode == 0) { // Por Turma
        for (const auto& t : data_.turmas()) {
            QString label = QString::fromStdString(t.nome() + " [" + t.id() + "]");
            combo_target_->addItem(label, QString::fromStdString(t.id()));
        }
    } else if (mode == 1) { // Por Professor
        for (const auto& p : data_.professores()) {
            QString label = QString::fromStdString(p.nome() + " [" + p.id() + "]");
            combo_target_->addItem(label, QString::fromStdString(p.id()));
        }
    } else if (mode == 2) { // Por Sala
        for (const auto& s : data_.salas()) {
            QString label = QString::fromStdString(s.nome() + " [" + s.id() + "]");
            combo_target_->addItem(label, QString::fromStdString(s.id()));
        }
    }

    combo_target_->blockSignals(false);
    renderTimetable();
}

void TimetableView::refreshView() {
    populateTargetCombo();
}

void TimetableView::onViewTypeChanged(int /*index*/) {
    populateTargetCombo();
}

void TimetableView::onTargetEntityChanged(int /*index*/) {
    renderTimetable();
}

void TimetableView::onSemestreFiltroChanged(int /*index*/) {
    renderTimetable();
}

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

            // Adicionar intervalos da turma
            const auto& jp = turma->jornadaPadrao();
            for (const auto& inter : jp.intervalosAtivos()) {
                horarios_set.insert({inter.start_slot, inter.end_slot});
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

    if (horarios_set.empty()) {
        table_->setRowCount(1);
        auto* empty_item = new QTableWidgetItem("Nenhuma aula gerada ou alocada para este registro neste semestre.");
        empty_item->setTextAlignment(Qt::AlignCenter);
        empty_item->setForeground(QColor("#94a3b8"));
        table_->setItem(0, 0, empty_item);
        table_->setSpan(0, 0, 1, 5);
        return;
    }

    table_->setRowCount(static_cast<int>(horarios_set.size()));
    QStringList vertical_headers;

    int row = 0;
    for (const auto& [start_s, end_s] : horarios_set) {
        model::TimeInterval row_inter(start_s, end_s);
        vertical_headers << QString::fromStdString(row_inter.toString());

        bool is_recess_row = false;
        if (mode == 0) {
            const auto* turma = data_.findTurma(id_str);
            if (turma) {
                const auto& jp = turma->jornadaPadrao();
                for (const auto& inter : jp.intervalosAtivos()) {
                    if (start_s == inter.start_slot && end_s == inter.end_slot) {
                        is_recess_row = true;
                        break;
                    }
                }
            }
        }

        for (int col = 0; col < 5; ++col) {
            auto dia = static_cast<model::DayOfWeek>(col);

            if (is_recess_row) {
                int dur = (end_s - start_s) * model::SLOT_MINUTES;
                auto* item = new QTableWidgetItem(QString("☕ INTERVALO (%1 min)").arg(dur));
                item->setTextAlignment(Qt::AlignCenter);
                item->setBackground(QColor(245, 158, 11, 40));
                item->setForeground(QColor("#f59e0b"));
                item->setFont(QFont("Segoe UI", 10, QFont::Bold));
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

                if (has_lab) {
                    item->setBackground(QColor(236, 72, 153, 35));
                    item->setForeground(QColor("#f472b6"));
                } else if (has_special) {
                    item->setBackground(QColor(139, 92, 246, 35));
                    item->setForeground(QColor("#a78bfa"));
                } else {
                    item->setBackground(QColor(37, 99, 235, 35));
                    item->setForeground(QColor("#60a5fa"));
                }

                item->setData(Qt::UserRole + 1, item->background());
                item->setData(Qt::UserRole + 2, item->foreground());
                table_->setItem(row, col, item);
            } else {
                auto* item = new QTableWidgetItem("-");
                item->setTextAlignment(Qt::AlignCenter);
                item->setForeground(QColor("#475569"));
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

            if (!clean.isEmpty()) {
                if (removeAccents(text).toLower().contains(clean)) {
                    matches++;
                    it->setBackground(QColor(245, 158, 11, 120)); // Destaque âmbar/ouro
                    it->setForeground(QColor("#fef08a"));
                } else {
                    it->setBackground(QColor(15, 23, 42, 160)); // Atenuado
                    it->setForeground(QColor("#475569"));
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
                "background-color: #450a0a; color: #fca5a5; border: 1px solid #ef4444; "
                "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
        } else {
            lbl_search_count_->setText(QString("%1 %2").arg(matches).arg(matches == 1 ? "aula encontrada" : "aulas encontradas"));
            lbl_search_count_->setStyleSheet(
                "background-color: #0c4a6e; color: #38bdf8; border: 1px solid #0284c7; "
                "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
        }
    }
}

void TimetableView::onCellContextMenu(const QPoint& pos) {
    auto* item = table_->itemAt(pos);
    if (!item) return;

    QString aula_id = item->data(Qt::UserRole).toString();
    if (aula_id.isEmpty()) return;

    std::string aid = aula_id.toStdString();
    auto* aula_aloc = data_.grade().findAula(aid);
    auto* aula_model = data_.findAula(aid);
    if (!aula_aloc || !aula_model) return;

    QMenu menu(this);
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

    menu.exec(table_->viewport()->mapToGlobal(pos));
}

} // namespace scheduler::ui
