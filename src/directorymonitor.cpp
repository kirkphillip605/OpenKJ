/*
 * Copyright (c) 2013-2019 Thomas Isaac Lightburn
 *
 *
 * This file is part of OpenKJ.
 *
 * OpenKJ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "directorymonitor.h"
#include "settings.h"
#include "karaokefileinfo.h"
#include "src/models/tablemodelkaraokesourcedirs.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtConcurrent>
#include <atomic>

extern Settings settings;

DirectoryMonitor::DirectoryMonitor(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this,       &DirectoryMonitor::onDirectoryChanged);
}

void DirectoryMonitor::addPaths(const QStringList &paths)
{
    if (!paths.isEmpty())
        m_watcher.addPaths(paths);
}

void DirectoryMonitor::removePaths(const QStringList &paths)
{
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
}

void DirectoryMonitor::onDirectoryChanged(const QString &path)
{
    if (!settings.dbDirectoryWatchEnabled())
        return;

    qInfo() << "DirectoryMonitor: directory changed:" << path;

    // Quickly collect candidate file paths (zip / cdg only).
    // This is cheap — no DB or tag I/O — so it's fine on the main thread.
    QStringList candidates;
    QDirIterator it(path);
    while (it.hasNext()) {
        const QString file = it.next();
        const QFileInfo fi(file);
        if (fi.isDir())
            continue;
        const QString suffix = fi.suffix().toLower();
        if (suffix != "zip" && suffix != "cdg")
            continue;
        candidates << file;
    }

    if (candidates.isEmpty())
        return;

    // Retrieve the SQLite file path so we can open a separate connection on
    // the worker thread (Qt SQL connections must not be shared between threads).
    const QString dbPath = QSqlDatabase::database().databaseName();

    // Run the heavy work (tag parsing + DB insert) on a thread-pool thread.
    QtConcurrent::run([this, candidates, dbPath]() {
        static std::atomic<int> s_counter{0};
        const QString connName =
            QStringLiteral("dirmon_%1").arg(s_counter.fetch_add(1));

        bool added = false;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
            db.setDatabaseName(dbPath);
            if (!db.open()) {
                qWarning() << "DirectoryMonitor: failed to open background DB connection";
                QSqlDatabase::removeDatabase(connName);
                return;
            }

            for (const QString &file : candidates) {
                // Check whether this file is already in the database.
                QSqlQuery checkQ(db);
                checkQ.prepare(
                    "SELECT EXISTS(SELECT 1 FROM dbsongs WHERE path = :p)");
                checkQ.bindValue(":p", file);
                if (!checkQ.exec() || !checkQ.next())
                    continue;
                if (checkQ.value(0).toBool())
                    continue; // already indexed

                qInfo() << "DirectoryMonitor: new file detected:" << file;

                // Parse artist / title / song-id from the filename.
                KaraokeFileInfo parser;
                parser.setFileName(file);
                parser.setPattern(SourceDir::SAT);

                const QFileInfo fi(file);
                const QString artist   = parser.getArtist();
                const QString title    = parser.getTitle();
                const QString discid   = parser.getSongId();
                const int     duration = parser.getDuration();
                const QString searchstr =
                    fi.completeBaseName() + ' ' + artist + ' ' + title + ' ' + discid;

                QSqlQuery insertQ(db);
                insertQ.prepare(
                    "INSERT OR IGNORE INTO dbSongs "
                    "(discid,artist,title,path,filename,duration,searchstring) "
                    "VALUES(:discid,:artist,:title,:path,:filename,:duration,:searchstring)");
                insertQ.bindValue(":discid",       discid);
                insertQ.bindValue(":artist",       artist);
                insertQ.bindValue(":title",        title);
                insertQ.bindValue(":path",         fi.filePath());
                insertQ.bindValue(":filename",     fi.completeBaseName());
                insertQ.bindValue(":duration",     duration);
                insertQ.bindValue(":searchstring", searchstr);

                if (insertQ.exec()) {
                    qInfo() << "DirectoryMonitor: added song to database:" << file;
                    added = true;
                } else {
                    qWarning() << "DirectoryMonitor: insert failed for" << file
                               << ":" << insertQ.lastError().text();
                }
            }

            db.close();
        }
        QSqlDatabase::removeDatabase(connName);

        // Signal is emitted from the worker thread; Qt auto-queues it to the
        // main thread because the connected slots live there.
        if (added)
            emit databaseUpdateComplete();
    });
}
