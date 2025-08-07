#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QPalette>
#include <QColor>

class Settings;

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum Theme { System = 0, Dark, Light };

    static ThemeManager &instance();

    void initialize(Settings *settings);
    Theme currentTheme() const;
    QString iconPath() const;
    QPalette palette() const;
    QColor highlightColor() const;
    bool isDark() const { return m_currentTheme == Dark; }

private:
    explicit ThemeManager(QObject *parent = nullptr);
    void detectSystemTheme();

    Settings *m_settings{nullptr};
    Theme m_currentTheme{Light};
};

#endif // THEMEMANAGER_H
