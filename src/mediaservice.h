#ifndef MEDIASERVICE_H
#define MEDIASERVICE_H

#include <QObject>
#include <QString>
#include <gst/gst.h>

class MediaService : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Idle,
        Loading,
        Playing,
        Paused,
        Stopped,
        Error
    };

    explicit MediaService(QObject *parent = nullptr);
    ~MediaService() override;

    void load(const QString &uri);
    void play();
    void pause();
    void stop();

signals:
    void stateChanged(MediaService::State state);
    void errorOccurred(const QString &message);

private:
    void setState(State state);
    void handleBusMessage(GstMessage *msg);
    void attemptRestart();

    GstElement *m_pipeline { nullptr };
    GstBus *m_bus { nullptr };
    State m_state { State::Idle };
    int m_retryCount { 0 };
    const int m_maxRetries { 3 };
};

#endif // MEDIASERVICE_H
