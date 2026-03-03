/*
 * Copyright (c) 2013-2024 Thomas Isaac Lightburn
 *
 * This file is part of OpenKJ.
 *
 * OpenKJ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "theme.h"
#include <QApplication>

namespace AppTheme {

// Modern "Glass" QSS — deep slate/dark-blue background with vibrant green accents
// and subtly rounded, semi-transparent "glass" buttons.
static const QString kModernQss = QStringLiteral(
    /* ── Base window / widget ─────────────────────────────────────────── */
    "QWidget {"
    "  background-color: #1a1f2e;"
    "  color: #e8eaf0;"
    "  font-family: 'Segoe UI', 'Helvetica Neue', sans-serif;"
    "}"

    /* ── Main window ───────────────────────────────────────────────────── */
    "QMainWindow {"
    "  background-color: #141824;"
    "}"

    /* ── Dialogs ───────────────────────────────────────────────────────── */
    "QDialog {"
    "  background-color: #1a1f2e;"
    "}"

    /* ── Menu bar / menus ─────────────────────────────────────────────── */
    "QMenuBar {"
    "  background-color: #141824;"
    "  color: #c8cad4;"
    "  border-bottom: 1px solid #2a3050;"
    "}"
    "QMenuBar::item:selected {"
    "  background-color: #252d45;"
    "}"
    "QMenu {"
    "  background-color: #1e2538;"
    "  color: #e8eaf0;"
    "  border: 1px solid #2a3050;"
    "}"
    "QMenu::item:selected {"
    "  background-color: #26a65b;"
    "  color: #ffffff;"
    "}"

    /* ── Push buttons — glass effect ──────────────────────────────────── */
    "QPushButton {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
    "              stop:0 rgba(60,75,110,200), stop:1 rgba(35,45,75,200));"
    "  color: #e8eaf0;"
    "  border: 1px solid rgba(100,120,180,120);"
    "  border-radius: 5px;"
    "  padding: 4px 12px;"
    "  min-height: 24px;"
    "}"
    "QPushButton:hover {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
    "              stop:0 rgba(80,100,150,220), stop:1 rgba(50,65,105,220));"
    "  border-color: rgba(130,160,220,160);"
    "}"
    "QPushButton:pressed {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
    "              stop:0 rgba(30,38,65,230), stop:1 rgba(50,65,105,230));"
    "}"
    "QPushButton:disabled {"
    "  color: #555a6e;"
    "  border-color: rgba(80,90,120,80);"
    "}"

    /* ── Tab widgets ──────────────────────────────────────────────────── */
    "QTabWidget::pane {"
    "  border: 1px solid #2a3050;"
    "  background-color: #1a1f2e;"
    "}"
    "QTabBar::tab {"
    "  background-color: #1e2538;"
    "  color: #9098b0;"
    "  padding: 6px 14px;"
    "  border: 1px solid #2a3050;"
    "  border-bottom: none;"
    "  border-top-left-radius: 4px;"
    "  border-top-right-radius: 4px;"
    "  margin-right: 2px;"
    "}"
    "QTabBar::tab:selected {"
    "  background-color: #252d45;"
    "  color: #e8eaf0;"
    "  border-bottom: 2px solid #26a65b;"
    "}"
    "QTabBar::tab:hover:!selected {"
    "  background-color: #202840;"
    "}"

    /* ── Table views ──────────────────────────────────────────────────── */
    "QTableView {"
    "  background-color: #1a1f2e;"
    "  alternate-background-color: #1f2540;"
    "  color: #e8eaf0;"
    "  gridline-color: #252d45;"
    "  selection-background-color: #26a65b;"
    "  selection-color: #ffffff;"
    "  border: 1px solid #2a3050;"
    "}"
    "QTableView::item:hover {"
    "  background-color: rgba(38,166,91,40);"
    "}"
    "QHeaderView::section {"
    "  background-color: #1e2538;"
    "  color: #9098b0;"
    "  padding: 4px 6px;"
    "  border: none;"
    "  border-bottom: 1px solid #2a3050;"
    "  border-right: 1px solid #2a3050;"
    "}"
    "QHeaderView::section:first {"
    "  border-left: none;"
    "}"

    /* ── List views ───────────────────────────────────────────────────── */
    "QListView {"
    "  background-color: #1a1f2e;"
    "  alternate-background-color: #1f2540;"
    "  color: #e8eaf0;"
    "  selection-background-color: #26a65b;"
    "  selection-color: #ffffff;"
    "  border: 1px solid #2a3050;"
    "}"

    /* ── Combo boxes ──────────────────────────────────────────────────── */
    "QComboBox {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
    "              stop:0 rgba(55,70,105,200), stop:1 rgba(35,45,75,200));"
    "  color: #e8eaf0;"
    "  border: 1px solid rgba(100,120,180,120);"
    "  border-radius: 4px;"
    "  padding: 3px 8px;"
    "  min-height: 22px;"
    "}"
    "QComboBox:hover {"
    "  border-color: rgba(130,160,220,160);"
    "}"
    "QComboBox QAbstractItemView {"
    "  background-color: #1e2538;"
    "  color: #e8eaf0;"
    "  selection-background-color: #26a65b;"
    "  selection-color: #ffffff;"
    "  border: 1px solid #2a3050;"
    "}"

    /* ── Line edits / spin boxes ─────────────────────────────────────── */
    "QLineEdit, QSpinBox, QDoubleSpinBox, QFontComboBox {"
    "  background-color: #252d45;"
    "  color: #e8eaf0;"
    "  border: 1px solid #2a3050;"
    "  border-radius: 4px;"
    "  padding: 3px 6px;"
    "}"
    "QLineEdit:focus, QSpinBox:focus {"
    "  border-color: #26a65b;"
    "}"

    /* ── Check boxes / radio buttons ─────────────────────────────────── */
    "QCheckBox, QRadioButton {"
    "  color: #c8cad4;"
    "  spacing: 6px;"
    "}"
    "QCheckBox::indicator, QRadioButton::indicator {"
    "  width: 14px;"
    "  height: 14px;"
    "}"

    /* ── Group boxes ──────────────────────────────────────────────────── */
    "QGroupBox {"
    "  border: 1px solid #2a3050;"
    "  border-radius: 5px;"
    "  margin-top: 8px;"
    "  color: #9098b0;"
    "}"
    "QGroupBox::title {"
    "  subcontrol-origin: margin;"
    "  subcontrol-position: top left;"
    "  padding: 0 4px;"
    "  color: #9098b0;"
    "}"

    /* ── Scroll bars ──────────────────────────────────────────────────── */
    "QScrollBar:vertical {"
    "  background: #141824;"
    "  width: 10px;"
    "  margin: 0;"
    "}"
    "QScrollBar::handle:vertical {"
    "  background: #2a3050;"
    "  border-radius: 5px;"
    "  min-height: 20px;"
    "}"
    "QScrollBar::handle:vertical:hover {"
    "  background: #26a65b;"
    "}"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
    "  height: 0;"
    "}"
    "QScrollBar:horizontal {"
    "  background: #141824;"
    "  height: 10px;"
    "  margin: 0;"
    "}"
    "QScrollBar::handle:horizontal {"
    "  background: #2a3050;"
    "  border-radius: 5px;"
    "  min-width: 20px;"
    "}"
    "QScrollBar::handle:horizontal:hover {"
    "  background: #26a65b;"
    "}"
    "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
    "  width: 0;"
    "}"

    /* ── Sliders ──────────────────────────────────────────────────────── */
    "QSlider::groove:horizontal {"
    "  background: #252d45;"
    "  height: 4px;"
    "  border-radius: 2px;"
    "}"
    "QSlider::handle:horizontal {"
    "  background: #26a65b;"
    "  border: none;"
    "  width: 14px;"
    "  height: 14px;"
    "  border-radius: 7px;"
    "  margin: -5px 0;"
    "}"
    "QSlider::sub-page:horizontal {"
    "  background: #26a65b;"
    "  border-radius: 2px;"
    "}"

    /* ── Status bar ───────────────────────────────────────────────────── */
    "QStatusBar {"
    "  background-color: #141824;"
    "  color: #9098b0;"
    "  border-top: 1px solid #2a3050;"
    "}"

    /* ── Tool tip ─────────────────────────────────────────────────────── */
    "QToolTip {"
    "  background-color: #1e2538;"
    "  color: #e8eaf0;"
    "  border: 1px solid #2a3050;"
    "  padding: 3px;"
    "}"

    /* ── Splitter ─────────────────────────────────────────────────────── */
    "QSplitter::handle {"
    "  background-color: #2a3050;"
    "}"
);

QString stylesheetForTheme(ThemeId id)
{
    switch (id) {
    case ThemeId::Modern:
        return kModernQss;
    default:
        return {};
    }
}

void applyTheme(ThemeId id)
{
    if (auto *app = qApp)
        app->setStyleSheet(stylesheetForTheme(id));
}

} // namespace AppTheme
