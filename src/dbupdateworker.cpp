#include "dbupdateworker.h"

DbUpdateWorker::DbUpdateWorker(QObject *parent) : QObject(parent) {
  m_updater = new DbUpdater();
  m_updater->moveToThread(&m_thread);
  connect(&m_thread, &QThread::finished, m_updater, &QObject::deleteLater);
  connect(m_updater, &DbUpdater::progressMessage, this,
          &DbUpdateWorker::progressMessage);
  connect(m_updater, &DbUpdater::stateChanged, this,
          &DbUpdateWorker::stateChanged);
  connect(m_updater, &DbUpdater::progressChanged, this,
          &DbUpdateWorker::progressChanged);
}

DbUpdateWorker::~DbUpdateWorker() { stop(); }

void DbUpdateWorker::start(const QList<QString> &paths,
                           DbUpdater::ProcessingOptions options) {
  connect(&m_thread, &QThread::started, this, [this, paths, options]() {
    bool success = m_updater->process(paths, options);
    emit finished(m_updater->getErrors(), m_updater->missingFilesCount(),
                  success);
  });
  m_thread.start();
}

void DbUpdateWorker::removeMissingFilesFromDatabase() {
  if (m_thread.isRunning()) {
    QMetaObject::invokeMethod(m_updater,
                              &DbUpdater::removeMissingFilesFromDatabase,
                              Qt::BlockingQueuedConnection);
  }
}

void DbUpdateWorker::stop() {
  if (m_thread.isRunning()) {
    m_thread.quit();
    m_thread.wait();
  }
}
