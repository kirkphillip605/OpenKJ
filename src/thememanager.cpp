#include "thememanager.h"
#include "settings.h"

#include <QApplication>
#include <QGuiApplication>
#include <QStyleFactory>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QColorScheme>
#endif
#include <QIcon>

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {}

ThemeManager &ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

void ThemeManager::initialize(Settings *settings) {
    m_settings = settings;
    detectSystemTheme();
    Theme pref = System;
    if (m_settings)
        pref = static_cast<Theme>(m_settings->theme());
    if (pref == System) {
        // m_currentTheme already set by detection
    } else {
        m_currentTheme = (pref == Dark) ? Dark : Light;
    }

    QPalette pal;
    if (m_currentTheme == Dark) {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        pal.setColor(QPalette::Window, QColor(53,53,53));
        pal.setColor(QPalette::WindowText, Qt::white);
        pal.setColor(QPalette::Base, QColor(42,42,42));
        pal.setColor(QPalette::AlternateBase, QColor(66,66,66));
        pal.setColor(QPalette::ToolTipBase, Qt::white);
        pal.setColor(QPalette::ToolTipText, QColor(53,53,53));
        pal.setColor(QPalette::Text, Qt::white);
        pal.setColor(QPalette::Button, QColor(53,53,53));
        pal.setColor(QPalette::ButtonText, Qt::white);
        pal.setColor(QPalette::BrightText, Qt::red);
        pal.setColor(QPalette::Link, QColor(42,130,218));
        pal.setColor(QPalette::Highlight, QColor(42,130,218));
        pal.setColor(QPalette::HighlightedText, Qt::white);
    } else {
        pal = QApplication::palette();
    }
    QApplication::setPalette(pal);
    QIcon::setThemeSearchPaths({":/theme/Icons"});
    QIcon::setThemeName(m_currentTheme == Dark ? "okjbreeze-dark" : "okjbreeze");
}

void ThemeManager::detectSystemTheme() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    auto scheme = QColorScheme::systemColorScheme();
    m_currentTheme = (scheme == QColorScheme::Dark) ? Dark : Light;
#else
    QColor bg = QGuiApplication::palette().color(QPalette::Window);
    m_currentTheme = (bg.lightness() < 128) ? Dark : Light;
#endif
}

ThemeManager::Theme ThemeManager::currentTheme() const {
    return m_currentTheme;
}

QString ThemeManager::iconPath() const {
    return (m_currentTheme == Dark) ? ":/theme/Icons/okjbreeze-dark/" : ":/theme/Icons/okjbreeze/";
}

QPalette ThemeManager::palette() const {
    return QApplication::palette();
}

QColor ThemeManager::highlightColor() const {
    return (m_currentTheme == Dark) ? QColor(180,180,0) : QColor("yellow");
}
