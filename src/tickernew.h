#ifndef TICKERNEW_H
#define TICKERNEW_H

#include <QQuickWidget>
#include <QFont>
#include <QPalette>

class TickerDisplayWidget : public QQuickWidget
{
    Q_OBJECT
public:
    explicit TickerDisplayWidget(QWidget *parent = nullptr);
    void setText(const QString &newText, bool force = false);
    QSize sizeHint() const override { return QQuickWidget::sizeHint(); }
    void setSpeed(int speed);
    QString getCurrentText() { return m_currentText; }
    void stop();
    void setTickerEnabled(bool enabled);
    void refresh();
    void setFont(const QFont &font);
    void setPalette(const QPalette &palette);

private:
    QString m_currentText;
};

#endif // TICKERNEW_H
