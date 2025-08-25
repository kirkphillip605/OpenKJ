#include "mediaservice.h"

#include <QTimer>

MediaService::MediaService(QObject *parent)
    : QObject(parent)
{
    if (!gst_is_initialized()) {
        gst_init(nullptr, nullptr);
    }

    m_pipeline = gst_element_factory_make("playbin", "media_service_pipeline");
    m_bus = gst_element_get_bus(m_pipeline);

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        while (gst_bus_have_pending(m_bus)) {
            auto msg = gst_bus_pop(m_bus);
            handleBusMessage(msg);
            gst_message_unref(msg);
        }
    });
    timer->start(50);
}

MediaService::~MediaService()
{
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
    }
    if (m_bus) {
        gst_object_unref(m_bus);
    }
}

void MediaService::load(const QString &uri)
{
    if (!m_pipeline)
        return;

    setState(State::Loading);
    auto gstUri = gst_filename_to_uri(uri.toLocal8Bit(), nullptr);
    g_object_set(m_pipeline, "uri", gstUri, nullptr);
    g_free(gstUri);
    gst_element_set_state(m_pipeline, GST_STATE_READY);
}

void MediaService::play()
{
    if (!m_pipeline)
        return;

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
}

void MediaService::pause()
{
    if (!m_pipeline)
        return;

    gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
}

void MediaService::stop()
{
    if (!m_pipeline)
        return;

    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    m_retryCount = 0;
    setState(State::Stopped);
}

void MediaService::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(m_state);
}

void MediaService::handleBusMessage(GstMessage *msg)
{
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError *err;
        gchar *debug;
        gst_message_parse_error(msg, &err, &debug);
        QString message = QString::fromUtf8(err->message);
        g_error_free(err);
        g_free(debug);
        m_retryCount++;
        if (m_retryCount <= m_maxRetries) {
            attemptRestart();
        } else {
            setState(State::Error);
            emit errorOccurred(message);
        }
        break;
    }
    case GST_MESSAGE_EOS:
        stop();
        break;
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(m_pipeline)) {
            GstState newState;
            gst_message_parse_state_changed(msg, nullptr, &newState, nullptr);
            switch (newState) {
            case GST_STATE_PLAYING:
                setState(State::Playing);
                break;
            case GST_STATE_PAUSED:
                setState(State::Paused);
                break;
            case GST_STATE_READY:
                if (m_state == State::Loading)
                    setState(State::Paused);
                break;
            default:
                break;
            }
        }
        break;
    default:
        break;
    }
}

void MediaService::attemptRestart()
{
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_element_set_state(m_pipeline, GST_STATE_READY);
    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    setState(State::Loading);
}
