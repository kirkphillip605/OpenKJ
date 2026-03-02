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

#ifndef DIRECTORYMONITOR_H
#define DIRECTORYMONITOR_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QStringList>

/**
 * @brief Watches a set of directories for new karaoke files and imports them
 *        into the database on a background thread to avoid blocking the UI.
 */
class DirectoryMonitor : public QObject
{
    Q_OBJECT

public:
    explicit DirectoryMonitor(QObject *parent = nullptr);

    /** Add paths to the watch list. Subdirectories should be added separately. */
    void addPaths(const QStringList &paths);

    /** Remove paths from the watch list. */
    void removePaths(const QStringList &paths);

    /** Returns all currently watched directories. */
    QStringList directories() const { return m_watcher.directories(); }

signals:
    /** Emitted (from the main thread) after one or more new songs were added. */
    void databaseUpdateComplete();

private slots:
    void onDirectoryChanged(const QString &path);

private:
    QFileSystemWatcher m_watcher;
};

#endif // DIRECTORYMONITOR_H
