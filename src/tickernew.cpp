#include "tickernew.h"

#include <QQuickItem>
#include <QQmlContext>
#include <QUrl>

TickerDisplayWidget::TickerDisplayWidget(QWidget *parent)
    : QQuickWidget(parent)
{
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setSource(QUrl(QStringLiteral("qrc:/qml/Ticker.qml")));
}

void TickerDisplayWidget::setText(const QString &newText, bool /*force*/)
{
    m_currentText = newText;
    if (auto obj = rootObject())
        obj->setProperty("text", newText);
}

void TickerDisplayWidget::setSpeed(int speed)
{
    if (auto obj = rootObject())
        obj->setProperty("speed", speed);
}

void TickerDisplayWidget::stop()
{
    setTickerEnabled(false);
}

void TickerDisplayWidget::setTickerEnabled(bool enabled)
{
    setVisible(enabled);
}

void TickerDisplayWidget::refresh()
{
    if (auto obj = rootObject())
        obj->setProperty("text", m_currentText);
}

void TickerDisplayWidget::setFont(const QFont &font)
{
    QQuickWidget::setFont(font);
    if (auto obj = rootObject())
        obj->setProperty("font", font);
}

void TickerDisplayWidget::setPalette(const QPalette &palette)
{
    QQuickWidget::setPalette(palette);
    if (auto obj = rootObject())
        obj->setProperty("color", palette.color(foregroundRole()));
}
