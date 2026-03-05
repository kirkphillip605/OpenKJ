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

#ifdef USE_QLEMENTINE
#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/ThemeManager.hpp>
#include <oclero/qlementine/icons/QlementineIcons.hpp>
#endif

#include <QStyleFactory>

namespace AppTheme {

struct ThemeEntry {
    QString displayName;
    QString resourcePath;
    bool    dark;
};

static const ThemeEntry kThemeTable[] = {
    { QStringLiteral("Light"),         QStringLiteral(":/themes/themes/light.json"),         false },
    { QStringLiteral("Dark"),          QStringLiteral(":/themes/themes/dark.json"),          true  },
    { QStringLiteral("Midnight Blue"), QStringLiteral(":/themes/themes/midnight-blue.json"), true  },
    { QStringLiteral("Charcoal"),      QStringLiteral(":/themes/themes/charcoal.json"),      true  },
    { QStringLiteral("Forest"),        QStringLiteral(":/themes/themes/forest.json"),        true  },
    { QStringLiteral("Amber"),         QStringLiteral(":/themes/themes/amber.json"),         true  },
};

static constexpr int kThemeCount = static_cast<int>(sizeof(kThemeTable) / sizeof(kThemeTable[0]));

#ifdef USE_QLEMENTINE
static oclero::qlementine::QlementineStyle *sStyle = nullptr;
#endif

QStringList availableThemeNames()
{
    QStringList names;
    names.reserve(kThemeCount);
    for (int i = 0; i < kThemeCount; ++i)
        names << kThemeTable[i].displayName;
    return names;
}

QString themeJsonPath(int index)
{
    if (index < 0 || index >= kThemeCount)
        index = 0;
    return kThemeTable[index].resourcePath;
}

bool isDarkTheme(int index)
{
    if (index < 0 || index >= kThemeCount)
        return false;
    return kThemeTable[index].dark;
}

void initializeStyle(int themeIndex)
{
    if (themeIndex < 0 || themeIndex >= kThemeCount)
        themeIndex = DefaultThemeIndex;

#ifdef USE_QLEMENTINE
    auto *style = new oclero::qlementine::QlementineStyle(qApp);
    style->setAnimationsEnabled(true);
    style->setAutoIconColor(oclero::qlementine::AutoIconColor::TextColor);
    style->setThemeJsonPath(kThemeTable[themeIndex].resourcePath);
    qApp->setStyle(style);
    sStyle = style;

    oclero::qlementine::icons::initializeIconTheme();
#else
    // Qt 5 fallback: use Fusion style with a dark palette when a dark theme is selected.
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    if (isDarkTheme(themeIndex)) {
        QPalette pal;
        pal.setColor(QPalette::Window,          QColor(53, 53, 53));
        pal.setColor(QPalette::WindowText,      Qt::white);
        pal.setColor(QPalette::Base,            QColor(42, 42, 42));
        pal.setColor(QPalette::AlternateBase,   QColor(66, 66, 66));
        pal.setColor(QPalette::ToolTipBase,     Qt::white);
        pal.setColor(QPalette::ToolTipText,     QColor(53, 53, 53));
        pal.setColor(QPalette::Text,            Qt::white);
        pal.setColor(QPalette::Button,          QColor(53, 53, 53));
        pal.setColor(QPalette::ButtonText,      Qt::white);
        pal.setColor(QPalette::BrightText,      Qt::red);
        pal.setColor(QPalette::Link,            QColor(42, 130, 218));
        pal.setColor(QPalette::Highlight,       QColor(42, 130, 218));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        qApp->setPalette(pal);
    }
#endif
}

void applyTheme(int themeIndex)
{
    if (themeIndex < 0 || themeIndex >= kThemeCount)
        themeIndex = DefaultThemeIndex;

#ifdef USE_QLEMENTINE
    if (sStyle) {
        sStyle->setThemeJsonPath(kThemeTable[themeIndex].resourcePath);
    }
#else
    Q_UNUSED(themeIndex);
#endif
}

} // namespace AppTheme
