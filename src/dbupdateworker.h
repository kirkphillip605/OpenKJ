#ifndef DBUPDATEWORKER_H
#define DBUPDATEWORKER_H

#include "dbupdater.h"
#include <QObject>
#include <QThread>

class DbUpdateWorker : public QObject {
  Q_OBJECT
public:
  explicit DbUpdateWorker(QObject *parent = nullptr);
  ~DbUpdateWorker();

  void start(const QList<QString> &paths, DbUpdater::ProcessingOptions options);

public slots:
  void removeMissingFilesFromDatabase();
  void stop();

signals:
  void progressMessage(const QString &msg);
  void stateChanged(QString state);
  void progressChanged(int progress, int max);
  void finished(QStringList errors, int missingFilesCount, bool success);

private:
  DbUpdater *m_updater{nullptr};
  QThread m_thread;
};

#endif // DBUPDATEWORKER_H
