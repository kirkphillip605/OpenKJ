#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThreadStorage>
#include <QElapsedTimer>

class DbManager
{
public:
    static DbManager &instance();

    void initialize(const QString &path);
    QSqlDatabase connection();
    void migrate();

    bool exec(QSqlQuery &query, qint64 thresholdMs = 50);
    bool execBatch(QSqlQuery &query, qint64 thresholdMs = 50);

private:
    DbManager();
    QString m_path;
    QThreadStorage<QSqlDatabase *> m_connections;
};

#endif // DBMANAGER_H
