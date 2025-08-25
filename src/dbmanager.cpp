#include "dbmanager.h"
#include <QSqlError>
#include <QThread>
#include <QVariant>
#include <QStringList>
#include <QDebug>

DbManager &DbManager::instance()
{
    static DbManager inst;
    return inst;
}

DbManager::DbManager() = default;

void DbManager::initialize(const QString &path)
{
    m_path = path;
    connection();
}

QSqlDatabase DbManager::connection()
{
    if (!m_connections.hasLocalData()) {
        QString connName = QStringLiteral("conn_%1").arg((qulonglong)QThread::currentThreadId());
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(m_path);
        db.open();
        QSqlQuery pragma(db);
        pragma.exec("PRAGMA journal_mode=WAL");
        pragma.exec("PRAGMA synchronous=NORMAL");
        pragma.exec("PRAGMA foreign_keys=ON");
        pragma.exec("PRAGMA cache_size=-131072");
        m_connections.setLocalData(new QSqlDatabase(db));
    }
    return *m_connections.localData();
}

bool DbManager::exec(QSqlQuery &query, qint64 thresholdMs)
{
    QElapsedTimer t;
    t.start();
    bool ok = query.exec();
    qint64 elapsed = t.elapsed();
    if (elapsed > thresholdMs) {
        qWarning() << "Slow query:" << elapsed << "ms" << query.executedQuery() << query.boundValues();
    }
    if (!ok) {
        qWarning() << query.lastError();
    }
    return ok;
}

bool DbManager::execBatch(QSqlQuery &query, qint64 thresholdMs)
{
    QElapsedTimer t;
    t.start();
    bool ok = query.execBatch();
    qint64 elapsed = t.elapsed();
    if (elapsed > thresholdMs) {
        qWarning() << "Slow batch:" << elapsed << "ms" << query.executedQuery() << query.boundValues();
    }
    if (!ok) {
        qWarning() << query.lastError();
    }
    return ok;
}

void DbManager::migrate()
{
    QSqlDatabase db = connection();
    QSqlQuery query(db);
    int schemaVersion = 0;
    query.exec("PRAGMA user_version");
    if (query.first())
        schemaVersion = query.value(0).toInt();

    if (schemaVersion < 106) {
        QStringList stmts = {
            "CREATE TABLE IF NOT EXISTS dbSongs ( songid INTEGER PRIMARY KEY AUTOINCREMENT, Artist COLLATE NOCASE, Title COLLATE NOCASE, DiscId COLLATE NOCASE, 'Duration' INTEGER, path VARCHAR(700) NOT NULL UNIQUE, filename COLLATE NOCASE, searchstring TEXT, plays INT DEFAULT(0), lastplay TIMESTAMP)",
            "CREATE TABLE IF NOT EXISTS rotationSingers ( singerid INTEGER PRIMARY KEY AUTOINCREMENT, name COLLATE NOCASE UNIQUE, 'position' INTEGER NOT NULL, 'regular' LOGICAL DEFAULT(0), 'regularid' INTEGER, addts TIMESTAMP)",
            "CREATE TABLE IF NOT EXISTS queueSongs ( qsongid INTEGER PRIMARY KEY AUTOINCREMENT, singer INT, song INTEGER NOT NULL, artist INT, title INT, discid INT, path INT, keychg INT, played LOGICAL DEFAULT(0), 'position' INT)",
            "CREATE TABLE IF NOT EXISTS regularSingers ( regsingerid INTEGER PRIMARY KEY AUTOINCREMENT, Name COLLATE NOCASE UNIQUE, ph1 INT, ph2 INT, ph3 INT)",
            "CREATE TABLE IF NOT EXISTS regularSongs ( regsongid INTEGER PRIMARY KEY AUTOINCREMENT, regsingerid INTEGER NOT NULL, songid INTEGER NOT NULL, 'keychg' INTEGER, 'position' INTEGER)",
            "CREATE TABLE IF NOT EXISTS sourceDirs ( path VARCHAR(255) UNIQUE, pattern INTEGER, custompattern INTEGER)",
            "CREATE TABLE IF NOT EXISTS bmsongs ( songid INTEGER PRIMARY KEY AUTOINCREMENT, Artist COLLATE NOCASE, Title COLLATE NOCASE, path VARCHAR(700) NOT NULL UNIQUE, Filename COLLATE NOCASE, Duration TEXT, searchstring TEXT)",
            "CREATE TABLE IF NOT EXISTS bmplaylists ( playlistid INTEGER PRIMARY KEY AUTOINCREMENT, title COLLATE NOCASE NOT NULL UNIQUE)",
            "CREATE TABLE IF NOT EXISTS bmplsongs ( plsongid INTEGER PRIMARY KEY AUTOINCREMENT, playlist INT, position INT, Artist INT, Title INT, Filename INT, Duration INT, path INT)",
            "CREATE TABLE IF NOT EXISTS bmsrcdirs ( path NOT NULL)",
            "CREATE TABLE custompatterns ( patternid INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, artistregex TEXT, artistcapturegrp INT, titleregex TEXT, titlecapturegrp INT, discidregex TEXT, discidcapturegrp INT)",
            "CREATE UNIQUE INDEX IF NOT EXISTS idx_path ON dbsongs(path)",
            "CREATE TABLE dbSongHistory ( id INTEGER PRIMARY KEY AUTOINCREMENT, filepath TEXT, artist TEXT, title TEXT, songid TEXT, timestamp TIMESTAMP)",
            "CREATE INDEX IF NOT EXISTS idx_filepath ON dbSongHistory(filepath)",
            "CREATE TABLE historySingers(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE)",
            "CREATE TABLE historySongs(id INTEGER PRIMARY KEY AUTOINCREMENT, historySinger INT NOT NULL, filepath TEXT NOT NULL, artist TEXT, title TEXT, songid TEXT, keychange INT DEFAULT(0), plays INT DEFAULT(0), lastplay TIMESTAMP)",
            "CREATE INDEX IF NOT EXISTS idx_historySinger on historySongs(historySinger)"
        };
        for (const QString &stmt : stmts) {
            QSqlQuery q(db);
            q.prepare(stmt);
            exec(q);
        }
        QSqlQuery setver(db);
        setver.prepare("PRAGMA user_version = 106");
        exec(setver);
    }
}
